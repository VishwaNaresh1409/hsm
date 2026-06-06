#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "DHT.h"

// ===== Configuration (UPDATE THESE) =====
const char* ssid = "765KV";
const char* password = "transistor";
const char* mqtt_server = "0cc63655558d460093c7a1a3185b0af2.s1.eu.hivemq.cloud";
const int   mqtt_port = 8883;
const char* mqtt_user = "farmer_1"; 
const char* mqtt_pass = "Abcd1234";

// ===== Pin Definitions =====
#define RE 4
#define DE 5
#define RXD2 16
#define TXD2 17
#define DHTPIN 27
#define DHTTYPE DHT11
#define MQ135_PIN 34
#define SOIL_PIN 35
#define RELAY_PIN 26

// ===== Objects =====
DHT dht(DHTPIN, DHTTYPE);
HardwareSerial mod(2);
WiFiClientSecure espClient;
PubSubClient client(espClient);

// NPK Modbus Commands
const byte nitro[] = {0x01,0x03,0x00,0x1e,0x00,0x01,0xe4,0x0c};
const byte phos[]  = {0x01,0x03,0x00,0x1f,0x00,0x01,0xb5,0xcc};
const byte pota[]  = {0x01,0x03,0x00,0x20,0x00,0x01,0x85,0xc0};
byte values[11];

// ===== Function Prototypes =====
void setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
byte readRS485(const byte *command);

void setup() {
    Serial.begin(9600);
    
    // Pins
    pinMode(RE, OUTPUT); pinMode(DE, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    // Sensors
    mod.begin(4800, SERIAL_8N1, RXD2, TXD2);
    dht.begin();

    setup_wifi();
    
    // HiveMQ Cloud requires SSL
    espClient.setInsecure(); 
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop();

    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 5000) { // Send every 5 seconds
        lastMsg = millis();

        // 1. Read All Sensors
        int nVal = readRS485(nitro); delay(100);
        int pVal = readRS485(phos);  delay(100);
        int kVal = readRS485(pota);  delay(100);
        
        float hum = dht.readHumidity();
        float temp = dht.readTemperature();
        int air = analogRead(MQ135_PIN);
        int soilRaw = analogRead(SOIL_PIN);
        int soilPercent = map(soilRaw, 4095, 1200, 0, 100); // Simple calibration
        soilPercent = constrain(soilPercent, 0, 100);

        // 2. Create JSON Payload
        JsonDocument doc;
        doc["n"] = nVal;
        doc["p"] = pVal;
        doc["k"] = kVal;
        doc["temp"] = isnan(temp) ? 0 : temp;
        doc["hum"] = isnan(hum) ? 0 : hum;
        doc["air"] = air;
        doc["soil"] = soilPercent;

        char buffer[256];
        serializeJson(doc, buffer);
        
        // 3. Publish to Cloud
        client.publish("farm/sensors", buffer);
        Serial.print("Published to HiveMQ: ");
        Serial.println(buffer);
    }
}

// Handle Pump Control from Streamlit
void callback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) msg += (char)payload[i];
    
    Serial.println("Message arrived [" + String(topic) + "]: " + msg);

    if (msg == "PUMP_ON") {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("PUMP STARTED");
        delay(5000); // Runs for 5 seconds
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("PUMP STOPPED");
    }
}

void setup_wifi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting HiveMQ connection...");
        if (client.connect("ESP32_Farm_Client", mqtt_user, mqtt_pass)) {
            Serial.println("connected");
            client.subscribe("farm/control");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5s");
            delay(5000);
        }
    }
}

byte readRS485(const byte *command) {
    digitalWrite(DE, HIGH); digitalWrite(RE, HIGH);
    delay(10);
    mod.write(command, 8);
    mod.flush();
    digitalWrite(DE, LOW); digitalWrite(RE, LOW);

    unsigned long start = millis();
    byte len = 0;
    while (mod.available() == 0 && millis() - start < 1000);
    while (mod.available() > 0 && len < 10) {
        values[len++] = mod.read();
    }
    return (len >= 5) ? values[4] : 0;
}
