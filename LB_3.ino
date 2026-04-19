#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "HX711.h"
#include <WiFi.h>
#include "ThingSpeak.h"

// ---------------- WiFi ----------------
const char* ssid = "admin123";
const char* pass = "admin123";

unsigned long channelID = 3308716;
const char* writeAPIKey = "0LUDSMWK3L4UP4EI";

WiFiClient client;

// ---------------- Sensors ----------------
Adafruit_MPU6050 mpu;

#define DOUT 5
#define CLK 4
HX711 scale;

// ---------------- Output ----------------
#define BUZZER 23
#define VIBRATION 15

// ---------------- Variables ----------------
float bodyWeight = 60.0;

String lastOutput = "Starting...";
int stateValue = 0;   // 0 SAFE, 1 MILD, 2 DANGER

// -------- TIMERS --------
unsigned long lastSerialTime = 0;
unsigned long lastThingSpeakTime = 0;

// ---------------- SETUP ----------------
void setup() {

  Serial.begin(115200);

  pinMode(BUZZER, OUTPUT);
  pinMode(VIBRATION, OUTPUT);

  digitalWrite(BUZZER, LOW);
  digitalWrite(VIBRATION, LOW);

  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1);
  }

  scale.begin(DOUT, CLK);
  scale.set_scale(420.0);
  scale.tare();

  WiFi.begin(ssid, pass);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected ✅");

  ThingSpeak.begin(client);
}

// ---------------- LOOP ----------------
void loop() {

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float angleX = atan2(
                   a.acceleration.x,
                   sqrt(a.acceleration.y * a.acceleration.y +
                        a.acceleration.z * a.acceleration.z)
                 ) * 180 / PI;

  float angleAbs = abs(angleX);

  float liftedWeight = scale.get_units(5);
  if (liftedWeight < 0 || liftedWeight > 200) liftedWeight = 0;

  float finalWeight = bodyWeight - liftedWeight;
  if (finalWeight < 0) finalWeight = 0;

  int angleLevel = constrain((int)(angleAbs / 10), 0, 5);
  int weightLevel = constrain((int)(liftedWeight + 0.5), 0, 5);

  String angleState, weightState;

  if (angleLevel == 0) angleState = "SAFE";
  else if (angleLevel <= 3) angleState = "MILD";
  else angleState = "DANGER";

  if (weightLevel == 0) weightState = "SAFE";
  else if (weightLevel <= 3) weightState = "MILD";
  else weightState = "DANGER";

  // -------- FINAL STATE --------
  if (angleState == "DANGER" || weightState == "DANGER") {
    stateValue = 2;
  }
  else if (angleState == "MILD" || weightState == "MILD") {
    stateValue = 1;
  }
  else {
    stateValue = 0;
  }

  String finalStateText;
  if (stateValue == 0) finalStateText = "SAFE";
  else if (stateValue == 1) finalStateText = "MILD";
  else finalStateText = "DANGER";

  // -------- Alerts --------
  if (stateValue == 2) {
    digitalWrite(BUZZER, HIGH);
    digitalWrite(VIBRATION, HIGH);
  }
  else if (stateValue == 1) {
    digitalWrite(VIBRATION, HIGH);
    digitalWrite(BUZZER, LOW);
  }
  else {
    digitalWrite(BUZZER, LOW);
    digitalWrite(VIBRATION, LOW);
  }

  // -------- APP OUTPUT --------
  String appOutput =
    "ANGLEX: " + String(angleAbs, 2) + "\n" +
    "FSRVALUE: " + String(liftedWeight, 2) + "\n" +
    "WEIGHT: " + String(finalWeight, 2) + "\n" +
    "STATE: " + finalStateText;

  lastOutput = appOutput;

  // -------- LAPTOP OUTPUT --------
  String serialOutput =
    "Body Weight: " + String(bodyWeight, 0) +
    " | Lifted Weight: " + String(liftedWeight, 0) +
    " | Final Weight: " + String(finalWeight, 0) + "\n" +
    "Angle: " + String(angleAbs, 0) +
    " | AngleLevel: " + String(angleLevel) +
    " | AngleState: " + angleState +
    " | WeightLevel: " + String(weightLevel) +
    " | WeightState: " + weightState +
    " | Final State: " + finalStateText;

  unsigned long currentMillis = millis();

  // -------- SERIAL --------
  if (currentMillis - lastSerialTime >= 3000) {
    Serial.println(serialOutput);
    Serial.println("--------------------------------");
    lastSerialTime = currentMillis;
  }

  // -------- THINGSPEAK --------
  if (currentMillis - lastThingSpeakTime >= 15000) {

    ThingSpeak.setStatus(lastOutput);     // ✅ TEXT OUTPUT
    ThingSpeak.setField(1, angleAbs);
    ThingSpeak.setField(2, liftedWeight);
    ThingSpeak.setField(3, finalWeight);
    ThingSpeak.setField(4, stateValue);   // ✅ STATE NUMBER

    ThingSpeak.writeFields(channelID, writeAPIKey);

    lastThingSpeakTime = currentMillis;
  }
}
