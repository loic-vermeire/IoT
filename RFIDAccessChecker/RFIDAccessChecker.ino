#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN 10

byte readCard[4];
String MasterTag = "D60E35F9";  // REPLACE this Tag ID with your Tag ID!!!
String tagID = "";

unsigned int rfidDelay = 2000;
unsigned long lastRfidScan;

// Create instances
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() 
{
  // Initiating
  Serial.begin(9600);
  SPI.begin(); // SPI bus
  mfrc522.PCD_Init(); // MFRC522
  Serial.println("Scan your rfid card");
}

void loop() 
{
  
  //Wait until new tag is available
  while (getID() && (millis() - rfidDelay > lastRfidScan)) 
  {

    lastRfidScan = millis();
    
    if (tagID == MasterTag) 
    {
      
      Serial.println("Access granted!");
    }
    else
    {
      Serial.println("Access Denied!");
    }


  }
}

//Read new tag if available
boolean getID() 
{
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
  return false;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) { //Since a PICC placed get Serial and continue
  return false;
  }
  tagID = "";
  for ( uint8_t i = 0; i < 4; i++) { // The MIFARE PICCs that we use have 4 byte UID
  //readCard[i] = mfrc522.uid.uidByte[i];
  if (mfrc522.uid.uidByte[i] < 16) { // Catch loss of leading zeroes in bytes
     tagID.concat(0); 
     tagID.concat(String(mfrc522.uid.uidByte[i], HEX)); // Adds the 4 bytes in a single String variable
    }
  else {
    tagID.concat(String(mfrc522.uid.uidByte[i], HEX)); // Adds the 4 bytes in a single String variable
  }
  }
  tagID.toUpperCase();
  //Serial.println(tagID);
  mfrc522.PICC_HaltA(); // Stop reading
  return true;
}
