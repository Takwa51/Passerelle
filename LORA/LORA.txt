from network import LoRa
import socket
import struct
import time
import machine
from machine import ADC

class LoRaClient:
    def __init__(self, client_id, frequency, sf, bandwidth, coding_rate, protocole_id):
        self.client_id = client_id
        self.protocole_id = protocole_id

        # Init LoRa
        self.lora = LoRa(
            mode=LoRa.LORA,
            region=LoRa.EU868,
            frequency=frequency,
            sf=sf,
            bandwidth=bandwidth,
            coding_rate=coding_rate
        )
        self.sock = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.sock.setblocking(False)

        # Init ADC pour lire la tension batterie sur P16
        self.adc = ADC()
        self.adc.vref(1100)  # valeur par défaut du FiPy
        self.battery_pin = self.adc.channel(pin='P16', attn=ADC.ATTN_11DB)

    def start(self):
        print(" Client LoRa avec lecture de batterie démarre...")

    def get_temperature(self):
        return 25.0 + (machine.rng() % 100) / 10.0

    def get_mac(self):
        return self.lora.mac()

    def get_rssi_simule(self):
        rssi = -100 + (machine.rng() % 50)
        return max(-128, min(127, rssi))  # clamp entre -128 et 127

    def get_battery_state(self):
        raw_mV = self.battery_pin.voltage()  # en millivolts
        tension_bat = raw_mV * 2 / 1000.0   # en Volts (diviseur de tension ×2)
        pourcentage = int(min(100, max(0, (tension_bat - 3.3) / (4.2 - 3.3) * 100)))
        return pourcentage

    def send(self):
        temperature = self.get_temperature()
        mac_addr = self.get_mac()
        rssi_simule = self.get_rssi_simule()
        battery_state = self.get_battery_state()

        # Nouvelle trame avec battery_state (17 octets)
        data = struct.pack('>If6sbbb', self.client_id, temperature, mac_addr, rssi_simule, self.protocole_id, battery_state)

        self.sock.send(data)

        print(" Données envoyées :")
        print("   client_id      =", self.client_id)
        print("   Température   = {:.2f} °C".format(temperature))
        print("   RSSI simulé    =", rssi_simule)
        print("   Protocole ID   =", self.protocole_id)
        print("   Batterie       = {} %".format(battery_state))
        print("   MAC            =", self.format_mac_addr(mac_addr))

    def format_mac_addr(self, mac_addr):
        return ":".join("{:02x}".format(b) for b in mac_addr)

# === Paramètres LoRa ===
CLIENT_ID = 1
FREQUENCY = 868000000
SPREADING_FACTOR = 7
BANDWIDTH = LoRa.BW_125KHZ
CODING_RATE = LoRa.CODING_4_5
PROTOCOLE_ID = 1  # LoRa

# === Démarrage du client ===
lora_client = LoRaClient(CLIENT_ID, FREQUENCY, SPREADING_FACTOR, BANDWIDTH, CODING_RATE, PROTOCOLE_ID)
lora_client.start()

while True:
    lora_client.send()
    time.sleep(5)  # Envoi toutes les 5 secondes
