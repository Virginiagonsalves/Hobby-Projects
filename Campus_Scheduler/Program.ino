#include <virtuabotixRTC.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// Pin Definitions
const int setPin = 2, okPin = 3, upPin = 4, downPin = 5;
const int displayAlarmsPin = 10, deleteAlarmPin = 11, buzzerPin = 9, relayPin = 12; // Add relayPin

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
  //seconds, minutes, hours, day of the week, date, month, year 0 for Sunday
  //myRTC.setDS1302Time (00, 15, 9, 6, 11, 8, 2024);
  pinMode(setPin, INPUT_PULLUP); 
  pinMode(okPin, INPUT_PULLUP);
  pinMode(upPin, INPUT_PULLUP); 
  pinMode(downPin, INPUT_PULLUP);
  pinMode(displayAlarmsPin, INPUT_PULLUP); 
  pinMode(deleteAlarmPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
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
  if (digitalRead(setPin) == LOW && currentTime - lastDebounceTime > debounceDelay) {
    lastDebounceTime = currentTime; 
    settingTime = true; 
    setStep = 0; 
    lcd.clear(); 
    lcd.print("Set Hour: "); 
    lcd.print(setHour); 
    delay(1000); 
    lastInteractionTime = millis(); 
    return;
  }
  if (digitalRead(displayAlarmsPin) == LOW && currentTime - lastDebounceTime > debounceDelay) {
    lastDebounceTime = currentTime; 
    lcd.clear();
    lcd.print("Alarms Entered"); // Debugging line
    delay(1000);
    displayAlarms(); 
    lastInteractionTime = millis(); 
    return;
  }
  if (digitalRead(deleteAlarmPin) == LOW && currentTime - lastDebounceTime > debounceDelay) {
    lastDebounceTime = currentTime; 
    lcd.clear(); 
    lcd.print("Deleting Alarm"); // Debugging line
    delay(1000); 
    deleteAlarm(); 
    lastInteractionTime = millis(); 
    return;
  }
  if (settingTime) {
    if ((digitalRead(upPin) == LOW || digitalRead(downPin) == LOW) && currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime;
      int increment = (digitalRead(upPin) == LOW) ? 1 : -1;
      if (setStep == 0) setHour = (setHour + increment + 24) % 24;
      else if (setStep == 1) setMinute = (setMinute + increment + 60) % 60;
      else if (setStep == 2) setSecond = (setSecond + increment + 60) % 60;
      else if (setStep == 3) setDayOfWeek = (setDayOfWeek + increment + 7) % 7; // Added dayOfWeek setting
      updateLCDForSetting();
      delay(500); 
      lastInteractionTime = millis();
    }
    if (digitalRead(okPin) == LOW && currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime; 
      setStep++;
      if (setStep == 4) { // Updated step count for day of week
        setAlarm(); 
        settingTime = false; 
      } else {
        updateLCDForSetting();
      }
      delay(500); 
      lastInteractionTime = millis();
    }
  }
}

void updateLCD() {
  myRTC.updateTime();
  lcd.clear();
  lcd.print("Time: "); 
  printFormatted(myRTC.hours); 
  lcd.print(":"); 
  printFormatted(myRTC.minutes); 
  lcd.print(":"); 
  printFormatted(myRTC.seconds);
  lcd.setCursor(0, 1);
  lcd.print("Date: "); 
  printFormatted(myRTC.dayofmonth); 
  lcd.print("/"); 
  printFormatted(myRTC.month); 
  lcd.print("/"); 
  lcd.print(myRTC.year);
  lastInteractionTime = millis();
}

void updateLCDForSetting() {
  lcd.clear();
  if (setStep == 0) 
    lcd.print("Set Hour: "), lcd.print(setHour);
  else if (setStep == 1) 
    lcd.print("Set Minute: "), lcd.print(setMinute);
  else if (setStep == 2) 
    lcd.print("Set Second: "), lcd.print(setSecond);
  else if (setStep == 3) {
    lcd.print("Set Day: ");
    switch (setDayOfWeek) { // Display the day of the week
      case 0: lcd.print("Sun"); break;
      case 1: lcd.print("Mon"); break;
      case 2: lcd.print("Tue"); break;
      case 3: lcd.print("Wed"); break;
      case 4: lcd.print("Thur"); break;
      case 5: lcd.print("Fri"); break;
      case 6: lcd.print("Sat"); break;
    }
  }
  lastInteractionTime = millis();
}

void checkAlarmTime() {
  myRTC.updateTime();
  int currentDayOfWeek = myRTC.dayofweek;
  for (int i = 0; i < alarmCount; i++) {
    if (alarms[i].active && myRTC.hours == alarms[i].hour && myRTC.minutes == alarms[i].minute && myRTC.seconds == alarms[i].second && currentDayOfWeek == alarms[i].dayOfWeek) {
      digitalWrite(relayPin, HIGH); // Activate relay
      digitalWrite(buzzerPin, HIGH); // Turn on buzzer
      delay(2000);
      digitalWrite(relayPin, LOW); // Deactivate relay after the delay
      digitalWrite(buzzerPin, LOW); // Turn off buzzer after the delay
      alarms[i].active = false;
      deleteInactiveAlarms();
      saveAlarmsToEEPROM();
    }
  }
}

