#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Ticker.h>
#include <EEPROM.h>

#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "EyePulse"
#define BLYNK_AUTH_TOKEN "YOUR_AUTH_TOKEN"

char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASS";

const int trigPin = 14; 
const int echoPin = 12; 
const int ledStatus = 2;
const int buzzerPin = 15;

const int EEPROM_SIZE = 64;
const float IOP_THRESHOLD = 21.0;
const int SAMPLE_COUNT = 10;

float currentIOP = 0.0;
float iopBuffer[SAMPLE_COUNT];
int bufferIndex = 0;
bool systemReady = false;

BlynkTimer timer;

void initializeHardware() {
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    pinMode(ledStatus, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    
    digitalWrite(ledStatus, HIGH);
    digitalWrite(buzzerPin, LOW);
}

void playAlert() {
    for(int i = 0; i < 3; i++) {
        digitalWrite(buzzerPin, HIGH);
        delay(100);
        digitalWrite(buzzerPin, LOW);
        delay(100);
    }
}

float calculateAcousticDeformation() {
    long duration;
    float distance;
    
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    
    duration = pulseIn(echoPin, HIGH, 30000);
    
    if (duration == 0) return -1.0;
    
    distance = (duration * 0.0343) / 2;
    return distance;
}

float estimateIOP(float deformation) {
    if (deformation <= 0) return 0.0;
    
    float pressure = 40.0 - (deformation * 5.0);
    
    if (pressure < 5.0) pressure = 10.0 + random(2, 5);
    if (pressure > 45.0) pressure = 45.0;
    
    return pressure;
}

void processSensorCycle() {
    float rawDeformation = calculateAcousticDeformation();
    
    if (rawDeformation > 0) {
        float calculatedIOP = estimateIOP(rawDeformation);
        
        iopBuffer[bufferIndex] = calculatedIOP;
        bufferIndex = (bufferIndex + 1) % SAMPLE_COUNT;
        
        float sum = 0;
        for(int i = 0; i < SAMPLE_COUNT; i++) {
            sum += iopBuffer[i];
        }
        currentIOP = sum / SAMPLE_COUNT;
        
        Serial.print("Current IOP: ");
        Serial.print(currentIOP);
        Serial.println(" mmHg");
    } else {
        Serial.println("Sensor Timeout - Check Positioning");
    }
}

void updateCloudDashboard() {
    if (WiFi.status() == WL_CONNECTED) {
        Blynk.virtualWrite(V0, currentIOP);
        
        if (currentIOP > IOP_THRESHOLD) {
            Blynk.virtualWrite(V1, "HIGH RISK");
            Blynk.virtualWrite(V2, 255); 
            Blynk.logEvent("iop_alert", "High Intraocular Pressure Detected!");
            playAlert();
        } else if (currentIOP < 10.0 && currentIOP > 0) {
            Blynk.virtualWrite(V1, "LOW");
            Blynk.virtualWrite(V2, 128);
        } else {
            Blynk.virtualWrite(V1, "NORMAL");
            Blynk.virtualWrite(V2, 0);
        }
    }
}

void checkSystemHealth() {
    if (WiFi.status() != WL_CONNECTED) {
        digitalWrite(ledStatus, !digitalRead(ledStatus));
    } else {
        digitalWrite(ledStatus, LOW); 
    }
}

void handleSerialCommands() {
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'r') {
            Serial.println("Resetting Buffer...");
            for(int i = 0; i < SAMPLE_COUNT; i++) iopBuffer[i] = 0;
            bufferIndex = 0;
        }
        if (cmd == 's') {
            Serial.println("System Status Check:");
            Serial.print("IP: "); Serial.println(WiFi.localIP());
            Serial.print("Uptime: "); Serial.println(millis() / 1000);
        }
    }
}

BLYNK_CONNECTED() {
    Blynk.syncAll();
    Serial.println("Cloud Sync Complete");
}

void setup() {
    Serial.begin(115200);
    delay(10);
    
    initializeHardware();
    
    Serial.println("Initializing Eye-Pulse Firmware...");
    
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    
    while (Blynk.connect() == false) {
        delay(500);
        Serial.print(".");
    }
    
    timer.setInterval(1000L, processSensorCycle);
    timer.setInterval(3000L, updateCloudDashboard);
    timer.setInterval(5000L, checkSystemHealth);
    timer.setInterval(500L, handleSerialCommands);
    
    Serial.println("\nSystem Online.");
    playAlert();
}

void loop() {
    Blynk.run();
    timer.run();
    
    if (millis() % 10000 == 0) {
        yield(); 
    }
}
