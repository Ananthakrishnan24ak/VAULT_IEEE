#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "mbedtls/md.h"
#include <SPI.h>
#include <TFT_eSPI.h> 
#include <Keypad.h>
#include "time.h"

// WiFi Credentials
const char* ssid = "IEEE";
const char* password = "ieee@123";
String SERVER_URL = "https://vault-cloudflare-8fu.pages.dev";
String CAMERA_API_KEY = "cameraapisecretkeyafagalglhlia";

// NTP Server Settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; 
const int   daylightOffset_sec = 0;

Preferences nvs;
TFT_eSPI tft = TFT_eSPI(); 

// --- Keypad Configuration ---
const byte ROWS = 4; 
const byte COLS = 4; 
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27}; 
byte colPins[COLS] = {26, 25, 33, 32}; 
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// --- UI & WiFi States ---
enum UIState { STATE_IDLE, STATE_MENU, STATE_ERROR_DISP, STATE_COUNTDOWN };
UIState currentState = STATE_IDLE;
String enteredPin = "";
unsigned long lastActivityTime = 0;
const unsigned long menuTimeout = 15000; 
unsigned long errorDisplayTime = 0;

// Dual WiFi Status Tracking variables
bool espWifiConnected = false;
bool xiaoWifiConnected = false;
unsigned long lastXiaoHeartbeat = 0;
const unsigned long xiaoTimeoutThreshold = 8000; 

// System Configs
unsigned int poll_interval_ms = 2000;
unsigned int auto_lock_secs = 30;
String master_pin_sha256 = "";
bool motion_detection = false;
unsigned int upload_interval_ms = 5000;

// Relay Pin (Solenoid)
const int RELAY_PIN = 4;
bool is_locked = true;
unsigned long unlock_time = 0;
bool pulse_active = false;

// Timers
unsigned long last_poll_cmds = 0;
unsigned long last_poll_config = 0;
unsigned long lastClockUpdate = 0;

void lockDoor();
void unlockDoor();
void pulseDoor();
void fetchConfig();
void drawIdleScreen();
void drawMenuScreen();
void updateClockDisplay();
void drawWiFiIndicators(bool forceRedraw = false);

void displayError(String errorMsg) {
  Serial.println("❌ ERROR: " + errorMsg);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawCentreString("⚠️ SYSTEM ERROR", tft.width() / 2, 20, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 70);
  tft.setTextSize(2);
  tft.println(errorMsg);
  
  currentState = STATE_ERROR_DISP;
  errorDisplayTime = millis(); 
}

void printMsg(String msg) {
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200); 
  Serial2.begin(115200, SERIAL_8N1, 16, 17); 

  tft.init();
  tft.setRotation(1);            
  tft.fillScreen(TFT_BLACK);      
  tft.setTextColor(TFT_WHITE, TFT_BLACK); 
  tft.setTextSize(2);             
  
  tft.setCursor(10, 10);
  tft.println("Connecting to WiFi...");

  pinMode(RELAY_PIN, OUTPUT);
  lockDoor();

  nvs.begin("vault", false);
  master_pin_sha256 = nvs.getString("master_pin_sha256", "");
  poll_interval_ms = nvs.getUInt("poll_interval_ms", 2000);
  auto_lock_secs = nvs.getUInt("auto_lock_secs", 30);

  WiFi.begin(ssid, password);
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) { 
    delay(500); 
    Serial.print(".");
    retryCount++;
  }
  
  if(WiFi.status() != WL_CONNECTED) {
    espWifiConnected = false;
    displayError("WiFi Connection\nFailed. Operating\nin offline mode.");
    delay(3000);
  } else {
    espWifiConnected = true;
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    fetchConfig();
  }
  
  drawIdleScreen();
}

String sha256(const String &input) {
  byte hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)input.c_str(), input.length());
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);
  String result = "";
  for (int i = 0; i < 32; i++) {
    if (hash[i] < 0x10) result += "0";
    result += String(hash[i], HEX);
  }
  return result;
}

void lockDoor() {
  digitalWrite(RELAY_PIN, LOW);
  is_locked = true;
  pulse_active = false;
  printMsg("🚪 Door LOCKED");
}

void unlockDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  is_locked = false;
  printMsg("🚪 Door UNLOCKED");
}

void pulseDoor() {
  unlockDoor();
  pulse_active = true;
  unlock_time = millis();
  printMsg("🚪 Door PULSING");
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("ACCESS GRANTED", tft.width()/2, tft.height()/2 - 10, 4);
  delay(2000);
  
  enteredPin = "";
  drawIdleScreen();
}

