
#include <arduino.h>
#define INTEWRVAL 16 // 16 sec is the normal
#define ONE_HOUR  3600000
static ulong lastReceived_Wind = 0;
static ulong lastSendToAzure = 0;

void sendEspNow_Wind(int direction, float speed_avg, float speed_max, int rssi);
// void send2Azure();

void AddIncoming(int direction, float speed_avg, float speed_max, int rssi) {
  sendEspNow_Wind(direction, speed_avg, speed_max, rssi);
  //   if (millis() - lastSendToAzure > ONE_HOUR)
  //     send2Azure();
  lastReceived_Wind = millis();
}
