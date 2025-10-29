#include <WiFi.h>

// 🛜 Paramètres du réseau Wi-Fi
const char* ssid = "IoT IMT Nord Europe";
const char* password = "72Hin@R*";
//const char* ssid = "AndroidAP3558";
//const char* password = "123456789";

// 🖧 Adresse IP et port de la passerelle (FiPy)
const char* server_ip = "10.120.2.158";
//const char* server_ip = "192.168.43.22";
const int server_port = 1234;

// 🔢 Températures simulées
float temperatures[] = {25.1, 23.5, 29.3, 15.1, 12.0, 11.2, 13.1, 26.5, 32.0, 30.3};
int index_temp = 0;

// 📡 ID du protocole Wi-Fi
const int protocole_id = 1;

// ⚡ ADC pour lire la tension batterie (GPIO35 recommandé)
const int BATTERY_ADC_PIN = 35;  // GPIO35 (entrée analogique)

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialisation ADC
  analogReadResolution(12); // Résolution 12 bits : 0-4095
  analogSetAttenuation(ADC_11db); // Plage de 0 à ~3.6V

  // Connexion Wi-Fi
  Serial.println("🔌 Connexion au réseau Wi-Fi...");
  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(1000);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connecté au Wi-Fi !");
    Serial.print("🖧 Adresse IP locale : ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Connexion Wi-Fi échouée. Redémarrage dans 10s...");
    delay(10000);
    ESP.restart();
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;

    if (client.connect(server_ip, server_port)) {
      float temp = temperatures[index_temp];
      int rssi = WiFi.RSSI();
      int battery_percent = lire_batterie();

      // ✉️ Préparer le message
      String message = "";
      message += "Temperature: " + String(temp) + "\n";
      message += "RSSI: " + String(rssi) + "\n";
      message += "Protocole_ID: " + String(protocole_id) + "\n";
      message += "Battery_State: " + String(battery_percent) + "\n";

      // 📤 Envoyer
      client.print(message);

      Serial.println("✅ Données envoyées au serveur :");
      Serial.println(message);

      index_temp = (index_temp + 1) % (sizeof(temperatures) / sizeof(float));
      client.stop();
    } else {
      Serial.println("❌ Échec de la connexion au serveur.");
    }
  } else {
    Serial.println("❗ Wi-Fi perdu. Reconnexion...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }

  delay(5000);  // ⏱️ Attente 5 secondes
}

// 🔋 Fonction pour lire et convertir la tension batterie en pourcentage
int lire_batterie() {
  int raw = analogRead(BATTERY_ADC_PIN);
  float tension_mesuree = (raw / 4095.0) * 3.6; // en volts
  float tension_reelle = tension_mesuree * 2.0; // pont diviseur ×2

  // 💡 Convention : 3.3 V = 0 %, 4.2 V = 100 %
  int pourcentage = int((tension_reelle - 3.3) / (4.2 - 3.3) * 100);
  pourcentage = constrain(pourcentage, 0, 100); // Clamp 0-100

  Serial.print("🔋 Tension mesurée (divisée): "); Serial.print(tension_mesuree); Serial.println(" V");
  Serial.print("🔋 Tension réelle estimée : "); Serial.print(tension_reelle); Serial.println(" V");
  Serial.print("🔋 Batterie estimée : "); Serial.print(pourcentage); Serial.println(" %");

  return pourcentage;
}
