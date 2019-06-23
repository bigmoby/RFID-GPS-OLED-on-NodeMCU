/*
  Project: ESP8266 ESP-12E module, NEO-6M GY-GPS6MV2 GPS module, 0.96" I2C OLED display module
  Function: This sketch listen to the GPS serial port,
  and when data received from the module, it's displays GPS data on 0.96" I2C OLED display module.

  ESP8266 ESP-12E module -> NEO-6M GY-GPS6MV2 GPS module
  VV (5V)     VCC
  GND         GND
  D10 (GPIO01) TX
  D9 (GPIO03) RX

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
  D4 (GPIO2)    SS
*/
#include <SoftwareSerial.h>           // include library code to allow serial communication on other digital pins of the Arduino board
#include <TinyGPS++.h>                // include the library code for GPS module
#include <Adafruit_ssd1306syp.h>      // include Adafruit_ssd1306syp library for OLED display
#include <EEPROM.h>                   // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>                      // RC522 Module uses SPI protocol
#include <MFRC522.h>                  // Library for Mifare RC522 Devices

// constants defined here:
#define EPROM_MEMORY_SIZE 512

static const int RXPin = 3, TXPin = 1; // GPS module RXPin - GPIO 03 (D9) and TXPin - GPIO 01 (D10)
static const uint32_t DefaultBaud = 9600; // default baud rate is 9600
static const double WAYPOINT_LAT = 51.508131, WAYPOINT_LON = -0.128002; // change coordinates of your waypoint

// Create MFRC522 instance.
#define SS_PIN 2 // Configurable RFID module SS pin (alias D4)
#define RST_PIN 0 // Configurable RFID module RESET pin (alias D3)
MFRC522 mfrc522(SS_PIN, RST_PIN);

bool programMode = false; // initialize programming mode to false

uint8_t successRead; // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4]; // Stores an ID read from EEPROM
byte readCard[4]; // Stores scanned ID read from RFID Module
byte masterCard[4]; // Stores master card's ID read from EEPROM

Adafruit_ssd1306syp display(4, 5); // create OLED display object (display (SDA, SCL)), SDA - GPIO 4, SCL - GPIO 5
TinyGPSPlus gps; // create the TinyGPS++ object
SoftwareSerial ss(RXPin, TXPin); // set the software serial connection to the GPS module

void setup() {
  EEPROM.begin(EPROM_MEMORY_SIZE);
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  display.initialize(); // initialize OLED display
  display.clear(); // clear OLED display
  display.setTextColor(WHITE); // set the WHITE color
  display.setTextSize(1); // set the font size 1
  display.setCursor(0, 0); // set the cursor to the OLED display top left corner (column 0, row 0)
  display.println("Setting up environment..."); 
  display.update(); // update OLED display
  delay(3000); // set delay for 3 seconds
  ss.begin(DefaultBaud); // initialise serial communication with GPS module

  //Protocol Configuration
  Serial.begin(DefaultBaud); // Initialize serial communications with PC
  SPI.begin(); // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init(); // Initialize MFRC522 Hardware

  Serial.println(F("Access Control Example...")); // For debugging purposes
  ShowReaderDetails(); // Show details of PCD - MFRC522 Card Reader details

  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    String message = "No Master Card Defined\nScan A PICC to Define as Master Card";
    Serial.println(message);
    genericDisplayMessage(message);

    do {
      successRead = getID(); // sets successRead to 1 when we get read from reader otherwise 0
      delay(200);
    }
    while (!successRead); // Program will not go further while you not get a successful read

    for (uint8_t j = 0; j < 4; j++) { // Loop 4 times
      EEPROM.write(2 + j, readCard[j]); // Write scanned PICC's UID to EEPROM, start from address 3
      EEPROM.commit();
    }

    EEPROM.write(1, 143); // Write to EEPROM we defined Master Card.
    EEPROM.commit();

    message = "Master Card Defined";
    Serial.println(message);
    genericDisplayMessage(message);
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for (uint8_t i = 0; i < 4; i++) { // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i); // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    // Visualize system is halted
    while (true); // do not go further
  }
}

///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if (!mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) { //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for (uint8_t i = 0; i < 4; i++) { //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void displayinfo() {
  display.clear();
  display.setCursor(0, 0);
  //display.print("DATE:    ");
  //display.print(gps.date.day());
  //display.print("-");
  //display.print(gps.date.month());
  //display.print("-");
  //display.println(gps.date.year());
  display.print("LATITUDE : ");
  display.println(gps.location.lat(), 6);
  display.print("LONGITUDE: ");
  display.println(gps.location.lng(), 6);
  display.print("UTC TIME : ");
  display.print(gps.time.hour());
  display.print(":");
  display.print(gps.time.minute());
  display.print(":");
  display.println(gps.time.second());
  display.print("ALTITUDE : ");
  display.print(gps.altitude.meters(), 2);
  display.println(" m");
  display.print("SATELLITE: ");
  display.println(gps.satellites.value());
  display.print("SPEED    : ");
  display.print(gps.speed.kmph(), 2);
  display.println(" km/h");
  display.print("COURSE   : ");
  display.print(gps.course.deg(), 2);
  display.println(" deg.");
  unsigned long distanceKmToWaypoint =
    (unsigned long) TinyGPSPlus::distanceBetween(
      gps.location.lat(),
      gps.location.lng(),
      WAYPOINT_LAT,
      WAYPOINT_LON) / 1000;
  display.print("KM TO WPT: ");
  display.println(distanceKmToWaypoint);
  display.update();
  delay(100);
}

void displayprogrammode() {
  display.clear();
  display.setCursor(0, 0);
  display.print("UTC TIME : ");
  display.print(gps.time.hour());
  display.print(":");
  display.print(gps.time.minute());
  display.print(":");
  display.println(gps.time.second());
  display.print("PROGRAM MODE...");
  display.update();
  delay(100);
}

