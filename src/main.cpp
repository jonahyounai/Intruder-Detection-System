#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

//  Pins 
const int PIR_PIN    = 35;   // HC-SR501 OUT  (input-only pin, 3.3V logic)
const int TRIG_PIN   = 26;   // HC-SR04 TRIG
const int ECHO_PIN   = 34;   // HC-SR04 ECHO  (input-only; USE 1k/2k DIVIDER!)
const int SERVO_PIN  = 13;   // SG90 signal
const int BUZZER_PIN = 25;   // Active buzzer (+)
const int SDA_PIN    = 21;   // OLED SDA
const int SCL_PIN    = 22;   // OLED SCL

//  OLED setup
const int     SCREEN_WIDTH  = 128;
const int     SCREEN_HEIGHT = 64;    // change to 32 if your OLED is 128x32
const uint8_t OLED_ADDR     = 0x3C;  // some modules are 0x3D
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//  Servo setup 
Servo scanServo;
const int SERVO_MIN   = 15;
const int SERVO_MAX   = 165;
const int SERVO_HOME  = 90;
const int SERVO_STEP  = 3;
const int SETTLE_MS   = 30;

//  Detection params 
const float DETECT_CM  = 100.0;
const float MAX_CM     = 400.0;
const unsigned long REARM_MS = 3000;

//  State machine 
enum State { IDLE, SCANNING, TRACKING };
State state = IDLE;

//  Function prototypes (C++ needs these) 
float readDistanceCM();
void  beep(int ms, int times, int gap);
void  showStatus(const char* l1, const char* l2, const char* l3);
void  showTarget(int angle, float range);
int   performScan(float &bestRange);

//  Read ultrasonic range 
float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 27000UL);
  if (dur == 0) return MAX_CM;
  float cm = (dur * 0.0343f) / 2.0f;
  return (cm > MAX_CM) ? MAX_CM : cm;
}

//  Buzzer 
void beep(int ms, int times = 1, int gap = 80) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(ms);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(gap);
  }
}

//  OLED status 
void showStatus(const char* l1, const char* l2 = "", const char* l3 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);  display.println(l1);
  if (l2[0]) { display.setCursor(0, 24); display.println(l2); }
  if (l3[0]) { display.setCursor(0, 40); display.println(l3); }
  display.display();
}

//  OLED target 
void showTarget(int angle, float range) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);  display.println("INTRUDER");
  display.setTextSize(1);
  display.setCursor(0, 28); display.print("Bearing: "); display.print(angle);    display.println(" deg");
  display.setCursor(0, 44); display.print("Range  : "); display.print(range, 1); display.println(" cm");
  display.display();
}

//  Setup 
void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found - check wiring / I2C address");
    while (true) delay(1000);
  }

  scanServo.setPeriodHertz(50);
  scanServo.attach(SERVO_PIN, 500, 2400);
  scanServo.write(SERVO_HOME);

  showStatus("SYSTEM ARMED", "Waiting for", "motion...");
  Serial.println("System armed. Waiting for PIR trigger.");
  delay(2000);   // HC-SR501 needs ~30-60s to fully stabilize on power-up
}

//  Scan 
int performScan(float &bestRange) {
  int   bestAngle = -1;
  float minDist   = DETECT_CM;

  for (int a = SERVO_MIN; a <= SERVO_MAX; a += SERVO_STEP) {
    scanServo.write(a);
    delay(SETTLE_MS);
    float d = readDistanceCM();
    Serial.printf("angle=%3d  dist=%.1f cm\n", a, d);
    if (d < minDist) { minDist = d; bestAngle = a; bestRange = d; }
  }
  scanServo.write(SERVO_HOME);
  return bestAngle;
}

//  Loop 
void loop() {
  switch (state) {
    case IDLE:
      if (digitalRead(PIR_PIN) == HIGH) {
        Serial.println("Motion detected -> scanning");
        showStatus("MOTION!", "Scanning", "area...");
        beep(60);
        state = SCANNING;
      }
      break;

    case SCANNING: {
      float range = MAX_CM;
      int angle = performScan(range);
      if (angle >= 0) {
        Serial.printf("Target @ %d deg, %.1f cm\n", angle, range);
        scanServo.write(angle);
        delay(300);
        state = TRACKING;
      } else {
        Serial.println("No target in range -> re-arming");
        showStatus("SYSTEM ARMED", "Waiting for", "motion...");
        state = IDLE;
      }
      break;
    }

    case TRACKING: {
      float range = readDistanceCM();
      showTarget(scanServo.read(), range);
      beep(120, 3);
      delay(REARM_MS);
      scanServo.write(SERVO_HOME);
      showStatus("SYSTEM ARMED", "Waiting for", "motion...");
      state = IDLE;
      break;
    }
  }
}