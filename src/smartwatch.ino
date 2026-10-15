#include <WiFi.h>
#include "time.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================================
// 1. GLOBAL CONFIGURATION (Change these)
// ==========================================
const char* ssid     = "WiFi ssid";
const char* password = "WiFi password";
const int   wifi_channel = 1;                // Set your 2.4GHz Wi-Fi channel (usually 1, 6, or 11)

// Timezone Settings (UTC+7)
const long  gmtOffset_sec = 25200;         
const int   daylightOffset_sec = 0; 

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Hardware Pins
#define BUTTON_PIN GPIO_NUM_2

// --- Deep Sleep Persistent Memory ---
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR unsigned long lastSyncTimestamp = 0; 

// ==========================================
// 2. HELPER FUNCTIONS
// ==========================================
void applyTimezone() {
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "WIB-7", 1); 
  tzset();
}

void connectToWiFiAndSync() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  // Connect using the global variables defined at the top
  if (wifi_channel > 0) {
    WiFi.begin(ssid, password, wifi_channel);
  } else {
    WiFi.begin(ssid, password);
  }
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 12) {
    delay(500);
    display.print(".");
    display.display();
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    display.println("\nConnected!");
    display.println("Syncing NTP...");
    display.display();
    
    applyTimezone();

    struct tm timeinfo;
    int timeoutCounter = 0;
    while (timeoutCounter < 20) {
      if (getLocalTime(&timeinfo)) {
        if (timeinfo.tm_year + 1900 >= 2026) {
          time_t now;
          time(&now);
          lastSyncTimestamp = now; 
          break; 
        }
      }
      delay(500); 
      timeoutCounter++;
    }
    
    if (timeoutCounter < 20) {
      display.println("Time Sync OK!");
    } else {
      display.println("NTP Sync Failed!");
    }
    display.display();
    delay(1000);
  } else {
    display.println("\nWiFi not found.");
    display.println("Using Local RTC.");
    display.display();
    delay(1500);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void displayTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("RTC Read Error");
    display.display();
    return;
  }
  
  display.clearDisplay();
  
  // Draw Time
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 15);
  display.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  
  // Draw Date
  display.setTextSize(1);
  display.setCursor(32, 45);
  display.printf("%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  
  display.display();
}

// ==========================================
// 3. MAIN SETUP & LOOP
// ==========================================
void setup() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;); 
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  applyTimezone();

  time_t now;
  time(&now);

  if (bootCount == 0) {
    bootCount++;
    connectToWiFiAndSync();
  } else {
    // Only try to connect to Wi-Fi if at least 1 hour (3600 seconds) has elapsed
    if (now - lastSyncTimestamp >= 3600) {
      connectToWiFiAndSync();
    }
  }

  esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
}

void loop() {
  displayTime();
  delay(200); 

  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(500); // Debounce check

    display.clearDisplay();
    display.display();
    
    esp_deep_sleep_start();
  }
}
