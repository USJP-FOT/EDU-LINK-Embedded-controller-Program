#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

// WiFi credentials
const char* ssid = "gim";
const char* password = "12345689";

// API URL for lock status
const char* API_GET_LOCK_STATE = "http://172.177.169.18:8080/locker/get-status?id=0";

// Define RFID module pins
#define RST_PIN 4
#define SS_PIN 5

// Define Lock and Hall Sensor pins
#define LOCK_PIN 14      // Solenoid lock control
#define LED_PIN 13       // LED (Controlled by Hall Sensor)
#define HALL_SENSOR_PIN 27 // Hall sensor

MFRC522 rfid(SS_PIN, RST_PIN);
bool lockState = true; // Initially locked
unsigned long lastApiCheckTime = 0;
const int apiCheckInterval = 5000; // Check API every 5 seconds

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
  checkHallSensor();
  checkRFID();
  checkLockStateFromAPI();
}

// Function to check Hall Sensor (LED Control)
void checkHallSensor() {
  if (digitalRead(HALL_SENSOR_PIN) == LOW) { 
    digitalWrite(LED_PIN, LOW);  // LED OFF when magnet detected
    Serial.println("Hall Sensor Active - LED OFF");
  } else {
    digitalWrite(LED_PIN, HIGH); // LED ON when no magnet
    Serial.println("Hall Sensor Inactive - LED ON");
  }
}

// Function to check RFID
void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return; // No new card detected
  }

  Serial.println("🔄 RFID Card Scanned! Checking API...");

  checkLockStateFromAPI();

  // Halt RFID card
  rfid.PICC_HaltA();
}

// Function to check lock state from API every 5 seconds
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

// Function to update lock state (Does NOT affect LED)
void updateLockState(bool state) {
  digitalWrite(LOCK_PIN, state ? HIGH : LOW); // HIGH = Locked, LOW = Unlocked
  lockState = state;

  if (!state) { // If unlocked
    Serial.println("🔓 Door Unlocked!");
    delay(5000); // Wait 5 seconds
    digitalWrite(LOCK_PIN, HIGH); // Lock again
    lockState = true;
    Serial.println("🔒 Door Automatically Locked!");
  } else {
    Serial.println("🔒 Door Locked!");
  }
}
