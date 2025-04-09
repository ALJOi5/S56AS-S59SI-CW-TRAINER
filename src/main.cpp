#include <Arduino.h>
#include <config.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

boolean modeToggle = false; //straight key
unsigned long prevMillis = 0;  // Last time we toggled
int buttonState = 0;           // Current button state
int lastButtonState = HIGH;    // Last button state (for detecting changes)
unsigned long debounceDelay = 20; // Debounce delay in milliseconds

// Rotary encoder state
int encoderPos = 5; 
int lastRE_CLK;
int currentRE_CLK;

#define DEBOUNCE_DELAY 10  // Adjust the debounce delay for smoother operation

unsigned long lastDebounceTime = 0;  // Last debounce time
int lastEncoderPos = 0;  // Store the last position to detect change

void updateText(String a);

void setup() {
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  pinMode(PADDLE_STATUS_LED, OUTPUT);
  pinMode(STRAIGHT_STATUS_LED, OUTPUT);
  digitalWrite(STRAIGHT_STATUS_LED, HIGH);
  pinMode(BUZZER, OUTPUT);
  pinMode(INPUT2_CW, INPUT_PULLUP);
  pinMode(INPUT1_CW, INPUT_PULLUP);
  pinMode(RE_CLK, INPUT_PULLUP);
  pinMode(RE_DT, INPUT_PULLUP);

  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;){
      digitalWrite(PADDLE_STATUS_LED, HIGH);
      digitalWrite(STRAIGHT_STATUS_LED, HIGH);
      delay(500);
      digitalWrite(PADDLE_STATUS_LED, LOW);
      digitalWrite(STRAIGHT_STATUS_LED, LOW);
      delay(500);
    }
  }
  updateText(String(encoderPos));
  delay(100);
}

void straight_loop() {
  if (digitalRead(INPUT1_CW) == LOW) {
    tone(BUZZER, 600);
  } else {
    noTone(BUZZER);
  }
}

void updateText(String a){

  display.clearDisplay();
  display.setTextSize(6);
  display.setTextColor(WHITE);

  String text = a;
  int16_t textWidth = text.length() * 6 * 6;
  int16_t xPos = (SCREEN_WIDTH - textWidth) / 2;
  int16_t yPos = (SCREEN_HEIGHT - 3 * 8) / 2;
  display.setCursor(xPos, yPos);

  display.println(text);

  display.display();
}
void paddle_loop() {
  
  unsigned long currentMillis = millis();

  // --- Rotary encoder handling ---
  currentRE_CLK = digitalRead(RE_CLK);

  if ((currentMillis - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (currentRE_CLK != lastRE_CLK) {
      unsigned long pulseInterval = currentMillis - lastDebounceTime;

      int step = 1;
      if (pulseInterval < 30) step = 5;
      else if (pulseInterval < 50) step = 4;
      else if (pulseInterval < 80) step = 3;
      else if (pulseInterval < 120) step = 2;

      if (digitalRead(RE_DT) != currentRE_CLK) {
        encoderPos += step;
      } else {
        encoderPos -= step;
      }

      if (encoderPos < 5) encoderPos = 5;
      if (encoderPos > 99) encoderPos = 99;

      if (encoderPos != lastEncoderPos) {
        Serial.print("Encoder Position: ");
        Serial.println(encoderPos);
        updateText(String(encoderPos));
        lastEncoderPos = encoderPos;
      }

      lastDebounceTime = currentMillis;
    }
  }

  lastRE_CLK = currentRE_CLK;

  // --- Paddle tone generation (non-blocking) ---
  static bool isPlaying = false;
  static unsigned long toneStartTime = 0;
  static unsigned long pauseStartTime = 0;
  static bool isInPause = false;

  int dot = 1200 / encoderPos;

  bool paddlePressed = digitalRead(INPUT1_CW) == LOW || digitalRead(INPUT2_CW) == LOW;

  if (paddlePressed && !isPlaying && !isInPause) {
    tone(BUZZER, 600);
    isPlaying = true;
    toneStartTime = currentMillis;
  }

  if (isPlaying && (currentMillis - toneStartTime >= dot)) {
    noTone(BUZZER);
    isPlaying = false;
    isInPause = true;
    pauseStartTime = currentMillis;
  }

  if (isInPause && (currentMillis - pauseStartTime >= dot)) {
    isInPause = false;
  }

  // If paddle is released, reset state
  if (!paddlePressed) {
    noTone(BUZZER);
    isPlaying = false;
    isInPause = false;
  }
}


void loop() {
  if (!modeToggle) {
    straight_loop();
  } else {
    paddle_loop();
  }

  // Read the button state
  int buttonMode = digitalRead(MODE_BUTTON);

  // Check if the button state has changed (LOW to HIGH transition)
  if (buttonMode == LOW && lastButtonState == HIGH && (millis() - prevMillis) > debounceDelay) {
    modeToggle = !modeToggle;
    digitalWrite(PADDLE_STATUS_LED, modeToggle);
    digitalWrite(STRAIGHT_STATUS_LED, !modeToggle);

    prevMillis = millis();  // Update the last debounce time

    // Print the mode to serial monitor
    if (!modeToggle) {
      Serial.println("Straight");
      noTone(BUZZER);
    } else {
      Serial.println("Paddle");
      noTone(BUZZER);
    }
  }

  lastButtonState = buttonMode;
}
