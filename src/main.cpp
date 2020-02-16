//******************************************//
//-----------------OTA-Block----------------//
//******************************************//
#include <ArduinoOTA.h>


//******************************************//
//---------------FastLED--Block-------------//
//******************************************//
#define FASTLED_INTERNAL
#define FASTLED_USE_GLOBAL_BRIGHTNESS 1
#include <FastLED.h>

// Here's how to control the LEDs from any two pins:
#ifndef DATAPIN
  #define DATAPIN = 21
#endif
#ifndef CLOCKPIN
  #define CLOCKPIN = 22
#endif

#define DATA_PIN DATAPIN
#define CLOCK_PIN CLOCKPIN

#ifndef DEFAULTNUMLED
#define DEFAULTNUMLED 20
#endif
const int numLEDS = DEFAULTNUMLED;
CRGB leds[numLEDS];


//******************************************//
//---------------ART-NET-Block--------------//
//******************************************//
#include <ArtnetWifi.h>
ArtnetWifi artnet;
int startUniverse = 0;
int endUniverse = startUniverse+int(numLEDS/512);
int numberOfChannels = numLEDS * 3; 
int startChannel = 0;
int brightnessChannel = 1;
int brightnessUniverse = 15;
int modeChannel = 255;
int modeUniverse = 4;


//******************************************//
//----------------MQTT-Block----------------//
//******************************************//
#include <WiFiClient.h>
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient mqttClient(espClient);
char mqtt_server[40] = "mqtt.hasi";
char mqtt_port[6] = "1883";
long lastReconnectAttempt = 0;
char mqtt_channel_mode[60];
char mqtt_channel_led[60];



//******************************************//
//-------------WIFI-Manager Block-----------//
//******************************************//
#include <FS.h>
#ifdef ESP32
  #include <SPIFFS.h>
#endif
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
WiFiManager wm;
#ifndef DEFAULTHOSTNAME
  #define DEFAULTHOSTNAME "VortexDot"
#endif
char hostname[40] = DEFAULTHOSTNAME;
//flag for saving data
bool shouldSaveConfig = false;

WiFiManagerParameter custom_hostname("hostname", "hostname", hostname, 40);
WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);


//******************************************//
//---------------Available Modes------------//
//******************************************//
#define BLACK 0
#define ARTNET 1
#define MQTT 2
#define RING 3
#define RAINBOW 4
#define FLASK 5

int mode = FLASK;


//******************************************//
//--------------Animation stuff-------------//
//******************************************//
static uint16_t tick = 0xffff;
long unsigned lastTime = 0;
uint16_t fps = 60;


//******************************************//
//-------------Animation(schnuppe)----------//
//******************************************//
#define NUM_RINGS 6
#define RING_RESOLUTION 1
#define RING_WIDTH 5

#ifndef MAX
#define MAX(x,y) ((x) >= (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x,y) ((x) <= (y) ? (x) : (y))
#endif

