#include <Arduino.h>
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <EEPROM.h>
#include <SimpleTimer.h>

// eeprom settings, no need to change this if no eeprom errors
#define NO_POWER_FLAG 7 
#define WITH_POWER_FLAG 16  
#define EEPROM_ADDR 8 

// Telegram
// please use @myidbot "/getgroupid" command to determine group/channel id
#define TG_CHAT_ID "YOUR OWN VALUE"
#include <UniversalTelegramBot.h>  
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
// please use @BothFather to create a bot, you need to add the bot to the group/channel
#define BOTtoken "YOUR OWN VALUE"
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
// Telegram end

// Wifi connection settings
#define WIFI_SSID "YOUR OWN VALUE"
#define WIFI_PASSWORD "YOUR OWN VALUE"

// Text messages
#define MSG_POWER_ON "Power is on💡"
#define MSG_POWER_OFF "Power is out⚡"

// Power probe pin on the board, up to 3.3v, please do not use 5v directly
#define EXTERNAL_POWER_PROBE_PIN 4

/** Maximum amount of rows in timestamp file */
const int maxRows = 5;

SimpleTimer timer;

boolean isEepromError = false;

boolean isEepromValid(int eeprom) {  
  return eeprom == WITH_POWER_FLAG || eeprom == NO_POWER_FLAG;
}


void appendTimestampAndBooleanToFile(const char* path, bool booleanValue) {
  // Read existing rows
  String rows[maxRows];
  int rowCount = readRowsFromFile(path, rows);

  // Get current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  // Create timestamp string
  char timeString[64];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);

  // Create new row
  String newRow = String(timeString) + ", " + (booleanValue ? "true" : "false");

  // Add new row to the list
  if (rowCount < maxRows) {
    rows[rowCount++] = newRow;
  } else {
    for (int i = 1; i < maxRows; i++) {
      rows[i - 1] = rows[i];
    }
    rows[maxRows - 1] = newRow;
  }

  // Write rows back to the file
  writeRowsToFile(path, rows, rowCount);

  // Print the appended timestamp and boolean value
  Serial.print("Appended: ");
  Serial.println(newRow);
}

int readRowsFromFile(const char* path, String rows[]) {
  int rowCount = 0;

  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return rowCount;
  }

  while (file.available() && rowCount < maxRows) {
    rows[rowCount++] = file.readStringUntil('\n');
  }
  file.close();

  return rowCount;
}

void writeRowsToFile(const char* path, String rows[], int rowCount) {
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  for (int i = 0; i < rowCount; i++) {
    file.println(rows[i]);
  }
  file.close();
}

String readLastRowFromFile(const char* path) {
  String rows[maxRows];
  int rowCount = readRowsFromFile(path, rows);
  if (rowCount == 0) {
    return "";
  }

  return rows[rowCount - 1];
}

String calculateTimeDifference(String lastRow) {
  int commaIndex = lastRow.indexOf(',');
  if (commaIndex == -1) {
    return "Invalid row format";
  }

  String lastTimestamp = lastRow.substring(0, commaIndex);
  lastTimestamp.trim();

  struct tm lastTimeinfo;
  if (strptime(lastTimestamp.c_str(), "%Y-%m-%d %H:%M:%S", &lastTimeinfo) == NULL) {
    return "Invalid timestamp format";
  }

  time_t lastTime = mktime(&lastTimeinfo);

  struct tm currentTimeinfo;
  if (!getLocalTime(&currentTimeinfo)) {
    return "Failed to obtain current time";
  }

  time_t currentTime = mktime(&currentTimeinfo);

  double secondsDifference = difftime(currentTime, lastTime);

  int days = secondsDifference / (60 * 60 * 24);
  int hours = ((int)secondsDifference % (60 * 60 * 24)) / (60 * 60);
  int minutes = ((int)secondsDifference % (60 * 60)) / 60;
  int seconds = (int)secondsDifference % 60;

  String timeDiffString = "";
  if (days > 0) {
    timeDiffString += String(days) + " d";
  }
  if (hours > 0) {
    if (timeDiffString.length() > 0) {
      timeDiffString += ", ";
    }
    timeDiffString += String(hours) + " hr";
  }
  if (minutes > 0) {
    if (timeDiffString.length() > 0) {
      timeDiffString += ", ";
    }
    timeDiffString += String(minutes) + " min";
  }
  if (seconds > 0) {
    if (timeDiffString.length() > 0) {
      timeDiffString += ", ";
    }
    timeDiffString += String(seconds) + " sec";
  }

  // If all values are zero
  if (timeDiffString.length() == 0) {
    timeDiffString = "0 sec";
  }

  return timeDiffString;
}


void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void readExternalPower() {
  
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  int eeprom = EEPROM.read(EEPROM_ADDR);
  if (!isEepromValid(eeprom)) {    
    //  eeprom write count is limited theoreticaly leading to an error here, 
    //  you can try to change address or use another esp32 board
    Serial.println("EEPROM error!");
    isEepromError = true; 
    return;
  } else {
    isEepromError = false;
  }

  boolean isPowerNow = digitalRead(EXTERNAL_POWER_PROBE_PIN) == HIGH;
  boolean isPowerBefore = eeprom == WITH_POWER_FLAG;

  Serial.print("status: ");
  Serial.println(isPowerNow ? "on" : "off");

  if (isPowerBefore != isPowerNow) {
    Serial.print("status change detected, trying to send the message...");

    String lastRow = readLastRowFromFile("/timestamps.txt");
    String timeDiff = calculateTimeDifference(lastRow);

    if (isPowerNow) {
      if (!bot.sendMessage(TG_CHAT_ID, MSG_POWER_ON + String("\ntime without power: ") + String(timeDiff), "")) {
        return;
      }
      EEPROM.write(EEPROM_ADDR, WITH_POWER_FLAG);
      EEPROM.commit();   
    } else {
      if (!bot.sendMessage(TG_CHAT_ID, MSG_POWER_OFF + String("\ntime with power: ") + String(timeDiff), "")) {
        return;
      }
      EEPROM.write(EEPROM_ADDR, NO_POWER_FLAG);
      EEPROM.commit();    
    }
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(EXTERNAL_POWER_PROBE_PIN, INPUT);

  Serial.println();
  Serial.println("Init watchdog:");
  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  } else {
    Serial.println("SPIFFS mounted successfully");
  }


  Serial.println("Init EEPROM:");
  if (!EEPROM.begin(16)) {
    while(true) {
      Serial.println("EEPROM fail");
      sleep(1);
    }
  }
  int eeprom = EEPROM.read(EEPROM_ADDR);
  Serial.println(eeprom);
  if (!isEepromValid(eeprom)) {
    // first start ever
    EEPROM.write(EEPROM_ADDR, WITH_POWER_FLAG);
    EEPROM.commit();  
    Serial.println("EEPROM initialized");
    eeprom = EEPROM.read(EEPROM_ADDR);
    Serial.println(eeprom);    
  } else {
    // next restarts
    Serial.println("EEPROM old value is valid");
  }

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  // the default value is too small leading to duplicated messages because "ok" from TG server is discarded
  bot.waitForResponse = 25000;

  

  timer.setInterval(5000, readExternalPower);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && !isEepromError) {
    // watchdog: if esp_task_wdt_reset() is not called for too long,
    // the board is restarted in an attempt to fix itself
    esp_task_wdt_reset();
  }
  timer.run();
}