void displayAlarms() {
  lcd.clear();
  delay(100); // Short delay to ensure the LCD has time to clear
  
  if (alarmCount == 0) {
    lcd.print("No Alarms Set");
    delay(2000); // Delay to make sure the message is visible
  } else {
    for (int i = 0; i < alarmCount; i++) {
      lcd.setCursor(0, 0); 
      lcd.print("Alarm "); 
      lcd.print(i + 1); 
      lcd.print(": ");
      lcd.setCursor(0, 1);
      if (alarms[i].active) {
        printFormatted(alarms[i].hour); 
        lcd.print(":"); 
        printFormatted(alarms[i].minute); 
        lcd.print(":"); 
        printFormatted(alarms[i].second);
        lcd.print(" ");
        switch (alarms[i].dayOfWeek) { // Display the day of the week
          case 0: lcd.print("Sun"); break;
          case 1: lcd.print("Mon"); break;
          case 2: lcd.print("Tue"); break;
          case 3: lcd.print("Wed"); break;
          case 4: lcd.print("Thu"); break;
          case 5: lcd.print("Fri"); break;
          case 6: lcd.print("Sat"); break;
        }
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
  lcd.clear();
  if (alarmCount == 0) {
    lcd.print("No Alarms Set"); 
    delay(2000); 
    lcd.clear(); 
    return;
  }
  lcd.print("Select Alarm to"); 
  lcd.setCursor(0, 1); 
  lcd.print("Delete: "); 
  lcd.print(selectedAlarmToDelete + 1);
  delay(2000); 
  lastInteractionTime = millis();

  while (true) {
    unsigned long currentTime = millis();
    if ((digitalRead(upPin) == LOW || digitalRead(downPin) == LOW) && currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime;
      selectedAlarmToDelete = (selectedAlarmToDelete + (digitalRead(upPin) == LOW ? 1 : alarmCount - 1)) % alarmCount;
      lcd.clear(); 
      lcd.print("Select Alarm to"); 
      lcd.setCursor(0, 1); 
      lcd.print("Delete: "); 
      lcd.print(selectedAlarmToDelete + 1);
      delay(2000); 
      lastInteractionTime = millis();
    }
    if (digitalRead(okPin) == LOW && currentTime - lastDebounceTime > debounceDelay) {
      lastDebounceTime = currentTime; 
      deleteSelectedAlarm(); 
      break;
    }
    if (millis() - lastInteractionTime > screenTimeout) { 
      lcd.clear(); 
      break; 
    }
  }
}

void deleteSelectedAlarm() {
  lcd.clear(); 
  lcd.print("Deleting Alarm"); 
  lcd.setCursor(0, 1); 
  lcd.print(selectedAlarmToDelete + 1);
  delay(2000); 
  lastInteractionTime = millis();
  for (int i = selectedAlarmToDelete; i < alarmCount - 1; i++) 
    alarms[i] = alarms[i + 1];
  alarmCount--; 
  saveAlarmsToEEPROM(); 
  lcd.clear(); 
  lcd.print("Alarm Deleted!"); 
  delay(2000); 
  lastInteractionTime = millis();
}

void deleteInactiveAlarms() {
  for (int i = 0; i < alarmCount; i++) {
    if (!alarms[i].active) {
      for (int j = i; j < alarmCount - 1; j++) 
        alarms[j] = alarms[j + 1];
      alarmCount--; 
      i--;
    }
  }
}

void setAlarm() {
  if (alarmCount >= maxAlarms) {
    lcd.clear(); 
    lcd.print("Alarm Limit Reached"); 
    delay(2000); 
    return;
  }
  alarms[alarmCount++] = {setHour, setMinute, setSecond, setDayOfWeek, true}; // Added dayOfWeek
  saveAlarmsToEEPROM();
  lcd.clear(); 
  lcd.print("Alarm Set!"); 
  delay(2000); 
  lastInteractionTime = millis();
}

void loadAlarmsFromEEPROM() {
  alarmCount = EEPROM.read(0);
  for (int i = 0; i < alarmCount; i++) {
    alarms[i].hour = EEPROM.read(1 + i * 5); // Updated address calculation
    alarms[i].minute = EEPROM.read(2 + i * 5); // Updated address calculation
    alarms[i].second = EEPROM.read(3 + i * 5); // Updated address calculation
    alarms[i].dayOfWeek = EEPROM.read(4 + i * 5); // Added dayOfWeek read
    alarms[i].active = EEPROM.read(5 + i * 5); // Updated address calculation
  }
}

void saveAlarmsToEEPROM() {
  EEPROM.write(0, alarmCount);
  for (int i = 0; i < alarmCount; i++) {
    EEPROM.write(1 + i * 5, alarms[i].hour); // Updated address calculation
    EEPROM.write(2 + i * 5, alarms[i].minute); // Updated address calculation
    EEPROM.write(3 + i * 5, alarms[i].second); // Updated address calculation
    EEPROM.write(4 + i * 5, alarms[i].dayOfWeek); // Added dayOfWeek write
    EEPROM.write(5 + i * 5, alarms[i].active); // Updated address calculation
  }
}

void printFormatted(int value) {
  if (value < 10) lcd.print("0");
  lcd.print(value);
}
