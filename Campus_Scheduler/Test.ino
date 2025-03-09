#include <virtuabotixRTC.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// Pin Definitions
const int pins[] = {2, 3, 4, 5, 10, 11, 9, 12}; // setPin, okPin, upPin, downPin, displayAlarmsPin, deleteAlarmPin, buzzerPin, relayPin

// LCD and RTC Initialization
LiquidCrystal_I2C lcd(0x27, 16, 2);
virtuabotixRTC myRTC(6, 7, 8); // SCLK, IO, CE pins

// Time Setting Variables
bool settingTime = false;
int setStep = 0, setHour = 0, setMinute = 0, setSecond = 0, setDayOfWeek = 0;

// Alarm Struct and Array
struct Alarm { int hour, minute, second, dayOfWeek; bool active; };
const int maxAlarms = 5;
Alarm alarms[maxAlarms];
int alarmCount = 0;

// Debounce timing and Screen Timeout
unsigned long lastDebounceTime = 0, debounceDelay = 200;
unsigned long lastInteractionTime = 0, screenTimeout = 4000;
int selectedAlarmToDelete = -1;

void setup() {
  Wire.begin();
  lcd.begin(16, 2);
  lcd.backlight();
  pinModePins();
  loadAlarmsFromEEPROM();
  lcd.print("Initializing...");
  delay(2000);
  lastInteractionTime = millis();
}

void loop() {
  handleSwitches();
  if (!settingTime) {
    updateLCD();
  } else if (millis() - lastInteractionTime > screenTimeout) {
    settingTime = false;
    lcd.clear();
  }
  checkAlarmTime();
}

void handleSwitches() {
  unsigned long currentTime = millis();
  for (int i = 0; i < 4; i++) { // Check for setPin, displayAlarmsPin, deleteAlarmPin, okPin
    if (digitalRead(pins[i]) == LOW && currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime;
      handleAction(i);
      lastInteractionTime = millis();
      return;
    }
  }

  if (settingTime) {
    if ((digitalRead(pins[2]) == LOW || digitalRead(pins[3]) == LOW) && currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime;
      int increment = (digitalRead(pins[2]) == LOW) ? 1 : -1;
      adjustSetting(increment);
      delay(500);
      lastInteractionTime = millis();
    }
  }
}

void handleAction(int pinIndex) {
  switch (pinIndex) {
    case 0: settingTime = true; setStep = 0; updateLCDForSetting(); break;
    case 1: displayAlarms(); break;
    case 2: deleteAlarm(); break;
    case 3: finalizeSetting(); break;
  }
}

void adjustSetting(int increment) {
  switch (setStep) {
    case 0: setHour = (setHour + increment + 24) % 24; break;
    case 1: setMinute = (setMinute + increment + 60) % 60; break;
    case 2: setSecond = (setSecond + increment + 60) % 60; break;
    case 3: setDayOfWeek = (setDayOfWeek + increment + 7) % 7; break;
  }
  updateLCDForSetting();
}

void finalizeSetting() {
  setStep++;
  if (setStep == 4) {
    setAlarm();
    settingTime = false;
  } else {
    updateLCDForSetting();
  }
}

void updateLCD() {
  myRTC.updateTime();
  lcd.clear();
  lcd.print("Time: "); printFormatted(myRTC.hours); lcd.print(":");
  printFormatted(myRTC.minutes); lcd.print(":"); printFormatted(myRTC.seconds);
  lcd.setCursor(0, 1);
  lcd.print("Date: "); printFormatted(myRTC.dayofmonth); lcd.print("/");
  printFormatted(myRTC.month); lcd.print("/"); lcd.print(myRTC.year);
  lastInteractionTime = millis();
}

void updateLCDForSetting() {
  lcd.clear();
  const char* labels[] = {"Hour: ", "Minute: ", "Second: ", "Day: "};
  if (setStep < 3) {
    lcd.print("Set "); lcd.print(labels[setStep]); lcd.print(setStep == 0 ? setHour : setStep == 1 ? setMinute : setStep == 2 ? setSecond : setDayOfWeek);
  } else {
    lcd.print("Set Day: ");
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    lcd.print(days[setDayOfWeek]);
  }
  lastInteractionTime = millis();
}

void checkAlarmTime() {
  myRTC.updateTime();
  int currentDayOfWeek = myRTC.dayofweek;
  for (int i = 0; i < alarmCount; i++) {
    if (alarms[i].active && myRTC.hours == alarms[i].hour && myRTC.minutes == alarms[i].minute && myRTC.seconds == alarms[i].second && currentDayOfWeek == alarms[i].dayOfWeek) {
      digitalWrite(pins[7], HIGH); // Relay
      digitalWrite(pins[6], HIGH); // Buzzer
      delay(2000);
      digitalWrite(pins[7], LOW);
      digitalWrite(pins[6], LOW);
      alarms[i].active = false;
      deleteInactiveAlarms();
      saveAlarmsToEEPROM();
    }
  }
}

