/*
 Basic ESP8266 MQTT example
 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"
*/

#include <WiFiEsp.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <avr/wdt.h>


// Emulate Serial1 on pins 6/7 if not present
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(6, 7); // RX, TX
#endif

#define RED_LED 2
#define GREEN_LED 3

#define RST_PIN 9
#define SS_PIN 10

// Update these with values suitable for your network.

byte readCard[4];
String MasterTag = "D60E35F9";  // REPLACE this Tag ID with your Tag ID!!!
String tagID = "";

unsigned int rfidDelay = 2000;
unsigned long lastRfidScan;

// Create instances
MFRC522 mfrc522(SS_PIN, RST_PIN);

//const char* ssid = "telenet-150EA";
//const char* password = "n05HJCWAMbSP";
//const char* ssid = "iot";
//const char* password = "wachtwoord";
const char* ssid = "VERMEIRE";
const char* password = "DRAMADRAMA";
const char* mqtt_server = "192.168.1.34";
int status = WL_IDLE_STATUS;

WiFiEspClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

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

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  wdt_reset();  // Reset the watchdog

  randomSeed(micros());

  printWifiStatus();
}

void callback(char* topic, byte* payload, unsigned int length) {
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  digitalWrite(GREEN_LED,HIGH);
  digitalWrite(RED_LED,LOW);
  delay(3000);
  digitalWrite(GREEN_LED,LOW);
  digitalWrite(RED_LED,HIGH);

  wdt_reset();  // Reset the watchdog
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      
      delay(5000);  // Wait 5 seconds before retrying
      
      wdt_reset();  // Reset the watchdog
    }
  }
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

void setup() {
  wdt_disable();  // Disable the watchdog and wait for more than 2 seconds,
  delay(3000);  // so that the Arduino doesn't keep resetting infinitely in case of wrong configuration
  wdt_enable(WDTO_8S); // enable watchdog timer on 8 seconds

  pinMode(RED_LED,OUTPUT);
  pinMode(GREEN_LED,OUTPUT);
  Serial.begin(115200);  // initialize serial for ESP module
  Serial1.begin(9600);  // initialize ESP module
  WiFi.init(&Serial1);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  SPI.begin(); // SPI bus
  mfrc522.PCD_Init(); // MFRC522
  Serial.println("Scan your rfid card");
}

void loop() {
  // attempting reconnect if connection failed
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  digitalWrite(RED_LED, HIGH);

  //Wait until new tag is available
  while (getID() && (millis() - rfidDelay > lastRfidScan)) 
  {
    lastRfidScan = millis();
    char message[MSG_BUFFER_SIZE];
    strcpy(message,tagID.c_str());   
    if (tagID == MasterTag) 
    {      
      Serial.println("Access granted!");
      message[tagID.length()] = '1';
      digitalWrite(GREEN_LED,HIGH);
      digitalWrite(RED_LED,LOW);
      delay(3000);
      digitalWrite(GREEN_LED,LOW);
      digitalWrite(RED_LED,HIGH);
    }
    else
    {
      Serial.println("Access Denied!");
      message[tagID.length()] = '0';
      for (int i = 0; i < 10; i++) {
        digitalWrite(RED_LED,LOW);
        delay(100);
        digitalWrite(RED_LED,HIGH);
        delay(100);
      }
    }
    Serial.print("Publish message: ");
    Serial.println(message);
    client.publish("outTopic", message);
  }

  wdt_reset();  // Reset the watchdog

  /*unsigned long now = millis();
  if (now - lastMsg > 30000) {
    lastMsg = now;
    ++value;
    snprintf (msg, MSG_BUFFER_SIZE, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);
  }*/
}
