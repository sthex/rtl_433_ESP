#include <WiFi.h>
#include <esp_now.h>

// uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // ALLE
uint8_t broadcastAddress[] = {0x82, 0x88, 0x88, 0x88, 0x88, 0x88}; // Pumpe gange

esp_now_peer_info_t peerInfo;
uint32_t lastReceived = 0;

typedef struct struct_message { //max 250 bytes!
  char pre[16];
  char payload[234];
} struct_message;
typedef struct wind_message { //max 250 bytes!
  char pre[16] = "wind";
  int direction;
  float speed_avg;
  float speed_max;
  int rssi;
} wind_message;
typedef struct tank_message { //max 250 bytes!
  char pre[16] = "tank";
  float temperature;
  int ullage_cm;
  int rssi;
} tank_message;

struct_message myData;
wind_message myWindData;
tank_message myTankData;

void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
  Serial.print("Bytes received: ");
  Serial.println(len);
  // memcpy(&myData, incomingData, sizeof(myData));
  // Serial.print("Char: ");
  // Serial.println(myData.a);
  // Serial.print("Int: ");
  // Serial.println(myData.b);
  // Serial.print("Float: ");
  // Serial.println(myData.c);
  // Serial.print("Counter: ");
  // Serial.println(myData.count);
  Serial.println();
}

void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("EspNow send: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Not delivered");
}

void setupEspNow() {
  strcpy(myData.pre, "rtl_433");
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  //esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  }
}

void sendEspNow(String msg) {
  if (msg.length() < 234) {
    strncpy(myData.payload, msg.c_str(), 234);
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&myData, sizeof(myData));

    if (result != ESP_OK) {
      Serial.println("Error sending EspNow data");
    }
  } else {
    Serial.printf("Can't send EspNow message of size &u\n", msg.length());
  }
}

void sendEspNow_Wind(int direction, float speed_avg, float speed_max, int rssi) {
  myWindData.direction = direction;
  myWindData.speed_avg = speed_avg;
  myWindData.speed_max = speed_max;
  myWindData.rssi = rssi;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&myWindData, sizeof(myWindData));
  if (result != ESP_OK) {
    Serial.println("Error sending EspNow wind data");
  }
}

void sendEspNow_Tank(int ullage_cm, float temperature, int rssi) {
  myTankData.temperature = temperature;
  myTankData.ullage_cm = ullage_cm;
  myWindData.rssi = rssi;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&myTankData, sizeof(myTankData));
  if (result != ESP_OK) {
    Serial.println("Error sending EspNow tank data");
  }
}