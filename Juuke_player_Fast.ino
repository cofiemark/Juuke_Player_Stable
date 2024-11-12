/*
   Thanks to Original Author: ryand1011 (https://github.com/ryand1011)
   Edited by Ananords - https://github.com/ananords

   See: https://github.com/miguelbalboa/rfid/tree/master/examples/rfid_write_personal_data

   Uses MIFARE RFID card using RFID-RC522 reader
   Uses MFRC522 - Library
   -----------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   -----------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
   SPI SS      SDA(SS)      10            53        D10        10               10
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15



  PS: IF THE PLAYER FAILS TO START WHEN THE SERIAL MONITOR IS NOT OPEN, TRY TO COMMENT OUT Serial.begin(115200);
*/
#include <SPI.h>
#include <MFRC522.h>
#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above

const int playPauseButton = 4;
const int shuffleButton = 3;
const byte volumePot = A0;
int prevVolume; 
byte volumeLevel = 0;
bool isPlaying = false;

MFRC522 mfrc522(SS_PIN, RST_PIN);  // RFID reader instance
SoftwareSerial mySoftwareSerial(3, 6); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// Debounce timing
unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 200;

void setup() {
  Serial.begin(115200);
  mySoftwareSerial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(playPauseButton, INPUT_PULLUP);
  pinMode(shuffleButton, INPUT_PULLUP);

  Serial.println(F("Initializing DFPlayer..."));
  if (!myDFPlayer.begin(mySoftwareSerial, false)) {
    Serial.println(F("Unable to begin DFPlayer! Check connections and SD card."));
    while (true);  // Halt if DFPlayer fails
  }
  Serial.println(F("DFPlayer Mini online. Place card on reader to play a specific song"));

  volumeLevel = map(analogRead(volumePot), 0, 1023, 0, 30);
  myDFPlayer.volume(volumeLevel);
  prevVolume = volumeLevel;

  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
}

void loop() {
  adjustVolumeIfNeeded();
  handleButtonPress(playPauseButton, togglePlayPause);
  handleButtonPress(shuffleButton, shufflePlay);
  processCardReading();
}

void adjustVolumeIfNeeded() {
  volumeLevel = map(analogRead(volumePot), 0, 1023, 0, 30);
  if (abs(volumeLevel - prevVolume) >= 3) {
    myDFPlayer.volume(volumeLevel);
    Serial.println(volumeLevel);  
    prevVolume = volumeLevel;
    delay(1);
  }
}

void handleButtonPress(int buttonPin, void (*action)()) {
  if (digitalRead(buttonPin) == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > debounceDelay) {
      action();
      lastButtonPressTime = currentTime;
    }
  }
}

void togglePlayPause() {
  if (isPlaying) {
    myDFPlayer.pause();
    isPlaying = false;
    Serial.println("Paused..");
  } else {
    isPlaying = true;
    myDFPlayer.start();
    Serial.println("Playing..");
  }
}

void shufflePlay() {
  myDFPlayer.randomAll();
  Serial.println("Shuffle Play");
  isPlaying = true;
}

void processCardReading() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println(F("**Card Detected:**"));
  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));

  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte buffer2[18];
  byte block = 1;
  byte len = 18;

  int retryCount = 3;
  while (retryCount--) {
    auto status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Authentication failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      continue;
    }

    status = mfrc522.MIFARE_Read(block, buffer2, &len);
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Reading failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      continue;
    }

    String number = parseCardNumber(buffer2);
    Serial.print(F("Card Number: "));
    Serial.println(number);

    myDFPlayer.play(number.toInt());
    isPlaying = true;
    Serial.println(F("\n**End Reading**\n"));
    break;
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

String parseCardNumber(byte* buffer) {
  String number = "";
  for (uint8_t i = 0; i < 16; i++) {
    number += (char)buffer[i];
  }
  number.trim();
  return number;
}
