#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEAddress.h>
#include <BLEAdvertisedDevice.h>
#include <BLE2902.h>
#include <stdlib.h>  // Pour random()

// === Configuration BLE ===
static BLEUUID serviceUUID((uint16_t)0xec00);
static BLEUUID charUUID((uint16_t)0xec0e);

#define FIRST_PROTOCOLE_ID 2
#define SECOND_PROTOCOLE_ID 3

#define ADC_PIN 35  // Broche ADC connectée au pont diviseur

// Variables de connexion
static bool doConnect = false;
static bool connected = false;
static BLEAddress* pServerAddress;
static BLEClient* pClient;
static BLERemoteCharacteristic* pRemoteCharacteristic;

// Températures fictives
float temperature_values[] = {30.0, 30.5, 31.1, 31.8, 32.3, 33.0, 33.5, 34.2, 35.0, 35.6};
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
  Serial.println("🔧 Initialisation du client BLE (PROTOCOLE_ID 3)...");

  analogReadResolution(12); // pour ESP32 (4096 niveaux)
  pinMode(ADC_PIN, INPUT);

  BLEDevice::init("");
}
void loop() {
  if (!connected) {
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);

    int retry = 0;
    while (!doConnect && retry < 10) {
      delay(500);
      retry++;
    }

    if (doConnect) {
      connectToServer();
      doConnect = false;
    }
  }

  if (connected && pRemoteCharacteristic != nullptr) {
    float temp = temperature_values[temp_index];
    int8_t rssi = constrain(pClient->getRssi(), -128, 127);
    uint8_t battery = getBatteryPercentage();

    // Trame 1 : protocole_id = 2
    uint8_t buffer1[7];
    memcpy(&buffer1[0], &temp, sizeof(float));
    buffer1[4] = (uint8_t)rssi;
    buffer1[5] = FIRST_PROTOCOLE_ID;
    buffer1[6] = battery;

    pRemoteCharacteristic->writeValue(buffer1, sizeof(buffer1));
    Serial.printf("📤 Envoyé ➜ Temp: %.2f °C | RSSI: %d | ID: %d | Batterie: %d %%\n", temp, rssi, FIRST_PROTOCOLE_ID, battery);

    delay(300);  // Petite pause

    // Trame 2 : protocole_id = 3
    uint8_t buffer2[7];
    memcpy(&buffer2[0], &temp, sizeof(float));
    buffer2[4] = (uint8_t)rssi;
    buffer2[5] = SECOND_PROTOCOLE_ID;
    buffer2[6] = battery;

    pRemoteCharacteristic->writeValue(buffer2, sizeof(buffer2));
    Serial.printf("📤 Envoyé ➜ Temp: %.2f °C | RSSI: %d | ID: %d | Batterie: %d %%\n", temp, rssi, SECOND_PROTOCOLE_ID, battery);

    temp_index = (temp_index + 1) % (sizeof(temperature_values) / sizeof(float));

    // Déconnexion
    pClient->disconnect();
    connected = false;
    Serial.println("🔌 Déconnecté du serveur BLE");
  }

  delay(random(5000, 10000));  // Pause entre les envois
}
