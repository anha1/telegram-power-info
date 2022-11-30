#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <EEPROM.h>
#include <SimpleTimer.h>


// eeprom, no need to change this
#define NO_POWER_FLAG 7 
#define WITH_POWER_FLAG 16  
#define EEPROM_ADDR 8 

// Telegram
#define TG_CHAT_ID "your value"
#include <UniversalTelegramBot.h>  
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#define BOTtoken "your value"
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
// Telegram end

// Wifi
#define WIFI_SSID "your value"
#define WIFI_PASSWORD "your value"

// Text messages
#define MSG_POWER_ON "Power is on💡"
#define MSG_POWER_OFF "Power is out⚡"

// Power probe pin on the board, up to 3.3v, please do not use 5v directly
#define EXTERNAL_POWER_PROBE_PIN 4

SimpleTimer timer;

boolean isEepromError = false;

boolean isEepromValid(int eeprom) {
  return eeprom == WITH_POWER_FLAG || eeprom == NO_POWER_FLAG;
}

void readExternalPower() {
  
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  int eeprom = EEPROM.read(EEPROM_ADDR);
  if (!isEepromValid(eeprom)) { 
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
    if (isPowerNow) {
      if (!bot.sendMessage(TG_CHAT_ID, MSG_POWER_ON, "")) {
        return;
      }
      EEPROM.write(EEPROM_ADDR, WITH_POWER_FLAG);
      EEPROM.commit();   
    } else {
      if (!bot.sendMessage(TG_CHAT_ID, MSG_POWER_OFF, "")) {
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
    EEPROM.write(EEPROM_ADDR, WITH_POWER_FLAG);
    EEPROM.commit();  
    Serial.println("EEPROM initialized");
    eeprom = EEPROM.read(EEPROM_ADDR);
    Serial.println(eeprom);    
  } else {
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

  timer.setInterval(5000, readExternalPower);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && !isEepromError) {
    esp_task_wdt_reset();
  }
  timer.run();
}
