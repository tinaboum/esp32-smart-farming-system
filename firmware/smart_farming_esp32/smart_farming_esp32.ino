/*
 * IoT-Based Smart Farming Management System
 * Reconstructed and cleaned from the two supplied project sketches.
 * Target: ESP32 using the Arduino framework
 * Sensor: DHT22 (not DHT11)
 *
 * Required libraries:
 *   - DHT sensor library by Adafruit
 *   - Firebase ESP32 Client by Mobizt (the library used by the original code)
 *
 * Security:
 *   Replace the credential placeholders locally. Do not commit real Wi-Fi or
 *   Firebase credentials to GitHub. Rotate the credentials exposed in the
 *   original uploaded sketches before reusing this project.
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "DHT.h"

// ------------------------- Pin assignment -------------------------
constexpr uint8_t WATER_LEVEL_PIN = 34;  // Original code used a digital input.
constexpr uint8_t SOIL_MOISTURE_PIN = 35;
constexpr uint8_t PUMP_PIN = 26;
constexpr uint8_t PIR_PIN = 27;  // Moved from boot-strapping GPIO12.
constexpr uint8_t BUZZER_PIN = 13;
constexpr uint8_t STATUS_LED_PIN = 5;
constexpr uint8_t LDR_PIN = 33;  // ADC1; original GPIO15/ADC2 conflicts with Wi-Fi.
constexpr uint8_t DHT_PIN = 4;

#define DHT_TYPE DHT22

// ----------------------- Local configuration ----------------------
// The original sketches used a threshold of 700 and treated values below it
// as an irrigation request. Calibrate this value using your actual probe.
constexpr int SOIL_IRRIGATION_THRESHOLD = 700;

// A value of 1 preserves the original "LDR > 0" behaviour. Replace it with a
// measured threshold after checking the LDR circuit in bright and dark states.
constexpr int LDR_ACTIVITY_THRESHOLD = 1;

constexpr float HUMIDITY_LED_THRESHOLD_PERCENT = 50.0F;
constexpr uint32_t DHT_SAMPLE_INTERVAL_MS = 2000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 2000;
constexpr uint32_t PUMP_ON_TIME_MS = 2000;
constexpr uint32_t PUMP_COOLDOWN_MS = 2000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

// --------------------- Credentials: fill locally ------------------
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define FIREBASE_API_KEY "YOUR_FIREBASE_WEB_API_KEY"
#define FIREBASE_DATABASE_URL "https://YOUR_PROJECT_ID-default-rtdb.firebaseio.com/"
#define FIREBASE_USER_EMAIL "YOUR_FIREBASE_USER_EMAIL"
#define FIREBASE_USER_PASSWORD "YOUR_FIREBASE_USER_PASSWORD"

FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;
DHT dht(DHT_PIN, DHT_TYPE);

enum class IrrigationState : uint8_t {
  Idle,
  Pumping,
  Cooldown
};

IrrigationState irrigationState = IrrigationState::Idle;
uint32_t irrigationStateStartedMs = 0;
uint32_t lastDhtSampleMs = 0;
uint32_t lastTelemetryMs = 0;

float airHumidityPercent = NAN;
float airTemperatureC = NAN;
int soilMoistureRaw = 0;
int waterLevelState = 0;
int motionState = 0;
int lightLevelRaw = 0;

bool dhtReadingIsValid() {
  return !isnan(airHumidityPercent) && !isnan(airTemperatureC);
}

bool soilRequestsIrrigation(int rawValue) {
  // This polarity matches the original implementation. Reverse the comparison
  // only if calibration proves that your probe reports larger values when dry.
  return rawValue < SOIL_IRRIGATION_THRESHOLD;
}

void setPump(bool enabled) {
  digitalWrite(PUMP_PIN, enabled ? HIGH : LOW);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  const uint32_t startedMs = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedMs < WIFI_CONNECT_TIMEOUT_MS) {
    Serial.print('.');
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi connection timed out; local control remains active.");
  }
}

void configureFirebase() {
  firebaseConfig.api_key = FIREBASE_API_KEY;
  firebaseConfig.database_url = FIREBASE_DATABASE_URL;
  firebaseConfig.token_status_callback = tokenStatusCallback;

  firebaseAuth.user.email = FIREBASE_USER_EMAIL;
  firebaseAuth.user.password = FIREBASE_USER_PASSWORD;

  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(3);
}

void sampleFastInputs() {
  waterLevelState = digitalRead(WATER_LEVEL_PIN);
  soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
  motionState = digitalRead(PIR_PIN);
  lightLevelRaw = analogRead(LDR_PIN);
}

void sampleDht22(uint32_t nowMs) {
  if (lastDhtSampleMs != 0 &&
      nowMs - lastDhtSampleMs < DHT_SAMPLE_INTERVAL_MS) {
    return;
  }

  lastDhtSampleMs = nowMs;
  const float newHumidity = dht.readHumidity();
  const float newTemperature = dht.readTemperature();

  if (isnan(newHumidity) || isnan(newTemperature)) {
    Serial.println("DHT22 read failed; keeping the previous valid values.");
    return;
  }

  airHumidityPercent = newHumidity;
  airTemperatureC = newTemperature;
}

void updateIrrigation(uint32_t nowMs) {
  switch (irrigationState) {
    case IrrigationState::Idle:
      if (soilRequestsIrrigation(soilMoistureRaw)) {
        setPump(true);
        irrigationState = IrrigationState::Pumping;
        irrigationStateStartedMs = nowMs;
      }
      break;

    case IrrigationState::Pumping:
      if (nowMs - irrigationStateStartedMs >= PUMP_ON_TIME_MS) {
        setPump(false);
        irrigationState = IrrigationState::Cooldown;
        irrigationStateStartedMs = nowMs;
      }
      break;

    case IrrigationState::Cooldown:
      if (nowMs - irrigationStateStartedMs >= PUMP_COOLDOWN_MS) {
        irrigationState = IrrigationState::Idle;
        irrigationStateStartedMs = nowMs;
      }
      break;
  }
}

void updateLocalOutputs() {
  const bool alarmCondition =
      motionState == HIGH && lightLevelRaw >= LDR_ACTIVITY_THRESHOLD;

  if (alarmCondition) {
    tone(BUZZER_PIN, 1000);
  } else {
    noTone(BUZZER_PIN);
  }

  if (dhtReadingIsValid()) {
    digitalWrite(STATUS_LED_PIN,
                 airHumidityPercent > HUMIDITY_LED_THRESHOLD_PERCENT
                     ? HIGH
                     : LOW);
  }
}

bool publishFloat(const char *path, float value) {
  if (!Firebase.setFloat(firebaseData, path, value)) {
    Serial.print("Firebase write failed for ");
    Serial.print(path);
    Serial.print(": ");
    Serial.println(firebaseData.errorReason());
    return false;
  }
  return true;
}

bool publishInt(const char *path, int value) {
  if (!Firebase.setInt(firebaseData, path, value)) {
    Serial.print("Firebase write failed for ");
    Serial.print(path);
    Serial.print(": ");
    Serial.println(firebaseData.errorReason());
    return false;
  }
  return true;
}

void publishTelemetry(uint32_t nowMs) {
  if (!Firebase.ready() ||
      (lastTelemetryMs != 0 &&
       nowMs - lastTelemetryMs < TELEMETRY_INTERVAL_MS)) {
    return;
  }

  lastTelemetryMs = nowMs;

  // These four paths preserve compatibility with the supplied application.
  if (dhtReadingIsValid()) {
    publishFloat("/h", airHumidityPercent);
    publishFloat("/t", airTemperatureC);
  }
  publishInt("/wl", waterLevelState);
  publishInt("/hs", soilMoistureRaw);

  Serial.printf(
      "T=%.1f C | RH=%.1f %% | soil=%d | water=%d | PIR=%d | LDR=%d | pump=%s\n",
      airTemperatureC,
      airHumidityPercent,
      soilMoistureRaw,
      waterLevelState,
      motionState,
      lightLevelRaw,
      irrigationState == IrrigationState::Pumping ? "ON" : "OFF");
}

void setup() {
  pinMode(WATER_LEVEL_PIN, INPUT);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

  pinMode(PUMP_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);

  setPump(false);
  noTone(BUZZER_PIN);
  digitalWrite(STATUS_LED_PIN, LOW);

  Serial.begin(115200);
  dht.begin();
  connectWiFi();
  configureFirebase();
}

void loop() {
  const uint32_t nowMs = millis();

  sampleFastInputs();
  sampleDht22(nowMs);
  updateIrrigation(nowMs);
  updateLocalOutputs();
  publishTelemetry(nowMs);

  delay(10);  // Cooperative yield; all control timing above remains non-blocking.
}
