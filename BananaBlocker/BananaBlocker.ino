// based on the https://github.com/narzan513/bananablocker
#include <Arduino.h> //  https://github.com/espressif/arduino-esp32/tree/master/cores/esp32
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>      // https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi
#include <esp_wifi.h>
#include <WebServer.h> // https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer
#include <ESPmDNS.h>   // https://github.com/espressif/arduino-esp32/tree/master/libraries/ESPmDNS
#endif

#include <DNSServer.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <FS.h>

// Configuration
#define DNS_PORT 53
#define WEB_PORT 80

#if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) 
#undef FLASH_BUTTON
#define FLASH_BUTTON 0  
#endif
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C61) 
#undef FLASH_BUTTON
#define FLASH_BUTTON 9  
#endif
#if defined(CONFIG_IDF_TARGET_ESP32C5)
#undef FLASH_BUTTON
#define FLASH_BUTTON 28  
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S31)
#undef FLASH_BUTTON
#define FLASH_BUTTON 61  
#endif

#define EEPROM_SIZE 512
#define MAX_BLOCKED_DOMAINS 200
#define DOMAIN_LENGTH 32

// Function declarations
void setupAccessPoint();
void setupWebServer();
void handleRoot();
void handleSetup();
void handleSetupPost();
void handleLogin();
void handleLogout();
void handleStats();
void handleToggleBlocking();
void handleGetBlocklist(); 
void handleAddDomain();
void handleRemoveDomain();
void handleFactoryReset();
void handleDNSQuery();
void handleNotFound();
bool isAuthenticated();
bool isDomainBlocked(String domain);
void checkFactoryReset();
void handleDNSRequests();
String extractDomainFromDNS(uint8_t* buffer, int len);
void sendDNSResponse(uint8_t* query, int queryLen, IPAddress clientIP, uint16_t clientPort, IPAddress responseIP);
void forwardDNSQuery(uint8_t* query, int queryLen, IPAddress clientIP, uint16_t clientPort);
void performFactoryReset();
String getSetupPage();
String getDashboardPage();
void saveConfig();
void loadConfig();
bool connectToWiFi();
void setupServices();
String getLoginPage();


// Network Configuration
const char* ap_ssid = "BananaBlocker";
const char* ap_password = "banana123";
const char* device_name = "BananaBlocker";

// Global objects
#if defined(ESP8266)
ESP8266WebServer server(WEB_PORT);
#else
WebServer server(WEB_PORT);
#endif
DNSServer dnsServer;
WiFiUDP udpDNS;
WiFiClient client;

// Configuration structure
struct Config {
  uint32_t magic;  // Magic number to verify valid config
  char wifi_ssid[32];
  char wifi_password[64];
  char admin_password[32];
  bool configured;
  bool blocking_enabled;
  uint32_t blocked_count;
  uint32_t allowed_count;
  uint16_t blocked_domains_count;
  char blocked_domains[MAX_BLOCKED_DOMAINS][DOMAIN_LENGTH];
};

#define CONFIG_MAGIC 0xBABE2024

Config config;
bool isAPMode = false;
unsigned long buttonPressTime = 0;
bool factoryResetTriggered = false;

// Simple session management
bool isLoggedIn = false;
unsigned long sessionExpiry = 0;

