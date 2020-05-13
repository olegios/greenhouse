#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <BlynkSimpleEsp8266.h>

#include <DHT.h>
#include <SPI.h>
#include <Wire.h>

#include "credentials.hpp"


#define BLYNK_PRINT        Serial
#define B_TEMRMINAL        V0
#define B_TEMPERATURE      V1
#define B_HUMIDITY         V2
#define B_SMOI_SENSOR      V3
#define B_TAP_STATUS_LED   V12
#define B_TAP_MODE_BUTTON  20
#define B_SMOI_TRIGER      30

#define DHT_SENSOR_PIN     2
#define DHT_SENSOR_TYPE    DHT11

#define SMOI_SENSOR_PIN    0
#define TAP_RELAY_PIN      5


typedef enum {
  AUTO,
  MANUAL
}tmode_t;


static BlynkTimer blynk_timer;
static WidgetTerminal blynk_terminal(B_TEMRMINAL);
static WidgetLED tap_status_led(B_TAP_STATUS_LED);

static DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);
static float temperature;
static float humidity;

static int smoi_sensor_value;
static int smoi_triger;
static tmode_t tap_mode;


static void wifi_connection(void);
static void arduino_ota_setup(void);
static void blynk_setup(void);
static void smoi_sensor_begin(void);
static void tap_relay_begin(void);

static void dht_sensor_data(void);
static void smoi_sensor_data(void);
static void check_soil_moisture(void);


void setup(void) {
  Serial.begin(115200);
  wifi_connection();
  arduino_ota_setup();
  blynk_setup();

  dht_sensor.begin();
  smoi_sensor_begin();
  tap_relay_begin();
}


void loop(void) {
  ArduinoOTA.handle();
  Blynk.run();
  blynk_timer.run();

  blynk_timer.setInterval(5000L, dht_sensor_data);
  blynk_timer.setInterval(5000L, smoi_sensor_data);
  blynk_timer.setInterval(1000L, check_soil_moisture);
}


static void wifi_connection(void) {
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


static void arduino_ota_setup(void) {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
  Serial.printf("OTA Ready, free space available: %.f bytes\n",(float)ESP.getFreeSketchSpace());
}


static void blynk_setup(void) {
  Blynk.begin(AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
  blynk_terminal.println(F("Setup successful"));
  blynk_terminal.flush();
}


static void dht_sensor_data(void) {
  temperature = dht_sensor.readTemperature();
  humidity = dht_sensor.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    temperature = -1;
    humidity = -1;
  }

  Blynk.virtualWrite(B_TEMPERATURE, temperature);
  Blynk.virtualWrite(B_HUMIDITY, humidity);
}


static void smoi_sensor_begin(void) {
  smoi_sensor_value = 100;
  smoi_triger = 50;
  Blynk.virtualWrite(B_SMOI_TRIGER, smoi_triger);
}

BLYNK_WRITE(B_SMOI_TRIGER) {
  smoi_triger = param.asInt();
  Serial.printf("Smoi triger: %d\n", smoi_triger);
}

static void smoi_sensor_data(void) {
  smoi_sensor_value = map(analogRead(SMOI_SENSOR_PIN),0,1024,100,0);
  Blynk.virtualWrite(B_SMOI_SENSOR, smoi_sensor_value);
}


BLYNK_WRITE(B_TAP_MODE_BUTTON) {
  tap_mode = (tap_mode == AUTO) ? MANUAL : AUTO;
  const char * property_color = (tap_mode == AUTO) ? "#43d356" : "#D3435C";
  Blynk.setProperty(B_TAP_MODE_BUTTON, "color", property_color);
  Blynk.setProperty(B_TAP_STATUS_LED, "color", property_color);
}

static void tap_relay_begin(void) {
  pinMode(TAP_RELAY_PIN, OUTPUT);
  tap_mode = AUTO;
}

static void check_soil_moisture()
{
  if (tap_mode == AUTO) {
    if (smoi_sensor_value < smoi_triger) {
      digitalWrite(TAP_RELAY_PIN, LOW);
      tap_status_led.on();
      Serial.println("TAP OPEN");
    } else {
      digitalWrite(TAP_RELAY_PIN, HIGH);
      tap_status_led.off();
      Serial.println("TAP CLOSE");
    }
  } else {
    digitalWrite(TAP_RELAY_PIN, LOW);
    tap_status_led.on();
    Serial.println("Manual_mode: TAP OPEN");
  }
}