#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEAddress.h>
#include <BLEAdvertisedDevice.h>
#include <BLE2902.h>

// === Configuration BLE ===
static BLEUUID serviceUUID((uint16_t)0xec00);
static BLEUUID charUUID((uint16_t)0xec0e);

#define PROTOCOLE_ID 2
#define ADC_PIN 35  // Broche ADC connectée au pont diviseur

// Variables de connexion
static bool doConnect = false;
static bool connected = false;
static BLEAddress* pServerAddress;
static BLEClient* pClient;
static BLERemoteCharacteristic* pRemoteCharacteristic;

// Températures fictives
float temperature_values[] = {20.0, 21.5, 22.3, 23.1, 24.0, 25.2, 26.1, 27.5, 28.0, 29.3};
int temp_index = 0;

// === Scan BLE ===
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == "FiPy Server") {
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    }
  }
};

// === Connexion au serveur BLE ===
void connectToServer() {
  Serial.println("🔗 Connexion au serveur BLE...");

  pClient = BLEDevice::createClient();
  pClient->connect(*pServerAddress);

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("❌ Service BLE introuvable");
    pClient->disconnect();
    return;
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("❌ Caractéristique BLE introuvable");
    pClient->disconnect();
    return;
  }

  connected = true;
  Serial.println("✅ Connecté au serveur BLE");
}

// === Lecture de l'état de batterie ===
uint8_t getBatteryPercentage() {
  int raw = analogRead(ADC_PIN);  // 0 → 4095
  float voltage = (raw / 4095.0) * 3.3 * 2.0;  // facteur 2 → diviseur de tension

  int percent = (int)((voltage - 3.3) / (4.2 - 3.3) * 100.0);
  percent = constrain(percent, 0, 100);
  return (uint8_t)percent;
}

void setup() {
  Serial.begin(115200);
  Serial.println("🔧 Initialisation du client BLE...");

  analogReadResolution(12); // pour ESP32 (4096 niveaux)
  pinMode(ADC_PIN, INPUT);

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
  if (doConnect) {
    connectToServer();
    doConnect = false;
  }

  if (connected) {
    float temp = temperature_values[temp_index];
    int8_t rssi = constrain(pClient->getRssi(), -128, 127);
    uint8_t battery = getBatteryPercentage();

    // Trame : float temp + int8 rssi + uint8 protocole_id + uint8 battery
    uint8_t buffer[7];
    memcpy(&buffer[0], &temp, sizeof(float));  // 4 octets
    buffer[4] = (uint8_t)rssi;                 // 1 octet
    buffer[5] = PROTOCOLE_ID;                  // 1 octet
    buffer[6] = battery;                       // 1 octet

    pRemoteCharacteristic->writeValue(buffer, sizeof(buffer));
    Serial.printf("📤 Envoyé ➜ Temp: %.2f °C | RSSI: %d | ID: %d | Batterie: %d %%\n", temp, rssi, PROTOCOLE_ID, battery);

    temp_index = (temp_index + 1) % (sizeof(temperature_values) / sizeof(float));
    delay(5000);
  }

  delay(200);
}
