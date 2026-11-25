#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "SSID";
const char* WIFI_PASSWORD = "PASSWORD";

const char* CLOUD_RTC_URL =
    "https://simple-test-number-1-default-rtdb.firebaseio.com/pet_feeder/rtc_time.json";
const char* CLOUD_COMMAND_URL =
    "https://simple-test-number-1-default-rtdb.firebaseio.com/pet_feeder/command.json";

const unsigned long UPLOAD_INTERVAL_MS = 5000;
unsigned long lastUploadMs = 0;

RTC_DS1307 rtc;

String twoDigits(int n) {
  return (n < 10) ? ("0" + String(n)) : String(n);
}

String readFormattedTime() {
  DateTime now = rtc.now();
  return String(now.year()) + "-" +
         twoDigits(now.month()) + "-" +
         twoDigits(now.day()) + " " +
         twoDigits(now.hour()) + ":" +
         twoDigits(now.minute()) + ":" +
         twoDigits(now.second());
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWi-Fi connection timed out.");
  }
}

void sendTimeToCloud(const String& formattedTime) {
  ensureWiFiConnected();
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  if (!http.begin(secureClient, CLOUD_RTC_URL)) return;

  http.addHeader("Content-Type", "application/json");
  String jsonPayload = "\"" + formattedTime + "\"";

  int httpResponseCode = http.PUT(jsonPayload);
  if (httpResponseCode > 0) {
    Serial.printf("RTC PUT status %d -> %s\n", httpResponseCode, formattedTime.c_str());
  } else {
    Serial.printf("RTC PUT error: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

void clearCommandInCloud() {
  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  if (!http.begin(secureClient, CLOUD_COMMAND_URL)) return;

  http.addHeader("Content-Type", "application/json");
  int code = http.PUT("\"none\"");
  Serial.printf("Cleared command in Firebase (HTTP %d)\n", code);
  http.end();
}

void checkCloudCommand() {
  ensureWiFiConnected();
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  if (!http.begin(secureClient, CLOUD_COMMAND_URL)) return;

  int httpCode = http.GET();
  if (httpCode == 200) {
    String body = http.getString();
    body.trim();

    if (body.indexOf("\"feed_now\"") >= 0) {
      Serial.println("=========================================");
      Serial.println("APP COMMAND RECEIVED: FEED NOW!");
      Serial.println("Payload: " + body);
      Serial.println("=========================================");

      // TODO: Trigger your feeder motor / dispensing routine here

      http.end();
      clearCommandInCloud();
      return;
    }
  } else if (httpCode < 0) {
    Serial.printf("Command GET error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(21, 22);

  if (!rtc.begin()) {
    Serial.println("DS1307 RTC not found! Check wiring and power.");
    while (true) {
      delay(1000);
    }
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC is not running; setting initial time from sketch compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  ensureWiFiConnected();
}

void loop() {
  unsigned long nowMs = millis();
  if (nowMs - lastUploadMs >= UPLOAD_INTERVAL_MS) {
    lastUploadMs = nowMs;

    String currentTime = readFormattedTime();
    Serial.println("RTC Time: " + currentTime);
    sendTimeToCloud(currentTime);
    checkCloudCommand();
  }
}
