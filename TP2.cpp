#include <Wire.h>
#include <Adafruit_LiquidCrystal.h>
#include <Keypad.h>
#include <Servo.h>

Adafruit_LiquidCrystal lcd(0);

#define BUZZER     11
#define SERVO_PIN  10
#define GREEN_LED  12
#define RED_LED    13

#define CORRECT_PIN  "1234"
#define MAX_FAILS    3
#define LOCKOUT_MS   10000UL
#define DOOR_OPEN_MS  3000UL
#define DOOR_OPEN    90
#define DOOR_CLOSE    0

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo servo;

char input[17] = "";
byte inputLen = 0;
byte fails = 0;
unsigned long lockedUntil = 0;

void beep(int freq, int ms) {
  tone(BUZZER, freq, ms);
  delay(ms);
  noTone(BUZZER);
}

void lcdMsg(const char* l1, const char* l2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l1);
  lcd.setCursor(0, 1);
  lcd.print(l2);
}

void resetInput() {
  inputLen = 0;
  input[0] = '\0';
}

void setup() {
  pinMode(BUZZER, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(BUZZER, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  servo.attach(SERVO_PIN);
  servo.write(DOOR_CLOSE);

  Wire.begin();
  lcd.begin(16, 2);
  lcd.setBacklight(1);
  lcd.clear();

  lcdMsg(" Enter PIN Code", "   PIN + #");
}

void loop() {
  unsigned long now = millis();

  if (lockedUntil) {
    if (now < lockedUntil) {
      char buf[17];
      snprintf(buf, sizeof(buf), "  Wait: %ds   ", (int)((lockedUntil - now) / 1000 + 1));
      lcd.setCursor(0, 1);
      lcd.print(buf);
      delay(500);
      return;
    }

    lockedUntil = 0;
    fails = 0;
    lcdMsg(" Enter PIN Code", "   PIN + #");
  }

  char key = keypad.getKey();
  if (!key) return;

  if (key == 'C') {
    resetInput();
    lcdMsg(" Enter PIN Code", "   PIN + #");
    return;
  }

  if (key == '#') {
    if (strcmp(input, CORRECT_PIN) == 0) {
      fails = 0;
      lcdMsg("  - CORRECT -", "Opening Door...");

      digitalWrite(GREEN_LED, HIGH);
      beep(1800, 300);
      digitalWrite(GREEN_LED, LOW);

      servo.write(DOOR_OPEN);
      delay(DOOR_OPEN_MS);
      servo.write(DOOR_CLOSE);

      lcdMsg(" Door CLOSED!", "");
      delay(1000);
    } else {
      fails++;

      digitalWrite(RED_LED, HIGH);
      beep(500, 250);
      delay(100);
      beep(500, 250);
      digitalWrite(RED_LED, LOW);

      if (fails >= MAX_FAILS) {
        lockedUntil = millis() + LOCKOUT_MS;
        lcdMsg("   SYSTEM    ", "LOCKED OUT!");
        beep(1200, 120);
        delay(100);
        beep(1200, 120);
        delay(100);
        beep(1200, 120);
      } else {
        char buf[17];
        snprintf(buf, sizeof(buf), "%d Attempts Left!", MAX_FAILS - fails);
        lcdMsg("- WRONG  CODE -", buf);
        delay(4000);
      }
    }

    resetInput();
    lcdMsg(" Enter PIN Code", "   PIN + #");
    return;
  }

  if (inputLen < 16) {
    input[inputLen++] = key;
    input[inputLen] = '\0';

    char stars[17] = "";
    for (byte i = 0; i < inputLen; i++) stars[i] = '*';
    lcdMsg("  Enter PIN:", stars);
  }
}
