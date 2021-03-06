#include "Arduino.h"
#include "BLE2902.h"
#include "BLEClient.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "driver/rtc_io.h"
#include <iostream>
#include <sstream>
#include <string>

using namespace std;
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
// int i;
int pinValue;
bool locked;
volatile byte state = LOW;
int timeoutTimer = 0;
const int LOCK = 26;  // I25
const int LIGHT = 27; // I32
stringstream tempMsg;
string rxValue; // Recevied string from connected device

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define NORMAL_PULSE 1450 // Pulse length for lock in ms

void pulse(int pinNumber) {
  digitalWrite(pinNumber, HIGH);
  delay(NORMAL_PULSE);
  digitalWrite(pinNumber, LOW);
}

void setLockStatus() {
  Serial.println("Här är jag");
  locked = !locked;
  Serial.println(locked);
}

void warn() {
  int timer = 0;
  digitalWrite(LIGHT, HIGH);
  while (timer < 10) {
    if (locked) {
      break;
    }
    delay(1000);
    timer++;
  }
  digitalWrite(LIGHT, LOW);
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
  case 1:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case 2:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case 3:
    Serial.println("Wakeup caused by timer");
    break;
  case 4:
    Serial.println("Wakeup caused by touchpad");
    break;
  case 5:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.println("Wakeup was not caused by deep sleep");
    break;
  }
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    timeoutTimer = 0;
    deviceConnected = true;
    pulse(LOCK);
    // Serial.println(locked);
  };

  void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // loads retrived data to a string
    rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      // Unlocks door if value retrived is "A"
      if (rxValue.find("A") != -1) {
        // Serial.println("Jag kom hit");
        pulse(LOCK);
      }
    }
  }
};

void setup() {
  rtc_gpio_init(GPIO_NUM_25);
  rtc_gpio_pullup_dis(GPIO_NUM_25);
  rtc_gpio_pulldown_en(GPIO_NUM_25);
  rtc_gpio_set_direction(GPIO_NUM_25, RTC_GPIO_MODE_INPUT_ONLY);
  Serial.begin(115200);
  pinMode(LOCK, OUTPUT);
  pinMode(LIGHT, OUTPUT);
  digitalWrite(LIGHT, 1);
  // Sets interupt pin and mode
  // attachInterrupt(digitalPinToInterrupt(STATUS_LOCK), setLockStatus, CHANGE);
  // Enable deepsleep

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_25, 1); // 1 = High, 0 = Low
  // Create the BLE Device
  BLEDevice::init("Boxly"); // Give it a name
  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new MyCallbacks());
  // Start the service
  pService->start();
  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println(rtc_gpio_is_valid_gpio(GPIO_NUM_25));
  Serial.println("Waiting for connection... PlatformIO");
}
// It would be good to add a condtition to only notify when a change has
// happend, to maximize power savings.
// Preferably at the first if-statment!
void loop() {
  timeoutTimer++;
  // Serial.println(rtc_gpio_get_level(GPIO_NUM_25));
  if (timeoutTimer == 300) {
    // Serial.println("Going to sleep");
    delay(1000);
    esp_deep_sleep_start();
  }
  delay(500);
}