// Default blocked domains - comprehensive ad server blocklist (200 domains)
const char* default_blocked_domains[] = {
  // Major ad networks
  "doubleclick.net", "googlesyndication.com", "googleadservices.com", "google-analytics.com",
  "googletagmanager.com", "facebook.com/tr", "amazon-adsystem.com", "adsystem.amazon.com",
  "scorecardresearch.com", "outbrain.com", "taboola.com", "2mdn.net", "adskeeper.co.uk",
  "rubiconproject.com", "criteo.com", "pubmatic.com", "openx.net", "advertising.com",
  "adsafeprotected.com", "moatads.com", "ads.yahoo.com", "adnxs.com", "adsystem.com",
  "adform.net", "bidswitch.net", "bluekai.com", "casalemedia.com", "contextweb.com",
  "demdex.net", "exelator.com", "exponential.com", "adsrvr.org", "agkn.com", "rlcdn.com",
  
  // Tracking and analytics
  "krxd.net", "mathtag.com", "addthis.com", "ads.twitter.com", "analytics.twitter.com",
  "ads.pinterest.com", "ads.linkedin.com", "googletagservices.com", "gstatic.com/ads",
  "amazon.co.uk/ads", "amazon.com/ads", "turn.com", "adsymptotic.com", "adsupplyads.com",
  "quantserve.com", "mookie1.com", "adsupplydsp.com", "adsupplydsp.net", "adsupplydsp.org",
  "adtech.de", "adtechus.com", "chartbeat.com", "comscore.com", "newrelic.com/ads",
  
  // Social media ads/tracking
  "ads.facebook.com", "analytics.facebook.com", "ads.instagram.com", "ads.youtube.com",
  "ads.snapchat.com", "ads.tiktok.com", "ads.reddit.com", "ads.spotify.com",
  "analytics.pinterest.com", "analytics.linkedin.com", "analytics.google.com",
  "marketing.twitter.com", "business.facebook.com/ads", "ads.microsoft.com",
  
  // Content recommendation engines
  "zemanta.com", "sharethrough.com", "revcontent.com", "mgid.com", "content.ad",
  "plista.com", "ligatus.com", "adsupply.com", "contentad.net", "nativo.com",
  "triplelift.com", "adsupply.net", "adpushup.com", "adyoulike.com", "adyoulike.net",
  
  // Video ads
  "fwmrm.net", "videologygroup.com", "spotxchange.com", "brightroll.com", "tubemogul.com",
  "tremormedia.com", "innovid.com", "yieldmo.com", "teads.tv", "outstream.io",
  "smartadserver.com", "adnuntius.com", "aniview.com", "beachfront.com", "videofy.me",
  
  // Mobile ads
  "inmobi.com", "mobfox.com", "millennialmedia.com", "tapjoy.com", "chartboost.com",
  "unity3d.com/ads", "vungle.com", "applovin.com", "supersonic.com", "ironsource.com",
  "adcolony.com", "startapp.com", "fyber.com", "smaato.com", "pubnative.net",
  
  // Retargeting and remarketing
  "adsupply.co", "retargetapp.com", "perfectaudience.com", "adroll.com", "retargeter.com",
  "chango.com", "triggit.com", "magnetic.com", "struq.com", "simpli.fi",
  "steelhouse.com", "fetchback.com", "nextperf.com", "criteo.net", "adsupply.biz",
  
  // Native advertising
  "nativo.net", "sharethrough.net", "adyoulike.com", "engagebdr.com", "adsupply.info",
  "contentad.com", "adclerks.com", "adprime.com", "adsupply.org", "adskeeper.com",
  "mgid.net", "revcontent.net", "zemanta.net", "plista.net", "ligatus.net",
  
  // Ad exchanges
  "nexage.com", "mobclix.com", "admob.com", "mediation.admob.com", "iadsdk.apple.com",
  "ads.yahoo.net", "ads.bing.com", "ads.yandex.com", "ads.baidu.com", "ads.naver.com",
  "doubleclick.com", "adsupply.tv", "adsupply.io", "adsupply.me", "adsupply.us",
  
  // Performance marketing
  "commission-junction.com", "linksynergy.com", "shareasale.com", "clickbank.net",
  "pepperjam.com", "partnerize.com", "impact.com", "tradedoubler.com", "zanox.com",
  "affiliatewindow.com", "webgains.com", "awin.com", "rakuten.com/ads", "skimlinks.com",
  
  // Ad verification
  "doubleverify.com", "integralads.com", "whiteoops.com", "pixalate.com", "adometry.com",
  "adsupply.pro", "adsupply.tech", "adsupply.site", "adsupply.online", "adsupply.digital",
  "moat.com", "adsupply.app", "adsupply.web", "adsupply.cloud", "adsupply.space",
  
  // Programmatic advertising
  "thetradedesk.com", "mediamath.com", "turn.net", "dataxu.com", "adobe.com/ads",
  "salesforce.com/ads", "amobee.com", "adsupply.ai", "adsupply.ml", "adsupply.tech",
  "displayio.com", "adsupply.live", "adsupply.news", "adsupply.world", "adsupply.global"
};

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to connect
  Serial.println("\n=== BananaBlocker Starting ===");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Flash chip size: %d bytes\n", ESP.getFlashChipSize());
  
  // Initialize flash button
  pinMode(FLASH_BUTTON, INPUT_PULLUP);
  
  // Initialize EEPROM and file system
  Serial.println("Initializing EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("Initializing LittleFS...");  
#if defined(ESP8266)
  bool LittleFS_ok = LittleFS.begin();
#else
  bool LittleFS_ok = LittleFS.begin(true); // FORMAT_LITTLEFS_IF_FAILED
#endif
  Serial.printf("LittleFS initialization: %s\n", LittleFS_ok ? "SUCCESS" : "FAILED");
  
  // Load configuration
  Serial.println("Loading configuration...");
  loadConfig();
  Serial.println("Configuration loaded successfully");
  
  // Check if device is configured
  if (!config.configured) {
    Serial.println("First boot - starting AP mode");
    setupAccessPoint();
  } else {
    Serial.printf("Configuration found - connecting to WiFi: %s\n", config.wifi_ssid);
    Serial.printf("Current WiFi mode before connection: %d\n", WiFi.getMode());
    
    if (!connectToWiFi()) {
      Serial.println("WiFi connection failed - starting AP mode for reconfiguration");
      // Reset configuration if WiFi fails
      config.configured = false;
      saveConfig();
      setupAccessPoint();
    } else {
      Serial.println("WiFi connected successfully - setting up services");
      setupServices();
      Serial.printf("Final WiFi mode: %d (1=STA, 2=AP, 3=STA+AP)\n", WiFi.getMode());
    }
  }
  
  Serial.println("Setting up web server...");
  setupWebServer();
  Serial.println("Web server setup complete");
  
  Serial.println("BananaBlocker ready!");
  Serial.printf("Final IP address: %s\n", isAPMode ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str());
}

void loop() {
  // Handle web server
  server.handleClient();
  
  // Handle DNS requests (captive portal in AP mode)
  if (isAPMode) {
    dnsServer.processNextRequest();
  } else {
    // Handle DNS blocking in station mode
    handleDNSRequests();
  }
  
  checkFactoryReset(); // Check for factory reset button
  #if defined(ESP8266)
  if (!isAPMode) {
    MDNS.update(); // Handle mDNS
  }
  #endif 
  delay(1);
}

