 #include <WiFi.h>
#include <HTTPClient.h>

// Wi-Fi credentials
const char* ssid = "gim";
const char* password = "12345689";

// API endpoint
const char* apiEndpoint = "http://192.168.52.68:8080/student/all";

void setup() {
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
}

void loop() {
  // Ensure Wi-Fi is still connected
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiEndpoint); // Specify the API endpoint

    // Send the HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {+
      // API call was successful
      String response = http.getString();
      Serial.println("HTTP Response Code: " + String(httpResponseCode));
      Serial.println("Response: " + response);
    } else {
      // Handle error
      Serial.println("Error in HTTP request");
    }

    http.end(); // Free resources
  } else {
    Serial.println("Wi-Fi not connected. Reconnecting...");
    WiFi.begin(ssid, password);
  }

  delay(5000); // Wait for 5 seconds
}
