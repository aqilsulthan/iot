#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Wire.h>

#define RFID_SS_PIN  5 
#define RFID_RST_PIN 27
#define LED_RED_PIN 32
#define LED_GREEN_PIN 33
#define LED_YELLOW_PIN 25
#define GATE_ID 2
#define BUZZER_PIN 26

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

const char* ssid = "ITS-WIFI-2G";
const char* password = "itssurabaya";
const char* mqtt_server = "10.15.43.80";

int buzzerFreq = 2000;
int buzzerChannel = 0;
int buzzerResolution = 8;

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  Serial.begin(9600);
  
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_RED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_RED_PIN, LOW);
  }

  client.setServer(mqtt_server, 8011);
  client.setCallback(callback);

  ledcSetup(buzzerChannel, buzzerFreq, buzzerResolution);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);
  
  SPI.begin();
  rfid.PCD_Init();

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "keluar") {
    if(messageTemp == "1") {
      digitalWrite(LED_GREEN_PIN, HIGH);
      ledcWriteTone(buzzerChannel, buzzerFreq);
      delay(100);
      ledcWriteTone(buzzerChannel, 0);
    } else {
      // Authentication failed
      digitalWrite(LED_RED_PIN, HIGH);
      ledcWriteTone(buzzerChannel, buzzerFreq);
      delay(100);
      ledcWriteTone(buzzerChannel, 0);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("")) {
      Serial.println("Connected to MQTT");
      client.subscribe("keluar");
    } else {
      delay(1000);
    }
  }
}

void loop() {
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);
  ledcWrite(buzzerChannel, 0);

  if(WiFi.status() == WL_CONNECTED){
    digitalWrite(LED_YELLOW_PIN, HIGH);
    if (!client.connected()) {
      Serial.println("Reconnected");
      reconnect();
    }
    
    client.loop();
    
    if (rfid.PICC_IsNewCardPresent()) {
      if (rfid.PICC_ReadCardSerial()) {
        digitalWrite(LED_YELLOW_PIN, LOW);
        MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        
        String uidString = "";
        for (int i = 0; i < rfid.uid.size; i++) {
          uidString += (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
        }

        char uidChars[100];
        uidString.toCharArray(uidChars, uidString.length() + 1);

        char data[100];
        int id_register_gate = 2;
        sprintf(data, "{\"id_register_gate\":%d,\"id_kartu_akses\": \"%s\"}", id_register_gate, uidChars);

        client.publish("keluar/result", data);

        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();

        delay(100);
      }
    }
  } else {
    Serial.println("WiFi Disconnected");
    digitalWrite(LED_RED_PIN, HIGH);
  }
}