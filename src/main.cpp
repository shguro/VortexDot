#include <FS.h>
#ifdef ESP32
  #include <SPIFFS.h>
#endif
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ArduinoOTA.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
WiFiManager wm;
char hostname[40] = "VortexDot";

//LED block
#define FASTLED_USE_GLOBAL_BRIGHTNESS 1
#include <FastLED.h>

// Here's how to control the LEDs from any two pins:
#define DATA_PIN    21
#define CLOCK_PIN   22

const int numLEDS = 20;
CRGB leds[numLEDS];


//Art-Net block
#include <ArtnetWifi.h>
ArtnetWifi artnet;
int startUniverse = 0;
int numberOfChannels = numLEDS * 3; 
int startChannel = 0;
int brightnessChannel = 1;
int brightnessUniverse = 15;
int modeChannel = 2;
int modeUniverse = 15;

//MQTT Block
#include <WiFiClient.h>
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient mqttClient(espClient);
char mqtt_server[40] = "atuin.hasi";
char mqtt_port[6] = "8080";
long lastReconnectAttempt = 0;

//Mode of Usage
#define ARTNET 1
#define MQTT 2

int mode = 1;

//flag for saving data
bool shouldSaveConfig = false;

WiFiManagerParameter custom_hostname("hostname", "hostname", hostname, 40);
WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);


void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  if (universe == modeUniverse)
  {
    mode = data[modeChannel-1];
  }
  if (mode == ARTNET)
  {
    // set brightness of the whole strip 
    if (universe == brightnessUniverse)
    {
      FastLED.setBrightness(data[brightnessChannel-1]);
    }
    // read universe and put into the right part of the display buffer
    for (int i = 0; i < length / 3; i++)
    {
      int led = i + (universe - startUniverse);
      if (led <= numLEDS)
      {
        leds[i].setRGB(data[i*3], data[i*3+1], data[i*3+2]);
      }
    }
    FastLED.show();
  }
}

//callback notifying us of the need to save config
void saveParamsCallback () {
  Serial.println(custom_mqtt_server.getValue());
}

boolean mqttReconnect() {
  if (mqttClient.connect("arduinoClient")) {
    // Once connected, publish an announcement...
    mqttClient.publish("outTopic","hello world");
    // ... and resubscribe
    mqttClient.subscribe("inTopic");
  }
  return mqttClient.connected();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP    
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  //reset settings - wipe credentials for testing
  //wm.resetSettings();
  wm.setSaveParamsCallback(saveParamsCallback);

  wm.setConfigPortalBlocking(false);
  //settingup a hostname
  wm.setHostname(hostname);

  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name
  if(wm.autoConnect(hostname)){
      Serial.println("connected...yeey :)");
  }
  else {
      Serial.println("Configportal running");
  }
  wm.startConfigPortal(hostname);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

/*
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument jsonDoc(512);
        DeserializationError error = deserializeJson(jsonDoc, buf.get());
        serializeJson(jsonDoc, Serial);
        if (error &&!jsonDoc.isNull()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, jsonDoc["mqtt_server"]);
          strcpy(mqtt_port, jsonDoc["mqtt_port"]);
          //strcpy(api_token, json["api_token"]);

          // if(json["ip"]) {
          //   Serial.println("setting custom ip from config");
          //   strcpy(static_ip, json["ip"]);
          //   strcpy(static_gw, json["gateway"]);
          //   strcpy(static_sn, json["subnet"]);
          //   Serial.println(static_ip);
          // } else {
          //   Serial.println("no custom ip in config");
          // }

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
*/

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length


  //add all your parameters here
  wm.addParameter(&custom_hostname);
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);

  //read updated parameters
  strcpy(hostname, custom_hostname.getValue());
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());

  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, numLEDS);
  FastLED.clear();
  FastLED.show();
  mqttClient.setServer(mqtt_server, String(mqtt_port).toInt());
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  wm.process();
  ArduinoOTA.handle();
  artnet.read();
  if (!mqttClient.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (mqttReconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected

    mqttClient.loop();
  }
}