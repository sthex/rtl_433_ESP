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