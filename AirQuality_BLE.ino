#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include "Adafruit_CCS811.h"
#include "Adafruit_SHT31.h"

// ================= BLE UUIDs (Nordic UART Service) =================
#define SERVICE_UUID      "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ================= SENSORS =================
// --- IDENTICAL TO ORIGINAL ---
#define MQ135_PIN 34
#define MQ136_PIN 35

#define RXD2 16
#define TXD2 17
HardwareSerial pmsSerial(2);

Adafruit_CCS811 ccs;
Adafruit_SHT31  sht31 = Adafruit_SHT31();

bool ccsOK  = false;
bool shtOK  = false;
int  pm25   = 0;

// ================= BLE GLOBALS =================
BLEServer*         pServer         = NULL;
BLECharacteristic* pTxChar         = NULL;
BLECharacteristic* pRxChar         = NULL;
bool               deviceConnected = false;
bool               oldConnected    = false;

// ================= PMS READ =================
// --- IDENTICAL TO ORIGINAL ---
void readPMS() {
  if (pmsSerial.available() >= 32) {
    uint8_t buf[32];
    pmsSerial.readBytes(buf, 32);
    if (buf[0] == 0x42 && buf[1] == 0x4D) {
      pm25 = (buf[12] << 8) | buf[13];
    }
  }
}

// ================= JSON (same logic as original handleData) =================
// --- IDENTICAL TO ORIGINAL ---
String buildJSON() {
  int   mq135 = analogRead(MQ135_PIN);
  int   mq136 = analogRead(MQ136_PIN);
  float temp  = 0, hum = 0;
  int   eco2  = 0, tvoc = 0;

  if (shtOK) {
    temp = sht31.readTemperature();
    hum  = sht31.readHumidity();
    if (isnan(temp)) temp = 0;
    if (isnan(hum))  hum  = 0;
  }

  if (ccsOK && ccs.available() && !ccs.readData()) {
    eco2 = ccs.geteCO2();
    tvoc = ccs.getTVOC();
  }

  String json = "{";
  json += "\"mq135\":"  + String(mq135)   + ",";
  json += "\"mq136\":"  + String(mq136)   + ",";
  json += "\"pm25\":"   + String(pm25)    + ",";
  json += "\"eco2\":"   + String(eco2)    + ",";
  json += "\"tvoc\":"   + String(tvoc)    + ",";
  json += "\"temp\":"   + String(temp, 1) + ",";
  json += "\"hum\":"    + String(hum,  1);
  json += "}\n";
  return json;
}

// ================= BLE SEND (chunked 20 bytes) =================
void bleSend(String data) {
  if (!deviceConnected) return;
  int len = data.length();
  int i   = 0;
  while (i < len) {
    int chunk = min(20, len - i);
    pTxChar->setValue((uint8_t*)(data.c_str() + i), chunk);
    pTxChar->notify();
    i += chunk;
    delay(10);
  }
}

// ================= BLE CALLBACKS =================
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE Client Connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE Client Disconnected");
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) {
    String rxValue = pChar->getValue().c_str();
    rxValue.trim();
    if (rxValue.indexOf("GET /data") >= 0) {
      bleSend(buildJSON());
    }
  }
};

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\nStarting ESP32...");

  // — BLE —
  BLEDevice::init("AirQuality");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxChar = pService->createCharacteristic(CHARACTERISTIC_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxChar->addDescriptor(new BLE2902());

  pRxChar = pService->createCharacteristic(CHARACTERISTIC_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxChar->setCallbacks(new RxCallbacks());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE started — device name: AirQuality");

  // — I2C — IDENTICAL TO ORIGINAL
  Wire.begin(21, 22);

  ccsOK = ccs.begin();
  Serial.println(ccsOK ? "CCS811 OK" : "CCS811 NOT DETECTED");

  shtOK = sht31.begin(0x44);
  Serial.println(shtOK ? "SHT31 OK" : "SHT31 NOT DETECTED");

  // — PMS UART — IDENTICAL TO ORIGINAL
  pmsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("PMS3003 UART Started");

  Serial.println("System Ready");
}

// ================= LOOP =================
unsigned long lastSend = 0;

void loop() {
  readPMS();  // IDENTICAL TO ORIGINAL

  // Auto-push data every 2.5s when phone is connected
  if (deviceConnected && millis() - lastSend > 2500) {
    bleSend(buildJSON());
    lastSend = millis();
  }

  // Restart advertising after disconnect
  if (!deviceConnected && oldConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Restarting BLE advertising...");
    oldConnected = false;
  }
  if (deviceConnected && !oldConnected) {
    oldConnected = true;
  }
}
