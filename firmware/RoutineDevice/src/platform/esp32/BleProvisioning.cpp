#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\BleProvisioning.cpp"
#include "BleProvisioning.h"
#include <Arduino.h>
#include <cstring>
#if ROUTINE_BLE_DEVELOPMENT
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gatt_common_api.h>

namespace routine::platform {
namespace {
BleProvisioning* instance=nullptr;
constexpr uint8_t kService[16]={0x00,0x0a,0x1d,0x2c,0x4b,0x8e,0x6f,0x9a,0x1b,0x4c,0x2e,0x5d,0x01,0x00,0x3a,0x7f};
constexpr uint8_t kConfig[16]={0x00,0x0a,0x1d,0x2c,0x4b,0x8e,0x6f,0x9a,0x1b,0x4c,0x2e,0x5d,0x02,0x00,0x3a,0x7f};
constexpr uint8_t kStatus[16]={0x00,0x0a,0x1d,0x2c,0x4b,0x8e,0x6f,0x9a,0x1b,0x4c,0x2e,0x5d,0x03,0x00,0x3a,0x7f};
const uint16_t serviceDeclaration=ESP_GATT_UUID_PRI_SERVICE,charDeclaration=ESP_GATT_UUID_CHAR_DECLARE,cccd=ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
const uint8_t writeProperty=ESP_GATT_CHAR_PROP_BIT_WRITE,statusProperties=ESP_GATT_CHAR_PROP_BIT_NOTIFY|ESP_GATT_CHAR_PROP_BIT_READ;
uint8_t empty[20]{},subscription[2]{};
esp_gatts_attr_db_t database[]={
 {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(&serviceDeclaration)),ESP_GATT_PERM_READ,16,16,const_cast<uint8_t*>(kService)}},
 {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(&charDeclaration)),ESP_GATT_PERM_READ,1,1,const_cast<uint8_t*>(&writeProperty)}},
 {{ESP_GATT_RSP_BY_APP},{ESP_UUID_LEN_128,const_cast<uint8_t*>(kConfig),ESP_GATT_PERM_WRITE,20,0,empty}},
 {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(&charDeclaration)),ESP_GATT_PERM_READ,1,1,const_cast<uint8_t*>(&statusProperties)}},
 {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_128,const_cast<uint8_t*>(kStatus),ESP_GATT_PERM_READ,20,0,empty}},
 {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,reinterpret_cast<uint8_t*>(const_cast<uint16_t*>(&cccd)),ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,2,2,subscription}}
};
}
void BleProvisioning::post(const Event& event){if(!queue_||xQueueSend(queue_,&event,0)!=pdTRUE)overflow_=true;}
void BleProvisioning::advertise(){
  if(!active_||linked_||advertisementReady_.load()!=7)return;
  esp_ble_adv_params_t parameters{};parameters.adv_int_min=0x100;parameters.adv_int_max=0x180;
  parameters.adv_type=ADV_TYPE_IND;parameters.own_addr_type=BLE_ADDR_TYPE_PUBLIC;
  parameters.channel_map=ADV_CHNL_ALL;parameters.adv_filter_policy=ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  if(esp_ble_gap_start_advertising(&parameters)!=ESP_OK)failed_=true;
}
void BleProvisioning::gap(esp_gap_ble_cb_event_t event,esp_ble_gap_cb_param_t* param){
  auto* self=instance;if(!self||!self->active_)return;
  if(event==ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT){if(param->adv_data_raw_cmpl.status!=ESP_BT_STATUS_SUCCESS)self->failed_=true;else self->advertisementReady_.fetch_or(1);}
  else if(event==ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT){if(param->scan_rsp_data_raw_cmpl.status!=ESP_BT_STATUS_SUCCESS)self->failed_=true;else self->advertisementReady_.fetch_or(2);}
  else if(event==ESP_GAP_BLE_ADV_START_COMPLETE_EVT){if(param->adv_start_cmpl.status!=ESP_BT_STATUS_SUCCESS)self->failed_=true;return;}
  else return;
  self->advertise();
}
void BleProvisioning::gatt(esp_gatts_cb_event_t event,esp_gatt_if_t interface,esp_ble_gatts_cb_param_t* param){
  auto* self=instance;if(!self||!self->active_)return;
  switch(event){
    case ESP_GATTS_REG_EVT:
      if(param->reg.status!=ESP_GATT_OK){self->failed_=true;break;}
      self->interface_=interface;
      if(esp_ble_gatts_create_attr_tab(database,interface,6,0)!=ESP_OK)self->failed_=true;
      break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
      if(param->add_attr_tab.status!=ESP_GATT_OK||param->add_attr_tab.num_handle!=6){self->failed_=true;break;}
      std::memcpy(self->handles_,param->add_attr_tab.handles,sizeof(self->handles_));
      if(esp_ble_gatts_start_service(self->handles_[0])!=ESP_OK)self->failed_=true;
      break;
    case ESP_GATTS_START_EVT:
      if(param->start.status!=ESP_GATT_OK)self->failed_=true;
      else {self->advertisementReady_.fetch_or(4);self->advertise();}break;
    case ESP_GATTS_CONNECT_EVT:
      if(self->linked_){esp_ble_gatts_close(interface,param->connect.conn_id);break;}
      self->connection_=param->connect.conn_id;self->linked_=true;self->subscribed_=false;self->post({1});break;
    case ESP_GATTS_DISCONNECT_EVT:
      if(self->connection_!=param->disconnect.conn_id)break;
      self->linked_=false;self->subscribed_=false;self->connection_=0xffff;self->post({2});break;
    case ESP_GATTS_WRITE_EVT: {
      if(param->write.conn_id!=self->connection_)break;
      if(param->write.handle==self->handles_[5]){
        self->subscribed_=param->write.len==2&&param->write.value[0]==1&&param->write.value[1]==0;break;
      }
      if(param->write.handle!=self->handles_[2])break;
      esp_gatt_status_t result=ESP_GATT_OK;
      if(param->write.is_prep||param->write.offset||!param->write.need_rsp)result=ESP_GATT_REQ_NOT_SUPPORTED;
      else if(!param->write.len||param->write.len>20)result=ESP_GATT_INVALID_ATTR_LEN;
      else {
        Event received;received.kind=3;received.size=uint8_t(param->write.len);std::memcpy(received.data,param->write.value,received.size);
        if(xQueueSend(self->queue_,&received,0)!=pdTRUE){self->overflow_=true;result=ESP_GATT_INSUF_RESOURCE;}
        provisioning::erase(&received,sizeof(received));
      }
      if(result!=ESP_GATT_OK)self->overflow_=true;
      if(param->write.need_rsp)esp_ble_gatts_send_response(interface,param->write.conn_id,param->write.trans_id,result,nullptr);
      break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
      esp_ble_gatts_send_response(interface,param->exec_write.conn_id,param->exec_write.trans_id,ESP_GATT_REQ_NOT_SUPPORTED,nullptr);break;
    default:break;
  }
}
bool BleProvisioning::start(){
  if(active_)return false;
  if(!queue_)queue_=xQueueCreate(24,sizeof(Event));if(!queue_)return false;
  instance=this;frame_.clear();pending_.clear();joining_=false;finishAt_=0;beganAt_=millis();
  advertisementReady_=0;linked_=false;failed_=false;overflow_=false;subscribed_=false;connection_=0xffff;
  std::memset(handles_,0,sizeof(handles_));xQueueReset(queue_);subscription[0]=subscription[1]=0;
  std::snprintf(name_,sizeof(name_),"YESO-%06llX",static_cast<unsigned long long>(ESP.getEfuseMac()&0xffffff));
  active_=true;status_="STARTING";
  esp_bt_controller_config_t config=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if(esp_bt_controller_init(&config)!=ESP_OK||esp_bt_controller_enable(ESP_BT_MODE_BLE)!=ESP_OK||esp_bluedroid_init()!=ESP_OK||esp_bluedroid_enable()!=ESP_OK){failed_=true;return false;}
  if(esp_ble_gap_register_callback(gap)!=ESP_OK||esp_ble_gatts_register_callback(gatt)!=ESP_OK||esp_ble_gatts_app_register(17)!=ESP_OK){failed_=true;return false;}
  esp_ble_gap_set_device_name(name_);
  uint8_t advertisement[21]={2,ESP_BLE_AD_TYPE_FLAG,ESP_BLE_ADV_FLAG_GEN_DISC|ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,17,ESP_BLE_AD_TYPE_128SRV_CMPL};
  std::memcpy(advertisement+5,kService,16);
  uint8_t scanResponse[18]{};const size_t length=std::strlen(name_);scanResponse[0]=uint8_t(length+1);scanResponse[1]=ESP_BLE_AD_TYPE_NAME_CMPL;std::memcpy(scanResponse+2,name_,length);
  if(esp_ble_gap_config_adv_data_raw(advertisement,sizeof(advertisement))!=ESP_OK||esp_ble_gap_config_scan_rsp_data_raw(scanResponse,length+2)!=ESP_OK)failed_=true;
  status_="WAITING";return !failed_;
}
void BleProvisioning::report(const char* status){
  status_=status;if(!handles_[4])return;
  auto* bytes=reinterpret_cast<uint8_t*>(const_cast<char*>(status));const size_t length=std::strlen(status);
  esp_ble_gatts_set_attr_value(handles_[4],length,bytes);
  if(linked_&&subscribed_)esp_ble_gatts_send_indicate(interface_,connection_,handles_[4],length,bytes,false);
}
void BleProvisioning::stop(NetworkClock& clock){
  if(!active_)return;active_=false;
  if(joining_)clock.cancelSetup();joining_=false;
  if(esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_ENABLED){
    esp_ble_gap_stop_advertising();if(linked_)esp_ble_gatts_close(interface_,connection_);
    esp_bluedroid_disable();
  }
  if(esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_INITIALIZED)esp_bluedroid_deinit();
  if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_ENABLED)esp_bt_controller_disable();
  if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_INITED)esp_bt_controller_deinit();
  Event queued;while(xQueueReceive(queue_,&queued,0)==pdTRUE)provisioning::erase(&queued,sizeof(queued));
  frame_.clear();pending_.clear();linked_=false;status_="OFF";clock.requestSync();
}
void BleProvisioning::tick(NetworkClock& clock,DeviceNetwork& network){
  if(!active_)return;
  if(failed_||uint32_t(millis()-beganAt_)>=300000||(finishAt_&&int32_t(millis()-finishAt_)>=0)){stop(clock);return;}
  if(overflow_.exchange(false)){
    Event queued;while(xQueueReceive(queue_,&queued,0)==pdTRUE)provisioning::erase(&queued,sizeof(queued));
    frame_.clear();report("PARSE_ERROR");
    if(linked_)esp_ble_gatts_close(interface_,connection_);
  }
  Event event;
  while(xQueueReceive(queue_,&event,0)==pdTRUE){
    if(event.kind==1||event.kind==2){frame_.clear();if(event.kind==2&&!joining_&&!finishAt_)advertise();}
    else if(event.kind==3&&!joining_&&!finishAt_){
      const auto result=frame_.push(event.data,event.size,millis());
      if(result==provisioning::FrameResult::Invalid)report("PARSE_ERROR");
      else if(result==provisioning::FrameResult::Complete){
        if(!provisioning::parseConfig(frame_.bytes(),frame_.size(),pending_,true)||!network.acceptProvisioning(pending_)){
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
