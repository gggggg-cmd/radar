#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo radarServo;

// ---------------- PINS ----------------
const int trigPin  = 9;
const int echoPin  = 10;
const int servoPin = 6;

// ---------------- SWEEP SETTINGS ----------------
const int minAngle = 15;
const int maxAngle = 165;
const int maxDistance = 200;   // cm

int angle = minAngle;
int dir = 1;

unsigned long lastStepTime = 0;
const int stepInterval = 40;

// ---------------- TRAIL ----------------
const int TRAIL_SIZE = 12;
int trailAngle[TRAIL_SIZE];
int trailDist[TRAIL_SIZE];
int trailIndex = 0;

// ---------------- DISTANCE ----------------
long readDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000);

  if (duration == 0) return -1;

  long dist = duration * 0.0343 / 2.0;

  if (dist < 2 || dist > 250) return -1;
  return dist;
}

// ---------------- TRAIL ----------------
void addTrailPoint(int a, int d) {
  if (d <= 0 || d > maxDistance) return;

  trailAngle[trailIndex] = a;
  trailDist[trailIndex] = d;
  trailIndex++;
  if (trailIndex >= TRAIL_SIZE) trailIndex = 0;
}

// ---------------- DRAW HELPERS ----------------
void drawHUD(int currentAngle, int distance) {
  // Outer HUD box
  display.drawRect(0, 0, 128, 11, SSD1306_WHITE);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(2, 2);
  display.print("A:");
  display.print(currentAngle);

  display.setCursor(38, 2);
  display.print("D:");
  if (distance > 0) {
    display.print(distance);
  } else {
    display.print("---");
  }

  display.setCursor(74, 2);
  display.print("cm");

  display.setCursor(104, 2);
  if (dir == 1) display.print(">>");
  else display.print("<<");
}

void drawRadarGrid() {
  int cx = 64;
  int cy = 63;

  // Base line
  display.drawLine(4, cy, 124, cy, SSD1306_WHITE);

  // Tactical arcs
  display.drawCircle(cx, cy, 15, SSD1306_WHITE);
  display.drawCircle(cx, cy, 30, SSD1306_WHITE);
  display.drawCircle(cx, cy, 45, SSD1306_WHITE);
  display.drawCircle(cx, cy, 60, SSD1306_WHITE);

  // Major scan guides
  for (int a = 30; a <= 150; a += 30) {
    float rad = radians(180 - a);
    int x = cx + 60 * cos(rad);
    int y = cy - 60 * sin(rad);
    display.drawLine(cx, cy, x, y, SSD1306_WHITE);
  }

  // Corner angle labels
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(2, 54);
  display.print("0");

  display.setCursor(57, 54);
  display.print("90");

  display.setCursor(104, 54);
  display.print("180");
}

void drawTrail() {
  int cx = 64;
  int cy = 63;

  for (int i = 0; i < TRAIL_SIZE; i++) {
    int d = trailDist[i];
    int a = trailAngle[i];

    if (d > 0 && d <= maxDistance) {
      float rad = radians(180 - a);
      int r = map(d, 0, maxDistance, 0, 60);

      int x = cx + r * cos(rad);
      int y = cy - r * sin(rad);

      if (x >= 0 && x < 128 && y >= 12 && y < 64) {
        display.drawPixel(x, y, SSD1306_WHITE);
      }
    }
  }
}

void drawSweepBeam(int currentAngle) {
  int cx = 64;
  int cy = 63;

  float rad = radians(180 - currentAngle);

  // Main sweep line
  int x1 = cx + 60 * cos(rad);
  int y1 = cy - 60 * sin(rad);
  display.drawLine(cx, cy, x1, y1, SSD1306_WHITE);

  // Slightly offset support lines to fake a beam effect
  float rad2 = radians(180 - (currentAngle - 2));
  float rad3 = radians(180 - (currentAngle + 2));

  int x2 = cx + 54 * cos(rad2);
  int y2 = cy - 54 * sin(rad2);

  int x3 = cx + 54 * cos(rad3);
  int y3 = cy - 54 * sin(rad3);

  display.drawLine(cx, cy, x2, y2, SSD1306_WHITE);
  display.drawLine(cx, cy, x3, y3, SSD1306_WHITE);
}

void drawTarget(int currentAngle, int distance) {
  if (distance <= 0 || distance > maxDistance) return;

  int cx = 64;
  int cy = 63;

  float rad = radians(180 - currentAngle);
  int r = map(distance, 0, maxDistance, 0, 60);

  int x = cx + r * cos(rad);
  int y = cy - r * sin(rad);

  if (x >= 3 && x < 125 && y >= 14 && y < 61) {
    // bold tactical target marker
    display.fillCircle(x, y, 2, SSD1306_WHITE);
    display.drawCircle(x, y, 4, SSD1306_WHITE);

    // crosshair feel
    display.drawLine(x - 6, y, x - 3, y, SSD1306_WHITE);
    display.drawLine(x + 3, y, x + 6, y, SSD1306_WHITE);
    display.drawLine(x, y - 6, x, y - 3, SSD1306_WHITE);
    display.drawLine(x, y + 3, x, y + 6, SSD1306_WHITE);
  }
}

void updateOLED(int currentAngle, int distance) {
  display.clearDisplay();

  drawHUD(currentAngle, distance);
  drawRadarGrid();
  drawTrail();
  drawSweepBeam(currentAngle);
  drawTarget(currentAngle, distance);

  display.display();
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  for (int i = 0; i < TRAIL_SIZE; i++) {
    trailAngle[i] = 0;
    trailDist[i] = -1;
  }

  radarServo.attach(servoPin);
  radarServo.write(angle);
  delay(400);

  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (1);
  }

  display.clearDisplay();
  display.drawRect(10, 18, 108, 28, SSD1306_WHITE);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 25);
  display.print("RADAR OK");
  display.display();
  delay(1000);

  Serial.println("================================");
  Serial.println("TACTICAL RADAR SYSTEM STARTED");
  Serial.println("OLED + SERVO + ULTRASONIC OK");
  Serial.println("Raw format: angle,distance");
  Serial.println("================================");
}

// ---------------- LOOP ----------------
void loop() {
  if (millis() - lastStepTime >= stepInterval) {
    lastStepTime = millis();

    radarServo.write(angle);
    delay(18);

    long dist = readDistanceCM();

    addTrailPoint(angle, dist);

    // parser-safe output
    Serial.print(angle);
    Serial.print(",");
    Serial.println(dist);

    // readable debug
    Serial.print("ANGLE: ");
    Serial.print(angle);
    Serial.print(" deg | DISTANCE: ");
    if (dist > 0) {
      Serial.print(dist);
      Serial.println(" cm");
    } else {
      Serial.println("No object");
    }
    Serial.println("------------------------");

    updateOLED(angle, dist);

    angle += dir;

    if (angle >= maxAngle) {
      angle = maxAngle;
      dir = -1;
    } else if (angle <= minAngle) {
      angle = minAngle;
      dir = 1;
    }
  }
}