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
bool unlockTimerActive = false;
unsigned long unlockStartTime = 0;
const int unlockDuration = 5000; // Auto-lock after 5 seconds

const String authorizedUID = "13519FFD"; 

void setup() {
  Serial.begin(115200);
  Serial.println("🔄 Initializing...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi!");

  SPI.begin();
  rfid.PCD_Init();
  SPI.setFrequency(8000000);

  pinMode(LOCK_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(HALL_SENSOR_PIN, INPUT);

  updateLockState(true);

  // ✅ Start Multithreaded Tasks
  xTaskCreatePinnedToCore(taskRFID, "RFID Task", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskHallSensor, "Hall Sensor Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskAPI, "API Task", 4096, NULL, 1, NULL, 0);
}

// ✅ Task for RFID Scanning
void taskRFID(void *pvParameters) {
  while (true) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String scannedUID = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        scannedUID += String(rfid.uid.uidByte[i], HEX);
      }
      scannedUID.toUpperCase();

      Serial.print("🔄 Scanned Card UID: ");
      Serial.println(scannedUID);

      if (scannedUID == authorizedUID) { 
        Serial.println("✅ Authorized Card Detected!");
        toggleLockState();
      } else {
        Serial.println("❌ Access Denied!");
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Let other tasks run
  }
}

// ✅ Task for Hall Sensor & LED Control
void taskHallSensor(void *pvParameters) {
  while (true) {
    bool penAvailable = digitalRead(HALL_SENSOR_PIN) == LOW;
    
    if (penAvailable != lastPenState) { 
      lastPenState = penAvailable;
      sendPenStatusUpdate(penAvailable);
    }

    digitalWrite(LED_PIN, penAvailable ? HIGH : LOW);
    Serial.println(penAvailable ? "⚫ Pen Detected - LED ON" : "🔵 Pen Not Detected - LED OFF");

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ✅ Task for API Updates
void taskAPI(void *pvParameters) {
  while (true) {
    checkLockStateFromAPI();
    handleUnlockTimer();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// ✅ Toggle Lock State
void toggleLockState() {
  lockState = !lockState;
  digitalWrite(LOCK_PIN, lockState ? HIGH : LOW);
  Serial.println(lockState ? "🔒 Door Locked." : "🔓 Door Unlocked.");
  sendLockStateUpdate();

  if (!lockState) {
    unlockTimerActive = true;
    unlockStartTime = millis();
  }
}

// ✅ Auto-Lock After 5s (Non-Blocking)
void handleUnlockTimer() {
  if (unlockTimerActive && millis() - unlockStartTime >= unlockDuration) {
    unlockTimerActive = false;
    lockState = true;
    digitalWrite(LOCK_PIN, HIGH);
    Serial.println("🔒 Door Automatically Locked!");
    sendLockStateUpdate();
  }
}

// ✅ API Lock State Check
void checkLockStateFromAPI() {
  HTTPClient http;
  http.begin(API_GET_LOCK_STATE);
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    String response = http.getString();
    bool apiStatus = (response == "true");
    if (apiStatus != lockState) updateLockState(apiStatus);
  } else {
    Serial.print("❌ API Request Failed! Code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// ✅ Update Lock State
void updateLockState(bool state) {
  lockState = state;
  digitalWrite(LOCK_PIN, state ? HIGH : LOW);
  Serial.println(state ? "🔒 Door Locked!" : "🔓 Door Unlocked!");
}

// ✅ API Lock Update
void sendLockStateUpdate() {
  HTTPClient http;
  http.begin(API_SET_LOCK_STATE);
  http.addHeader("Content-Type", "application/json");
  String jsonPayload = "{\"id\":0, \"set\":true}";
  int httpResponseCode = http.PUT(jsonPayload);
  http.end();
  
  Serial.println(httpResponseCode == 200 ? "✅ Lock state updated!" : "❌ Lock state update failed!");
}

// ✅ Send Pen Status to API
void sendPenStatusUpdate(bool penStatus) {
  HTTPClient http;
  String apiUrl = String(API_SET_PEN_STATUS) + (penStatus ? "false" : "true");
  
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.PUT("{}");  
  http.end();

  Serial.println(httpResponseCode == 200 ? "✅ Pen status updated!" : "❌ Pen status update failed!");
}