void loadConfig() {
  Serial.println("Loading configuration from LittleFS...");
  
  // Try to load from file first
  File configFile = LittleFS.open("/config.dat", "r");
  if (configFile && configFile.size() == sizeof(config)) {
    configFile.readBytes((char*)&config, sizeof(config));
    configFile.close();
    
    Serial.printf("Magic number: 0x%08X (expected: 0x%08X)\n", config.magic, CONFIG_MAGIC);
    Serial.printf("Raw configured flag: %d (should be 1 for true)\n", config.configured);
    Serial.printf("Raw SSID: '%s'\n", config.wifi_ssid);
    Serial.printf("SSID length: %d\n", strlen(config.wifi_ssid));
    
    // Check if configuration is valid
    if (config.magic == CONFIG_MAGIC && config.configured == true && strlen(config.wifi_ssid) > 0) {
      Serial.println("Valid configuration found in LittleFS!");
      Serial.printf("WiFi SSID: %s\n", config.wifi_ssid);
      Serial.printf("WiFi Password: %s\n", strlen(config.wifi_password) > 0 ? "***SET***" : "***EMPTY***");
      Serial.printf("Admin Password: %s\n", config.admin_password);
      Serial.printf("Configured: %s\n", config.configured ? "YES" : "NO");
      Serial.printf("Loaded %d blocked domains\n", config.blocked_domains_count);
      Serial.printf("Blocked: %d, Allowed: %d\n", config.blocked_count, config.allowed_count);
      return;
    }
  }
  
  Serial.println("No valid configuration found, initializing defaults");
  
  // Initialize with defaults
  memset(&config, 0, sizeof(config));
  config.magic = CONFIG_MAGIC;
  // No default admin password - must be set during setup
  config.configured = false;
  config.blocking_enabled = true;
  config.blocked_count = 0;
  config.allowed_count = 0;
  config.blocked_domains_count = 0;
  
  // Add default blocked domains
  int default_count = sizeof(default_blocked_domains) / sizeof(default_blocked_domains[0]);
  int max_domains = (default_count < MAX_BLOCKED_DOMAINS) ? default_count : MAX_BLOCKED_DOMAINS;
  for (int i = 0; i < max_domains; i++) {
    strncpy(config.blocked_domains[i], default_blocked_domains[i], DOMAIN_LENGTH - 1);
    config.blocked_domains[i][DOMAIN_LENGTH - 1] = '\0';
    config.blocked_domains_count++;
  }
  
  saveConfig();
  Serial.printf("Loaded %d blocked domains\n", config.blocked_domains_count);
  Serial.printf("Blocked: %d, Allowed: %d\n", config.blocked_count, config.allowed_count);
}

void saveConfig() {
  Serial.println("Saving configuration to LittleFS...");
  config.magic = CONFIG_MAGIC; // Ensure magic number is set
  
  File configFile = LittleFS.open("/config.dat", "w");
  if (configFile) {
    size_t written = configFile.write((uint8_t*)&config, sizeof(config));
    configFile.close();
    
    Serial.printf("LittleFS write result: %s (%d bytes)\n", written == sizeof(config) ? "SUCCESS" : "FAILED", written);
    
    if (written == sizeof(config)) {
      Serial.println("Configuration saved to LittleFS");
      Serial.printf("Saved Magic: 0x%08X\n", config.magic);
      Serial.printf("Saved SSID: '%s'\n", config.wifi_ssid);
      Serial.printf("Saved Password: '%s'\n", strlen(config.wifi_password) > 0 ? "***SET***" : "***EMPTY***");
      Serial.printf("Configured flag: %s\n", config.configured ? "true" : "false");
    } else {
      Serial.println("Failed to save configuration to LittleFS");
    }
  } else {
    Serial.println("Failed to open config file for writing");
  }
}