//******************************************//
//-------------Animation(kolben)------------//
//******************************************//
#define NUM_UV_LEDS 14
#define NUM_RGB_LEDS 82

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  if (universe == modeUniverse-1)
  {
    mode = data[modeChannel-1];
  }else if (mode == ARTNET){
    // set brightness of the whole strip 
    if (universe == brightnessUniverse-1)
    {
      FastLED.setBrightness(data[brightnessChannel-1]);
    }else if(universe >= startUniverse && universe<=endUniverse){
      // read universe and put into the right part of the display buffer
      for (int i = 0; i < length / 3; i++)
      {
        int led = i + (universe - startUniverse);
        if (led <= numLEDS)
        {
          leds[i].setRGB(data[i*3], data[i*3+1], data[i*3+2]);
        }
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
  if (mqttClient.connect(hostname)) {
    // Once connected, publish an announcement...
    // mqttClient.publish("outTopic","hello world");
    // ... and resubscribe
    mqttClient.subscribe(mqtt_channel_mode);
    Serial.print("mqtt: subscriped ");
    Serial.println(mqtt_channel_mode);
    mqttClient.subscribe(mqtt_channel_led);
    Serial.print("mqtt: subscriped ");
    Serial.println(mqtt_channel_led);
  }
  return mqttClient.connected();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char tmpMode[5];
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  if (!strcmp(topic,mqtt_channel_mode)){
    strncpy(tmpMode, (char*)payload, length);
    mode = atoi((const char*)tmpMode);
    Serial.println(mode);
  }else if (!strcmp(topic,mqtt_channel_led) && mode == MQTT){
    Serial.println("test");
    for (unsigned int i = 0; i < length && i < numLEDS*3; i+=3){
      leds[i/3].setRGB(payload[i], payload[i+1], payload[i+2]);
    }
    FastLED.show();
  }
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}


//animations

//classic tube
void render_rings (const uint16_t t)
{
  static uint16_t positions[NUM_RINGS];
  static uint8_t colors[NUM_RINGS * 3];
  static int8_t speeds[NUM_RINGS];
  uint8_t i, j, col[3];

  for (i = 0; i < NUM_RINGS; i++)
    {
      if (speeds[i] != 0)
        positions[i] += speeds[i];
      if (positions[i] < 0 || positions[i] >= numLEDS + 2 * RING_WIDTH)
        speeds[i] = 0;

      if (speeds[i] == 0 && random (10) == 0)
        {
          uint8_t basecolor;
          
          if (random (2) == 0)
            {
              positions[i] = 0;
              speeds[i] = random (3);            
            }
          else
            {
              positions[i] = numLEDS + 2 * RING_WIDTH - 1;
              speeds[i] = - random (3);            
            }

          basecolor = random (3);
          
          colors[i*3 + (basecolor + 0) % 3] = random (256);
          colors[i*3 + (basecolor + 1) % 3] = random (256);
          colors[i*3 + (basecolor + 2) % 3] = random (32);
        }
    }
    
  for (i = RING_WIDTH; i < numLEDS + RING_WIDTH; i++)
    {
      col[0] = col[1] = col[2] = 0;

      for (j = 0; j < NUM_RINGS; j++)
        {
          if (speeds[j] == 0)
            continue;
            
          col[0] = MIN (255, MAX (i, positions[j]) - MIN (i, positions[j]) < RING_WIDTH ? col[0] + colors[j*3+0] : col[0]);
          col[1] = MIN (255, MAX (i, positions[j]) - MIN (i, positions[j]) < RING_WIDTH ? col[1] + colors[j*3+1] : col[1]);
          col[2] = MIN (255, MAX (i, positions[j]) - MIN (i, positions[j]) < RING_WIDTH ? col[2] + colors[j*3+2] : col[2]);
        }
      leds[i-RING_WIDTH].setRGB(col[0], col[1], col[2]);
      //pixels.setPixelColor (i - RING_WIDTH, glut[col[0]], glut[col[1]], glut[col[2]]);
    }
  FastLED.show();
}

//Flask standard animation

void render_flask (const uint16_t t)
{
  static uint8_t uv_leds[NUM_UV_LEDS] = {0,1,7,15,23,31,39,47,55,63,71,79,87,95};
  static uint8_t rgb_leds[NUM_RGB_LEDS] = {2,3,4,5,6,8,9,10,11,12,13,14,16,17,18,19,20,21,22,24,25,26,27,28,29,30,32,33,34,35,36,37,38,40,41,42,43,44,45,46,48,49,50,51,52,53,54,56,57,58,59,60,61,62,64,65,66,67,68,69,70,72,73,74,75,76,77,78,80,81,82,83,84,85,86,88,89,90,91,92,93,94};

  uint8_t uv_col=0;
  uint8_t rgb_col=0;

  float uv_freq=1.0f;
  float rgb_freq=3.0f;

  float uv_speed=0.1f;
  float rgb_speed=0.05f;

  for(uint8_t i=0;i<NUM_UV_LEDS;i++){
      uv_col=255*(.5+.5*sin((i+t*uv_speed)*2.0f*M_PI*uv_freq/float(NUM_UV_LEDS-1)));
      leds[uv_leds[i]].setRGB(uv_col, uv_col, uv_col);
  }
  for(uint8_t i=0;i<NUM_RGB_LEDS;i++){
      rgb_col=80*MAX(0.0f,(-.3+sin((i+t*rgb_speed)*2.0f*M_PI*rgb_freq/float(NUM_RGB_LEDS-1))));
      leds[rgb_leds[i]].setRGB(rgb_col, 0, 0);
  }

  FastLED.show();
}

void setup() {
  //******************************************//
  //-------------WIFI-Manager Block-----------//
  //******************************************//

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
  if(wm.autoConnect(hostname))
  {
    Serial.println("connected...yeey :)");
  } else
  {
     Serial.println("Configportal running");
  }
  //FIXIT: optional starting by something
  //wm.startConfigPortal(hostname);

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


  //******************************************//
  //-----------------OTA-Block----------------//
  //******************************************//

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostname);

  // No authentication by default
  //ArduinoOTA.setPassword("bugsbunny");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
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


  //******************************************//
  //---------------ART-NET-Block--------------//
  //******************************************//
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);


  //******************************************//
  //---------------FastLED--Block-------------//
  //******************************************//
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, numLEDS);
  FastLED.clear();
  FastLED.show();


  //******************************************//
  //----------------MQTT-Block----------------//
  //******************************************//
  sprintf(mqtt_channel_mode, "%s%s%s","/",hostname,"/mode/");
  sprintf(mqtt_channel_led, "%s%s%s","/",hostname,"/led/");

  mqttClient.setServer(mqtt_server, String(mqtt_port).toInt());
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  //OTA Updater
  ArduinoOTA.handle();

  //Webinterface
  wm.process();

  //art-net
  artnet.read();

  //mqtt
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

  //Animations
  if(lastTime+(unsigned long)(1000000/fps) <= micros()) //chack of framerate
  {
    //Mode selection
    switch(mode)
    {
      case RING:
        render_rings(tick);
        break;
      case BLACK:
        fill_solid( leds, numLEDS, CRGB::Black);
        FastLED.show();
        break;
      case FLASK:
        render_flask(tick);
        break;
      
    }
    //animation tick
    tick--;
    lastTime = micros();
  }
}
