/*
 * Smart Detector - full version
 * (detection + visual warning + SMS/call alert + window opening + fan extraction)
 *
 * Hardware : Arduino UNO, MQ-5 gas sensor, 16x2 I2C LCD, red / yellow / green LEDs,
 *            SIM800L GSM module, SG90 servo (window), DC fan driven through a transistor/MOSFET
 * Libraries: LiquidCrystal I2C (Frank de Brabander), Servo, SoftwareSerial
 *
 * Zones:
 *   value >  DANGER_THRESHOLD  -> red LED, fan ON, window opened (90 deg), SMS + call
 *   value >  WARNING_THRESHOLD -> yellow LED, fan ON, window closed (0 deg), SMS + call
 *   otherwise                  -> green LED, fan OFF
 */

#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ----------------------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------------------

const char PHONE_NUMBER[] = "+212XXXXXXXXX";  // number that receives the SMS and the call

const uint8_t PIN_RED_LED    = 2;
const uint8_t PIN_GREEN_LED  = 3;
const uint8_t PIN_YELLOW_LED = 4;
// Must not be 10 or 11: those pins are used by SoftwareSerial for the SIM800L.
const uint8_t PIN_FAN        = 5;
const uint8_t PIN_SERVO      = 9;
const uint8_t PIN_SIM_RX     = 10;  // Arduino RX  <- SIM800L TXD
const uint8_t PIN_SIM_TX     = 11;  // Arduino TX  -> SIM800L RXD
const uint8_t PIN_GAS_SENSOR = A0;

const int DANGER_THRESHOLD  = 500;
const int WARNING_THRESHOLD = 250;

// Set to false to alert only in the danger zone, as in the flowchart of the presentation.
const bool ALERT_ON_WARNING = true;

const int WINDOW_CLOSED_DEG = 0;
const int WINDOW_OPEN_DEG   = 90;
const unsigned long SERVO_STEP_DELAY_MS = 15;

// Minimum time between two alerts of the same level, so a reading oscillating around a
// threshold does not flood the phone with SMS and calls.
const unsigned long ALERT_COOLDOWN_MS = 120000UL;
// The SIM800L needs a few seconds to finish sending the SMS before it accepts a call.
const unsigned long SMS_TO_CALL_DELAY_MS = 5000;

const uint8_t LCD_ADDRESS = 0x27;
const uint8_t LCD_COLUMNS = 16;
const uint8_t LCD_ROWS    = 2;

const unsigned long LOOP_DELAY_MS = 100;

// ----------------------------------------------------------------------------

enum GasLevel : uint8_t { GAS_SAFE, GAS_WARNING, GAS_DANGER };

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
SoftwareSerial sim(PIN_SIM_RX, PIN_SIM_TX);
Servo windowServo;

int windowAngle = WINDOW_CLOSED_DEG;
GasLevel lastAlertLevel = GAS_SAFE;
unsigned long lastAlertMs = 0;

GasLevel classify(int value) {
  if (value > DANGER_THRESHOLD)  return GAS_DANGER;
  if (value > WARNING_THRESHOLD) return GAS_WARNING;
  return GAS_SAFE;
}

// Overwrites the whole row instead of calling lcd.clear() every loop, which flickers.
void printLine(uint8_t row, const String &text) {
  lcd.setCursor(0, row);
  lcd.print(text);
  for (uint8_t i = text.length(); i < LCD_COLUMNS; i++) {
    lcd.print(' ');
  }
}

void setLeds(bool red, bool yellow, bool green) {
  digitalWrite(PIN_RED_LED, red ? HIGH : LOW);
  digitalWrite(PIN_YELLOW_LED, yellow ? HIGH : LOW);
  digitalWrite(PIN_GREEN_LED, green ? HIGH : LOW);
}

void setFan(bool on) {
  digitalWrite(PIN_FAN, on ? HIGH : LOW);
}

void moveWindowTo(int targetDeg) {
  int step = (targetDeg > windowAngle) ? 1 : -1;
  while (windowAngle != targetDeg) {
    windowAngle += step;
    windowServo.write(windowAngle);
    delay(SERVO_STEP_DELAY_MS);
  }
}

void sendSms(const char *text) {
  sim.println(F("AT+CMGF=1"));  // text mode
  delay(200);
  sim.print(F("AT+CMGS=\""));
  sim.print(PHONE_NUMBER);
  sim.println(F("\""));
  delay(200);
  sim.print(text);
  delay(100);
  sim.write(26);  // Ctrl+Z ends the message
  delay(200);
}

void callNumber() {
  sim.print(F("ATD"));
  sim.print(PHONE_NUMBER);
  sim.println(F(";"));
}

void alertUser(GasLevel level, int gasValue) {
  char message[48];
  snprintf(message, sizeof(message), "Smart Detector: gas %s (%d)",
           level == GAS_DANGER ? "DANGER" : "warning", gasValue);
  Serial.print(F("ALERT -> "));
  Serial.println(message);

  sendSms(message);
  delay(SMS_TO_CALL_DELAY_MS);
  callNumber();
}

void updateAlert(GasLevel level, int gasValue) {
  unsigned long now = millis();
  bool cooledDown = (now - lastAlertMs) >= ALERT_COOLDOWN_MS;
  bool escalated = level > lastAlertLevel;
  bool alertable = (level == GAS_DANGER) || (level == GAS_WARNING && ALERT_ON_WARNING);

  if (alertable && (escalated || cooledDown)) {
    alertUser(level, gasValue);
    lastAlertLevel = level;
    lastAlertMs = millis();
  } else if (level == GAS_SAFE && cooledDown) {
    lastAlertLevel = GAS_SAFE;
  }
}

void setup() {
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_YELLOW_LED, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_GAS_SENSOR, INPUT);
  setFan(false);

  Serial.begin(9600);
  sim.begin(9600);

  windowServo.attach(PIN_SERVO, 500, 2500);
  windowServo.write(windowAngle);

  lcd.init();
  lcd.backlight();
  printLine(0, "    Welcome");
  printLine(1, " Smart Detector");
  delay(2000);
}

void loop() {
  int gasValue = analogRead(PIN_GAS_SENSOR);
  Serial.println(gasValue);

  GasLevel level = classify(gasValue);

  switch (level) {
    case GAS_DANGER:
      printLine(0, "Dangereux!!!");
      printLine(1, String(gasValue));
      setLeds(true, false, false);
      setFan(true);
      moveWindowTo(WINDOW_OPEN_DEG);
      break;

    case GAS_WARNING:
      printLine(0, "l'environnement!");
      printLine(1, String(gasValue));
      setLeds(false, true, false);
      setFan(true);
      moveWindowTo(WINDOW_CLOSED_DEG);
      break;

    case GAS_SAFE:
      printLine(0, "aucun gas detect");
      printLine(1, String(gasValue));
      setLeds(false, false, true);
      setFan(false);
      break;
  }

  updateAlert(level, gasValue);

  delay(LOOP_DELAY_MS);
}