void displayAlarms() {
  lcd.clear();
  delay(100);
  
  if (alarmCount == 0) {
    lcd.print("No Alarms Set");
    delay(2000);
  } else {
    for (int i = 0; i < alarmCount; i++) {
      lcd.setCursor(0, 0); lcd.print("Alarm "); lcd.print(i + 1); lcd.print(": ");
      lcd.setCursor(0, 1);
      if (alarms[i].active) {
        printFormatted(alarms[i].hour); lcd.print(":"); printFormatted(alarms[i].minute); lcd.print(":");
        printFormatted(alarms[i].second); lcd.print(" ");
        const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        lcd.print(days[alarms[i].dayOfWeek]);
      } else {
        lcd.print("Inactive");
      }
      delay(2000);
      lcd.clear();
      lastInteractionTime = millis();
    }
  }
}

void deleteAlarm() {
  if (alarmCount == 0) {
    lcd.clear(); lcd.print("No Alarms Set"); delay(2000); lcd.clear(); return;
  }
  lcd.print("Select Alarm to"); lcd.setCursor(0, 1); lcd.print("Delete: "); lcd.print(selectedAlarmToDelete + 1);
  delay(2000); lastInteractionTime = millis();
  
  while (true) {
    unsigned long currentTime = millis();
    if ((digitalRead(pins[2]) == LOW || digitalRead(pins[3]) == LOW) && currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime;
      selectedAlarmToDelete = (selectedAlarmToDelete + (digitalRead(pins[2]) == LOW ? 1 : alarmCount - 1)) % alarmCount;
      lcd.clear(); lcd.print("Select Alarm to"); lcd.setCursor(0, 1); lcd.print("Delete: "); lcd.print(selectedAlarmToDelete + 1);
      delay(2000); lastInteractionTime = millis();
    }
    if (digitalRead(pins[3]) == LOW && currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime; deleteSelectedAlarm(); break;
    }
    if (millis() - lastInteractionTime > screenTimeout) { lcd.clear(); break; }
  }
}

void deleteSelectedAlarm() {
  lcd.clear(); lcd.print("Deleting Alarm"); lcd.setCursor(0, 1); lcd.print(selectedAlarmToDelete + 1);
  delay(2000); lastInteractionTime = millis();
  for (int i = selectedAlarmToDelete; i < alarmCount - 1; i++) alarms[i] = alarms[i + 1];
  alarmCount--;
  saveAlarmsToEEPROM();
  lcd.clear(); lcd.print("Alarm Deleted!"); delay(2000); lastInteractionTime = millis();
}

void deleteInactiveAlarms() {
  for (int i = 0; i < alarmCount; i++) {
    if (!alarms[i].active) {
      for (int j = i; j < alarmCount - 1; j++) alarms[j] = alarms[j + 1];
      alarmCount--;
      i--;
    }
  }
}

void setAlarm() {
  if (alarmCount >= maxAlarms) {
    lcd.clear(); lcd.print("Alarm Limit Reached"); delay(2000); return;
  }
  alarms[alarmCount++] = {setHour, setMinute, setSecond, setDayOfWeek, true};
  saveAlarmsToEEPROM();
  lcd.clear(); lcd.print("Alarm Set!"); delay(2000); lastInteractionTime = millis();
}

void loadAlarmsFromEEPROM() {
  alarmCount = EEPROM.read(0);
  for (int i = 0; i < alarmCount; i++) {
    int baseAddress = 1 + i * 5;
    alarms[i] = { EEPROM.read(baseAddress), EEPROM.read(baseAddress + 1), EEPROM.read(baseAddress + 2), EEPROM.read(baseAddress + 3), EEPROM.read(baseAddress + 4) };
  }
}

void saveAlarmsToEEPROM() {
  EEPROM.write(0, alarmCount);
  for (int i = 0; i < alarmCount; i++) {
    int baseAddress = 1 + i * 5;
    EEPROM.write(baseAddress, alarms[i].hour);
    EEPROM.write(baseAddress + 1, alarms[i].minute);
    EEPROM.write(baseAddress + 2, alarms[i].second);
    EEPROM.write(baseAddress + 3, alarms[i].dayOfWeek);
    EEPROM.write(baseAddress + 4, alarms[i].active);
  }
}

void printFormatted(int value) {
  if (value < 10) lcd.print("0");
  lcd.print(value);
}

void pinModePins() {
  for (int i = 0; i < 8; i++) {
    pinMode(pins[i], i < 4 ? INPUT_PULLUP : OUTPUT);
  }
}