void drawWiFiIndicators(bool forceRedraw) {
  static bool prevEspState = false;
  static bool prevXiaoState = false;
  if (espWifiConnected == prevEspState && xiaoWifiConnected == prevXiaoState && !forceRedraw) {
    return; 
  }
  prevEspState = espWifiConnected;
  prevXiaoState = xiaoWifiConnected;
  
  int topY = 8;
  int espX = tft.width() - 55;  
  int xiaoX = tft.width() - 25; 
  
  tft.setTextSize(1);
  uint16_t espColor = espWifiConnected ? TFT_GREEN : TFT_RED;
  tft.fillRect(espX, topY, 16, 16, TFT_BLACK); 
  tft.drawCircle(espX + 8, topY + 14, 2, espColor);
  tft.drawCircle(espX + 8, topY + 14, 6, espColor);
  tft.drawCircle(espX + 8, topY + 14, 11, espColor);
  tft.setTextColor(espColor, TFT_BLACK);
  tft.drawString("E", espX - 8, topY + 4);
  
  uint16_t xiaoColor = xiaoWifiConnected ? TFT_GREEN : TFT_RED;
  tft.fillRect(xiaoX, topY, 16, 16, TFT_BLACK); 
  tft.drawCircle(xiaoX + 8, topY + 14, 2, xiaoColor);
  tft.drawCircle(xiaoX + 8, topY + 14, 6, xiaoColor);
  tft.drawCircle(xiaoX + 8, topY + 14, 11, xiaoColor);
  tft.setTextColor(xiaoColor, TFT_BLACK);
  tft.drawString("X", xiaoX - 8, topY + 4);
  tft.setTextSize(2); 
}

void drawIdleScreen() {
  currentState = STATE_IDLE;
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString("SECURE VAULT SYSTEM", tft.width() / 2, 25, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Press any key to enter", tft.width() / 2, tft.height() - 40, 2);
  drawWiFiIndicators(true); 
  updateClockDisplay();
}

void updateClockDisplay() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ return; }
  char timeHourMin[9];
  char timeDate[20];
  strftime(timeHourMin, sizeof(timeHourMin), "%H:%M:%S", &timeinfo);
  strftime(timeDate, sizeof(timeDate), "%A, %B %d", &timeinfo);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString(timeHourMin, tft.width() / 2, tft.height() / 2 - 20, 4);
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.drawCentreString(timeDate, tft.width() / 2, tft.height() / 2 + 20, 2);
}

void drawMenuScreen() {
  currentState = STATE_MENU;
  lastActivityTime = millis();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("A: Face Recognition", 20, 35, 2);
  tft.drawString("Enter PIN & press #:", 20, 75, 2);
  tft.drawRect(20, 105, 200, 35, TFT_BLUE);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("*: Clear/Exit", 20, tft.height() - 40, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(enteredPin, 30, 113, 2);
  drawWiFiIndicators(true); 
}

void fetchConfig() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(SERVER_URL + "/api/v1/esp/config");
  http.addHeader("X-API-Key", CAMERA_API_KEY);
  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      if (doc["data"].is<JsonObject>()) {
        master_pin_sha256 = doc["data"]["master_pin_sha256"].as<String>();
        poll_interval_ms = doc["data"]["poll_interval_ms"].as<unsigned int>();
        auto_lock_secs = doc["data"]["auto_lock_seconds"].as<unsigned int>();
      } else {
        master_pin_sha256 = doc["master_pin_sha256"].as<String>();
        poll_interval_ms = doc["poll_interval_ms"].as<unsigned int>();
        auto_lock_secs = doc["auto_lock_seconds"].as<unsigned int>();
      }
      nvs.putString("master_pin_sha256", master_pin_sha256);
      nvs.putUInt("poll_interval_ms", poll_interval_ms);
      nvs.putUInt("auto_lock_secs", auto_lock_secs);
    }
  } else {
    displayError("Config Sync Fail\nHTTP Code: " + String(code));
  }
  http.end();
}

void ackCommand(int id, bool success) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(SERVER_URL + "/api/v1/esp/commands/" + String(id) + "/ack");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", CAMERA_API_KEY);
  String payload = success ? "{\"success\":true}" : "{\"success\":false}";
  http.POST(payload);
  http.end();
}

void pollCommands() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(SERVER_URL + "/api/v1/esp/commands/pending");
  http.addHeader("X-API-Key", CAMERA_API_KEY);
  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      JsonObject cmd;
      if (doc["data"]["command"].is<JsonObject>()) {
        cmd = doc["data"]["command"];
      } else if (doc["command"].is<JsonObject>()) {
        cmd = doc["command"];
      }
      if (!cmd.isNull()) {
        int id = cmd["id"].as<int>();
        String type = cmd["type"].as<String>();
        if (type == "UNLOCK") { unlockDoor(); } 
        else if (type == "LOCK") { lockDoor(); } 
        else if (type == "PULSE") { pulseDoor(); }
        ackCommand(id, true);
      }
    }
  } else if (code != -11) { 
    displayError("Poll Cmd Failed\nHTTP Code: " + String(code));
  }
  http.end();
}

int authenticatePin(String pin) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_URL + "/api/v1/esp/auth/pin");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", CAMERA_API_KEY);
    String payload = "{\"pin\":\"" + pin + "\"}";
    int code = http.POST(payload);
    if (code > 0) {
      if (code == 200) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        http.end();
        if (doc["success"].as<bool>() && doc["data"]["granted"].as<bool>()) { return 1; }
        return 0; 
      } else {
        displayError("Server Auth Error\nStatus Code: " + String(code));
        http.end();
        return -1; 
      }
    } else {
      displayError("Network Error:\n" + http.errorToString(code));
      http.end();
      return -1; 
    }
  }
  String hashed = sha256(pin);
  if (master_pin_sha256.length() > 0 && hashed == master_pin_sha256) { return 1; }
  return 0;
}

