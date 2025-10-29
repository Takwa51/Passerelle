from network import Bluetooth, WLAN, LoRa
import struct
import socket
import time
import select
import ujson
from umqtt_simple import MQTTClient

# === Configuration Wi-Fi ===
# ssid = 'IoT IMT Nord Europe'
# password = '72Hin@R*'
ssid = 'AndroidAP3558'
password = '123456789'

def configurer_reseau():
    wlan = WLAN(mode=WLAN.STA)
    wlan.connect(ssid, auth=(WLAN.WPA2, password))
    while not wlan.isconnected():
        print('Connexion au Wi-Fi en cours...')
        time.sleep(1)
    print(' Connecté au Wi-Fi')
    print(' Adresse IP : {}'.format(wlan.ifconfig()[0]))
    return wlan.ifconfig()[0]

# === Configuration MQTT ===
# MQTT_BROKER = '10.89.1.249'  # IP du PC avec  Mosquitto installé
MQTT_BROKER = '192.168.43.90'
MQTT_PORT = 1883
MQTT_TOPIC = b'iot/temperature'

client_mqtt = MQTTClient("fipy_gateway", MQTT_BROKER, port=MQTT_PORT)

def connecter_mqtt():
    try:
        client_mqtt.connect()
        print(" Connecté au broker MQTT")
    except Exception as e:
        print(" Erreur de connexion MQTT :", e)

# === Envoi MQTT ===
def envoyer_temperature_mqtt(temp, protocole, protocole_id, rssi, battery_state=None):
    data = {
        "temperature": temp,
        "protocole": protocole,
        "protocole_id": protocole_id,
        "rssi": rssi,
        "battery_state": battery_state,
        "timestamp": time.time()
    }
    payload = ujson.dumps(data)
    try:
        client_mqtt.publish(MQTT_TOPIC, payload)
        print(" MQTT publié :", payload)
    except Exception as e:
        print(" Erreur publication MQTT :", e)

# === BLE ===
bluetooth = Bluetooth()
bluetooth.init()
SERVICE_UUID = 0xec00
CHARACTERISTIC_UUID = 0xec0e
def afficher_donnees(protocole, temp, rssi, protocole_id, battery_state=None):
    msg = "[{}] ➜ Température: {:.2f} °C | RSSI: {} | Protocole ID: {}".format(protocole, temp, rssi, protocole_id)
    if battery_state is not None:
        msg += " | État batterie: {}%".format(battery_state)
    print(msg)

def handle_client_ble(value):
    try:
        if isinstance(value, bytes) and len(value) == 7:
            temperature = struct.unpack('f', value[0:4])[0]
            rssi = struct.unpack('b', value[4:5])[0]
            protocole_id = struct.unpack('B', value[5:6])[0]
            battery_state = struct.unpack('B', value[6:7])[0]
            afficher_donnees("BLE", temperature, rssi, protocole_id, battery_state)
            envoyer_temperature_mqtt(temperature, "BLE", protocole_id, rssi, battery_state)
        else:
            print(" Données BLE inattendues (attendu 7 octets) :", value)
    except Exception as e:
        print(" Erreur traitement BLE :", e)

def chr1_handler(chr, value):
    if isinstance(value, tuple) and len(value) > 1:
        value = value[1]
    handle_client_ble(value)

def conn_cb(event):
    if event == Bluetooth.CLIENT_CONNECTED:
        print(" Client BLE connecté")
    elif event == Bluetooth.CLIENT_DISCONNECTED:
        print(" Client BLE déconnecté")

# === Initialisation réseau et MQTT ===
wlan_ip = configurer_reseau()
connecter_mqtt()

# === Socket Wi-Fi TCP entrante depuis client ESP32 ===
tcp_port = 1234
tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
tcp_sock.bind((wlan_ip, tcp_port))
tcp_sock.listen(1)
tcp_sock.setblocking(False)
print('✅ Serveur TCP Wi-Fi prêt sur {}:{}'.format(wlan_ip, tcp_port))

# === LoRa ===
lora = LoRa(mode=LoRa.LORA, region=LoRa.EU868, frequency=868000000, sf=7)
lora_sock = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
lora_sock.setblocking(False)
print('✅ Serveur LoRa démarré...')

# === BLE ===
bluetooth.set_advertisement(name='FiPy Server', service_uuid=SERVICE_UUID)
bluetooth.advertise(True)
srv1 = bluetooth.service(uuid=SERVICE_UUID, isprimary=True, nbr_chars=1)
chr1 = srv1.characteristic(uuid=CHARACTERISTIC_UUID, value=b'')
chr1.callback(trigger=Bluetooth.CHAR_WRITE_EVENT, handler=chr1_handler)
bluetooth.callback(trigger=Bluetooth.CLIENT_CONNECTED | Bluetooth.CLIENT_DISCONNECTED, handler=conn_cb)
print('✅ Serveur BLE prêt...')

# === Boucle principale ===
tcp_client_sock = None

while True:
    sockets_to_check = [tcp_sock if tcp_client_sock is None else tcp_client_sock, lora_sock]
    ready, _, _ = select.select(sockets_to_check, [], [], 1.0)

    for sock in ready:
        if sock is tcp_sock:
            try:
                tcp_client_sock, addr = tcp_sock.accept()
                tcp_client_sock.setblocking(False)
                print(' Connexion TCP acceptée de', addr)
            except Exception as e:
                print(' Erreur acceptation TCP:', e)

        elif sock is tcp_client_sock:
            try:
                data = tcp_client_sock.recv(1024)
                if not data:
                    tcp_client_sock.close()
                    tcp_client_sock = None
                    print(' Connexion TCP fermée')
                    continue

                decoded = data.decode().strip()
                lines = decoded.split('\n')
                temperature = rssi = protocole_id = battery_state = None

                for line in lines:
                    if line.startswith('Temperature:'):
                        temperature = float(line.split(':')[1].strip())
                    elif line.startswith('RSSI:'):
                        rssi = int(line.split(':')[1].strip())
                    elif line.startswith('Protocole_ID:'):
                        protocole_id = int(line.split(':')[1].strip())
                    elif line.startswith('Battery_State:'):
                        battery_state = int(line.split(':')[1].strip())

                if temperature is not None and rssi is not None and protocole_id is not None:
                    afficher_donnees("Wi-Fi", temperature, rssi, protocole_id, battery_state)
                    envoyer_temperature_mqtt(temperature, "Wi-Fi", protocole_id, rssi, battery_state)
                else:
                    print(" Données TCP incomplètes :", decoded)

                tcp_client_sock.close()
                tcp_client_sock = None

            except Exception as e:
                print(" Erreur socket TCP Wi-Fi :", e)
                tcp_client_sock.close()
                tcp_client_sock = None

        elif sock is lora_sock:
            try:
                data = lora_sock.recv(256)
                if data and len(data) == 17:
                    client_id, temperature, mac_addr, rssi, protocole_id, battery_state = struct.unpack('>If6sbbb', data)
                    afficher_donnees("LoRa", temperature, rssi, protocole_id, battery_state)
                    envoyer_temperature_mqtt(temperature, "LoRa", protocole_id, rssi, battery_state)
                else:
                    print(" Données LoRa incorrectes :", data)
            except Exception as e:
                print(" Erreur réception LoRa :", e)

    time.sleep(0.01)
