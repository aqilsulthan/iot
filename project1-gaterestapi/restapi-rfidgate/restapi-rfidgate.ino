#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define RED_LED_PIN 32
#define GREEN_LED_PIN 33
#define YELLOW_LED_PIN 25 
#define RFID_SS_PIN 5
#define RFID_RST_PIN 27
#define BUZZER_PIN 26
#define GATE_ID 2

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

const char* ssid = "ITS-WIFI1-2G";
const char* password = "itssurabaya";
const char* server = "http://10.15.43.75:3000/masuk";

int freq = 2000;
int channel = 0;
int resolution = 8;

void setup() {
  Serial.begin(9600);
  
  Serial.println("Starting..");

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);

  ledcSetup(channel, freq, resolution);
  ledcAttachPin(BUZZER_PIN, channel);

  WiFi.begin(ssid, password);

  Serial.println("Connecting to WiFi...");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(YELLOW_LED_PIN, LOW);
    delay(1000);
    digitalWrite(YELLOW_LED_PIN, HIGH);
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
  
  SPI.begin();
  rfid.PCD_Init();

  Serial.println("Place an RFID/NFC tag on the reader");
}

void loop() {
  ledcWriteTone(channel, 0);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);

  if(WiFi.status() == WL_CONNECTED){
    WiFiClient client;
    HTTPClient http;

    digitalWrite(YELLOW_LED_PIN, HIGH);
    if (rfid.PICC_IsNewCardPresent()) { 
      if (rfid.PICC_ReadCardSerial()) {
        digitalWrite(YELLOW_LED_PIN, LOW);
        http.begin(client, server);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String data = "gate_id=23&card_id=";

        MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        Serial.print("RFID/NFC Tag Type: ");
        Serial.println(rfid.PICC_GetTypeName(piccType));

        Serial.print("UID:");
        String uidString = "";
        for (int i = 0; i < rfid.uid.size; i++) {
          uidString += (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
        }
        Serial.println(uidString);

        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();

        Serial.print("UID: ");
        Serial.println(uidString);

        data += uidString;

        http.POST(data);

        if(http.getString() == "1") {
          digitalWrite(GREEN_LED_PIN, HIGH);
          ledcWriteTone(channel, 2000);
          delay(100);
          ledcWriteTone(channel, 0);
        } else {
          digitalWrite(RED_LED_PIN, HIGH);
          ledcWriteTone(channel, 2000);
          delay(100);
          ledcWriteTone(channel, 0);
        }

        http.end();

        delay(50);
      }
    }
  } else {
    Serial.println("WiFi Disconnected");
    WiFi.begin(ssid, password);

    while(WiFi.status() != WL_CONNECTED) {
      Serial.println("Connecting to WiFi...");
      digitalWrite(YELLOW_LED_PIN, HIGH);
      delay(500);
      digitalWrite(YELLOW_LED_PIN, LOW);
    }
  }
}