bool connectToWiFi() {
  Serial.printf("Attempting to connect to WiFi: %s\n", config.wifi_ssid);
  
  // Properly disconnect from AP mode first
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  delay(1000);
  
  // Set to station mode only
  WiFi.mode(WIFI_STA);
  delay(500);
  
  WiFi.begin(config.wifi_ssid, config.wifi_password);
  
  Serial.print("Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  WiFi.setSleep(false);
#if not defined(ESP8266)
  esp_wifi_set_ps(WIFI_PS_NONE); // Esp32 enters the power saving mode by default,
#endif
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("================================");
    Serial.println("🍌 BananaBlocker WiFi Connected!");
    Serial.println("================================");
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("WiFi Mode: %d (1=STA, 2=AP, 3=STA+AP)\n", WiFi.getMode());
    Serial.println("================================");
    Serial.printf("🌐 Open browser and go to: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("🔗 Or try: http://bananablock.local\n");
    Serial.println("================================");
    return true;
  }
  
  Serial.println("");
  Serial.printf("WiFi connection failed after %d attempts\n", attempts);
  Serial.printf("WiFi Status: %d\n", WiFi.status());
  Serial.println("WiFi Status meanings:");
  Serial.println("0=WL_IDLE_STATUS, 1=WL_NO_SSID_AVAIL, 2=WL_SCAN_COMPLETED");
  Serial.println("3=WL_CONNECTED, 4=WL_CONNECT_FAILED, 5=WL_CONNECTION_LOST, 6=WL_DISCONNECTED");
  return false;
}

void setupAccessPoint() {
  isAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.println("Access Point started");
  Serial.printf("SSID: %s\n", ap_ssid);
  Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  // Setup captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

void setupServices() {
  isAPMode = false;
  
  // Ensure AP is completely disabled
  WiFi.softAPdisconnect(true);
  Serial.println("Access Point disabled");
  
  // Setup UDP DNS server for ad blocking
  udpDNS.begin(DNS_PORT);
  Serial.printf("DNS server started on port %d\n", DNS_PORT);
  Serial.printf("Set your router's DNS to: %s\n", WiFi.localIP().toString().c_str());
  Serial.println("BananaBlocker DNS filtering is now active!");
  
  // Setup mDNS
  if (MDNS.begin(device_name)) {
    Serial.println("mDNS responder started");
    MDNS.addService("http", "tcp", WEB_PORT);
  }
}

void setupWebServer() {
  Serial.println("Setting up web server routes...");
  
  // API endpoints - register root handler FIRST, before serveStatic
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, handleSetup);
  server.on("/setup", HTTP_POST, handleSetupPost);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/api/stats", HTTP_GET, handleStats);
  server.on("/api/blocklist", HTTP_GET, handleGetBlocklist);
  server.on("/api/blocklist", HTTP_POST, handleAddDomain);
  server.on("/api/blocklist", HTTP_DELETE, handleRemoveDomain);
  server.on("/api/toggle", HTTP_POST, handleToggleBlocking);
  server.on("/api/reset", HTTP_POST, handleFactoryReset);
  
  // 404 handler
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started");
  Serial.println("Registered routes:");
  Serial.println("  GET  /");
  Serial.println("  GET  /setup");
  Serial.println("  POST /setup");
  Serial.println("  POST /login");
  Serial.println("  GET  /logout");
  Serial.println("  GET  /api/stats");
  Serial.println("  GET  /api/blocklist");
  Serial.println("  POST /api/blocklist");
  Serial.println("  DELETE /api/blocklist");
  Serial.println("  POST /api/reset");
}

void handleRoot() {
  Serial.printf("Root request from: %s\n", server.client().remoteIP().toString().c_str());
  Serial.printf("isAPMode: %s\n", isAPMode ? "true" : "false");
  
  if (isAPMode) {
    // Always serve setup page in AP mode
    Serial.println("Serving setup page (AP mode)");
    server.send(200, "text/html", getSetupPage());
    return;
  }
  
  // Check if user is authenticated
  if (!isAuthenticated()) {
    Serial.println("User not authenticated, serving login page");
    server.send(200, "text/html", getLoginPage());
    return;
  }
  
  Serial.println("User authenticated, serving dashboard");
  server.send(200, "text/html", getDashboardPage());
}

void handleSetup() {
  // Always serve setup page in AP mode, redirect if not in AP mode
  if (!isAPMode) {
    server.send(404, "text/plain", "Setup not available");
    return;
  }
  
  // Add headers to help with captive portal detection
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(200, "text/html", getSetupPage());
}



void handleSetupPost() {
  if (!isAPMode) {
    server.send(404, "text/plain", "Setup not available");
    return;
  }
  
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String adminPass = server.arg("admin_password");
  
  if (ssid.length() > 0 && adminPass.length() > 0) {
    // Save configuration
    strcpy(config.wifi_ssid, ssid.c_str());
    strcpy(config.wifi_password, password.c_str());
    strcpy(config.admin_password, adminPass.c_str());
    
    config.configured = true;
    saveConfig();
    
    Serial.printf("Configuration saved. SSID: %s\n", ssid.c_str());
    Serial.printf("Form data - SSID: '%s', Password length: %d\n", ssid.c_str(), password.length());
    
    // Send response
    server.send(200, "text/html", 
      "<html><body style='font-family: Arial; text-align: center; margin-top: 50px;'>"
      "<div style='max-width: 500px; margin: 0 auto; padding: 20px; border-radius: 10px; background: #f5f5f5;'>"
      "<h2 style='color: #FFE135;'>BANANA Configuration Saved!</h2>"
      "<p>BananaBlocker is restarting and connecting to: <strong>" + ssid + "</strong></p>"
      "<p><strong>How to find your BananaBlocker after restart:</strong></p>"
      "<ol style='text-align: left;'>"
      "<li><strong>Serial Monitor:</strong> Watch Arduino IDE serial monitor for IP address</li>"
      "<li><strong>Router Admin:</strong> Check connected devices for 'BananaBlocker'</li>"
      "<li><strong>Network Scanner:</strong> Use apps like 'Fing' to scan your network</li>"
      "<li><strong>mDNS:</strong> Try <a href='http://bananablock.local' style='color: #ff6b35;'>http://bananablock.local</a></li>"
      "<li><strong>IP Range:</strong> Common ranges: 192.168.1.x or 192.168.0.x</li>"
      "</ol>"
      "<p style='background: #fff3cd; padding: 10px; border-radius: 5px; font-size: 14px;'>"
      "💡 <strong>Tip:</strong> Keep this browser tab open and try the mDNS link in 30 seconds!"
      "</p>"
      "</div>"
      "</body></html>");
    
    // Give time for response to be sent and EEPROM to settle
    delay(2000);
    
    Serial.println("Restarting device...");
    ESP.restart();
  } else {
    String errorMsg = "";
    if (ssid.length() == 0) {
      errorMsg += "WiFi network name (SSID) is required<br>";
    }
    if (adminPass.length() == 0) {
      errorMsg += "Admin password is required<br>";
    }
    
    server.send(400, "text/html", 
      "<html><body style='font-family: Arial; text-align: center; margin-top: 50px;'>"
      "<h2 style='color: #d32f2f;'>Setup Error</h2>"
      "<p>" + errorMsg + "</p>"
      "<a href='/setup' style='color: #ff6b35;'>Go back to setup</a>"
      "</body></html>");
  }
}

void handleLogin() {
  String password = server.arg("password");
  
  Serial.printf("Login attempt from: %s\n", server.client().remoteIP().toString().c_str());
  Serial.printf("Provided password: '%s'\n", password.c_str());
  Serial.printf("Expected password: '%s'\n", config.admin_password);
  Serial.printf("Password match: %s\n", password == config.admin_password ? "YES" : "NO");
  
  if (password == config.admin_password) {
    // Set global login state and session expiry (24 hours)
    isLoggedIn = true;
    sessionExpiry = millis() + (24UL * 60UL * 60UL * 1000UL); // 24 hours in milliseconds
    
    Serial.printf("Login successful from IP: %s\n", server.client().remoteIP().toString().c_str());
    Serial.printf("Session expires at: %lu\n", sessionExpiry);
    
    server.send(200, "application/json", "{\"success\": true}");
  } else {
    Serial.println("Login failed - invalid password");
    server.send(401, "application/json", "{\"success\": false, \"error\": \"Invalid password\"}");
  }
}

void handleLogout() {
  // Clear the authenticated session
  isLoggedIn = false;
  sessionExpiry = 0;
  Serial.println("User logged out, session cleared");
  
  server.send(200, "text/html", getLoginPage());
}

void handleStats() {
  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\": \"Unauthorized\"}");
    return;
  }
  
  String json = "{";
  json += "\"blocked\": " + String(config.blocked_count) + ",";
  json += "\"allowed\": " + String(config.allowed_count) + ",";
  json += "\"total_domains\": " + String(config.blocked_domains_count) + ",";
  json += "\"blocking_enabled\": " + String(config.blocking_enabled ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleToggleBlocking() {
  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\": \"Unauthorized\"}");
    return;
  }
  
  config.blocking_enabled = !config.blocking_enabled;
  saveConfig();
  
  String json = "{";
  json += "\"success\": true,";
  json += "\"blocking_enabled\": " + String(config.blocking_enabled ? "true" : "false");
  json += "}";
  
  Serial.printf("Blocking toggled: %s\n", config.blocking_enabled ? "ON" : "OFF");
  server.send(200, "application/json", json);
}

