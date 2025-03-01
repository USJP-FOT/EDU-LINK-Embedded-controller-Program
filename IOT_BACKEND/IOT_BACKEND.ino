#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

// WiFi credentials
const char* ssid = "gim";
const char* password = "12345689";

// API URLs
const char* API_GET_LOCK_STATE = "http://172.177.169.18:8080/locker/get-status?id=0";
const char* API_SET_LOCK_STATE = "http://172.177.169.18:8080/locker/set-status?id=0&set=true";

// Define RFID module pins
#define RST_PIN 4
#define SS_PIN 5

// Define Lock and Hall Sensor pins
#define LOCK_PIN 14        // Solenoid lock control
#define LED_PIN 13         // LED controlled by Hall Sensor
#define HALL_SENSOR_PIN 27 // Hall sensor

MFRC522 rfid(SS_PIN, RST_PIN);
bool lockState = true; // Initially locked
unsigned long lastApiCheckTime = 0;
const int apiCheckInterval = 2000; // Check API every 5 seconds

// Define the authorized RFID card UID (13519FFD)
byte authorizedUID[] = { 0x13, 0x51, 0x9f, 0xfd };

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  // Initialize SPI and RFID module
  SPI.begin(18, 19, 23);
  rfid.PCD_Init();
  
  // Set pin modes
  pinMode(LOCK_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(HALL_SENSOR_PIN, INPUT);

  // Start with locked state
  updateLockState(true);
}

void loop() {
  checkHallSensor();  // ✅ Hall sensor now works independently (no API response needed)
  checkRFID();        
  checkLockStateFromAPI(); 
}

// ✅ Function to check Hall Sensor (Independent LED Control)
void checkHallSensor() {
  if (digitalRead(HALL_SENSOR_PIN) == LOW) { 
    digitalWrite(LED_PIN, LOW);  // LED OFF when magnet detected
    Serial.println("🔵 Magnet Detected - LED OFF");
  } else {
    digitalWrite(LED_PIN, HIGH); // LED ON when no magnet
    Serial.println("⚫ Magnet Not Detected - LED ON");
  }
}

// ✅ Function to check RFID card
void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return; // No new card detected
  }

  Serial.print("🔄 RFID Card Scanned! UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Check if scanned card matches the authorized UID
  if (isAuthorizedCard()) {
    Serial.println("✅ Authorized Card! Unlocking...");
    updateLockState(false); // Unlock the locker
  } else {
    Serial.println("❌ Unauthorized Card! Access Denied.");
  }

  // Halt RFID card
  rfid.PICC_HaltA();
}

// ✅ Function to check if the scanned RFID card matches the authorized UID
bool isAuthorizedCard() {
  if (rfid.uid.size != sizeof(authorizedUID)) {
    return false; // UID size mismatch
  }

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] != authorizedUID[i]) {
      return false; // UID doesn't match
    }
  }

  return true; // UID matches
}

// ✅ Function to check lock state from API every 5 seconds
void checkLockStateFromAPI() {
  if (millis() - lastApiCheckTime < apiCheckInterval) {
    return; // Only check API every 5 seconds
  }

  lastApiCheckTime = millis(); // Update last check time

  HTTPClient http;
  http.begin(API_GET_LOCK_STATE);

  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) { // If request is successful
    String response = http.getString();
    Serial.print("🔍 API Response: ");
    Serial.println(response);
    
    http.end();

    bool apiStatus = (response == "true"); // true = locked, false = unlocked
    if (apiStatus != lockState) { // Only update if state changes
      updateLockState(apiStatus);
    }
  } else {
    Serial.print("❌ API Request Failed! Code: ");
    Serial.println(httpResponseCode);
    
    http.end();
  }
}

// ✅ Function to update lock state
void updateLockState(bool state) {
  digitalWrite(LOCK_PIN, state ? HIGH : LOW); // HIGH = Locked, LOW = Unlocked
  lockState = state;

  if (!state) { // If unlocked
    Serial.println("🔓 Door Unlocked!");

    // Send PATCH request **before** waiting 5 seconds
    sendLockStateUpdate();

    delay(5000); // Wait 5 seconds

    // Relock the door
    digitalWrite(LOCK_PIN, HIGH);
    lockState = true;
    Serial.println("🔒 Door Automatically Locked!");
  } else {
    Serial.println("🔒 Door Locked!");
  }
}

// ✅ Function to send API request when lock is unlocked
void sendLockStateUpdate() {
  HTTPClient http;
  http.begin(API_SET_LOCK_STATE);
  http.addHeader("Content-Type", "application/json"); // Set content type

  String jsonPayload = "{\"id\":0, \"set\":true}"; // JSON body

  int httpResponseCode = http.PATCH(jsonPayload); // Send PATCH request

  if (httpResponseCode == 200) {
    Serial.println("✅ Lock state update sent to API!");
  } else {
    Serial.print("❌ Failed to send lock state update! Code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}
