#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\Board.cpp"
#include "Board.h"
#include "HardwareConfig.h"
#include <SPI.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>

namespace routine::platform {
namespace {
constexpr int kPanelWidth = 240, kPanelHeight = 320;
}
Board::Board() : display_(&SPI, hardware::kCs, hardware::kDc, hardware::kReset) {}
bool Board::begin() {
  gpio_set_level(static_cast<gpio_num_t>(hardware::kBuzzer), hardware::kBuzzerActiveHigh ? 0 : 1);
  pinMode(hardware::kBuzzer, OUTPUT);
  buzzerLevel(false);
  for (const auto pin : hardware::kButtons) pinMode(pin, hardware::kButtonActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  constexpr size_t bytes = kPanelWidth * kPanelHeight * sizeof(uint16_t);
  pixels_ = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!pixels_) pixels_ = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!pixels_) return false;
  SPI.begin(hardware::kSclk, -1, hardware::kMosi, hardware::kCs);
  display_.init(kPanelWidth, kPanelHeight, SPI_MODE0);
  display_.setSPISpeed(hardware::kSpiFrequency);
  display_.setRotation(hardware::kRotation);
  display_.invertDisplay(hardware::kInvertDisplay);
  gestures_.begin(millis(),pressed(),inputContext_.load());
  inputQueue_=xQueueCreate(16,sizeof(input::InputEvent));
  if(!inputQueue_||xTaskCreate(inputTask,"buttons",3072,this,2,&inputTask_)!=pdPASS)return false;
  return true;
}
void Board::present() {
  if (pixels_) display_.drawRGBBitmap(0, 0, pixels_, kPanelWidth, kPanelHeight);
}
uint8_t Board::pressed() const {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < hardware::kButtonCount; ++i)
    if (digitalRead(hardware::kButtons[i]) == (hardware::kButtonActiveLow ? LOW : HIGH)) mask |= uint8_t(1u << i);
  return mask;
}
void Board::armButton() {
  if(inputTask_)vTaskSuspend(inputTask_);
  if(inputQueue_)xQueueReset(inputQueue_);
  gestures_.begin(millis(),pressed(),inputContext_.load());gestureBusy_=pressed();inputOverflow_=false;
  setupRequested_=false;
  if(inputTask_)vTaskResume(inputTask_);
}
void Board::inputTask(void* context){
  auto& self=*static_cast<Board*>(context);
  for(;;){
    const auto event=self.gestures_.update(millis(),self.pressed(),self.inputContext_.load());
#if ROUTINE_BLE_DEVELOPMENT
    if(self.gestures_.takeSetup())self.setupRequested_=true;
#endif
    self.gestureBusy_=self.gestures_.busy();
    if(event.intent!=input::Intent::None&&xQueueSend(self.inputQueue_,&event,0)!=pdTRUE)self.inputOverflow_=true;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
input::InputEvent Board::pollInput() {
  if(inputOverflow_.load()){armButton();return {};}
  input::InputEvent event;if(inputQueue_)xQueueReceive(inputQueue_,&event,0);return event;
}
SleepResult Board::lightSleep(uint32_t seconds) {
  if (inputBusy() || buzzer_.active() || !seconds || seconds > 86400)
    return {ESP_ERR_INVALID_STATE, ESP_SLEEP_WAKEUP_UNDEFINED};
  // Any of the three buttons wakes the device.
  esp_err_t error = ESP_OK;
  for (const auto pin : hardware::kButtons)
    if (error == ESP_OK) error = gpio_wakeup_enable(static_cast<gpio_num_t>(pin), hardware::kButtonActiveLow ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
  if (error == ESP_OK) error = esp_sleep_enable_gpio_wakeup();
  if (error == ESP_OK) error = esp_sleep_enable_timer_wakeup(uint64_t(seconds) * 1000000ULL);
  if (error != ESP_OK) {
    disableButtonWakeup();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    return {error, ESP_SLEEP_WAKEUP_UNDEFINED};
  }
  buzzerLevel(false);
  if(inputTask_)vTaskSuspend(inputTask_);
  display_.enableDisplay(false);
  display_.enableSleep(true);
  delay(120);  // ST7789 sleep-in/out command recovery time.
  error = esp_light_sleep_start();  // RAM/framebuffer survive this call.
  const auto cause = error == ESP_OK ? esp_sleep_get_wakeup_cause() : ESP_SLEEP_WAKEUP_UNDEFINED;
  // Capture a held wake button before LCD recovery. Releasing it is not a tap.
  armButton();
  display_.enableSleep(false);
  delay(120);
  present();
  display_.enableDisplay(true);
  disableButtonWakeup();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  return {error, cause};
}
void Board::disableButtonWakeup() {
  for (const auto pin : hardware::kButtons) gpio_wakeup_disable(static_cast<gpio_num_t>(pin));
}
void Board::buzzerLevel(bool active) {
  digitalWrite(hardware::kBuzzer, active == hardware::kBuzzerActiveHigh ? HIGH : LOW);
}
void Board::updateBuzzer() { buzzerLevel(buzzer_.update(millis())); }
void Board::beep(uint16_t durationMs) {
  buzzer_.start(millis(), durationMs);
  updateBuzzer();
}
}
