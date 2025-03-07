 #include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

// WiFi credentials
const char* ssid = "ZLT S10_165310";
const char* password = "Abhi37304";

// API URLs
const char* API_GET_LOCK_STATE = "http://172.177.169.18:8080/locker/get-status?id=0";
const char* API_SET_LOCK_STATE = "http://172.177.169.18:8080/locker/set-status?id=0&set=true";
const char* API_SET_PEN_STATUS = "http://172.177.169.18:8080/locker/set-pen-status?id=0&set="; 

// RFID & Hall sensor pins
#define RST_PIN 4
#define SS_PIN 5
#define LOCK_PIN 14
#define LED_PIN 13
#define HALL_SENSOR_PIN 27

MFRC522 rfid(SS_PIN, RST_PIN);
bool lockState = true;
bool lastPenState = false;
unsigned long lastApiCheckTime = 0;
const int apiCheckInterval = 2000;

// Authorized RFID UID
const String authorizedUID = "13519FFD"; 

void setup() {
  Serial.begin(115200);
  Serial.println("🔄 Initializing...");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi!");

  // Initialize SPI and RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // Increase SPI speed for faster RFID reading
  //SPI.setFrequency(10000000);  // 10MHz (faster RFID reading)

  // Set pin modes
  pinMode(LOCK_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(HALL_SENSOR_PIN, INPUT);

  updateLockState(true);
}

void loop() {
  checkRFID();         // **Give priority to RFID**
  checkHallSensor();   
  checkLockStateFromAPI(); 
}

// ✅ Faster RFID Check
void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String scannedUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    scannedUID += String(rfid.uid.uidByte[i], HEX);
  }
  scannedUID.toUpperCase();

  Serial.print("🔄 Scanned Card UID: ");
  Serial.println(scannedUID);

  if (scannedUID == authorizedUID) { 
    Serial.println("✅ Authorized Card Detected!");

    lockState = !lockState; 
    digitalWrite(LOCK_PIN, lockState ? HIGH : LOW);

    if (!lockState) {
      Serial.println("🔓 Door Unlocked.");
      delay(5000);
      digitalWrite(LOCK_PIN, HIGH);
      lockState = true;
      Serial.println("🔒 Door Automatically Locked!");
    } else {
      Serial.println("🔒 Door Locked.");
    }
  } else {
    Serial.println("❌ Access Denied!");
  }

  // Reset RFID reader for next scan (makes detection faster)
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ✅ Optimized Hall Sensor Check
void checkHallSensor() {
  bool penAvailable = digitalRead(HALL_SENSOR_PIN) == LOW;
  
  if (penAvailable != lastPenState) {
    lastPenState = penAvailable;
    sendPenStatusUpdate(penAvailable);
  }

  digitalWrite(LED_PIN, penAvailable ? HIGH : LOW);
  Serial.println(penAvailable ? "⚫ Pen Detected - LED ON" : "🔵 Pen Not Detected - LED OFF");
}

// ✅ Optimized API Check (Runs Every 2s)
void checkLockStateFromAPI() {
  if (millis() - lastApiCheckTime < apiCheckInterval) {
    return;
  }
  lastApiCheckTime = millis();

  HTTPClient http;
  http.begin(API_GET_LOCK_STATE);
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.print("🔍 API Response: ");
    Serial.println(response);
    
    http.end();

    bool apiStatus = (response == "true");
    if (apiStatus != lockState) {
      updateLockState(apiStatus);
    }
  } else {
    Serial.print("❌ API Request Failed! Code: ");
    Serial.println(httpResponseCode);
    http.end();
  }
}

// ✅ Lock State Update
void updateLockState(bool state) {
  lockState = state;
  digitalWrite(LOCK_PIN, state ? HIGH : LOW);

  if (!state) {
    Serial.println("🔓 Door Unlocked!");
    sendLockStateUpdate();
    delay(5000);
    digitalWrite(LOCK_PIN, HIGH);
    lockState = true;
    Serial.println("🔒 Door Automatically Locked!");
  } else {
    Serial.println("🔒 Door Locked!");
  }
}

// ✅ Faster Lock Update to API
void sendLockStateUpdate() {
  HTTPClient http;
  http.begin(API_SET_LOCK_STATE);
  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{\"id\":0, \"set\":true}";
  int httpResponseCode = http.PUT(jsonPayload);

  if (httpResponseCode == 200) {
    Serial.println("✅ Lock state update sent to API!");
  } else {
    Serial.print("❌ Failed to send lock state update! Code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// ✅ Faster Pen Status Update
void sendPenStatusUpdate(bool penStatus) {
  HTTPClient http;
  String apiUrl = String(API_SET_PEN_STATUS) + (penStatus ? "false" : "true");
  
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PUT("{}");  

  if (httpResponseCode == 200) {
    Serial.print("✅ Pen status updated: ");
    Serial.println(penStatus ? "PEN AVAILABLE" : "PEN NOT AVAILABLE");
  } else {
    Serial.print("❌ Failed to send pen status update! Code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}
