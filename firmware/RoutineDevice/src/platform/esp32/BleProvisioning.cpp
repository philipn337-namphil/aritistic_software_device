#include "BleProvisioning.h"
#include <Arduino.h>
#include <cstring>
#if ROUTINE_BLE_DEVELOPMENT
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace routine::platform {
namespace {
BleProvisioning* instance=nullptr;
}
// ESP32-S3 on arduino-esp32 3.3.x ships NimBLE only, so the GATT server uses
// the Arduino BLE wrapper. The wire contract (UUIDs, 20-byte writes, status
// strings) is unchanged from the earlier Bluedroid adapter.
struct BleCallbacks : BLEServerCallbacks, BLECharacteristicCallbacks {
  void onConnect(BLEServer* server,ble_gap_conn_desc* desc) override {
    auto* self=instance;if(!self||!self->active_)return;
    if(self->linked_){server->disconnect(desc->conn_handle);return;}
    self->connection_=desc->conn_handle;self->linked_=true;self->post({1});
  }
  void onDisconnect(BLEServer*,ble_gap_conn_desc* desc) override {
    auto* self=instance;if(!self||!self->active_||self->connection_!=desc->conn_handle)return;
    self->linked_=false;self->connection_=0xffff;self->post({2});
  }
  void onWrite(BLECharacteristic* characteristic,ble_gap_conn_desc* desc) override {
    auto* self=instance;if(!self||!self->active_||desc->conn_handle!=self->connection_)return;
    const size_t length=characteristic->getLength();
    if(!length||length>20){self->overflow_=true;return;}
    BleProvisioning::Event received;received.kind=3;received.size=uint8_t(length);
    std::memcpy(received.data,characteristic->getData(),length);
    if(xQueueSend(self->queue_,&received,0)!=pdTRUE)self->overflow_=true;
    provisioning::erase(&received,sizeof(received));
  }
};
namespace {
BleCallbacks callbacks;
}
void BleProvisioning::post(const Event& event){if(!queue_||xQueueSend(queue_,&event,0)!=pdTRUE)overflow_=true;}
void BleProvisioning::advertise(){
  if(!active_||linked_)return;
  if(!BLEDevice::getAdvertising()->start())failed_=true;
}
bool BleProvisioning::start(){
  if(active_)return false;
  if(!queue_)queue_=xQueueCreate(24,sizeof(Event));if(!queue_)return false;
  instance=this;frame_.clear();pending_.clear();joining_=false;finishAt_=0;beganAt_=millis();
  linked_=false;failed_=false;overflow_=false;connection_=0xffff;xQueueReset(queue_);
  std::snprintf(name_,sizeof(name_),"YESO-%06llX",static_cast<unsigned long long>(ESP.getEfuseMac()&0xffffff));
  active_=true;status_="STARTING";
  // stop() deinitialises the stack, so every start builds a fresh server.
  if(!BLEDevice::init(name_)){failed_=true;return false;}
  server_=BLEDevice::createServer();
  if(!server_){failed_=true;return false;}
  server_->setCallbacks(&callbacks);
  server_->advertiseOnDisconnect(false);
  BLEService* service=server_->createService(provisioning::kServiceUuid);
  if(!service){failed_=true;return false;}
  BLECharacteristic* config=service->createCharacteristic(provisioning::kConfigUuid,BLECharacteristic::PROPERTY_WRITE);
  statusCharacteristic_=service->createCharacteristic(provisioning::kStatusUuid,BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_NOTIFY);
  if(!config||!statusCharacteristic_){failed_=true;return false;}
  config->setCallbacks(&callbacks);
  statusCharacteristic_->setValue("WAITING");
  service->start();
  BLEAdvertising* advertising=BLEDevice::getAdvertising();
  advertising->addServiceUUID(provisioning::kServiceUuid);
  advertising->setScanResponse(true);  // Name goes to the scan response.
  status_="WAITING";advertise();
  return !failed_;
}
void BleProvisioning::report(const char* status){
  status_=status;if(!statusCharacteristic_)return;
  statusCharacteristic_->setValue(status);
  if(linked_)statusCharacteristic_->notify();  // Sent only to subscribed clients.
}
void BleProvisioning::stop(NetworkClock& clock){
  if(!active_)return;active_=false;
  if(joining_)clock.cancelSetup();joining_=false;
  if(BLEDevice::getInitialized()){
    if(linked_&&server_)server_->disconnect(connection_);
    BLEDevice::deinit(false);  // Deletes server/advertising and stops the controller.
  }
  server_=nullptr;statusCharacteristic_=nullptr;
  Event queued;while(xQueueReceive(queue_,&queued,0)==pdTRUE)provisioning::erase(&queued,sizeof(queued));
  frame_.clear();pending_.clear();linked_=false;status_="OFF";clock.requestSync();
}
void BleProvisioning::tick(NetworkClock& clock,DeviceNetwork& network){
  if(!active_)return;
  if(failed_||uint32_t(millis()-beganAt_)>=300000||(finishAt_&&int32_t(millis()-finishAt_)>=0)){stop(clock);return;}
  if(overflow_.exchange(false)){
    Event queued;while(xQueueReceive(queue_,&queued,0)==pdTRUE)provisioning::erase(&queued,sizeof(queued));
    frame_.clear();report("PARSE_ERROR");
    if(linked_&&server_)server_->disconnect(connection_);
  }
  Event event;
  while(xQueueReceive(queue_,&event,0)==pdTRUE){
    if(event.kind==1||event.kind==2){frame_.clear();if(event.kind==2&&!joining_&&!finishAt_)advertise();}
    else if(event.kind==3&&!joining_&&!finishAt_){
      const auto result=frame_.push(event.data,event.size,millis());
      if(result==provisioning::FrameResult::Invalid)report("PARSE_ERROR");
      else if(result==provisioning::FrameResult::Complete){
        if(!provisioning::parseConfig(frame_.bytes(),frame_.size(),pending_,true,kDevelopmentServerUrl)||!network.acceptProvisioning(pending_)){
          pending_.clear();report("PARSE_ERROR");
        }else{
          report("RECEIVED");joining_=clock.configure(pending_.ssid,pending_.password);
          provisioning::erase(pending_.password,sizeof(pending_.password));
          if(!joining_){pending_.clear();report("WIFI_FAIL");}
        }
        frame_.clear();
      }
    }
    provisioning::erase(&event,sizeof(event));
  }
  if(frame_.expire(millis()))report("PARSE_ERROR");
  if(joining_){
    const auto state=clock.setupState();
    if(state==provisioning::JoinState::Committed){
      joining_=false;const bool queued=network.finishProvisioning(pending_);pending_.clear();
      report("WIFI_OK");if(!queued)Serial.println("BLE: Wi-Fi saved; API pairing requires recovery");
      finishAt_=millis()+3000; // Allow notify delivery before stopping the radio.
    }else if(state==provisioning::JoinState::Failed){joining_=false;pending_.clear();report("WIFI_FAIL");if(!linked_)advertise();}
  }
}
}
#else
namespace routine::platform {
bool BleProvisioning::start(){return false;}
void BleProvisioning::stop(NetworkClock&){}
void BleProvisioning::tick(NetworkClock&,DeviceNetwork&){}
}
#endif
