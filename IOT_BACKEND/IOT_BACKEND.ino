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

// RFID & Hall sensor pins
#define RST_PIN 4
#define SS_PIN 5
#define LOCK_PIN 14
#define LED_PIN 13
#define HALL_SENSOR_PIN 27

MFRC522 rfid(SS_PIN, RST_PIN);
bool lockState = true; // Start locked
unsigned long lastApiCheckTime = 0;
const int apiCheckInterval = 2000; // API check every 2s

// Authorized RFID UID (Change this to your own)
const String authorizedUID = "13519FFD"; 

void setup() {
  Serial.begin(115200);
  Serial.println("🔄 Initializing...");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi!");

  // Initialize SPI and RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // Set SPI speed to 4 MHz for faster communication
  SPI.setFrequency(4000000);

  // Set pin modes
  pinMode(LOCK_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(HALL_SENSOR_PIN, INPUT);

  // Start in locked state
  updateLockState(true);
}

void loop() {
  checkHallSensor();  
  checkRFID();        
  checkLockStateFromAPI(); 
}

// ✅ Hall sensor LED control
void checkHallSensor() {
  if (digitalRead(HALL_SENSOR_PIN) == LOW) { 
    digitalWrite(LED_PIN, LOW);  
    Serial.println("⚫ Magnet Detected - LED OFF");
  } else {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("🔵 Magnet Not Detected - LED ON");
  }
}

// ✅ RFID checking
void checkRFID() {
  // Only check for a new card if there’s no ongoing scan
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Read scanned card UID
  String scannedUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    scannedUID += String(rfid.uid.uidByte[i], HEX);
  }
  scannedUID.toUpperCase();

  Serial.print("🔄 Scanned Card UID: ");
  Serial.println(scannedUID);

  // Check if scanned UID matches authorized UID
  if (scannedUID == authorizedUID) { 
    Serial.println("✅ Authorized Card Detected!");

    // Toggle lock state **BEFORE** setting pin values
    lockState = !lockState; 

    if (!lockState) { 
      // Unlock the door
      digitalWrite(LOCK_PIN, LOW);  // Relay ON (active-low unlock)
      Serial.println("🔓 Door Unlocked.");
      
      // Automatically lock after 5 seconds
      delay(5000);  
      digitalWrite(LOCK_PIN, HIGH); // Relay OFF (active-low lock)
      lockState = true;
      Serial.println("🔒 Door Automatically Locked!");
    } else {
      // Lock the door manually
      digitalWrite(LOCK_PIN, HIGH); // Relay OFF (active-low lock)
      Serial.println("🔒 Door Locked.");
    }
  } else {
    Serial.println("❌ Access Denied!");
  }

  // Halt the RFID card
  rfid.PICC_HaltA();
}

// ✅ API lock state check (every 2s)
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

// ✅ Lock state update
void updateLockState(bool state) {
  lockState = state;
  digitalWrite(LOCK_PIN, state ? HIGH : LOW);

  if (!state) {
    Serial.println("🔓 Door Unlocked!");
    sendLockStateUpdate();
    delay(5000);
    digitalWrite(LOCK_PIN, HIGH); // Lock again after 5s
    lockState = true;
    Serial.println("🔒 Door Automatically Locked!");
  } else {
    Serial.println("🔒 Door Locked!");
  }
}

// ✅ Send lock update to API
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
