#include <WiFi.h>
#include <Wire.h>
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include "proto/service.grpc.pb.h"

#define SS_PIN  5 
#define RST_PIN 27
#define RED_PIN 32
#define GREEN_PIN 33
#define YELLOW_PIN 25
#define GATE_ID 2
#define BUZZER_PIN 26

MFRC522 rfid(SS_PIN, RST_PIN);

const char* ssid = "ITS-WIFI-2G";
const char* password = "itssurabaya";
const char* grpc_server = "10.15.43.80:50051";

WiFiClient espClient;

long lastMsg = 0;
char msg[50];
int value = 0;

class GRPCClient {
public:
    explicit GRPCClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(service::DoorAccessService::NewStub(channel)) {}

    bool VerifyAccess(const std::string& gateId, const std::string& cardId) {
        service::VerifyAccessRequest request;
        request.set_gate_id(gateId);
        request.set_card_id(cardId);

        service::VerifyAccessResponse response;

        grpc::ClientContext context;
        grpc::Status status = stub_->VerifyAccess(&context, request, &response);

        if (status.ok()) {
            return response.success();
        } else {
            std::cout << "RPC failed with error code: " << status.error_code() << std::endl;
            return false;
        }
    }

private:
    std::unique_ptr<service::DoorAccessService::Stub> stub_;
};

void setup() {
    Serial.begin(9600);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(RED_PIN, HIGH);
        delay(250);
        digitalWrite(RED_PIN, LOW);
    }

    ledcSetup(channel, freq, resolution);
    ledcAttachPin(BUZZER_PIN, channel);

    SPI.begin();
    rfid.PCD_Init();

    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(YELLOW_PIN, OUTPUT);
}

void loop() {
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, LOW);
    ledcWrite(channel, 0);

    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(YELLOW_PIN, HIGH);

        if (rfid.PICC_IsNewCardPresent()) {
            if (rfid.PICC_ReadCardSerial()) {
                digitalWrite(YELLOW_PIN, LOW);
                MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

                String uidString = "";
                for (int i = 0; i < rfid.uid.size; i++) {
                    uidString += (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
                }

                std::string grpc_server_str(grpc_server);
                std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(grpc_server_str, grpc::InsecureChannelCredentials());
                GRPCClient grpcClient(channel);

                bool accessGranted = grpcClient.VerifyAccess(std::to_string(GATE_ID), uidString.c_str());

                if (accessGranted) {
                    // Success
                    digitalWrite(GREEN_PIN, HIGH);
                    ledcWriteTone(channel, freq);
                    delay(100);
                    ledcWriteTone(channel, 0);
                } else {
                    // Failed
                    digitalWrite(RED_PIN, HIGH);
                    ledcWriteTone(channel, freq);
                    delay(100);
                    ledcWriteTone(channel, 0);
                }

                rfid.PICC_HaltA(); // halt PICC
                rfid.PCD_StopCrypto1(); // stop encryption on PCD

                delay(100);
            }
        }
    } else {
        Serial.println("WiFi Disconnected");
        digitalWrite(RED_PIN, HIGH);
    }
}
