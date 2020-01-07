/*
 * Authors: Loic Vermeire & Anton Thomas
 * Email: loic.vermeire@student.kdg.be
 *        anton.thomas@student.kdg.be
*/

#include <WiFiEsp.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <avr/wdt.h>
#include <LiquidCrystal.h>

// Emulate Serial1 on pins 6/7 if not present
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(6, 7); // RX, TX
#endif

//LED pins
#define RED_LED 2
#define GREEN_LED 3

//RFID pins
#define RST_PIN 9
#define SS_PIN 10

// Create instances
MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiEspClient espClient;
PubSubClient client(espClient);
// lcd(RS,E,D4,D5,D6,D7) mapped to analog
LiquidCrystal lcd(A1, A0, A2, A3, A4, A5);

//variables related to wifi connection
//update if necessary
const char* ssid = "AndroidAP3113";
const char* password = "iotExamen";
const char* mqtt_server = "BROKER_IP";
int status = WL_IDLE_STATUS;

//rfid tag variables
byte readCard[4];
String MasterTag = "D60E35F9";  // REPLACE this Tag ID with your Tag ID!!!
String tagID = "";

//lcd variables
bool lcdDefault;
unsigned short lcdChangeDelay = 3000; //LCD prints default message after 3s, change this value as desired
unsigned long lastLcdChange;

//msg buffer for publishing
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

//variables related to lock
enum LOCK_STATE {
  LOCK_OPEN,
  LOCK_CLOSED,
  LOCK_DENIED
};
LOCK_STATE lockState;
LOCK_STATE lastLockState;
unsigned long lockOpenMillis;
unsigned short lockOpenDelay = 3000; //Lock remains open for 3s, change this value as desired

//delay for rfid scan
unsigned short rfidDelay = 3000; //One scan every 3s, change this value as desired
unsigned long lastRfidScan;

//delay for mqtt loop
byte mqttLoopDelay = 100; //short delay to avoid stressing esp over SoftwareSerial
unsigned long lastMqttLoop;

void setup() {
  wdt_disable();  // Disable the watchdog and wait for more than 2 seconds,
  delay(3000);  // so that the Arduino doesn't keep resetting infinitely in case of wrong configuration
  wdt_enable(WDTO_8S); // enable watchdog timer on 8 seconds

  //setup LEDs and lock
  lockState = LOCK_CLOSED;
  lastLockState = lockState;
  pinMode(RED_LED,OUTPUT);
  pinMode(GREEN_LED,OUTPUT);
  digitalWrite(RED_LED, HIGH);
  
  Serial.begin(9600);  // initialize serial for console
  Serial1.begin(9600);  // initialize ESP module

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.noBlink();
  lcd.clear();
  // Print a message to the LCD.
  lcd.print("Initialising...");
  
  WiFi.init(&Serial1); //initialize wifi connection
  setup_wifi();
  
  client.setServer(mqtt_server, 1883); //initialize mqtt
  client.setCallback(callback); 

  SPI.begin(); // SPI bus
  mfrc522.PCD_Init(); // MFRC522

  //setup complete blink LED
  for (int i = 0; i < 4; i++) {
        digitalWrite(RED_LED,LOW);
        delay(100);
        digitalWrite(RED_LED,HIGH);
        delay(100);
  }

  //default LCD message
  writeLcdDefault();
}

void loop() {
  wdt_reset();

  //non-blocking delay needed to not overload ESP
  if (millis() - mqttLoopDelay >= lastMqttLoop) {
    // attempting reconnect if connection failed
    if (!client.connected()) {
      reconnect();
    }
    //if there is a message, publish it
    if (msg[0] != '\0') {
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish("outTopic", msg);
      msg[0] = '\0'; //empty message
    }
    client.loop();
  }

  //rfidDelay limits subsequent RFID scans
  if (millis() - rfidDelay >= lastRfidScan) {
    check_RFID();  
  }

  //check the state of the lock
  checkLockState();

  //revert lcd to default message after delay
  if ((!lcdDefault) && (millis() - lcdChangeDelay >= lastLcdChange)) {
    writeLcdDefault();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  payload[length] = '\0'; 
  if (!strcmp(topic,"inTopic")) {
    if (!strcmp((char *)payload,"unlock")) {
      lockState = LOCK_OPEN;
      lockOpenMillis = millis();
      Serial.println("Door Unlocked");
      writeLcd("Door Unlocked");
    }
  }


  wdt_reset();  // Reset the watchdog
}

void checkLockState() {
  
    //close lock after a certain delay
    if (lockState == LOCK_OPEN && (millis() - lockOpenDelay >= lockOpenMillis)) {
      lockState = LOCK_CLOSED;
    }

    //check change in state of lock
    if (lastLockState != lockState) {
      lastLockState = lockState;
      if (lockState == LOCK_OPEN) {
        digitalWrite(GREEN_LED,HIGH);
        digitalWrite(RED_LED,LOW);
        //code to open lock
      } 
      else if (lockState == LOCK_DENIED) {
        //blink 6 times
        for (int i = 0; i < 6; i++) {
          digitalWrite(RED_LED,LOW);
          delay(100);
          digitalWrite(RED_LED,HIGH);
          delay(100);
        }
        lockState = LOCK_CLOSED;
      }
      else {
        digitalWrite(GREEN_LED,LOW);
        digitalWrite(RED_LED,HIGH);
        //code to close lock
      }
    }

  wdt_reset();  
}

void check_RFID() {
  //Wait until new tag is available
  while (getID()) 
  {
    lastRfidScan = millis();
    strcpy(msg,tagID.c_str());   
    if (tagID == MasterTag) 
    {      
      lockState = LOCK_OPEN;
      lockOpenMillis = millis();
      Serial.println("Access Granted!");
      writeLcd("Access Granted!");
      msg[tagID.length()] = '1';
    }
    else
    {
      lockState = LOCK_DENIED;
      Serial.println("Access Denied!");
      writeLcd("Access Denied!");
      msg[tagID.length()] = '0';
    }
  }
  wdt_reset();  // Reset the watchdog  
}

// Read new tag if available
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
  wdt_reset();  // Reset the watchdog
  return true;
}

void printWifiStatus()
{
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  wdt_reset();  // Reset the watchdog
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    wdt_reset(); //reset before each connection attempt
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "Arduino reconnected after timeout");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
      
      delay(3000);  // Wait 3 seconds before retrying
      
    }
  }
  wdt_reset();  // Reset the watchdog
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue
    while (true);
  }
  
  wdt_reset(); //reset before connection attempt 
  WiFi.begin(ssid, password);

  wdt_reset();  // Reset the watchdog

  randomSeed(micros());

  printWifiStatus();
}

void writeLcd(char* msg) {
  lcd.clear();
  lcd.print(msg);
  lastLcdChange = millis();
  lcdDefault = 0;
}

void writeLcdDefault() {
  lcd.clear();
  lcd.print("Please scan RFID");
  lcdDefault = 1;
}
