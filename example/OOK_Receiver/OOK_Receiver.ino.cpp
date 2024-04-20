/*
 Basic rtl_433_ESP example for OOK/ASK Devices

*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <SSD1306Wire.h>
#include <Wire.h>
#include <rtl_433_ESP.h>

// #include "espNow.h"
RTC_DATA_ATTR int modeAvg = 10;

#include "Azure.h"
// #include "wind.h"

#define OLED_ADDR 0x3c
#define RST_OLED  16
#define SDA_OLED  21
#define SCL_OLED  22
SSD1306Wire display(OLED_ADDR, OLED_SDA, OLED_SCL);

#ifndef RF_MODULE_FREQUENCY
#  define RF_MODULE_FREQUENCY 433.92
#endif

#define JSON_MSG_BUFFER 512

char messageBuffer[JSON_MSG_BUFFER];

rtl_433_ESP rf; // use -1 to disable transmitter

int count = 0;
static ulong lastmillisCotech;
String wind_dir[9] = {"N ", "NE", "E ", "SE", "S ", "SW", "W ", "NW", "?"};
String info;

#pragma region Average to azure
static ulong lastAverageSentAzure;
static ulong lastAverage10;
float wind_avg10[60];
int wind_dir10[60];
int wind_dir60[6];
float wind_avg60[6];
float wind_max60[6];
int index10 = 0;
int index60 = 0;

float GetAvg10() {
  if (index10 == 0) return 0;
  float all = 0;
  int i = 0;
  for (i = 0; i < index10; i++) {
    all += wind_avg10[i];
  }
  return all / i;
}
float GetDir10() {
  if (index10 == 0) return 0;
  float windAvgX;
  float windAvgY;
  int i = 0;
  for (i = 0; i < index10; i++) {
    float theta = wind_dir10[i] / 180.0 * PI;
    windAvgX += cos(theta);
    windAvgY += sin(theta);
  }
  // result is -180 to 180. change this to 0-360.
  float theta = atan2(windAvgY, windAvgX) / PI * 180;
  if (theta < 0) theta += 360;
  return theta;
}
void SendAvgAzure10() {
  Serial.println("SendAvgAzure10()");
  telemetry_payload = "{ \"deviceId\":\"" + String(IOT_CONFIG_DEVICE_ID) +
                      "\", \"w10dir\":" + String(wind_dir60[index60]) +
                      ", \"w10avg\":" + String(wind_avg60[index60]) +
                      ", \"w10max\":" + String(wind_max60[index60]) + "}";

  delay(1000);
  send_azure();

  //   2024 / 4 / 20 11 : 37 : 11 [INFO] Sending telemetry :
  //   2024 / 4 / 20 11 : 37 : 11 [INFO] { "deviceId" : "rtl433wind", "w10dir" : 360,", "w10avg": 0.00,", "w10max": 0.00 }
}

void SendAvgAzure() {
  Serial.println("SendAvgAzure()");
  telemetry_payload = "{ \"deviceId\": \"" + String(IOT_CONFIG_DEVICE_ID) +
                      ", \"avg6\":" + String(wind_avg60[5]) +
                      ", \"max6\":" + String(wind_max60[5]) +
                      ", \"dir5\":" + String(wind_dir60[4]) +
                      ", \"avg5\":" + String(wind_avg60[4]) +
                      ", \"max5\":" + String(wind_max60[4]) +
                      ", \"dir4\":" + String(wind_dir60[3]) +
                      ", \"avg4\":" + String(wind_avg60[3]) +
                      ", \"max4\":" + String(wind_max60[3]) +
                      ", \"dir3\":" + String(wind_dir60[2]) +
                      ", \"avg3\":" + String(wind_avg60[2]) +
                      ", \"max3\":" + String(wind_max60[2]) +
                      ", \"dir2\":" + String(wind_dir60[1]) +
                      ", \"avg2\":" + String(wind_avg60[1]) +
                      ", \"max2\":" + String(wind_max60[1]) +
                      ", \"dir1\":" + String(wind_dir60[0]) +
                      ", \"avg1\":" + String(wind_avg60[0]) +
                      ", \"max1\":" + String(wind_max60[0]) + "}";
  delay(1000);
  send_azure();
  //{ "deviceId": "rtl433wind, "avg6":6.99, "max6":10.90, "dir5":16, "avg5":7.81, "max5":13.50, "dir4":34, "avg4":7.15, "max4":12.50, "dir3":32, "avg3":6.90, "max3":11.20, "dir2":38, "avg2":7.81, "max2":12.90, "dir1":21, "avg1":7.93, "max1":12.90}
}

void AddWind(int dir, float avg, float peak) {
  //   Serial.printf("%02x", buf[i]);
  String msg = (String("AddWind: ") + dir + "deg " + avg + "m/s " + peak + "m/s");
  Serial.println(msg);

  wind_dir10[index10] = dir;
  wind_avg10[index10] = avg;
  if (peak > wind_max60[index60])
    wind_max60[index60] = peak;
  index10++;
  if (lastAverage10 == 0) {
    lastAverage10 = millis();
    lastAverageSentAzure = millis();
  }
  if (millis() - lastAverage10 >= 600000) { // 10 min
    wind_dir60[index60] = GetDir10();
    wind_avg60[index60] = GetAvg10();
    Serial.printf("Avg dir=%f Avg wind=%f", wind_dir60[index60], wind_avg60[index60]);
    if (modeAvg == 10)
      SendAvgAzure10();
    lastAverage10 = millis();
    index10 = 0;
    if (++index60 >= 6) {
      if (modeAvg == 60)
        SendAvgAzure();
      index60 = 0;
      index10 = 0;
      lastAverageSentAzure = millis();
    }
  }
  //   else
  // Serial.printf("Avg dir=%f Avg wind=%f index10=%d\n", GetDir10(), GetAvg10(), index10);
}
#pragma endregion

void logJson(JsonObject& jsondata) {
#if defined(ESP8266) || defined(ESP32) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
  char JSONmessageBuffer[jsondata.measureLength() + 1];
#else
  char JSONmessageBuffer[JSON_MSG_BUFFER];
#endif
  jsondata.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
#if defined(setBitrate) || defined(setFreqDev) || defined(setRxBW)
  Log.setShowLevel(false);
  Log.notice(F("."));
  Log.setShowLevel(true);
#else
  Log.notice(F("Received message : %s" CR), JSONmessageBuffer);

  telemetry_payload = JSONmessageBuffer;
  send_azure();

#endif
}

void AddInfo(String msg) {
  int index = info.indexOf("\n");
  if (index > 0) {
    int num = 0;
    int i = index;
    do {
      num++;
      i = info.indexOf("\n", i + 1);
    } while (i >= 0);

    if (num >= 5) // max 5 lines
      info = info.substring(index + 1);
  }
  info += msg + "\n";
}

void rtl_433_Callback(char* message) {
  DynamicJsonBuffer jsonBuffer2(JSON_MSG_BUFFER);
  JsonObject& RFrtl_433_ESPdata = jsonBuffer2.parseObject(message);
  logJson(RFrtl_433_ESPdata);
  count++;

  if (RFrtl_433_ESPdata["model"] == "Cotech-513326") {
    ulong timeSinceLast = millis() - lastmillisCotech;
    if (timeSinceLast < 100) return; // SKIP DUAL MESSAGE
    int index = RFrtl_433_ESPdata["wind_dir_deg"];
    index /= 45;
    String msg = (String("Wind: ") + wind_dir[index] + " " + RFrtl_433_ESPdata["wind_max_m_s"].as<String>() + " m/s  dt:" + String((int)(timeSinceLast / 1000.0)) + "s");
    AddInfo(msg);
    if (modeAvg > 0)
      AddWind(RFrtl_433_ESPdata["wind_dir_deg"], RFrtl_433_ESPdata["wind_avg_m_s"], RFrtl_433_ESPdata["wind_max_m_s"]);
    // AddIncoming(RFrtl_433_ESPdata["wind_dir_deg"], RFrtl_433_ESPdata["wind_avg_m_s"], RFrtl_433_ESPdata["wind_max_m_s"], RFrtl_433_ESPdata["rssi"]);
    lastmillisCotech = millis();
  } else if (RFrtl_433_ESPdata["model"] == "TS-FT002") {
    String msg = (String("Depth cm. = ") + RFrtl_433_ESPdata["depth_cm"].as<String>());
    AddInfo(msg);
    // sendEspNow_Tank(RFrtl_433_ESPdata["depth_cm"], RFrtl_433_ESPdata["temperature_C"], RFrtl_433_ESPdata["rssi"]);
  } else
    AddInfo(String("Model ") + RFrtl_433_ESPdata["model"].as<String>());

  display.clear();
  display.drawString(0, 1, info);
  display.display(); // displays content in buffer
}

void initOLED() {
  display.init(); // clears screen
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
}
void showOLEDMessage(String line1, String line2, String line3) {
  display.clear();
  display.drawString(0, 0, line1); //  adds to buffer
  display.drawString(0, 12, line2);
  display.drawString(0, 24, line3);
  //   display.drawString(0, 36, "line4");
  display.display(); // displays content in buffer
}

void setup() {
  //   Serial.begin(115200);
  Serial.begin(250000);
  delay(500);
  initOLED();
  showOLEDMessage(String("OOK_Receiver"), String(IOT_CONFIG_DEVICE_ID), String("Setup Azure ..."));

  //   setupEspNow();
  setup_azure();

  showOLEDMessage(String("OOK_Receiver"), String("Init rtl_433 ..."), String(""));

#ifndef LOG_LEVEL
  LOG_LEVEL_SILENT
#endif
  Log.begin(LOG_LEVEL, &Serial);
  Log.notice(F(" " CR));
  Log.notice(F("****** setup ******" CR));
  rf.initReceiver(RF_MODULE_RECEIVER_GPIO, RF_MODULE_FREQUENCY);
  rf.setCallback(rtl_433_Callback, messageBuffer, JSON_MSG_BUFFER);
  rf.enableReceiver();
  Log.notice(F("****** setup complete ******" CR));
  rf.getModuleStatus();
  showOLEDMessage(String("OOK_Receiver"), String("rtl_433_ESP"), String("Setup done"));
}

unsigned long uptime() {
  static unsigned long lastUptime = 0;
  static unsigned long uptimeAdd = 0;
  unsigned long uptime = millis() / 1000 + uptimeAdd;
  if (uptime < lastUptime) {
    uptime += 4294967;
    uptimeAdd += 4294967;
  }
  lastUptime = uptime;
  return uptime;
}

int next = uptime() + 30;

#if defined(setBitrate) || defined(setFreqDev) || defined(setRxBW)

#  ifdef setBitrate
#    define TEST    "setBitrate"
#    define STEP    2
#    define stepMin 1
#    define stepMax 300
#  elif defined(setFreqDev) // 17.24 was suggested
#    define TEST    "setFrequencyDeviation"
#    define STEP    1
#    define stepMin 5
#    define stepMax 200
#  elif defined(setRxBW)
#    define TEST "setRxBandwidth"

#    ifdef RF_SX1278
#      define STEP    5
#      define stepMin 5
#      define stepMax 250
#    else
#      define STEP    5
#      define stepMin 58
#      define stepMax 812
// #      define STEP    0.01
// #      define stepMin 202.00
// #      define stepMax 205.00
#    endif
#  endif
float step = stepMin;
#endif

void loop() {
  rf.loop();
  loop_azure();
#if defined(setBitrate) || defined(setFreqDev) || defined(setRxBW)
  char stepPrint[8];
  if (uptime() > next) {
    next = uptime() + 120; // 60 seconds
    dtostrf(step, 7, 2, stepPrint);
    Log.notice(F(CR "Finished %s: %s, count: %d" CR), TEST, stepPrint, count);
    step += STEP;
    if (step > stepMax) {
      step = stepMin;
    }
    dtostrf(step, 7, 2, stepPrint);
    Log.notice(F("Starting %s with %s" CR), TEST, stepPrint);
    count = 0;

    int16_t state = 0;
#  ifdef setBitrate
    state = rf.setBitRate(step);
    RADIOLIB_STATE(state, TEST);
#  elif defined(setFreqDev)
    state = rf.setFrequencyDeviation(step);
    RADIOLIB_STATE(state, TEST);
#  elif defined(setRxBW)
    state = rf.setRxBandwidth(step);
    if ((state) != RADIOLIB_ERR_NONE) {
      Log.notice(F(CR "Setting  %s: to %s, failed" CR), TEST, stepPrint);
      next = uptime() - 1;
    }
#  endif

    rf.receiveDirect();
    // rf.getModuleStatus();
  }
#endif
}