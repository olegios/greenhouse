#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "credentials.hpp"


static void wifi_connection();


void setup() {
  Serial.begin(115200);
  wifi_connection();
}


void loop() {
}


static void wifi_connection() {
  WiFi.hostname("Greenhouse");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.print("\nConnected, IP address: ");
  Serial.println(WiFi.localIP());
}
