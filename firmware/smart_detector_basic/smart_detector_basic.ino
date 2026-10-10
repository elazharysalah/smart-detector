/*
 * Smart Detector - basic version (detection + visual warning)
 *
 * Hardware : Arduino UNO, MQ-5 gas sensor, 16x2 I2C LCD, red / yellow / green LEDs
 * Libraries: LiquidCrystal I2C (Frank de Brabander, Arduino Library Manager)
 *
 * Reads the MQ-5 analog output and classifies it into three zones:
 *   value >  DANGER_THRESHOLD  -> "Dangereux!!!"       + red LED
 *   value >  WARNING_THRESHOLD -> "l'environnement!"   + yellow LED
 *   otherwise                  -> "aucun gas detect"   + green LED
 */

#include <LiquidCrystal_I2C.h>

const uint8_t PIN_RED_LED    = 2;
const uint8_t PIN_GREEN_LED  = 3;
const uint8_t PIN_YELLOW_LED = 4;
const uint8_t PIN_GAS_SENSOR = A0;

const int DANGER_THRESHOLD  = 500;
const int WARNING_THRESHOLD = 250;

const uint8_t LCD_ADDRESS = 0x27;
const uint8_t LCD_COLUMNS = 16;
const uint8_t LCD_ROWS    = 2;

const unsigned long LOOP_DELAY_MS = 100;

enum GasLevel : uint8_t { GAS_SAFE, GAS_WARNING, GAS_DANGER };

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

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

void setup() {
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_YELLOW_LED, OUTPUT);
  pinMode(PIN_GAS_SENSOR, INPUT);

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  printLine(0, "    Welcome");
  printLine(1, " Smart Detector");
  delay(2000);
}

void loop() {
  int gasValue = analogRead(PIN_GAS_SENSOR);
  Serial.println(gasValue);

  switch (classify(gasValue)) {
    case GAS_DANGER:
      printLine(0, "Dangereux!!!");
      setLeds(true, false, false);
      break;

    case GAS_WARNING:
      printLine(0, "l'environnement!");
      setLeds(false, true, false);
      break;

    case GAS_SAFE:
      printLine(0, "aucun gas detect");
      setLeds(false, false, true);
      break;
  }
  printLine(1, String(gasValue));

  delay(LOOP_DELAY_MS);
}
