# VortexDot
Small firmware for ESP32/8266 for intelligent LEDs

Features:
  * starts with an Access Point for configure the WIFI
  * Supporting many Diffrent LED Strips with FastLED
  * ART-Net
  * MQTT

# MQTT
Channel: /\<hostname\>/mode  
These channel are set the Mode of the Strip
  
Channel: /\<hostname\>/led  
Byte Array with 3Bytes per LED

# Art-Net

# Modes
0: Black -- every LED are Dark
1: Art-Net -- Art-Net set the pixels
2: MQTT -- MQTT set the pixels
3: Ring -- an nice animation
