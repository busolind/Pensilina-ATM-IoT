#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <TaskScheduler.h>
#include <WiFiClientSecureBearSSL.h>
#include <list>

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";

String apiUrl = "https://giromilano.atm.it/proxy.ashx";
String stopCode = "14033";

#define LINES_SIZE 10

// Expires On	Thursday, 28 July 2022 at 14:00:00
// const char fingerprint[] PROGMEM = "72 84 14 05 5A 4B 27 DD 07 44 FC 00 96 4E 9B 06 42 0B 9C 7F";
// pare funzionare anche senza fingerprint

String stopJSON = "";
String *lineIds;
String *waitMessages;

unsigned long last_action = 0;

// ToDo:
//std::list<String> bulletins;

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

void https_request() {
  String postPayload = "url=tpPortal/geodata/pois/stops/" + stopCode;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

  //client->setFingerprint(fingerprint);
  client->setInsecure();

  HTTPClient https;

  Serial.print("[HTTPS] begin...\n");
  if (https.begin(*client, apiUrl)) { // HTTPS

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
        String payload = https.getString();
        stopJSON = payload;
        Serial.println(payload);
      }
    } else {
      Serial.printf("failed, error: %s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void parse_stopJSON() {
  if (stopJSON != "") {
    DynamicJsonDocument doc(10000);
    DeserializationError error = deserializeJson(doc, stopJSON);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      JsonArray linesArray = doc["Lines"].as<JsonArray>();

      lineIds = new String[LINES_SIZE];
      waitMessages = new String[LINES_SIZE];

      int i = 0;
      for (JsonVariant line : linesArray) {
        Serial.print(line["Line"]["LineId"].as<String>() + "\t");
        Serial.println(line["WaitMessage"].as<String>());

        lineIds[i] = line["Line"]["LineId"].as<String>();
        waitMessages[i] = line["WaitMessage"].as<String>();

        i++;
      }

      //lineId = doc["Lines"][0]["Line"]["LineId"].as<String>();
      //waitMessage = doc["Lines"][0]["WaitMessage"].as<String>();
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
}

void loop() {
  unsigned long now = millis();
  if (now - last_action > 15000) {
    last_action = now;
    if (WiFi.status() == WL_CONNECTED) {
      https_request();
    }
    parse_stopJSON();
    //Serial.println(lineId + "\t" + waitMessage);
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
  }
}

//PROBLEM: this stops working after a while, the response stops being printed and the JSON stops being parsed