#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Servo.h>

LiquidCrystal lcd(A0, A1, A2, A3, A4, A5);

#define BUZZER       11
#define SERVO_PIN    10
#define GREEN_LED    12
#define RED_LED      13

#define USER_PIN     "1234"
#define ADMIN_PIN    "9999"
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
byte colPins[COLS]  = {6, 7, 8, 9};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo servo;

char  input[17]       = "";
byte  inputLen        = 0;
byte  fails           = 0;
unsigned long lockedUntil = 0;

bool protectedMode  = false;
bool adminLockout   = false;
unsigned long lastAlertMs = 0;

void beep(int freq, int ms) {
  tone(BUZZER, freq, ms);
  delay(ms);
  noTone(BUZZER);
}

void lcdMsg(const char* l1, const char* l2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
}

void resetInput() { inputLen = 0; input[0] = '\0'; }

void showHome() {
  if (protectedMode)
    lcdMsg("[PROTECTED MODE]", "  PIN + #");
  else
    lcdMsg(" Enter PIN Code", "   PIN + #");
}

void logEvent(const char* msg) {
  Serial.print("[");
  Serial.print(millis() / 1000);
  Serial.print("s] ");
  Serial.println(msg);
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  servo.attach(SERVO_PIN);
  servo.write(DOOR_CLOSE);

  lcd.begin(16, 2);

  logEvent("System started.");
  showHome();
}

void loop() {
  unsigned long now = millis();

  // --- ADMIN LOCKOUT (protected mode) ---
  if (adminLockout) {
    if (now - lastAlertMs >= 1500) {
      lastAlertMs = now;
      digitalWrite(RED_LED, HIGH);
      tone(BUZZER, 400, 300);
      delay(300);
      noTone(BUZZER);
      digitalWrite(RED_LED, LOW);
    }
    char key = keypad.getKey();
    if (!key) return;
    if (key == 'C') { resetInput(); lcdMsg("* LOCKED OUT! *", " ADMIN PIN + #"); return; }
    if (key == '#') {
      if (strcmp(input, ADMIN_PIN) == 0) {
        adminLockout = false;
        fails = 0;
        resetInput();
        logEvent("ADMIN: System unlocked by ADMIN.");
        lcdMsg("  UNLOCKED!", "  ADMIN OK    ");
        digitalWrite(GREEN_LED, HIGH);
        beep(1200, 300);
        digitalWrite(GREEN_LED, LOW);
        delay(2000);
        showHome();
      } else {
        resetInput();
        logEvent("ADMIN LOCKOUT: Failed attempt with wrong PIN.");
        lcdMsg("* LOCKED OUT! *", " ADMIN PIN + #");
      }
      return;
    }
    if (inputLen < 16) {
      input[inputLen++] = key; input[inputLen] = '\0';
      char stars[17] = "";
      for (byte i = 0; i < inputLen; i++) stars[i] = '*';
      lcdMsg("* LOCKED OUT! *", stars);
    }
    return;
  }

  if (lockedUntil) {
    if (now < lockedUntil) {
      char buf[17];
      snprintf(buf, sizeof(buf), "  Wait: %ds   ", (int)((lockedUntil - now) / 1000 + 1));
      lcd.setCursor(0, 1); lcd.print(buf);
      delay(500);
      return;
    }
    lockedUntil = 0; fails = 0;
    showHome();
  }

  char key = keypad.getKey();
  if (!key) return;

  if (key == 'C') {
    resetInput();
    showHome();
    return;
  }

  if (key == '#') {

    // --- ADMIN ---
    if (strcmp(input, ADMIN_PIN) == 0) {
      protectedMode = !protectedMode;
      fails = 0;
      lockedUntil = 0;
      resetInput();
      if (protectedMode) {
        logEvent("ADMIN: Protected Mode ACTIVATED.");
        lcdMsg("[PROTECTED MODE]", "  ACTIVATED!  ");
        digitalWrite(GREEN_LED, HIGH);
        beep(1200, 200); beep(1400, 200);
        digitalWrite(GREEN_LED, LOW);
      } else {
        logEvent("ADMIN: Protected Mode DEACTIVATED.");
        lcdMsg(" Protected Mode", " DEACTIVATED!  ");
        beep(1000, 300);
      }
      delay(2000);
      showHome();
      return;
    }

    // --- NORMAL USER ---
    if (strcmp(input, USER_PIN) == 0) {
      fails = 0;
      logEvent("ACCESS: Login successful. Door opened.");
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
      digitalWrite(RED_LED, HIGH);
      beep(500, 250); delay(100); beep(500, 250);
      digitalWrite(RED_LED, LOW);

      if (protectedMode) {
        adminLockout = true;
        lastAlertMs  = 0;
        logEvent("FAILURE [PROTECTED]: Wrong code. System locked. Requires ADMIN.");
        lcdMsg("* LOCKED OUT! *", " ADMIN PIN + #");
      } else {
        fails++;
        Serial.print("["); Serial.print(millis() / 1000);
        Serial.print("s] FAILURE: Attempt "); Serial.print(fails);
        Serial.print("/"); Serial.print(MAX_FAILS);
        Serial.println(" with wrong code.");
        if (fails >= MAX_FAILS) {
          lockedUntil = millis() + LOCKOUT_MS;
          logEvent("LOCKOUT: Maximum attempts reached. Locked for 10s.");
          lcdMsg("   SYSTEM     ", " LOCKED OUT!");
          beep(300, 120); delay(100); beep(300, 120); delay(100); beep(300, 120);
        } else {
          char buf2[17];
          snprintf(buf2, sizeof(buf2), "%d Attempts Left!", MAX_FAILS - fails);
          lcdMsg("- WRONG  CODE -", buf2);
          delay(4000);
        }
      }
    }

    resetInput();
    showHome();
    return;
  }

  if (inputLen < 16) {
    input[inputLen++] = key;
    input[inputLen]   = '\0';
    char stars[17] = "";
    for (byte i = 0; i < inputLen; i++) stars[i] = '*';
    lcdMsg("  Enter PIN:", stars);
  }
}
