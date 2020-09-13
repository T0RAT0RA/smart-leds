#include <LittleFS.h>

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <PubSubClient.h>

#include <Button.h>

#include <ArduinoJson.h>

#include "FastLED.h"


char mqtt_server[40] = "homeassistant.local";
char mqtt_username[10] = "";
char mqtt_password[34] = "";

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;

#define RESET_BUTTON_PIN 2
#define DATA_PIN 3
#define NUM_LEDS 35
CRGB leds[NUM_LEDS];

bool isOn = false;

int R = 255;
int G = 255;
int B = 255;

uint8_t brightness = 255;

Button resetButton = Button(RESET_BUTTON_PIN);

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  // convert payload to String
  String value = String((char*)payload);

  Serial.print(String(topic));
  Serial.print(": ");
  Serial.println(value);

  if(String(topic) == "atelier/rgb1/light/switch")
  {
    if (value == "ON" && !isOn) {
      isOn = true;
      Serial.println("-> ON");
      client.publish("atelier/rgb1/light/status", "ON");
    } else if (value == "OFF") {
      isOn = false;
      Serial.println("-> OFF");
      client.publish("atelier/rgb1/light/status", "OFF");
    }
  }

  if(String(topic) == "atelier/rgb1/brightness/set")
  {
    brightness  = atoi(value.c_str ());

    char brightnessBuffer[5];
    sprintf(brightnessBuffer, "%i", brightness);
    client.publish("atelier/rgb1/brightness/status", brightnessBuffer);
    Serial.print("BRIGHTNESS -> ");
    Serial.println(brightnessBuffer);

    Serial.print("brightness -> ");
    Serial.println(brightness);
  }

  if(String(topic) == "atelier/rgb1/rgb/set")
  {
    R = value.substring(0, value.indexOf(',')).toInt();
    G = value.substring(value.indexOf(',')+1, value.lastIndexOf(',')).toInt();
    B = value.substring(value.lastIndexOf(',')+1).toInt();

    Serial.print("rgb -> ");
    Serial.println(value);

    char rgbBuffer[5];
    sprintf(rgbBuffer, "%i,%i,%i", R, G, B);

    client.publish("atelier/rgb1/rgb/status", rgbBuffer);
    Serial.print("RGB -> ");
    Serial.println(rgbBuffer);
  }

  if (isOn) {
    for(int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB(R, G, B);
        leds[i].maximizeBrightness();
        leds[i] %= brightness;
    }
    FastLED.show();
  } else {
    for(int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB(0, 0, 0);
    }
    FastLED.show();
  }


}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(mqtt_username);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.publish("atelier/rgb1/light/status", "OFF");
      client.subscribe("atelier/rgb1/light/switch");
      client.subscribe("atelier/rgb1/brightness/set");
      client.subscribe("atelier/rgb1/rgb/set");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
}

void setup() {
  Serial.begin(115200);

  resetButton.begin();

  if (LittleFS.begin()) {
    Serial.println("mounted FS");
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");

      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        StaticJsonDocument<256> json;
        DeserializationError jsonError = deserializeJson(json, buf.get());
        serializeJsonPretty(json, Serial);
        if (!jsonError) {
          Serial.println("\nparsed json");

          strcpy(mqtt_password, json["mqtt_password"]);

          Serial.println(mqtt_password);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    } else {
      Serial.println("config file doesn't exist");
    }
  } else {
    Serial.println("failed to mount FS");
  }

  // id/name, placeholder/prompt, default, length
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "mqtt server", mqtt_server, 40);
  wifiManager.addParameter(&custom_mqtt_server);

  WiFiManagerParameter custom_mqtt_username("mqtt_username", "mqtt username", "leds", 10);
  wifiManager.addParameter(&custom_mqtt_username);

  WiFiManagerParameter custom_mqtt_password("mqtt_password", "mqtt password", "WS2811", 34);
  wifiManager.addParameter(&custom_mqtt_password);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.autoConnect("ESP-LEDS", "esp8266-01");

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_username, custom_mqtt_username.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  reconnect();

  server.on("/rgb", []() {
    char rgbBuffer[5];
    sprintf(rgbBuffer, "%i,%i,%i", R, G, B);
    server.send(200, "text/plain", rgbBuffer);
  });
  server.serveStatic("/test", LittleFS, "/index.html");
  server.begin();

  FastLED.addLeds<WS2811, DATA_PIN, BRG>(leds, NUM_LEDS);

  for(int i=0; i<NUM_LEDS; i=i+3) {
      leds[i] = CRGB(255, 0, 0);
      leds[i+1] = CRGB(0, 255, 0);
      leds[i+2] = CRGB(0, 0, 255);
      FastLED.show();
      delay(25);
      leds[i] = CRGB(0,0,0);
      leds[i+1] = CRGB(0, 0, 0);
      leds[i+2] = CRGB(0, 0, 0);
      FastLED.show();
  }
  for(int i=NUM_LEDS; i>0; i=i-3) {
      leds[i] = CRGB(255, 0, 0);
      leds[i-1] = CRGB(0, 255, 0);
      leds[i-2] = CRGB(0, 0, 255);
      FastLED.show();
      delay(25);
      leds[i] = CRGB(0,0,0);
      leds[i-1] = CRGB(0, 0, 0);
      leds[i-2] = CRGB(0, 0, 0);
      FastLED.show();
  }
}

void loop() {
  if (resetButton.pressed()) {
    Serial.println("reset settings wifiManager");
    // wifiManager.resetSettings();
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  server.handleClient();

}