void handleGetBlocklist() {
  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\": \"Unauthorized\"}");
    return;
  }
  
  String json = "[";
  for (int i = 0; i < config.blocked_domains_count; i++) {
    if (i > 0) json += ",";
    json += "\"" + String(config.blocked_domains[i]) + "\"";
  }
  json += "]";
  
  server.send(200, "application/json", json);
}

void handleAddDomain() {
  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\": \"Unauthorized\"}");
    return;
  }
  
  String domain = server.arg("domain");
  domain.toLowerCase();
  
  if (domain.length() == 0) {
    server.send(400, "application/json", "{\"error\": \"Domain required\"}");
    return;
  }
  
  // No limit - user can add unlimited domains
  
  // Check if domain already exists
  for (int i = 0; i < config.blocked_domains_count; i++) {
    if (strcmp(config.blocked_domains[i], domain.c_str()) == 0) {
      server.send(400, "application/json", "{\"error\": \"Domain already exists\"}");
      return;
    }
  }
  
  // Add domain
  strcpy(config.blocked_domains[config.blocked_domains_count], domain.c_str());
  config.blocked_domains_count++;
  saveConfig();
  
  server.send(200, "application/json", "{\"success\": true}");
}

void handleRemoveDomain() {
  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\": \"Unauthorized\"}");
    return;
  }
  
  String domain = server.arg("domain");
  
  // Find and remove domain
  for (int i = 0; i < config.blocked_domains_count; i++) {
    if (strcmp(config.blocked_domains[i], domain.c_str()) == 0) {
      // Shift remaining domains
      for (int j = i; j < config.blocked_domains_count - 1; j++) {
        strcpy(config.blocked_domains[j], config.blocked_domains[j + 1]);
      }
      config.blocked_domains_count--;
      saveConfig();
      server.send(200, "application/json", "{\"success\": true}");
      return;
    }
  }
  
  server.send(404, "application/json", "{\"error\": \"Domain not found\"}");
}

void handleFactoryReset() {
  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\": \"Unauthorized\"}");
    return;
  }
  
  performFactoryReset();
  server.send(200, "application/json", "{\"success\": true}");
}

void handleDNSQuery() {
  String domain = server.arg("domain");
  
  if (isDomainBlocked(domain)) {
    config.blocked_count++;
    saveConfig();
    server.send(200, "text/plain", "127.0.0.1"); // Block by returning localhost
  } else {
    config.allowed_count++;
    saveConfig();
    server.send(404, "text/plain", "Not blocked"); // Allow through
  }
}

