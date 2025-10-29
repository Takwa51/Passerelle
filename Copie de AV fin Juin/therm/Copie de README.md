# Ordre d’apparition des messages dans le terminal de la passerelle

L’ordre d’apparition (WiFi → 4 BLE → 3 LoRa → WiFi…) dans le terminal de la passerelle dépend de plusieurs facteurs, et n’est pas uniquement lié à la boucle principale. Voici une explication détaillée :

## 1. Boucle événementielle = ordre d’arrivée réel
- La boucle événementielle (`select`) traite les données **dès qu’elles sont disponibles**.
- L’ordre d’affichage reflète l’ordre **réel de réception** par la passerelle, indépendamment du protocole.

## 2. BLE fonctionne via callback (hors `select`)
- Dans le code, BLE utilise un callback :
  ```c
  // Exemple simplifié
  void chr1_handler(...) { 
      printf("BLE reçu!"); // Affichage immédiat
  }