void genericDisplayMessage(const String & message) {
  display.clear();
  display.setCursor(0, 0);
  display.print(message);
  display.update();
  delay(1000);
}

void successWrite() {
  display.clear();
  display.setCursor(0, 0);
  display.print("SUCCESS WRITE!");
  display.update();
  delay(100);
}

void failedWrite() {
  display.clear();
  display.setCursor(0, 0);
  display.print("FAILED WRITE.");
  display.update();
  delay(100);
}

void successDelete() {
  display.clear();
  display.setCursor(0, 0);
  display.print("SUCCESS DELETE!");
  display.update();
  delay(100);
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted(uint16_t setDelay,
             const String & message) {
  display.clear();
  display.setCursor(0, 0);
  display.print(message);
  display.update();
  delay(setDelay); // Hold dysplay info
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied(const String & message) {
  display.clear();
  display.setCursor(0, 0);
  display.print(message);
  display.update();
  delay(1000);
}

void loop() {
  do {
    successRead = getID(); // sets successRead to 1 when we get read from reader otherwise 0

    if (programMode) {
      displayprogrammode();
    } else {
      displayinfo(); // run procedure displayinfo
      smartDelay(500); // run procedure smartDelay
      if (millis() > 5000 && gps.charsProcessed() < 10) {
        display.setCursor(0, 0);
        display.println("No GPS detected:");
        display.println(" check wiring.");
      }
    }
  }
  while (!successRead); //the program will not go further while you are not getting a successful read

  if (programMode) {
    if (isMaster(readCard)) { //When in program mode check First If master card scanned again to exit program mode

      String message = "Master Card Scanned\nExiting Program Mode\n-----------------------------";
      Serial.println(message);
      genericDisplayMessage(message);

      programMode = false;
      return;
    } else {
      if (findID(readCard)) { // If scanned card is known delete it
        String message = "I know this PICC, removing...";
        Serial.println(message);
        genericDisplayMessage(message);

        deleteID(readCard);

        message = "-----------------------------\nScan a PICC to ADD or REMOVE to EEPROM";
        Serial.println(message);
        genericDisplayMessage(message);
      } else { // If scanned card is not known add it
        String message = "I do not know this PICC, adding...";
        Serial.println(message);
        genericDisplayMessage(message);

        writeID(readCard);
        message = "-----------------------------\nScan a PICC to ADD or REMOVE to EEPROM";
        Serial.println(message);
        genericDisplayMessage(message);
      }
    }
  } else {
    if (isMaster(readCard)) { // If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;

      String message = "Hello Master - Entered Program Mode";
      Serial.println(message);
      genericDisplayMessage(message);

      uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that

      message = "Scan a PICC to ADD or REMOVE to EEPROM\nScan Master Card again to Exit Program Mode\n-----------------------------\nFounded record(s) on EEPROM " + count;
      Serial.println(message);
      genericDisplayMessage(message);
    } else {
      if (findID(readCard)) { // If not, see if the card is in the EEPROM
        String message = "Welcome, You shall pass";
        Serial.println(message);
        granted(500, message);
      } else { // If not, show that the ID was not valid
        String message = "You shall not pass";
        Serial.println(message);
        genericDisplayMessage(message);

        denied(message);
      }
    }
  }
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID(uint8_t number) {
  uint8_t start = (number * 4) + 2; // Figure out starting position
  for (uint8_t i = 0; i < 4; i++) { // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i); // Assign values read from EEPROM to array
  }
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
bool isMaster(byte test[]) {
  Serial.println(F("Will be tested if it's a master card"));
  return checkTwo(test, masterCard);
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID(byte a[]) {
  if (!findID(a)) { // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0); // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = (num * 4) + 6; // Figure out where the next slot starts
    num++; // Increment the counter by one
    EEPROM.write(0, num); // Write the new count to the counter
    for (uint8_t j = 0; j < 4; j++) { // Loop 4 times
      EEPROM.write(start + j, a[j]); // Write the array values to EEPROM in the right position
    }
    EEPROM.commit();
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  } else {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID(byte a[]) {
  if (!findID(a)) { // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite(); // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  } else {
    uint8_t num = EEPROM.read(0); // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot; // Figure out the slot number of the card
    uint8_t start; // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping; // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT(a); // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--; // Decrement the counter by one
    EEPROM.write(0, num); // Write the new count to the counter
    for (j = 0; j < looping; j++) { // Loop the card shift times
      EEPROM.write(start + j, EEPROM.read(start + 4 + j)); // Shift the array values to 4 places earlier in the EEPROM
    }
    for (uint8_t k = 0; k < 4; k++) { // Shifting loop
      EEPROM.write(start + j + k, 0);
    }
    EEPROM.commit();
    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
bool checkTwo(byte a[], byte b[]) {
  for (uint8_t k = 0; k < 4; k++) { // Loop 4 times
    if (a[k] != b[k]) { // IF a != b then false, because: one fails, all fail
      return false;
    }
  }
  return true;
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT(byte find[]) {
  uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that
  for (uint8_t i = 1; i <= count; i++) { // Loop once for each EEPROM entry
    readID(i); // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard)) { // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i; // The slot number of the card
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
bool findID(byte find[]) {
  uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that
  
  String message = "Founded record(s) on EEPROM " + count;
  Serial.println(message);
  genericDisplayMessage(message);

  for (uint8_t i = 1; i < count; i++) { // Loop once for each EEPROM entry
    readID(i); // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard)) { // Check to see if the storedCard read from EEPROM
      return true;
    }
  }
  return false;
}

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}