void handleNotFound() {
  Serial.printf("404 request: %s from %s\n", server.uri().c_str(), server.client().remoteIP().toString().c_str());
  
  if (isAPMode) {
    // Captive portal - serve setup page directly for any unknown request
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(200, "text/html", getSetupPage());
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

bool isAuthenticated() {
  unsigned long currentTime = millis();
  
  Serial.printf("Login status: %s\n", isLoggedIn ? "LOGGED_IN" : "NOT_LOGGED_IN");
  Serial.printf("Session expires in: %lu ms\n", sessionExpiry > currentTime ? sessionExpiry - currentTime : 0);
  
  // Check if logged in and session hasn't expired
  bool authenticated = isLoggedIn && (currentTime < sessionExpiry);
  
  Serial.printf("Authentication result: %s\n", authenticated ? "AUTHENTICATED" : "NOT_AUTHENTICATED");
  return authenticated;
}

bool isDomainBlocked(String domain) {
  // If blocking is disabled, never block anything
  if (!config.blocking_enabled) {
    return false;
  }
  
  domain.toLowerCase();
  
  for (int i = 0; i < config.blocked_domains_count; i++) {
    if (domain.indexOf(config.blocked_domains[i]) != -1) {
      Serial.printf("Blocked: %s (matches %s)\n", domain.c_str(), config.blocked_domains[i]);
      return true;
    }
  }
  
  return false;
}

void checkFactoryReset() {
  if (digitalRead(FLASH_BUTTON) == LOW) {
    if (buttonPressTime == 0) {
      buttonPressTime = millis();
    } else if (millis() - buttonPressTime > 4000 && !factoryResetTriggered) {
      factoryResetTriggered = true;
      Serial.println("Factory reset triggered!");
      performFactoryReset();
    }
  } else {
    buttonPressTime = 0;
    factoryResetTriggered = false;
  }
}

void handleDNSRequests() {
  int packetSize = udpDNS.parsePacket();
  if (packetSize == 0) return;
  
  IPAddress clientIP = udpDNS.remoteIP();
  uint16_t clientPort = udpDNS.remotePort();
  
  // Read the DNS query
  uint8_t buffer[512];
  int len = udpDNS.read(buffer, sizeof(buffer));
  
  if (len < 12) return; // Invalid DNS packet
  
  // Extract domain name from DNS query
  String domain = extractDomainFromDNS(buffer, len);
  
  if (domain.length() > 0) {
    Serial.printf("DNS Query: %s from %s\n", domain.c_str(), clientIP.toString().c_str());
    
    if (isDomainBlocked(domain)) {
      // Block the domain by returning our own IP (redirect to block page)
      sendDNSResponse(buffer, len, clientIP, clientPort, WiFi.localIP());
      config.blocked_count++;
      Serial.printf("BLOCKED: %s\n", domain.c_str());
    } else {
      // Forward to upstream DNS (Google DNS)
      forwardDNSQuery(buffer, len, clientIP, clientPort);
      config.allowed_count++;
    }
  }
}

String extractDomainFromDNS(uint8_t* buffer, int len) {
  if (len < 12) return "";
  
  String domain = "";
  int pos = 12; // Start after DNS header
  
  while (pos < len && buffer[pos] != 0) {
    int labelLen = buffer[pos++];
    if (labelLen == 0) break;
    
    if (domain.length() > 0) domain += ".";
    
    for (int i = 0; i < labelLen && pos < len; i++) {
      domain += (char)buffer[pos++];
    }
  }
  
  return domain;
}

void sendDNSResponse(uint8_t* query, int queryLen, IPAddress clientIP, uint16_t clientPort, IPAddress responseIP) {
  uint8_t response[512];
  
  // Copy query as base for response
  memcpy(response, query, queryLen);
  
  // Set response flags
  response[2] = 0x81; // Response + Recursion Available
  response[3] = 0x80; // No error
  
  // Add answer section
  int answerPos = queryLen;
  
  // Answer: Name (pointer to question)
  response[answerPos++] = 0xC0;
  response[answerPos++] = 0x0C;
  
  // Type A
  response[answerPos++] = 0x00;
  response[answerPos++] = 0x01;
  
  // Class IN
  response[answerPos++] = 0x00;
  response[answerPos++] = 0x01;
  
  // TTL (60 seconds)
  response[answerPos++] = 0x00;
  response[answerPos++] = 0x00;
  response[answerPos++] = 0x00;
  response[answerPos++] = 0x3C;
  
  // Data length (4 bytes for IPv4)
  response[answerPos++] = 0x00;
  response[answerPos++] = 0x04;
  
  // IP address
  response[answerPos++] = responseIP[0];
  response[answerPos++] = responseIP[1];
  response[answerPos++] = responseIP[2];
  response[answerPos++] = responseIP[3];
  
  // Update answer count
  response[6] = 0x00;
  response[7] = 0x01;
  
  // Send response
  udpDNS.beginPacket(clientIP, clientPort);
  udpDNS.write(response, answerPos);
  udpDNS.endPacket();
}

void forwardDNSQuery(uint8_t* query, int queryLen, IPAddress clientIP, uint16_t clientPort) {
  // Forward to Google DNS (8.8.8.8)
  WiFiUDP forwardUDP;
  forwardUDP.begin(0); // Use random port
  
  IPAddress googleDNS(8, 8, 8, 8);
  forwardUDP.beginPacket(googleDNS, 53);
  forwardUDP.write(query, queryLen);
  forwardUDP.endPacket();
  
  // Wait for response
  unsigned long timeout = millis() + 1000; // 1 second timeout
  while (millis() < timeout) {
    int packetSize = forwardUDP.parsePacket();
    if (packetSize > 0) {
      uint8_t response[512];
      int responseLen = forwardUDP.read(response, sizeof(response));
      
      // Forward response back to client
      udpDNS.beginPacket(clientIP, clientPort);
      udpDNS.write(response, responseLen);
      udpDNS.endPacket();
      break;
    }
    delay(1);
  }
  
  forwardUDP.stop();
}

void performFactoryReset() {
  Serial.println("Performing factory reset...");
  
  // Clear EEPROM
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  
  // Clear LittleFS
  LittleFS.format();
  
  Serial.println("Factory reset complete. Restarting...");
  delay(1000);
  ESP.restart();
}

// HTML Pages
String getSetupPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>BananaBlocker Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 40px; background: #f5f5f5; }
        .container { max-width: 500px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #B8860B; text-align: center; margin-bottom: 30px; }
        .logo { text-align: center; font-size: 3em; margin-bottom: 10px; color: #B8860B; font-weight: bold; }
        input, select { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }
        button { width: 100%; padding: 12px; background: #FFE135; color: #333; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; font-weight: bold; }
        button:hover { background: #F5D920; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">BANANA</div>
        <h1>BananaBlocker Setup</h1>
        <p style="text-align: center; color: #666;">Welcome! Let's get your BananaBlocker connected to your network.</p>
        
        <form method="post" action="/setup">
            <div class="form-group">
                <label>WiFi Network Name (SSID):</label>
                <input type="text" name="ssid" id="ssid" placeholder="Enter your WiFi network name" required>
            </div>
            
            <div class="form-group">
                <label>WiFi Password:</label>
                <input type="password" name="password" id="password">
            </div>
            
            <div class="form-group">
                <label>Admin Password (required):</label>
                <input type="password" name="admin_password" placeholder="Create a secure admin password" required>
            </div>
            
            <button type="submit">Connect to Network</button>
        </form>
    </div>
    

</body>
</html>
)rawliteral";
}

String getLoginPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>BananaBlocker Admin</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 0; background: linear-gradient(135deg, #FFE135, #F5D920); min-height: 100vh; display: flex; align-items: center; justify-content: center; }
        .login-container { background: white; padding: 40px; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); text-align: center; min-width: 300px; }
        .logo { font-size: 4em; margin-bottom: 10px; color: #B8860B; font-weight: bold; }
        h1 { color: #333; margin-bottom: 30px; }
        input { width: 100%; padding: 15px; margin: 10px 0; border: 1px solid #ddd; border-radius: 8px; box-sizing: border-box; font-size: 16px; }
        button { width: 100%; padding: 15px; background: #FFE135; color: #333; border: none; border-radius: 8px; cursor: pointer; font-size: 16px; margin-top: 10px; font-weight: bold; }
        button:hover { background: #F5D920; }
        .error { color: #d32f2f; margin-top: 10px; }
        .stats { background: #f5f5f5; padding: 15px; border-radius: 8px; margin-bottom: 20px; }
        .stats div { margin: 5px 0; }
    </style>
</head>
<body>
    <div class="login-container">
        <h1>BananaBlocker</h1>
        

        
        <form onsubmit="login(event)">
            <input type="password" id="password" placeholder="Admin Password" required>
            <button type="submit">Login</button>
        </form>
        <div id="error" class="error"></div>
    </div>
    
    <script>

        
        function login(event) {
            event.preventDefault();
            
            var password = document.getElementById('password').value;
            var formData = new FormData();
            formData.append('password', password);
            
            fetch('/login', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    // Simply redirect to home page - server will remember the IP
                    window.location.href = '/';
                } else {
                    document.getElementById('error').textContent = 'Invalid password';
                }
            })
            .catch(error => {
                document.getElementById('error').textContent = 'Login failed';
            });
        }
    </script>
</body>
</html>
)rawliteral";
}

String getDashboardPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>BananaBlocker Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 0; background: #f5f5f5; }
        .header { background: linear-gradient(135deg, #FFE135, #F5D920); color: #333; padding: 20px; text-align: center; }
        .logo { font-size: 2em; display: inline-block; margin-right: 10px; color: #B8860B; font-weight: bold; }
        .logout { float: right; background: rgba(0,0,0,0.1); border: none; color: #333; padding: 8px 15px; border-radius: 5px; cursor: pointer; font-weight: bold; margin-left: 10px; }
        .logout:hover { background: rgba(0,0,0,0.2); }
        .toggle-btn { float: right; border: none; color: #333; padding: 8px 15px; border-radius: 5px; cursor: pointer; font-weight: bold; }
        .toggle-on { background: #4caf50; }
        .toggle-off { background: #f44336; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; margin-bottom: 30px; }
        .stat-card { background: white; padding: 25px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); text-align: center; }
        .stat-number { font-size: 2.5em; font-weight: bold; margin-bottom: 10px; }
        .blocked { color: #d32f2f; }
        .allowed { color: #4caf50; }
        .domains { color: #B8860B; }
        .section { background: white; padding: 25px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); margin-bottom: 20px; }
        .section h2 { color: #333; margin-top: 0; }
        .add-domain { display: flex; gap: 10px; margin-bottom: 20px; }
        .add-domain input { flex: 1; padding: 12px; border: 1px solid #ddd; border-radius: 5px; }
        .add-domain button { padding: 12px 20px; background: #FFE135; color: #333; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; }
        .add-domain button:hover { background: #F5D920; }
        .domain-list { max-height: 300px; overflow-y: auto; }
        .domain-item { display: flex; justify-content: space-between; align-items: center; padding: 10px; border-bottom: 1px solid #eee; }
        .domain-item:last-child { border-bottom: none; }
        .remove-btn { background: #d32f2f; color: white; border: none; padding: 5px 10px; border-radius: 3px; cursor: pointer; font-size: 12px; }
        .remove-btn:hover { background: #b71c1c; }
        .reset-btn { background: #d32f2f; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; }
        .reset-btn:hover { background: #b71c1c; }
    </style>
</head>
<body>
    <div class="header">
        <button class="logout" onclick="logout()">Logout</button>
        <button id="toggle-btn" class="toggle-btn toggle-on" onclick="toggleBlocking()">ON</button>
        <h1 style="display: inline-block; margin: 0;">BananaBlocker Dashboard</h1>
    </div>
    
    <div class="container">
        <div class="stats-grid">
            <div class="stat-card">
                <div class="stat-number blocked" id="blocked-count">-</div>
                <div>Requests Blocked</div>
            </div>
            <div class="stat-card">
                <div class="stat-number allowed" id="allowed-count">-</div>
                <div>Requests Allowed</div>
            </div>
            <div class="stat-card">
                <div class="stat-number domains" id="domains-count">-</div>
                <div>Blocked Domains</div>
            </div>
        </div>
        
        <div class="section">
            <h2>Manage Blocked Domains</h2>
            <div class="add-domain">
                <input type="text" id="new-domain" placeholder="Enter domain to block (e.g., ads.example.com)">
                <button onclick="addDomain()">Add Domain</button>
            </div>
            <div class="domain-list" id="domain-list">
                Loading domains...
            </div>
        </div>
        
        <div class="section">
            <h2>System Settings</h2>
            <button class="reset-btn" onclick="factoryReset()">Factory Reset</button>
            <p style="color: #666; margin-top: 10px;">
                This will erase all settings and return to setup mode. You can also press and hold the flash button for 4 seconds.
            </p>
        </div>
    </div>
    
    <script>
                 function loadStats() {
             fetch('/api/stats')
                 .then(response => response.json())
                 .then(data => {
                     document.getElementById('blocked-count').textContent = data.blocked || 0;
                     document.getElementById('allowed-count').textContent = data.allowed || 0;
                     document.getElementById('domains-count').textContent = data.total_domains || 0;
                     
                     // Update toggle button state
                     const toggleBtn = document.getElementById('toggle-btn');
                     if (data.blocking_enabled) {
                         toggleBtn.textContent = 'ON';
                         toggleBtn.className = 'toggle-btn toggle-on';
                     } else {
                         toggleBtn.textContent = 'OFF';
                         toggleBtn.className = 'toggle-btn toggle-off';
                     }
                 })
                 .catch(error => console.error('Error loading stats:', error));
         }
        
        function loadDomains() {
            fetch('/api/blocklist')
                .then(response => response.json())
                .then(domains => {
                    var list = document.getElementById('domain-list');
                    if (domains.length === 0) {
                        list.innerHTML = '<p style="text-align: center; color: #666;">No blocked domains configured</p>';
                        return;
                    }
                    
                    var html = '';
                    for(var i = 0; i < domains.length; i++) {
                        html += '<div class="domain-item">' +
                               '<span>' + domains[i] + '</span>' +
                               '<button class="remove-btn" onclick="removeDomain(\'' + domains[i] + '\')">Remove</button>' +
                               '</div>';
                    }
                    list.innerHTML = html;
                })
                .catch(error => {
                    console.error('Error loading domains:', error);
                    document.getElementById('domain-list').innerHTML = 'Error loading domains';
                });
        }
        
        function addDomain() {
            var input = document.getElementById('new-domain');
            var domain = input.value.trim();
            
            if (!domain) {
                alert('Please enter a domain');
                return;
            }
            
            var formData = new FormData();
            formData.append('domain', domain);
            
            fetch('/api/blocklist', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    input.value = '';
                    loadDomains();
                    loadStats();
                } else {
                    alert(data.error || 'Failed to add domain');
                }
            })
            .catch(error => {
                console.error('Error adding domain:', error);
                alert('Failed to add domain');
            });
        }
        
        function removeDomain(domain) {
            if (!confirm('Remove ' + domain + ' from blocklist?')) return;
            
            fetch('/api/blocklist?domain=' + encodeURIComponent(domain), {
                method: 'DELETE'
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    loadDomains();
                    loadStats();
                } else {
                    alert(data.error || 'Failed to remove domain');
                }
            })
            .catch(error => {
                console.error('Error removing domain:', error);
                alert('Failed to remove domain');
            });
        }
        
        function factoryReset() {
            if (!confirm('This will erase all settings and restart in setup mode. Continue?')) return;
            
            fetch('/api/reset', { method: 'POST' })
                .then(() => {
                    alert('Factory reset initiated. Device will restart in setup mode.');
                    setTimeout(() => window.location.href = '/', 3000);
                })
                .catch(error => console.error('Error during reset:', error));
        }
        
        function toggleBlocking() {
            fetch('/api/toggle', { method: 'POST' })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        loadStats(); // Refresh to show new state
                    }
                })
                .catch(error => console.error('Error toggling blocking:', error));
        }
        
        function logout() {
            window.location.href = '/logout';
        }
        
        setInterval(loadStats, 30000);
        
        loadStats();
        loadDomains();
        
        document.getElementById('new-domain').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                addDomain();
            }
        });
    </script>
</body>
</html>
)rawliteral";
}
