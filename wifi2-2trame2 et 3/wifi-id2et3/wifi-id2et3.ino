#include <WiFi.h>

const char* ssid = "IoT IMT Nord Europe";
const char* password = "72Hin@R*";
//const char* ssid = "AndroidAP3558";
//const char* password = "123456789";

//  Adresse IP et port de la passerelle (FiPy)
//const char* server_ip = "192.168.43.22";
const char* server_ip = "10.120.2.158";
const int server_port = 1234;

// 🔢 Températures simulées
float temperatures[] = {25.1, 23.5, 29.3, 15.1, 12.0, 11.2, 13.1, 26.5, 32.0, 30.3};
int index_temp = 0;

// ⚡ ADC pour lire la tension batterie (GPIO35 recommandé)
const int BATTERY_ADC_PIN = 35;

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

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
    float temp = temperatures[index_temp];
    int rssi = WiFi.RSSI();
    int battery_percent = lire_batterie();

    // 📨 Première trame (Protocole_ID = 2)
    WiFiClient client1;
    if (client1.connect(server_ip, server_port)) {
      String message1 = "";
      message1 += "Temperature: " + String(temp) + "\n";
      message1 += "RSSI: " + String(rssi) + "\n";
      message1 += "Protocole_ID: 2\n";
      message1 += "Battery_State: " + String(battery_percent) + "\n";

      client1.print(message1);
      Serial.println("✅ Trame envoyée (Protocole_ID = 2) :");
      Serial.println(message1);
      client1.stop();
    } else {
      Serial.println("❌ Échec connexion serveur (trame 1)");
    }

    delay(300); // Petite pause

    // 📨 Deuxième trame (Protocole_ID = 3)
    WiFiClient client2;
    if (client2.connect(server_ip, server_port)) {
      String message2 = "";
      message2 += "Temperature: " + String(temp) + "\n";
      message2 += "RSSI: " + String(rssi) + "\n";
      message2 += "Protocole_ID: 3\n";
      message2 += "Battery_State: " + String(battery_percent) + "\n";

      client2.print(message2);
      Serial.println("✅ Trame envoyée (Protocole_ID = 3) :");
      Serial.println(message2);
      client2.stop();
    } else {
      Serial.println("❌ Échec connexion serveur (trame 2)");
    }

    index_temp = (index_temp + 1) % (sizeof(temperatures) / sizeof(float));
  } else {
    Serial.println("❗ Wi-Fi perdu. Reconnexion...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }

  delay(5000);
}

int lire_batterie() {
  int raw = analogRead(BATTERY_ADC_PIN);
  float tension_mesuree = (raw / 4095.0) * 3.6;
  float tension_reelle = tension_mesuree * 2.0;

  int pourcentage = int((tension_reelle - 3.3) / (4.2 - 3.3) * 100);
  pourcentage = constrain(pourcentage, 0, 100);

  Serial.print("🔋 Tension (divisée): "); Serial.print(tension_mesuree); Serial.println(" V");
  Serial.print("🔋 Tension réelle : "); Serial.print(tension_reelle); Serial.println(" V");
  Serial.print("🔋 Batterie : "); Serial.print(pourcentage); Serial.println(" %");

  return pourcentage;
}
