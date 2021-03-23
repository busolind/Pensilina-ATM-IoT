#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <SSD1306Wire.h> //https://github.com/ThingPulse/esp8266-oled-ssd1306
#include <StreamUtils.h>
#include <TaskScheduler.h>
#include <WiFiClientSecureBearSSL.h>
#include <list>

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";

String apiUrl = "https://giromilano.atm.it/proxy.ashx";
//String stopCode = "12493"; //Via Celoria (Istituto Besta)
//String stopCode = "14033"; //Via Bellini V.le Casiraghi (Sesto S.G.)
String stopCode = "11492"; //VERY LONG response, tram
//String stopCode = "12587"; //Sesto Marelli M1 dir Bicocca

// Expires On	Thursday, 28 July 2022 at 14:00:00
// const char fingerprint[] PROGMEM = "72 84 14 05 5A 4B 27 DD 07 44 FC 00 96 4E 9B 06 42 0B 9C 7F";
// pare funzionare anche senza fingerprint

//DISPLAY
#define SDA D2
#define SCL D1
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32); // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.

//String stopJSON = "";
std::vector<String> lineIds;
std::vector<String> waitMessages;

unsigned long last_action = 0;

// ToDo:
//std::list<String> bulletins;

//HTTPS STREAM??

BearSSL::WiFiClientSecure client;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void parse_stopJSON(String stopJSON) {
  if (stopJSON != "") {
    StaticJsonDocument<200> filter;
    filter["Lines"][0]["Line"]["LineId"] = true;
    filter["Lines"][0]["WaitMessage"] = true;

    StaticJsonDocument<1000> doc;
    DeserializationError error = deserializeJson(doc, stopJSON, DeserializationOption::Filter(filter));

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      JsonArray linesArray = doc["Lines"].as<JsonArray>();

      //lineIds = new String[LINES_SIZE];
      //waitMessages = new String[LINES_SIZE];

      lineIds.clear();
      waitMessages.clear();

      for (JsonVariant line : linesArray) {
        Serial.print(line["Line"]["LineId"].as<String>() + "\t");
        Serial.println(line["WaitMessage"].as<String>());

        String lineId = line["Line"]["LineId"].as<String>();
        String waitMessage = line["WaitMessage"].as<String>();

        lineIds.push_back(lineId);
        waitMessages.push_back(waitMessage);
      }
    }
  }
}

void parse_stopJSON_stream(BearSSL::WiFiClientSecure &str_client) {

  StaticJsonDocument<200> filter;
  filter["Lines"][0]["Line"]["LineId"] = true;
  filter["Lines"][0]["WaitMessage"] = true;

  //ReadLoggingStream loggingStream(str_client, Serial);

  ReadBufferingStream bufferingStream(str_client, 64);

  //Skip to start of JSON (necessary because of some numeric value on first line of the stream. My use of the stream is not ideal)
  while (char(bufferingStream.peek()) != '{') {
    bufferingStream.read();
  }

  /*
  while (bufferingStream.available()) {
    Serial.print(char(bufferingStream.read()));
  }
*/

  StaticJsonDocument<1000> doc;
  DeserializationError error = deserializeJson(doc, bufferingStream, DeserializationOption::Filter(filter));

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  } else {
    JsonArray linesArray = doc["Lines"].as<JsonArray>();

    lineIds.clear();
    waitMessages.clear();

    for (JsonVariant line : linesArray) {
      Serial.print(line["Line"]["LineId"].as<String>() + "\t");
      Serial.println(line["WaitMessage"].as<String>());

      String lineId = line["Line"]["LineId"].as<String>();
      String waitMessage = line["WaitMessage"].as<String>();

      lineIds.push_back(lineId);
      waitMessages.push_back(waitMessage);
    }
  }
}

void https_request() {
  String postPayload = "url=tpPortal/geodata/pois/stops/" + stopCode;

  //client->setFingerprint(fingerprint);
  client.setInsecure();

  HTTPClient https;

  Serial.print("[HTTPS] begin...\n");
  if (https.begin(client, apiUrl)) { // HTTPS

    https.addHeader("Content-Type", "application/x-www-form-urlencoded");

    Serial.print("[HTTPS] POST...\n");
    // start connection and send HTTP header
    int httpCode = https.POST(postPayload);

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == 400) {
        //String stopJSON = https.getString();
        //Serial.println(stopJSON);
        //parse_stopJSON(stopJSON);
        parse_stopJSON_stream(client);
      }
    } else {
      Serial.printf("failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void draw_display() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  if (lineIds.size() > 0) {
    display.drawString(0, 0, lineIds.at(0));
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(127, 0, waitMessages.at(0));
  }

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (lineIds.size() > 1) {
    display.drawString(0, 10, lineIds.at(1));
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(127, 10, waitMessages.at(1));
  }
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.display();
}

void setup() {
  Serial.begin(115200);
  setup_wifi();

  display.init();
}

void loop() {
  unsigned long now = millis();
  if (now - last_action > 15000) {
    last_action = now;
    if (WiFi.status() == WL_CONNECTED) {
      https_request();
    }
    draw_display();
    //parse_stopJSON();
    //Serial.println(lineId + "\t" + waitMessage);
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
  }
}

//PROBLEM: this stops working after a while and free heap keeps decreasing, the response stops being printed and the
//JSON stops being parsed.

//It seems that this memory leak was because of assigning to global variables. Maybe a problem with garbage collection.
//Need to fix it. [maybe fixed]