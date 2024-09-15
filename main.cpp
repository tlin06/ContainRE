#include <Wire.h>
#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <HTTPClient.h>


// If using the breakout with SPI, define the pins for SPI communication.
#define PN532_SCK  (2)
#define PN532_MOSI (13)
#define PN532_SS   (14)
#define PN532_MISO (15)

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield

// Uncomment just _one_ line below depending on how your breakout or shield
// is connected to the Arduino:

// Use this line for a breakout with a software SPI connection (recommended):
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Use this line for a breakout with a hardware SPI connection.  Note that
// the PN532 SCK, MOSI, and MISO pins need to be connected to the Arduino's
// hardware SPI SCK, MOSI, and MISO pins.  On an Arduino Uno these are
// SCK = 13, MOSI = 11, MISO = 12.  The SS line can be any digital IO pin.
//Adafruit_PN532 nfc(PN532_SS);

// Or use this line for a breakout or shield with an I2C connection:
//Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// Or use hardware Serial:
//Adafruit_PN532 nfc(PN532_RESET, &Serial1);

const char *serverip = "http://192.168.140.106:5000/update_signal";
const char *phKerb = "hacker";
char lastKerberos[16];
int lastKerbMillis;
const char *phStation = "HackMIT_2024";
bool checkInSwitch = true;
void IRAM_ATTR modeChange() {
  checkInSwitch = digitalRead(12);
  digitalWrite(33, checkInSwitch);
  Serial.println(checkInSwitch ? "Changed to CHECK IN MODE" : "Changed to CHECK OUT MODE");
}

void setup(void) {

  pinMode(12, INPUT_PULLUP);
  pinMode(33, OUTPUT);
  checkInSwitch = digitalRead(12);
  digitalWrite(33, checkInSwitch);
  attachInterrupt(12, modeChange, CHANGE);
  Serial.begin(115200);
  while (!Serial) delay(10); // for Leonardo/Micro/Zero
  Serial.println("Hello!");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }

  Serial.println("Setting up WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin("takeout container", "hackmit24");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);

  Serial.println("Waiting for an ISO14443A Card ...");
}



void loop(void) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    digitalWrite(4, HIGH);
    // Display some basic information about the card
    Serial.println("Device tapped!");
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");

    if (uidLength == 4)
    {
      // We probably have a Mifare Classic card ...
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	  // Start with block 4 (the first block of sector 1) since sector 0
	  // contains the manufacturer data and it's probably better just
	  // to leave it alone unless you know what you're doing
      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);

      if (success)
      {
        uint8_t data[16];

        // If you want to write something to block 4 to test with, uncomment
		// the following line and this text should be read back in a minute
        //memcpy(data, (const uint8_t[]){ 'a', 'd', 'a', 'f', 'r', 'u', 'i', 't', '.', 'c', 'o', 'm', 0, 0, 0, 0 }, sizeof data);
        // success = nfc.mifareclassic_WriteDataBlock (4, data);

        // Try to read the contents of block 4
        success = nfc.mifareclassic_ReadDataBlock(4, data);
        if (data[4] == 145) {
          data[0] = 'P';
          data[1] = 'H';
          data[2] = 'I';
          data[3] = 'D';
          strncpy((char*)&(data[4]), "hacker24", 8);
          nfc.mifareclassic_WriteDataBlock(4, data);
        }
        if (data[0] == 'T' && data[1] == 'R' && data[2] == 'A' && data[3] == 'Y') {
          Serial.print("Tray ID: ");
          Serial.println(data[4]);
          if (millis() - lastKerbMillis < 5000) {
            tone(16, 880, 350);
            lastKerbMillis = 0;

            HTTPClient http;
            WiFiClient wifi;
            http.begin(wifi, serverip);

            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            char httpRequestData[256];
            sprintf(httpRequestData, "id=%d&kerberos=%s&station=%s&io=%s", data[4], lastKerberos, phStation, checkInSwitch ? "in" : "out");
            //sprintf(httpRequestData, "%s", "strength=1234");
            int respcode = http.POST(httpRequestData);
            Serial.print("HTTP Response code: ");
            Serial.println(respcode);
            http.end();
          } else {
            Serial.println("Last kerb tap was too long ago!");
          }
        } else if (data[0] == 'P' && data[1] == 'H' && data[2] == 'I' && data[3] == 'D') {
          Serial.print("This user checked in: ");
          Serial.println((char*)&(data[4]));
          Serial.println("They have 5 seconds to tap a tray ID.");
          strncpy(lastKerberos, (const char*)&(data[4]), 8);
          lastKerbMillis = millis();
          tone(16, 440, 450);
        } else {
          Serial.print("Initializing new Tray ID as: ");
          Serial.println(uid[0] ^ uid[1] ^ uid[2] ^ uid[3]);
          data[4] = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];
          data[0] = 'T';
          data[1] = 'R';
          data[2] = 'A';
          data[3] = 'Y';
          nfc.mifareclassic_WriteDataBlock(4, data);
        }
        
        while(nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya));
        delay(300);
      }
    }

    if (uidLength == 7)
    {
      // We probably have a Mifare Ultralight card ...
      Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");
      Serial.println("(unimplemented)");
    }
  }
}

