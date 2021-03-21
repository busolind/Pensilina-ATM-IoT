#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <TaskScheduler.h>
#include <WiFiClientSecureBearSSL.h>

const char *ssid = "IOT_TEST";
const char *password = "IOT_TEST";

char serverAddress[] = "giromilano.atm.it"; // server address
int port = 443;

Scheduler ts;

//WiFiClient espClient;

// Expires On	Thursday, 28 July 2022 at 14:00:00
// const char fingerprint[] PROGMEM = "72 84 14 05 5A 4B 27 DD 07 44 FC 00 96 4E 9B 06 42 0B 9C 7F";
// pare funzionare anche senza fingerprint

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

void setup() {
  Serial.begin(115200);
  setup_wifi();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    String postPayload = "url=tpPortal/geodata/pois/stops/12493";

    //client.post("/proxy.ashx", contentType, postPayload);

    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

    //client->setFingerprint(fingerprint);
    client->setInsecure();

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, "https://giromilano.atm.it/proxy.ashx")) { // HTTPS

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
          Serial.println("RESP LENGTH: " + https.getSize());
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
  delay(10000);
}