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
const char* API_SET_PEN_STATUS = "http://172.177.169.18:8080/locker/set-pen-status?id=0&set=";  // Base URL for Hall sensor API
..
// RFID & Hall sensor pins
#define RST_PIN 4
#define SS_PIN 5
#define LOCK_PIN 14
#define LED_PIN 13
#define HALL_SENSOR_PIN 27

MFRC522 rfid(SS_PIN, RST_PIN);
bool lockState = true;  // Start locked
bool lastPenState = false; // Store last pen state
unsigned long lastApiCheckTime = 0;
const int apiCheckInterval = 2000; // API check every 2 seconds

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
  
  // Set SPI speed to 8 MHz for faster communication
  SPI.setFrequency(8000000);

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

// ✅ Hall sensor check with API request
void checkHallSensor() {
  bool penAvailable = digitalRead(HALL_SENSOR_PIN) == LOW; // Pen available if LOW
  
  if (penAvailable != lastPenState) { // If state changes, send API request
    lastPenState = penAvailable;
    sendPenStatusUpdate(penAvailable);
  }

  if (penAvailable) {
    digitalWrite(LED_PIN, LOW);
    Serial.println("🔵 Pen Not Detected - LED OFF");
  } else {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("⚫ Pen Detected - LED ON");
  }
}

// ✅ Send Pen Status API request (PUT Request)
void sendPenStatusUpdate(bool penStatus) {
  HTTPClient http;
  String apiUrl = String(API_SET_PEN_STATUS) + (penStatus ? "false" : "true");
  
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");

  // Sending empty JSON body since the URL already has the parameters
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

// ✅ Optimized RFID Checking (Faster & More Reliable)
void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    rfid.PCD_Init(); // Reset RFID module if no card is detected
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

  if (scannedUID == authorizedUID) { 
    Serial.println("✅ Authorized Card Detected!");
    lockState = !lockState;
    
    digitalWrite(LOCK_PIN, lockState ? HIGH : LOW);
    Serial.println(lockState ? "🔒 Door Locked." : "🔓 Door Unlocked.");
    delay(5000);
    digitalWrite(LOCK_PIN, HIGH); // Lock again after 5s
    lockState = true;
    delay(500); // Short delay to avoid multiple reads
     
  //  sendLockStateUpdate();
  } else {
    Serial.println("❌ Access Denied!");
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
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
    delay(5000);
    sendLockStateUpdate();
    digitalWrite(LOCK_PIN, HIGH); // Lock again after 5s
    lockState = true;
    Serial.println("🔒 Door Automatically Locked!");
  } else {
    Serial.println("🔒 Door Locked!");
  }
}

// ✅ Send lock update to API (PUT request)
void sendLockStateUpdate() {
  HTTPClient http;
  http.begin(API_SET_LOCK_STATE);
  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{\"id\":0, \"set\":false}";
  int httpResponseCode = http.PUT(jsonPayload);

  if (httpResponseCode == 200) {
    Serial.println("✅ Lock state update sent to API!");
  } else {
    Serial.print("❌ Failed to send lock state update! Code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}
