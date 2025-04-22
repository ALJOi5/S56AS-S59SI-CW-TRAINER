#include <Arduino.h>
#include <config.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define EEPROM_ADDR_WPM 0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

boolean modeToggle = true;
unsigned long prevMillis = 0;
int lastButtonState = HIGH;
unsigned long debounceDelay = 190;

int encoderPos = 30;
int lastRE_CLK;
int currentRE_CLK;

#define ENCODER_DEBOUNCE 5
unsigned long lastEncoderChange = 0;
int lastEncoderPos = 0;

// CLICK button debounce
int lastClickState = HIGH;
unsigned long lastClickMillis = 0;
const unsigned long clickDebounceDelay = 200;

// For non-blocking display message
bool showSavedMessage = false;
unsigned long savedMessageStartTime = 0;
const unsigned long savedMessageDuration = 1800;

void updateText(String a);
void showSavedWPMMessage();

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
  pinMode(CLICK, INPUT_PULLUP);

  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;) {
      digitalWrite(PADDLE_STATUS_LED, HIGH);
      digitalWrite(STRAIGHT_STATUS_LED, HIGH);
      delay(500);
      digitalWrite(PADDLE_STATUS_LED, LOW);
      digitalWrite(STRAIGHT_STATUS_LED, LOW);
      delay(500);
    }
  }

  int savedWPM = EEPROM.read(EEPROM_ADDR_WPM);
  if (savedWPM < 5 || savedWPM > 99) {
    encoderPos = 30;
    Serial.println("No valid WPM in EEPROM. Using default: 30");
  } else {
    encoderPos = savedWPM;
    Serial.print("Loaded WPM from EEPROM: ");
    Serial.println(encoderPos);
  }

  updateText(String(encoderPos));
  digitalWrite(PADDLE_STATUS_LED, HIGH);
  digitalWrite(STRAIGHT_STATUS_LED, LOW);
  delay(100);

  lastRE_CLK = digitalRead(RE_CLK);
}

void straight_loop() {
  if (digitalRead(INPUT1_CW) == LOW) {
    tone(BUZZER, 600);
  } else {
    noTone(BUZZER);
  }
}

void updateText(String a) {
  display.clearDisplay();
  display.setTextSize(6);
  display.setTextColor(WHITE);
  int16_t textWidth = a.length() * 6 * 6;
  int16_t xPos = (SCREEN_WIDTH - textWidth) / 2;
  int16_t yPos = (SCREEN_HEIGHT - 3 * 8) / 2;
  display.setCursor(xPos, yPos);
  display.println(a);
  display.display();
}

void showSavedWPMMessage() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  int16_t xPos = (SCREEN_WIDTH - 6 * 12) / 2;  // Approx width of "Saved WPM"
  int16_t yPos = (SCREEN_HEIGHT - 16) / 2;
  display.setCursor(xPos-19, yPos);
  display.println("Saved WPM");
  display.display();
}

void handleEncoder() {
  currentRE_CLK = digitalRead(RE_CLK);
  static int encoderPulseCount = 0;
  static int lastDirection = 0;

  if ((millis() - lastEncoderChange) > ENCODER_DEBOUNCE) {
    if (currentRE_CLK != lastRE_CLK) {
      int direction = digitalRead(RE_DT) == currentRE_CLK ? 1 : -1;

      if (direction != lastDirection) {
        encoderPulseCount = 0;
      }

      encoderPulseCount++;
      lastDirection = direction;

      if (encoderPulseCount >= 2) {
        int step = 1;
        unsigned long interval = millis() - lastEncoderChange;

        if (interval < 100) step = 2;

        encoderPos += direction * step;
        encoderPos = constrain(encoderPos, 5, 99);

        if (encoderPos != lastEncoderPos) {
          if (!showSavedMessage) updateText(String(encoderPos));
          Serial.print("Encoder Position: ");
          Serial.println(encoderPos);
          lastEncoderPos = encoderPos;
        }

        encoderPulseCount = 0;
        lastEncoderChange = millis();
      }

      lastRE_CLK = currentRE_CLK;
    }
  }
}

void paddle_loop() {
  handleEncoder();

  static bool isPlaying = false;
  static unsigned long toneStartTime = 0;
  static unsigned long pauseStartTime = 0;
  static bool isInPause = false;
  static unsigned long toneDuration = 0;

  int dot = 1200 / encoderPos;
  unsigned long currentMillis = millis();

  bool paddlePressed1 = digitalRead(INPUT1_CW) == LOW;
  bool paddlePressed2 = digitalRead(INPUT2_CW) == LOW;

  bool paddlePressed = modeToggle ? (paddlePressed1 || paddlePressed2) : paddlePressed1;

  if (paddlePressed && !isPlaying && !isInPause) {
    tone(BUZZER, 600);
    isPlaying = true;
    toneStartTime = currentMillis;

    if (modeToggle) {
      toneDuration = paddlePressed2 ? (3 * dot) : dot;
    } else {
      toneDuration = dot;
    }
  }

  if (isPlaying && (currentMillis - toneStartTime >= toneDuration)) {
    noTone(BUZZER);
    isPlaying = false;
    isInPause = true;
    pauseStartTime = currentMillis;
  }

  if (isInPause && (currentMillis - pauseStartTime >= dot)) {
    isInPause = false;
  }

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

  int buttonMode = digitalRead(MODE_BUTTON);

  if (buttonMode == LOW && lastButtonState == HIGH && (millis() - prevMillis) > debounceDelay) {
    modeToggle = !modeToggle;
    digitalWrite(PADDLE_STATUS_LED, modeToggle);
    digitalWrite(STRAIGHT_STATUS_LED, !modeToggle);
    prevMillis = millis();
    Serial.println(modeToggle ? "Paddle" : "Straight");
    noTone(BUZZER);
  }

  lastButtonState = buttonMode;

  // CLICK button to save WPM
  int clickButton = digitalRead(CLICK);

  if (clickButton == LOW && lastClickState == HIGH && (millis() - lastClickMillis) > clickDebounceDelay) {
    EEPROM.write(EEPROM_ADDR_WPM, encoderPos);
    Serial.print("Saved WPM to EEPROM: ");
    Serial.println(encoderPos);
    showSavedMessage = true;
    savedMessageStartTime = millis();
    showSavedWPMMessage();
    lastClickMillis = millis();
  }

  lastClickState = clickButton;

  // Non-blocking hide "Saved WPM" message
  if (showSavedMessage && millis() - savedMessageStartTime >= savedMessageDuration) {
    showSavedMessage = false;
    updateText(String(encoderPos));
  }
}