void runCameraCountdown() {
  currentState = STATE_COUNTDOWN;
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawCentreString("LOOK AT THE CAMERA", tft.width() / 2, 30, 2);
  
  // Turn flash early during countdown
  Serial2.println("FLASH_ON"); 
  delay(100);

  tft.setTextSize(5); 
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  tft.drawCentreString("3", tft.width() / 2, tft.height() / 2 - 20, 4);
  delay(1000);
  
  tft.fillRect(tft.width()/2 - 30, tft.height()/2 - 40, 60, 60, TFT_BLACK); 
  tft.drawCentreString("2", tft.width() / 2, tft.height() / 2 - 20, 4);
  delay(1000);
  
  tft.fillRect(tft.width()/2 - 30, tft.height()/2 - 40, 60, 60, TFT_BLACK);
  tft.drawCentreString("1", tft.width() / 2, tft.height() / 2 - 20, 4);
  delay(1000);

  tft.setTextSize(2);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString("PROCESSING FACE...", tft.width() / 2, tft.height() / 2 - 10, 2);

  // Take photo when countdown hits zero
  Serial2.println("FACE_VERIFY"); 
}

void loop() {
  unsigned long now = millis();
  
  espWifiConnected = (WiFi.status() == WL_CONNECTED);
  if (!espWifiConnected) { WiFi.begin(ssid, password); }
  
  if (xiaoWifiConnected && (now - lastXiaoHeartbeat >= xiaoTimeoutThreshold)) {
    xiaoWifiConnected = false;
  }

  if (currentState != STATE_ERROR_DISP && currentState != STATE_COUNTDOWN) {
    drawWiFiIndicators(false);
  }

  if (espWifiConnected) {
    if (now - last_poll_cmds >= poll_interval_ms) {
      last_poll_cmds = now;
      pollCommands();
    }
    if (now - last_poll_config >= 60000) {
      last_poll_config = now;
      fetchConfig();
    }
  }

  if (pulse_active && (now - unlock_time >= (auto_lock_secs * 1000))) { lockDoor(); }

  if (currentState == STATE_ERROR_DISP && (now - errorDisplayTime >= 4000)) { drawIdleScreen(); }

  char key = customKeypad.getKey();
  if (key) {
    if (currentState == STATE_IDLE || currentState == STATE_ERROR_DISP) {
      drawMenuScreen();
    } else if (currentState == STATE_MENU) {
      lastActivityTime = now; 
      
      if (key == 'A') {
        runCameraCountdown(); 
      } 
      else if (key == '*') {
        enteredPin = "";
        drawIdleScreen();
      } 
      else if (key == '#') {
        if (enteredPin.length() > 0) {
          tft.drawString("Verifying PIN...", 20, 150, 2);
          int authResult = authenticatePin(enteredPin);
          if (authResult == 1) { pulseDoor(); } 
          else if (authResult == 0) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("❌ INVALID PIN!       ", 20, 150, 2);
            delay(1500);
            enteredPin = "";
            drawMenuScreen();
          } 
        }
      } 
      else if (key != 'B' && key != 'C' && key != 'D') {
        if (enteredPin.length() < 8) { 
          enteredPin += key;
          String masked = "";
          for(int i=0; i<enteredPin.length(); i++) masked += "*";
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          tft.drawString(masked + " ", 30, 108, 2);
        }
      }
    }
  }

  if (currentState == STATE_MENU && (now - lastActivityTime >= menuTimeout)) {
    enteredPin = "";
    drawIdleScreen();
  }

  if (currentState == STATE_IDLE && (now - lastClockUpdate >= 1000)) {
    lastClockUpdate = now;
    updateClockDisplay();
  }
  
  if (Serial2.available()) {
    String resp = Serial2.readStringUntil('\n');
    resp.trim();
    
    if (resp == "XIAO_WIFI_OK") { xiaoWifiConnected = true; lastXiaoHeartbeat = now; } 
    else if (resp == "XIAO_WIFI_DEAD") { xiaoWifiConnected = false; }
    
    // EXCLUSIVE VERIFICATION BARRIER: Only unlocks door when "FACE_SUCCESS" lands safely
    else if (resp.startsWith("FACE_SUCCESS")) { pulseDoor(); } 
    else if (resp.startsWith("FACE_FAIL")) {
      if (currentState == STATE_COUNTDOWN || currentState == STATE_MENU) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("❌ UNKNOWN FACE!", tft.width()/2, tft.height()/2 - 10, 2);
        delay(2000);
        drawMenuScreen();
      }
    } 
    else if (resp.startsWith("FACE_ERROR") || resp.startsWith("FACE_NET_ERROR") || resp.startsWith("UPLOAD_FAIL")) {
      displayError("XIAO Camera Error:\n" + resp);
    }
  }
}