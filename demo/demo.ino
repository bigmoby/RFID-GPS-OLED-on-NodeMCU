/*
  Project: ESP8266 ESP-12E module, NEO-6M GY-GPS6MV2 GPS module, 0.96" I2C OLED display module
  Function: This sketch listen to the GPS serial port,
  and when data received from the module, it's displays GPS data on 0.96" I2C OLED display module.

  ESP8266 ESP-12E module -> NEO-6M GY-GPS6MV2 GPS module
  VV (5V)     VCC
  GND         GND
  D10 (GPIO01) RX
  D9 (GPIO03) TX

  ESP8266 ESP-12E module -> White 0.96" I2C OLED display module
  3V3        VCC
  GND        GND
  D1 (GPIO5) SCL
  D2 (GPIO4) SDA

  ESP8266 ESP-12E module -> RC522
  3V3           VCC
  GND           GND
  D3 (GPIO0)    RST
  D6 (GPIO12)   MISO
  D7 (GPIO13)   MOSI
  D5 (GPIO14)   SCK
  D4 (GPIO2)    SDA
*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TinyGPS++.h>                                  // Tiny GPS Plus Library
#include <SoftwareSerial.h>                             // Software Serial Library so we can use other Pins for communication with the GPS module
#include <Adafruit_ssd1306syp.h>                        // Adafruit oled library for display


//----SERIAL CONFIG ----
#define SERIAL_SPEED        115200

//----WIFI CONFIG ----
const char* ssid = "Vodafone-34426516";
const char* password =  "T3dT,}wh=7<+csj";

//----MQTT CONFIG ----
const char* mqttServer = "m20.cloudmqtt.com";
const int mqttPort = 17024;
const char* mqttUser = "tapxbquu";
const char* mqttPassword = "W7c8SsIhNOsz";

#define MQTT_TOPIC_COMMON       "esp8266/Demo1/common"
#define MQTT_TOPIC_GPS          "esp8266/Demo1/gps"

WiFiClient espClient;
PubSubClient mqtt_client(espClient);
bool mqtt_status = false;

Adafruit_ssd1306syp display(4, 5);                      // OLED display (SDA to Pin 4), (SCL to Pin 5)

static const int RXPin = 3, TXPin = 1;                  // GPS module RXPin - GPIO 03 (D9) and TXPin - GPIO 01 (D10)
static const uint32_t GPSBaud = 9600;                   // Ublox GPS default Baud Rate is 9600

TinyGPSPlus gps;                                        // Create an Instance of the TinyGPS++ object called gps
SoftwareSerial ss(RXPin, TXPin);                        // The serial connection to the GPS device

String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

void setup() {
  //    Serial.begin(GPSBaud);
  ss.begin(GPSBaud);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //      Serial.println("Connecting to WiFi..");
  }

  //    Serial.println("Connected to the WiFi network");
  mqtt_client.setServer(mqttServer, mqttPort);

  while (!mqtt_client.connected()) {
    //      Serial.println("Connecting to MQTT...");
    String clientName;
    clientName += "esp8266-";
    uint8_t mac[6];
    WiFi.macAddress(mac);
    clientName += macToStr(mac);
    clientName += "-";
    clientName += String(micros() & 0xff, 16);

    if (mqtt_client.connect((char*) clientName.c_str(), mqttUser, mqttPassword )) {
      //        Serial.println("connected");
      mqtt_status = true;
    } else {
      //        Serial.print("failed with state ");
      //        Serial.print(mqtt_client.state());
      mqtt_status = false;
      delay(2000);
    }
  }

  mqtt_client.publish(MQTT_TOPIC_COMMON, "Hello from ESP8266");

  display.initialize();                                 // Initialize OLED display
  display.clear();                                      // Clear OLED display
  display.setTextSize(1);                               // Set OLED text size to small
  display.setTextColor(WHITE);                          // Set OLED color to White
  display.setCursor(0, 0);                              // Set cursor to 0,0
  display.println("GPS example");
  display.println(TinyGPSPlus::libraryVersion());
  display.update();                                     // Update display
  delay(1500);                                          // Pause 1.5 seconds
}

void loop()
{
  display.clear();
  display.setCursor(0, 0);
  display.print("DATE:    ");
  display.print(gps.date.day());
  display.print("-");
  display.print(gps.date.month());
  display.print("-");
  display.println(gps.date.year());
  display.print("Latitude  : ");
  display.println(gps.location.lat(), 5);
  display.print("Longitude : ");
  display.println(gps.location.lng(), 4);
  display.print("Satellites: ");
  display.println(gps.satellites.value());
  display.print("Elevation : ");
  display.print(gps.altitude.meters());
  display.println("m");
  display.print("Time UTC  : ");
  display.print(gps.time.hour());                       // GPS time UTC
  display.print(":");
  display.print(gps.time.minute());                     // Minutes
  display.print(":");
  display.println(gps.time.second());                   // Seconds

  display.update();                                     // Update display
  delay(200);

  smartDelay(500);                                      // Run Procedure smartDelay

  if (millis() > 5000 && gps.charsProcessed() < 10)
    display.println(F("No GPS data received: check wiring"));

  if (mqtt_status) {
    String payload = "{\"lat\":";
    payload += String(gps.location.lat(), 5);
    payload += ",\"long\":";
    payload += String(gps.location.lng(), 4);
    payload += "}";
    mqtt_client.publish(MQTT_TOPIC_GPS, (char*) payload.c_str());
    delay(3000);
  }
}

static void smartDelay(unsigned long ms)                // This custom version of delay() ensures that the gps object is being "fed".
{
  unsigned long start = millis();
  do
  {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}
