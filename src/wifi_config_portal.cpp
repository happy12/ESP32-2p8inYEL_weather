#include "wifi_config_portal.h"

// Wraps a WiFiClient stream so that read() blocks (with yields) until a byte
// arrives or the connection closes / timeout expires.  This prevents ArduinoJson
// from treating the gaps between TLS decrypt records as EOF (IncompleteInput).
class BlockingStream : public Stream {
public:
    BlockingStream(WiFiClient& s, unsigned long timeoutMs)
        : _s(s), _deadline(millis() + timeoutMs) {}
    int available() override { return _s.available(); }
    int peek()      override { return _s.peek(); }
    size_t write(uint8_t) override { return 0; }
    int read() override {
        while (!_s.available()) {
            if (millis() > _deadline || !_s.connected()) return -1;
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        return _s.read();
    }
private:
    WiFiClient& _s;
    unsigned long _deadline;
};


#include <AsyncTCP.h>
#include <WiFiUdp.h>//to handle some disconnect events shit
#include <ESPmDNS.h> //to setup custom name instead of using local IP address, once the wifi is setup
#include <Preferences.h>//manages preference files
#include <LittleFS.h>//LittleFS file system
#include <ArduinoJson.h>
#include <ping/ping_sock.h> // Native ESP-IDF Ping headers
#include <lwip/inet.h>// Native ESP-IDF Ping headers
#include <lwip/netdb.h>// Native ESP-IDF Ping headers
#include <lwip/sockets.h>// Native ESP-IDF Ping headers

#define MAX_SSID_LIST 20
#define WIFI_CONNECTED_BIT BIT0
Preferences preferences;
DNSServer dnsServer;
static AsyncWebServer server(80);
WiFiUDP udp;
static bool udpStarted = false;
const uint16_t localPort = 42103;
static EventGroupHandle_t wifiEventGroup = nullptr;
static TimerHandle_t saveTimer = nullptr;
char mdnsHostname[32]; //will contain the mdns hostname

void setWifiHostname(const char* name) {
    strncpy(mdnsHostname, name, sizeof(mdnsHostname) - 1);
    mdnsHostname[sizeof(mdnsHostname) - 1] = '\0'; // Ensure null termination
}

void initWifiEventGroup() {
    if (!wifiEventGroup) {
        wifiEventGroup = xEventGroupCreate();
    }
};
void initWiFi() {
    static bool registered = false;
    initWifiEventGroup();
    if (!registered) {
        WiFi.onEvent(WiFiEvent);
        registered = true;
    }
};


void startUDP() {
    if (!udpStarted) {
        udp.begin(localPort);
        udpStarted = true;
    }
}
void stopUDP() {
    if (udpStarted) {
        udp.stop();
        udpStarted = false;
    }
}
void startServer() {
    server.begin();
}
void stopServer() {
    server.end();
}

void startMDNS() {
  if (mdnsHostname[0] != '\0') {
    MDNS.end();
    if (MDNS.begin(mdnsHostname)) { // Set the hostname
      MDNS.addService("http", "tcp", 80);//being explicit to start the http
      MDNS.addServiceTxt("http", "tcp", "board", ESP.getChipModel());//add info for the router to know
      MDNS.addServiceTxt("http", "tcp", "version", charFirmwareVersion);//add info for the router to know
      MDNS.addServiceTxt("http", "tcp", "description", charDescription);
      MDNS.addServiceTxt("http", "tcp", "author", charAuthor);
      //MDNS.addServiceTxt("http", "tcp", "manufacturer", charAuthor);
      //MDNS.addServiceTxt("http", "tcp", "name", charDescription);
      //MDNS.addService("workstation", "tcp", 9);
      DEBUG_PRINTF("mDNS responder started: %s.local",mdnsHostname);
    }
  }//mdnsHostname    
}//startMDNS
void stopMDNS() {
    MDNS.end();
}

void triggerTaskStartMDNS(uint32_t delayMS) {

    TimerHandle_t t = xTimerCreate(
      "udp_delay",
      pdMS_TO_TICKS(delayMS), // Give the network stack the time to start
      pdFALSE,
      nullptr,
      [](TimerHandle_t self){
        startUDP();
        startMDNS();
        xTimerDelete(self, 0);//delete itself to avoid leaks
      });
    xTimerStart(t, 0);
}//triggerTaskStartMDNS

//handle event such as disconnect
void WiFiEvent(WiFiEvent_t event){
  static bool servicesRunning = false;
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      DEBUG_PRINT("WiFi connected, IP: ");
      DEBUG_PRINTLN(WiFi.localIP());
      xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
      if (!servicesRunning) {
        triggerTaskStartMDNS(500);
        servicesRunning = true;
      }
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        DEBUG_PRINTLN("WiFi IP lost");
        xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT);
        if (servicesRunning) {
          stopUDP();
          stopMDNS();
          servicesRunning = false;
        }
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      DEBUG_PRINTLN("WiFi lost connection (Arduino event). Reconnecting...");
      xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT);
      if (servicesRunning) {
        stopUDP();
        // stopServer() intentionally omitted — AsyncWebServer must stay up
        stopMDNS();
        servicesRunning = false;
      }
      break;
    default:
      break;
  }//switch
}//WiFiEvent


// Runs in the FreeRTOS timer service task — single-threaded, no concurrent access possible.
static void saveTimerCallback(TimerHandle_t xTimer) {
  DEBUG_PRINTLN("Saving preferences in background...");
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    savePreferences();
    xSemaphoreGive(xMutex);
  } else {
    DEBUG_PRINTLN("Save preferences failed: Mutex timeout.");
  }
}

// xTimerChangePeriod() atomically starts the timer if dormant, or resets the
// countdown if already running — safe to call from any task context.
void triggerTaskSavePreference(uint32_t delayMS){
  if (saveTimer == nullptr) {
    saveTimer = xTimerCreate("deferred_save", pdMS_TO_TICKS(delayMS), pdFALSE, nullptr, saveTimerCallback);
  }
  if (saveTimer != nullptr) {
    xTimerChangePeriod(saveTimer, pdMS_TO_TICKS(delayMS), 0); // starts or resets
  }
}//triggerTaskSavePreference

bool savePreferences() {
  bool ok = true;
  const float err = 0.0001f;
  if ( preferences.begin("settings", false) ){// "false" means Read/Write. FIlename must be 15 characters or fewer
    //some embeded template functions, the idea is to save the writing cycles (only write when data is)
    auto saveBool = [&](const char* key, bool value) -> bool {
      if (preferences.getBool(key, !value) != value) {
        return preferences.putBool(key, value);
      }
      return true;// nothing to do, but still OK
    };

    auto saveUInt8 = [&](const char* key, uint8_t number) -> bool {
      if (preferences.getUChar(key, 0xFF) != number) {
        return (preferences.putUChar(key, number) == sizeof(uint8_t));
      }
      return true;// nothing to do, but still OK
    };

    auto saveFloat = [&](const char* key, float number) -> bool {
      float old = preferences.getFloat(key, NAN);
      if (isnan(old) || fabs(old - number) > err) {
        return (preferences.putFloat(key, number) == sizeof(float));
      }
      return true;// nothing to do, but still OK
    };

    auto saveColor = [&](const char* key, uint16_t color) -> bool {
        uint16_t old = preferences.getUShort(key, color ^ 0xFFFF);
        if (old != color) {
          return (preferences.putUShort(key, color) == sizeof(uint16_t));
        }
        return true;// nothing to do, but still OK
    };

    auto saveString = [&](const char* key, const char* value) -> bool {
      //if (strcmp(preferences.getString(key, "").c_str(), value) != 0) {
      String stored = preferences.getString(key, "");//unfortunately, need to create a string variable
      if (strcmp(stored.c_str(), value) != 0) {
        return (preferences.putString(key, value) == strlen(value));
      }
      return true;// nothing to do, but still OK
    };

    //then go to town

    ok &= saveUInt8("timezone", i_timeZone);
    ok &= saveBool("resetDaily", isResetDaily);
    ok &= saveUInt8("resetHour", reset_localHour);
    ok &= saveUInt8("resetMinute", reset_localMinute);

    ok &= saveColor("colBackgnd", colorBackground);
    ok &= saveColor("colFrame", colorCurrentFrame);

    //ok &= saveUInt8("apiDaysTAF", apiDaysForecast);
    ok &= saveUInt8("apiTAFloc", apiForecastLocation);
    ok &= saveUInt8("TempUnit", temperatureUnit);
    ok &= saveUInt8("TempDigit", temperatureDigit);
    ok &= saveUInt8("WindUnit", windUnit);
    ok &= saveUInt8("WindDigit", windDigit);

    ok &= saveBool("drawFrame", isDrawCurrentFrame);

    ok &= saveString("apiUrlBase", apiUrlBase);
    ok &= saveString("apiKEY", apiKEY);
    ok &= saveString("apiUrlLoc", apiUrlLocation);
    ok &= saveString("apiUrlFore", apiUrlForecast);
    ok &= saveString("apiLoc1", apiLocation1);
    ok &= saveString("apiLoc2", apiLocation2);

    preferences.end();
    return ok;
  }//if  
  return false;
}//savePreferences

bool loadPreferences() {
  if ( preferences.begin("settings", true) ){// "true" means Read-Only
    size_t len = 0;
    i_timeZone      = preferences.getUChar("timezone", 0);//0=UTC 20=pacific time
    isResetDaily   = preferences.getBool("resetDaily", true);
    reset_localHour   = preferences.getUChar("resetHour", 0);
    reset_localMinute = preferences.getUChar("resetMinute", 0);

    colorBackground = preferences.getUShort("colBackgnd", 0xffff);//default=white
    colorCurrentFrame  = preferences.getUShort("colFrame", 0x07FFu);//default=cyan

    //apiDaysForecast = preferences.getUChar("apiDaysTAF", 1);
    apiForecastLocation = preferences.getUChar("apiTAFloc", 1);
    temperatureUnit = preferences.getUChar("TempUnit", 1);//default F
    temperatureDigit = preferences.getUChar("TempDigit", 0);
    windUnit = preferences.getUChar("WindUnit", 0);//default KT
    windDigit = preferences.getUChar("WindDigit", 0);
    isDrawCurrentFrame = preferences.getBool("drawFrame", false);
    

    len = preferences.getString("apiUrlBase", apiUrlBase, sizeof(apiUrlBase));
    if (len == 0) strlcpy(apiUrlBase, "https://api.weatherapi.com/v1/current.json?key=", sizeof(apiUrlBase));
    len = preferences.getString("apiKEY", apiKEY, sizeof(apiKEY));
    if (len == 0) strlcpy(apiKEY, "1234", sizeof(apiKEY));
    len = preferences.getString("apiUrlFore", apiUrlForecast, sizeof(apiUrlForecast));
    if (len == 0) strlcpy(apiUrlForecast, "&days=", sizeof(apiUrlForecast));
    len = preferences.getString("apiUrlLoc", apiUrlLocation, sizeof(apiUrlLocation));
    if (len == 0) strlcpy(apiUrlLocation, "&aqi=no&q=", sizeof(apiUrlLocation));
    len = preferences.getString("apiLoc1", apiLocation1, sizeof(apiLocation1));
    if (len == 0) strlcpy(apiLocation1, "Seattle", sizeof(apiLocation1));
    len = preferences.getString("apiLoc2", apiLocation2, sizeof(apiLocation2));
    if (len == 0) strlcpy(apiLocation2, "Seattle", sizeof(apiLocation2));

    preferences.end();
    return true;
  }//if
  return false;
}//loadPreferences


void CaptiveRequestHandler::handleRequest(AsyncWebServerRequest *request) {
    request->send(200, "text/html", contentHTMLportal);
}//handleRequest

void handleSaveWifiCredentials(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
        char str_ssid[256];
        char str_pwd[256];
        snprintf(str_ssid, sizeof(str_ssid), "%s", request->getParam("ssid", true)->value().c_str());
        snprintf(str_pwd, sizeof(str_pwd), "%s", request->getParam("password", true)->value().c_str());
        DEBUG_PRINT("Received SSID: "); DEBUG_PRINTLN(str_ssid);
        DEBUG_PRINT("Received Password: "); DEBUG_PRINTLN(str_pwd);
        DEBUG_PRINT("Saving credentials to Flash...");
        if (strlen(str_ssid)>0) {
          if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            preferences.begin("wifi-config", false);
            preferences.putBytes("ssid",    str_ssid, strlen(str_ssid) + 1);//should we check that len is more than 0?
            preferences.putBytes("password",str_pwd,  strlen(str_pwd) + 1);
            preferences.end();
            xSemaphoreGive(xMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));//wait one more second
          }//mutex
          DEBUG_PRINTLN("Saved!");
        }
        else{
          DEBUG_PRINTLN("Nothing to save.. empty SSID");
        }        
        request->send(200, "text/html", "Settings saved! ESP32 is restarting to connect...");
        vTaskDelay(pdMS_TO_TICKS(2000));// Give the ESP32 a second to send the response before restarting
        DEBUG_PRINTLN("Requesting a restart");
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
          shouldRestart = true;
          timer_previousRestart = millis();
          xSemaphoreGive(xMutex);
        }
    } else {
        request->send(400, "text/plain", "Missing SSID or Password");
    }
}//handleSave

void Preferences_deleteWifiCredential(const bool isRestart) {
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    preferences.begin("wifi-config", false); // Open namespace in read/write
    preferences.clear();
    preferences.end();
    xSemaphoreGive(xMutex);
    vTaskDelay(pdMS_TO_TICKS(1000));//wait one more second
  };//mutex
  DEBUG_PRINTLN("WiFi Config Deleted!");
  vTaskDelay(pdMS_TO_TICKS(1000));
  if (isRestart) {
    DEBUG_PRINTLN("Restarting...bye");
    ESP.restart();
  }
}//Preferences_deleteWifiCredential

template <size_t Nssid, size_t Npwd>
void Preferences_readWifiCredential(char (&str_ssid)[Nssid] , char (&str_pwd)[Npwd]){
  size_t len = 0;
  str_ssid[Nssid - 1] = '\0';//clear the last character as a safety net
  str_pwd[Npwd - 1] = '\0';//clear the last character as a safety net
  DEBUG_PRINT("Reading WiFi credentials from Flash...");
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
  preferences.begin("wifi-config", true); // Open in read-only mode
    len = preferences.getBytes("ssid", str_ssid, Nssid-1);
    if (len == 0) str_ssid[0] = '\0';//nothing was found, so must ensure the array is empty
    len = preferences.getBytes("password", str_pwd, Npwd-1);
    if (len == 0) str_pwd[0] = '\0';//nothing was found, so must ensure the array is empty
  preferences.end();
  xSemaphoreGive(xMutex);
  };//mutex
  DEBUG_PRINTLN("Done Reading!");
}//Preferences_readWifiCredential

// Reconnect using credentials stored in NVS, so reconnection works even if the
// WiFi driver's in-RAM config was lost after an internal watchdog reset.
void reconnectWifi() {
  char str_ssid[256] = "";
  char str_pwd[256]  = "";
  Preferences_readWifiCredential(str_ssid, str_pwd);
  if (str_ssid[0] != '\0') {
    WiFi.begin(str_ssid, str_pwd);
  } else {
    WiFi.begin(); // no stored credentials — fall back to driver RAM config
  }
}

void Wifi_connect(const char* str_ssid, const char* str_pwd){
  if (((str_ssid != nullptr)&&(str_ssid[0] != '\0'))&&((str_pwd != nullptr)&&(str_pwd[0] != '\0'))) {
    DEBUG_PRINT("Attempting to connect to: [");
    DEBUG_PRINT(str_ssid); DEBUG_PRINTLN("]");
    dnsServer.stop();//stop the dns server, so to stop the access point mode
    WiFi.mode(WIFI_STA);//put in station mode (and attempt normal wifi connection on next line)
    WiFi.setAutoReconnect(true); // This tells the ESP32 to automatically reconnect to the last saved SSID
    WiFi.persistent(false);// false=use preference for credentials; true= tells the ESP32 to automatically reconnect to the last saved SSID
    WiFi.setSleep(false);//this would prevent random drop, should set it to true on battery mode
    WiFi.setHostname(charHostName);//for the router to recognize the device
    WiFi.begin(str_ssid, str_pwd);
    // Wait 10 seconds for connection
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000L;//10 seconds
    uint32_t start = millis();
    while ( (WiFi.status() != WL_CONNECTED) && (millis() - start < WIFI_CONNECT_TIMEOUT_MS) ) {
        vTaskDelay(pdMS_TO_TICKS(200));
        DEBUG_PRINT(".");
    }//while
  }//if
}//Wifi_connect

void GetESPid(char *cname, size_t nameLEN){
  uint64_t chipid = ESP.getEfuseMac();
  char buf[13];
  sprintf(buf, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  snprintf(cname, nameLEN, "%s", buf);
}//GetESPid

void SsidScanner(AsyncResponseStream *stream) {
  char strBarRSSI[16];
  int n = WiFi.scanComplete();
  if (n <= 0) { //-2 = WIFI_SCAN_RUNNING, -1: fail, 0 found, just do it again
     n = WiFi.scanNetworks();
  }
  if (n <= 0) return;
  if (n > MAX_SSID_LIST) n = MAX_SSID_LIST; // Cap to prevent buffer overflow
  // Create an array of indices [0, 1, 2, ... n-1]
  int indices[MAX_SSID_LIST];
  for (int i = 0; i < n; i++) indices[i] = i;//for
  // Sort the indices based on RSSI (Bubble Sort for simplicity)
  //we have at least 2 ssid at this point
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
        // Swap indices if the next one has a stronger signal
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
      }//if
    }//for
  }//for
  // Build the HTML using the sorted indices
  for (int i = 0; i < n; ++i) {
    int id = indices[i]; // Get the index of the i-th strongest network
    dbm_to_SignalBars(strBarRSSI,WiFi.RSSI(id));
    stream->printf("<option value='%s'>%s (%s)%s</option>", 
            WiFi.SSID(id).c_str(), WiFi.SSID(id).c_str(), strBarRSSI,
            (WiFi.encryptionType(id) == WIFI_AUTH_OPEN ? "" : " 🔒") );
  }//for
  WiFi.scanDelete(); // Free memory
  return;
}//SsidScanner

void StartWifiCaptivePortal(const char *cstr_basic, char* hostname, size_t hostnameLEN) {
  char str_espID[13];
  DEBUG_PRINT("Getting ESP ID...");
  GetESPid(str_espID, sizeof(str_espID));
  DEBUG_PRINTLN(str_espID);
  char str_withID[32];
  snprintf(str_withID, sizeof(str_withID), "%s_%s", cstr_basic, str_espID);
  WiFi.mode(WIFI_AP);//access point mode WIFI_AP (access point) WIFI_AP_STA (hybrid both access point and normal station)
#if ASYNCWEBSERVER_WIFI_SUPPORTED
  if (!WiFi.softAP(str_withID)) {
    DEBUG_PRINTLN("Soft Access Point creation failed.");
    while (1);
  }//if
  WiFi.scanNetworks(true);
  DEBUG_PRINT("Captive Portal: ["); DEBUG_PRINT(str_withID); DEBUG_PRINTLN("]");
  //start dns server only in access point mode
  dnsServer.start(53, "*", WiFi.softAPIP()); //The "Wildcard" DNS Ensure your DNS is actually catching everything
#endif
  server.on("/save", HTTP_POST, handleSaveWifiCredentials);
  //add routes for specific phone so the wifi portal can show up quickly:
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){ request->redirect("/"); });// Android / Chrome
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){ request->redirect("/"); });// Apple / iOS
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){ request->redirect("/"); });// Windows
  server.onNotFound([](AsyncWebServerRequest *request) { request->redirect("/"); });// Generic catch-all (to force the captive portal to show up)
  server.on("/scanWifi", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *stream = request->beginResponseStream("text/html");
    SsidScanner(stream);
    request->send(stream);
  });
  server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", WiFi.macAddress().c_str());
    /*char macaddress[18];
    strlcpy(macaddress, WiFi.macAddress().c_str(), sizeof(macaddress));
    AsyncResponseStream *stream = request->beginResponseStream("text/html"); 
    stream->printf(macaddress);
    request->send(stream);*/
  });
  //this handler should be last, it is the captive portal to setup the wifi
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  // only when requested from AP
  snprintf(hostname, hostnameLEN, "%s", str_withID);
  //return str_withID;
}//StartWifiCaptivePortal

void printStreamToSerial(HTTPClient &http) {
  int totalLimit = http.getSize();
  int bytesRead = 0;
  WiFiClient* stream = http.getStreamPtr();
  DEBUG_PRINTF("--- Response Body Start --- (size %d bytes) ---\n", totalLimit);
  unsigned long startTimeout = millis();
  while (bytesRead < totalLimit && (millis() - startTimeout < 3000)) {
        while (stream->available() && bytesRead < totalLimit) {
            char c = stream->read();
            DEBUG_PRINT(c);
            bytesRead++;
            startTimeout = millis(); // Reset timeout as long as data flows
        }//while
        vTaskDelay(pdMS_TO_TICKS(1)); // Give the background WiFi tasks a moment to breathe
    }//while
  DEBUG_PRINTLN("\n--- Response Body End ---");  
}//printStreamToSerial

void fetchWeatherData(WeatherCurrent &wcurrent, const char *apiLoc, const uint8_t locationIndex, bool currentOnly) {
  if (WiFi.status() == WL_CONNECTED) {
    WeatherCurrent localCurrent;
    //uint8_t temp_apiDaysForecast=0;
    uint8_t temp_apiForecastLocation=1;
    uint8_t temp_tempUnit=0;
    uint8_t temp_tempDigit=0;
    uint8_t temp_windUnit=0;
    uint8_t temp_windDigit=0;
    char temp_apiUrlBase[128];
    char temp_apiKEY[64];
    char temp_apiUrlLocation[16];
    char temp_apiLocation[32];
    char temp_apiUrlForecast[16];

    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      localCurrent = wcurrent;
      //temp_apiDaysForecast = apiDaysForecast;
      temp_apiForecastLocation = apiForecastLocation;
      temp_tempUnit = temperatureUnit;
      temp_tempDigit = temperatureDigit;
      temp_windUnit = windUnit;
      temp_windDigit = windDigit;
      strlcpy(temp_apiUrlBase, apiUrlBase, sizeof(temp_apiUrlBase));
      strlcpy(temp_apiKEY, apiKEY, sizeof(temp_apiKEY));
      strlcpy(temp_apiUrlLocation, apiUrlLocation, sizeof(temp_apiUrlLocation));
      strlcpy(temp_apiLocation, apiLoc, sizeof(temp_apiLocation));
      strlcpy(temp_apiUrlForecast, apiUrlForecast, sizeof(temp_apiUrlForecast));
      xSemaphoreGive(xMutex);
    }//mutex

    // For the designated forecast location, swap current.json → forecast.json and
    // append the &days=N suffix so the single API response carries both current
    // weather and forecast data.
    // currentOnly=true: Phase 1 uses current.json for both locations to keep heap pressure low.
    // forecast.json is fetched separately in Phase 1.5, after C1+C2 sprites free ~75KB.
    const bool doForecast = !currentOnly && (locationIndex == temp_apiForecastLocation);
    char fetchUrlBase[128];
    char fetchUrlSuffix[24] = {};
    if (doForecast) {
      const char* p = strstr(temp_apiUrlBase, "current.json");
      if (p) {
        //snprintf(fetchUrlBase, sizeof(fetchUrlBase), "%.*sforecast.json",
        //         (int)(p - temp_apiUrlBase), temp_apiUrlBase);
        // Keep everything before AND after "current.json" (e.g. "?key=")
        const char* afterCurrent = p + strlen("current.json");
        snprintf(fetchUrlBase, sizeof(fetchUrlBase), "%.*sforecast.json%s",
            (int)(p - temp_apiUrlBase), temp_apiUrlBase, afterCurrent);
      } else {
        strlcpy(fetchUrlBase, temp_apiUrlBase, sizeof(fetchUrlBase));
      }
      snprintf(fetchUrlSuffix, sizeof(fetchUrlSuffix), "%s%u",
               temp_apiUrlForecast, 3);//always 3 days forcast for now
    } else {
      strlcpy(fetchUrlBase, temp_apiUrlBase, sizeof(fetchUrlBase));
    }

    localCurrent = fetchWeatherDataCurrent(fetchUrlBase, temp_apiKEY,
                     temp_apiUrlLocation, temp_apiLocation,
                     doForecast ? fetchUrlSuffix : "",
                     temp_tempUnit, temp_tempDigit,
                    temp_windUnit, temp_windDigit);

    if (localCurrent.isFetchOK && localCurrent.urlicon[0] != '\0') {
      fetchWeatherIcon(localCurrent.pathicon, sizeof(localCurrent.pathicon),
                       "http:", localCurrent.urlicon, "/cache");
    }

    if (localCurrent.isFetchOK) {
      // Successful fetch: write everything back.
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        wcurrent = localCurrent;
        xSemaphoreGive(xMutex);
      } else {
        DEBUG_PRINTLN("fetchWeatherData: mutex timeout — write-back dropped");
      }
    } else if (!doForecast) {
      // current.json failed: mark as stale but preserve last-known values so the
      // display still shows the previous temperature / wind / location under the X.
      DEBUG_PRINTLN("[fetch] current.json failed — preserving previous data, isFetchOK=false");
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        wcurrent.isFetchOK = false;
        xSemaphoreGive(xMutex);
      }
    } else {
      // forecast.json (Phase 1.5) failed: current.json data is still valid — do not
      // touch isFetchOK so the current-weather sprites keep showing correctly.
      // TAF fields retain their previous values (stale but not zeroed).
      DEBUG_PRINTLN("[fetch] forecast.json failed — preserving all previous data");
    }
  }//wifi connected
}//fetchWeatherData

// Fetch the three TAF day icons for the forecast location.
// Called AFTER fetchWeatherData + xTaskNotifyGive(MainCode1Task) so that Core 1
// can draw the current-weather sprites concurrently while Core 0 fetches these icons.
// Reads TAFurlicon[] from the global under mutex, fetches each icon, then writes
// TAFpathicon[] back to the global under mutex.
void fetchWeatherTAFIcons(WeatherCurrent &wcurrent) {
  char urlicons[3][64] = {};
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    for (int i = 0; i < 3; i++) {
      strlcpy(urlicons[i], wcurrent.TAFurlicon[i], sizeof(urlicons[i]));
    }
    xSemaphoreGive(xMutex);
  }
  char pathicons[3][24] = {};
  for (int i = 0; i < 3; i++) {
    if (urlicons[i][0] != '\0') {
      fetchWeatherIcon(pathicons[i], sizeof(pathicons[i]),
                       "http:", urlicons[i], "/cache");
    }
  }
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    for (int i = 0; i < 3; i++) {
      strlcpy(wcurrent.TAFpathicon[i], pathicons[i], sizeof(wcurrent.TAFpathicon[i]));
    }
    xSemaphoreGive(xMutex);
  }
}//fetchWeatherTAFIcons

WeatherCurrent fetchWeatherDataCurrent(const char*baseurl, const char*apikey, const char*urllocation, const char*location, const char*urlforecast, const uint8_t tUnit, const uint8_t tDigit, const uint8_t wUnit, const uint8_t wDigit)
{
  WeatherCurrent weather;
  weather.t_unit = tUnit;
  weather.t_digit = tDigit;
  weather.w_unit = wUnit;
  weather.w_digit = wDigit;
  float wind_mph = 0.0f;
  float gust_mph = 0.0f;
  char urlBuffer[256];
  snprintf(urlBuffer, sizeof(urlBuffer), "%s%s%s%s%s", baseurl, apikey, urllocation, location, urlforecast);
  // Normalize to HTTPS — WeatherAPI refuses plain HTTP connections
  if (strncmp(urlBuffer, "http://", 7) == 0) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "https://%s", urlBuffer + 7);
    strlcpy(urlBuffer, tmp, sizeof(urlBuffer));
  }
  std::strncpy(weather.location, location, sizeof(weather.location));
  WiFiClientSecure client;
  client.setInsecure(); // Tell the client NOT to check the SSL certificate
  //client.setCACert(rootCACertificate);
  HTTPClient http;
  http.useHTTP10(true);    // disable chunked encoding so getStream() works reliably
  http.setConnectTimeout(10000);
  http.setTimeout(30000); // forecast.json with days=3 is large; needs more time over TLS
  http.addHeader("Connection", "close"); // ensure server closes stream after body, prevents IncompleteInput
  if (!http.begin(client, urlBuffer)) {
    DEBUG_PRINTLN("HTTP begin failed");
    return weather;
  }
  int httpCode = http.GET();
  //DEBUG_PRINTF("HTTP GET response for %s: %s\n", httpCode, http.errorToString(httpCode).c_str());
  if (httpCode == HTTP_CODE_OK) {
#if MATDEBUG
      //printStreamToSerial(http);
#endif
    JsonDocument doc;
    bool parseOK = false;
    {
      JsonDocument filter;
      filter["location"]["name"]          = true;
      filter["location"]["localtime"]    = true;
      filter["current"]["temp_c"]          = true;
      filter["current"]["temp_f"]          = true;
      filter["current"]["dewpoint_c"]      = true;
      filter["current"]["condition"]["text"] = true;
      filter["current"]["condition"]["icon"] = true;
      filter["current"]["wind_mph"]        = true;
      filter["current"]["wind_degree"]     = true;
      filter["current"]["wind_dir"]        = true;
      filter["current"]["gust_mph"]        = true;
      filter["current"]["cloud"]           = true;
      filter["current"]["vis_miles"]       = true;
      filter["forecast"]["forecastday"][0]["date"]                            = true;
      filter["forecast"]["forecastday"][0]["day"]["maxtemp_c"]                = true;
      filter["forecast"]["forecastday"][0]["day"]["maxtemp_f"]                = true;
      filter["forecast"]["forecastday"][0]["day"]["mintemp_c"]                = true;
      filter["forecast"]["forecastday"][0]["day"]["mintemp_f"]                = true;
      filter["forecast"]["forecastday"][0]["day"]["maxwind_mph"]              = true;
      filter["forecast"]["forecastday"][0]["day"]["avgvis_miles"]             = true;
      filter["forecast"]["forecastday"][0]["day"]["totalprecip_mm"]           = true;
      filter["forecast"]["forecastday"][0]["day"]["totalsnow_cm"]             = true;
      filter["forecast"]["forecastday"][0]["day"]["daily_chance_of_rain"]     = true;
      filter["forecast"]["forecastday"][0]["day"]["daily_chance_of_snow"]     = true;
      filter["forecast"]["forecastday"][0]["day"]["condition"]["icon"]        = true;
      filter["forecast"]["forecastday"][0]["day"]["condition"]["text"]        = true;
      

      // WiFiClientSecure decrypts in fixed-size TLS records; stream.read() returns -1
      // between records, which ArduinoJson treats as EOF → IncompleteInput.
      // BlockingStream::read() yields until a byte arrives or the connection closes,
      // so ArduinoJson always gets the full document.  Zero extra heap allocation.
      BlockingStream bs(http.getStream(), 20000);
      DeserializationError error = deserializeJson(doc, bs, DeserializationOption::Filter(filter));
      parseOK = !error;
      if (error) {
        DEBUG_PRINTF("JSON deserialization failed for [%s]: %s\n", urlBuffer, error.c_str());
      }
    }// filter destroyed here — heap pool freed before doc is read
    if (parseOK) {
      // numeric fields — read directly, default if key missing
      if (weather.t_unit==1){//F
        weather.temp      = doc["current"]["temp_f"]           | -999.0f;
      }
      else{
        weather.temp      = doc["current"]["temp_c"]           | -999.0f;
      }
      weather.dewpoint_c  = doc["current"]["dewpoint_c"]       | -999.0f;
      wind_mph            = doc["current"]["wind_mph"]         | 0.0f;
      weather.wind_degree = doc["current"]["wind_degree"]      | 0;
      gust_mph            = doc["current"]["gust_mph"]         | 0.0f;
      weather.cloud       = doc["current"]["cloud"]            | 0;
      weather.vis_miles   = doc["current"]["vis_miles"]        | 0.0f;
      if (weather.w_unit==0){//knots
        weather.wind = mph2kts(wind_mph);
        weather.gust = mph2kts(gust_mph);
      }
      else if (weather.w_unit==2){//kph
        weather.wind = mph2kph(wind_mph);
        weather.gust = mph2kph(gust_mph);
      }
      else{//mph
        weather.wind = wind_mph;
        weather.gust = gust_mph;
      }
      
      
      // strings — MUST be copied before doc goes out of scope
      memset(weather.location, 0, sizeof(weather.location));
      memset(weather.condition, 0, sizeof(weather.condition));
      memset(weather.urlicon, 0, sizeof(weather.urlicon));
      memset(weather.localtime, 0, sizeof(weather.localtime));
      strlcpy(weather.location, doc["location"]["name"] | location, sizeof(weather.location));
      strlcpy(weather.condition, doc["current"]["condition"]["text"] | "", sizeof(weather.condition));
      strlcpy(weather.urlicon, doc["current"]["condition"]["icon"] | "", sizeof(weather.urlicon));
      strlcpy(weather.localtime, doc["location"]["localtime"] | "", sizeof(weather.localtime));
      // Parse forecast days (present only when using forecast.json URL)
      JsonArray forecastdays = doc["forecast"]["forecastday"].as<JsonArray>();
      int ndays = (int)forecastdays.size();
      if (ndays > 3) ndays = 3;
      // Zero-initialise slots the API didn't fill so stale values from a previous
      // fetch cycle are never shown (can happen if the API returns fewer than 3 days).
      for (int i = ndays; i < 3; i++) {
        weather.TAFmaxtemp[i]        = 0.0f;
        weather.TAFmintemp[i]        = 0.0f;
        weather.TAFmaxwind_kts[i]    = 0.0f;
        weather.TAFvis_miles[i]      = 0.0f;
        weather.TAFprecip_mm[i]      = 0.0f;
        weather.TAFsnow_cm[i]        = 0.0f;
        weather.TAFchance_of_rain[i] = 0;
        weather.TAFchance_of_snow[i] = 0;
        weather.TAFday[i][0]         = '\0';
        weather.TAFpathicon[i][0]    = '\0';
      }
      for (int i = 0; i < ndays; i++) {
        JsonObject d = forecastdays[i]["day"];
        weather.TAFmaxtemp[i]        = (tUnit == 1) ? (d["maxtemp_f"]           | 0.0f) : (d["maxtemp_c"]           | 0.0f);
        weather.TAFmintemp[i]        = (tUnit == 1) ? (d["mintemp_f"]           | 0.0f) : (d["mintemp_c"]           | 0.0f);
        wind_mph                     = d["maxwind_mph"]                         | 0.0f;
        weather.TAFvis_miles[i]      = d["avgvis_miles"]                        | 0.0f;
        weather.TAFprecip_mm[i]      = d["totalprecip_mm"]                      | 0.0f;
        weather.TAFsnow_cm[i]        = d["totalsnow_cm"]                        | 0.0f;
        weather.TAFchance_of_rain[i] = d["daily_chance_of_rain"]                | 0;
        weather.TAFchance_of_snow[i] = d["daily_chance_of_snow"]                | 0;
        const char* dateStr = forecastdays[i]["date"] | "";
        if (dateStr[0] != '\0') {
          int y, mo, dd;
          if (sscanf(dateStr, "%d-%d-%d", &y, &mo, &dd) == 3) {
            struct tm t = {};
            t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = dd;
            mktime(&t);
            strftime(weather.TAFday[i], sizeof(weather.TAFday[i]), "%A", &t);
          }
        }//if

        if (weather.w_unit==0){//knots
          weather.TAFmaxwind_kts[i] = mph2kts(wind_mph);
        }
        else if (weather.w_unit==2){//kph
          weather.TAFmaxwind_kts[i] = mph2kph(wind_mph);
        }
        else{//mph
          weather.TAFmaxwind_kts[i] = wind_mph;
        }

        memset(weather.TAFurlicon[i],   0, sizeof(weather.TAFurlicon[i]));
        memset(weather.TAFcondition[i], 0, sizeof(weather.TAFcondition[i]));
        strlcpy(weather.TAFurlicon[i],   d["condition"]["icon"] | "", sizeof(weather.TAFurlicon[i]));
        strlcpy(weather.TAFcondition[i], d["condition"]["text"] | "", sizeof(weather.TAFcondition[i]));
      }
      weather.isFetchOK = true;
      DEBUG_PRINTF("Temp: %.1f\n",    weather.temp);
      DEBUG_PRINTF("Dewpoint: %.1f C\n",          weather.dewpoint_c);
      DEBUG_PRINTF("Cloud: %d%%  Vis: %.1f miles\n", weather.cloud, weather.vis_miles);
      DEBUG_PRINTF("Condition: %s\n",             weather.condition);
    }// doc destroyed here — heap pool freed
  }//http OK
  else{
    DEBUG_PRINTF("HTTP GET failed for [%s]: %d (%s)\n", urlBuffer, httpCode, http.errorToString(httpCode).c_str());
  }
  http.end();
  return weather;
}//fetchWeatherDataSingle



void fetchWeatherIcon(char*pathicon, const size_t sizepath, const char*baseurl, const char*urlicon, const char*foldercache)
{
  if (!pathicon) return;
  if (!baseurl) return;

  char urlBuffer[256];
  snprintf(urlBuffer, sizeof(urlBuffer), "%s%s", baseurl, urlicon);

  // Build cache filename as "day_113.png" or "night_113.png" so day and night
  // icons for the same condition code are stored as separate cache entries.
  const char* lastSlash = strrchr(urlicon, '/');
  if (lastSlash && lastSlash > urlicon) {
    const char* prevSlash = lastSlash - 1;
    while (prevSlash > urlicon && *prevSlash != '/') prevSlash--;
    if (*prevSlash == '/') {
      // segment between the two slashes is "day" or "night"
      snprintf(pathicon, sizepath, "%s/%.*s_%s",
               foldercache,
               (int)(lastSlash - prevSlash - 1), prevSlash + 1,
               lastSlash + 1);
    } else {
      snprintf(pathicon, sizepath, "%s/%s", foldercache, lastSlash + 1);
    }
  } else {
    snprintf(pathicon, sizepath, "%s/%s", foldercache, lastSlash ? lastSlash + 1 : urlBuffer);
  }

  // skip download if already cached
  bool alreadyCached = false;
  if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
    alreadyCached = LittleFS.exists(pathicon);
    xSemaphoreGive(filesystemMutex);
  }
  if (alreadyCached) {
    DEBUG_PRINTLN("Icon already fetched (Icon)");
    return;
  }

  // Guard against filling LittleFS: clear the icon cache if space is low
  static const size_t ICON_CACHE_MIN_FREE = 50000;
  if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
    if (freeSpace < ICON_CACHE_MIN_FREE) {
      DEBUG_PRINTF("LittleFS low (%u B free) — clearing icon cache\n", freeSpace);
      File cdir = LittleFS.open(foldercache);
      if (cdir && cdir.isDirectory()) {
        File f = cdir.openNextFile();
        while (f) {
          char fp[64];
          snprintf(fp, sizeof(fp), "%s/%s", foldercache, f.name());
          f.close();
          LittleFS.remove(fp);
          f = cdir.openNextFile();
        }
        cdir.close();
      }
    }
    xSemaphoreGive(filesystemMutex);
  }

  // Plain HTTP (no TLS) for the icon CDN — icons are public static images.
  // Using WiFiClientSecure here would require a second TLS handshake (~36KB heap)
  // right after fetchWeatherDataCurrent's TLS session, which fragments the heap
  // enough to prevent the next weather data TLS session from succeeding.
  WiFiClient *client = new WiFiClient;
  if (!client) {
    DEBUG_PRINTLN("Failed to allocate WiFiClient (Icon)");
    return;
  }
  HTTPClient http;
  http.useHTTP10(true);    // disable chunked encoding so getStream() works reliably
  http.setConnectTimeout(5000);
  http.setTimeout(10000); // Add read timeout
  if (!http.begin(*client, urlBuffer)) {
    DEBUG_PRINTLN("HTTP begin failed (Icon)");
    delete client;
    return;
  }
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int size = http.getSize();
    if (size > 0) {
      // Open file under mutex (brief hold), then release before streaming
      File file;
      if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        file = LittleFS.open(pathicon, "w");
        xSemaphoreGive(filesystemMutex);
      }
      if (file) {
        // Stream into LittleFS in small stack chunks — avoids heap fragmentation
        // that would prevent the next TLS handshake from getting contiguous RAM
        WiFiClient& stream = http.getStream();
        uint8_t chunk[512];
        size_t written = 0;
        while (written < (size_t)size) {
          size_t toRead = min((size_t)sizeof(chunk), (size_t)size - written);
          size_t n = stream.readBytes(chunk, toRead);
          if (n == 0) break;
          file.write(chunk, n);
          written += n;
        }
        // close() MUST always be called — it commits the file size to the LittleFS
        // directory entry. If skipped, f.size() returns 0 on the next open.
        // No mutex needed here: MainCore1 only reads cache files after fetchWeatherData
        // has fully returned (signalled by xTaskNotifyGive), so there is no concurrent reader.
        file.close();
        DEBUG_PRINTF("Icon fetched into cache: %s (%d bytes)\n", pathicon, written);
      }
      else {
        DEBUG_PRINTLN("Failed to open icon file for writing (Icon)");
      }
    }
    else {
      DEBUG_PRINTF("Invalid Content-Length for icon [%s]: %d (Icon)\n", urlicon, size);
    }
  }//http OK
  else{
    DEBUG_PRINTF("HTTP GET failed for icon [%s]: %d (%s) (Icon)\n", urlicon, httpCode, http.errorToString(httpCode).c_str());
  }
  http.end();
  delete client; // Clean up memory
}//fetchWeatherIcon



///dashboard main webpage, can be ommitted if the device does not need to show data or take input from/to user
void handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", contentHTMLroot);
}//handleRoot

void SetupWifiNormalMode(const char *cstr_basic, char* hostname, size_t hostnameLEN) {
  char ipBuffer[16];
  IPAddress ip = WiFi.localIP();
  snprintf(ipBuffer, sizeof(ipBuffer), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  DEBUG_PRINT("Wifi connection succesful. IP: "); DEBUG_PRINTLN(ipBuffer);
  snprintf(hostname, hostnameLEN, "%s", ipBuffer);

  //set name instead of ip address (can be omitted if we don't care to have a dashboard)
  setWifiHostname(cstr_basic);
  snprintf(hostname, hostnameLEN, "%s.local", cstr_basic);

  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request){
    DEBUG_PRINTLN("PING HIT");
    request->send(200, "text/plain", "pong");
  });

  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request){//method just to test the time function and display time on the webpage
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo, 10)) {// Use a 0ms or very small (10ms) timeout. If the time is set, it returns instantly. If it's NOT set, it won't hang your server for 10 seconds.
            request->send(200, "text/plain", "Time Syncing Error...");
            return;
        }
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%H:%M:%S %Z", &timeinfo);
        AsyncResponseStream *stream = request->beginResponseStream("text/html"); 
        stream->printf(buffer);
        request->send(stream);
      });

  server.on("/api/check-crash", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"crashed\":false}");// Just send a "no crash" response for now
    return;
    AsyncResponseStream *stream = request->beginResponseStream("application/json");
    stream->print("{");// 1. Start the main JSON Object
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (LittleFS.exists(filename_crash_report)) {
        File file = LittleFS.open(filename_crash_report, "r");
        if (file && file.size() >= sizeof(CrashData)) {
          file.seek(file.size() - sizeof(CrashData));// Seek to the very last record in the file
          CrashData lastRecord;
          file.read((uint8_t*)&lastRecord, sizeof(CrashData));
          file.close();
          stream->print("\"crashed\":true,");
          stream->printf("\"reason\":%d,", lastRecord.reasonCode);
          stream->printf("\"frag\":%.1f,", lastRecord.fragmentation);
          stream->printf("\"time\":%u,", lastRecord.uptimeMillis);
          stream->printf("\"message\":\"%s\",", lastRecord.message);
          stream->printf("\"taskName\":\"%s\"", lastRecord.taskName);//no coma for the last
        }//if
        else{ stream->print("\"crashed\":false"); }
      }//if file
      else { stream->print("\"crashed\":false"); }
      xSemaphoreGive(xMutex);
    }//mutex
    else { stream->print("\"error\":\"Mutex Timeout\"");  }
    stream->print("}");
    request->send(stream);
  });///api/check-crash

  
  //the following line is for a dashboard, and can be ommitted if the device does not need to show data or take input from/to user:
  server.on("/fetchPreferences", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream *stream = request->beginResponseStream("application/json");
      JsonDocument doc;
      //AirportList localCopy;
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {//increase from 50 to 100 if too many 'Busy' response
        // copy shared data while locked
        char deviceName[32];
        strlcpy(deviceName, charDeviceName, sizeof(deviceName));
        char fw[16];
        strlcpy(fw, charFirmwareVersion, sizeof(fw));
        int8_t temp_timeZone = i_timeZone;
        uint8_t temp_resetHour = reset_localHour;
        uint8_t temp_resetMinute = reset_localMinute;
        bool temp_isResetDaily = isResetDaily;

        uint16_t temp_colorBackground = colorBackground;
        uint16_t temp_colorCurrentFrame = colorCurrentFrame;
        //uint8_t temp_apiDaysForecast = apiDaysForecast;
        uint8_t temp_apiForecastLocation = apiForecastLocation;
        uint8_t temp_tempUnit = temperatureUnit;
        uint8_t temp_tempDigit = temperatureDigit;
        uint8_t temp_windUnit = windUnit;
        uint8_t temp_windDigit = windDigit;
        bool temp_isDrawCurrentFrame = isDrawCurrentFrame;
        char temp_apiUrlBase[64];
        char temp_apiKEY[64];
        char temp_apiUrlLocation[16];
        char temp_apiUrlForecast[16];
        char temp_apiLocation1[32];
        char temp_apiLocation2[32];
        strlcpy(temp_apiUrlBase, apiUrlBase, sizeof(temp_apiUrlBase));
        strlcpy(temp_apiKEY, apiKEY, sizeof(temp_apiKEY));
        strlcpy(temp_apiUrlLocation, apiUrlLocation, sizeof(temp_apiUrlLocation));
        strlcpy(temp_apiUrlForecast, apiUrlForecast, sizeof(temp_apiUrlForecast));
        strlcpy(temp_apiLocation1, apiLocation1, sizeof(temp_apiLocation1));
        strlcpy(temp_apiLocation2, apiLocation2, sizeof(temp_apiLocation2));

        xSemaphoreGive(xMutex);

        stream->print("{");
        stream->printf("\"deviceName\":\"%s\",", deviceName);
        stream->printf("\"fwVersion\":\"%s\",", fw);
        stream->printf("\"timezone\":%d,", temp_timeZone);
        stream->printf("\"isResetDaily\":%s,", temp_isResetDaily ? "true" : "false");
        stream->printf("\"resetHour\":%d,", temp_resetHour);
        stream->printf("\"resetMinute\":%d,", temp_resetMinute);
        stream->printf("\"tempUnit\":%d,", temp_tempUnit);
        stream->printf("\"tempDigit\":%d,", temp_tempDigit);
        stream->printf("\"windUnit\":%d,", temp_windUnit);
        stream->printf("\"windDigit\":%d,", temp_windDigit);
        stream->printf("\"isDrawFrame\":%s,", temp_isDrawCurrentFrame ? "true" : "false");
        
        stream->printf("\"colorBackgnd\":%d,", temp_colorBackground);
        stream->printf("\"colorFrame\":%d,", temp_colorCurrentFrame);
        stream->printf("\"api_urlBase\":\"%s\",", temp_apiUrlBase);
        stream->printf("\"api_key\":\"%s\",", temp_apiKEY);
        stream->printf("\"api_urlLocation\":\"%s\",", temp_apiUrlLocation);
        stream->printf("\"api_location1\":\"%s\",", temp_apiLocation1);
        stream->printf("\"api_location2\":\"%s\",", temp_apiLocation2);
        stream->printf("\"api_urlForecast\":\"%s\",", temp_apiUrlForecast);
        //stream->printf("\"api_daysForecast\":%d,", temp_apiDaysForecast);
        stream->printf("\"api_ForecastLoc\":%d", temp_apiForecastLocation);
        

  
        stream->print("}");
        request->send(stream);
      }
      else{
        request->send(503, "text/plain", "Busy");
      }
    });

  //the following line is for a dashboard, and can be ommitted if the device does not need to show data or take input from/to user:
  server.on("/savePreferences", HTTP_POST,
    // ? Request handler — called when the full request is done
    [](AsyncWebServerRequest *request) {

    if (request->_tempObject == nullptr) {
        request->send(400, "text/plain", "No body");
        DEBUG_PRINTLN("Error: Data received no body.");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (char*)request->_tempObject);
    free(request->_tempObject);
    request->_tempObject = nullptr;
    if (err) {
        request->send(400, "text/plain", "JSON error");
        DEBUG_PRINTLN("Error: No JSON data received.");
        return;
    }
    bool temp_isResetDaily = false;
    uint8_t temp_timeZone = 0;
    uint8_t temp_resetHour = 0;
    uint8_t temp_resetMinute = 0;
    uint16_t temp_colorBackground = 0;
    uint16_t temp_colorCurrentFrame = 0;
    //uint8_t temp_apiDaysForecast = 0;
    uint8_t temp_apiForecastLocation = 1;
    uint8_t temp_tempUnit = 0;
    uint8_t temp_tempDigit = 0;
    uint8_t temp_windUnit = 0;
    uint8_t temp_windDigit = 0;
    bool temp_isDrawCurrentFrame = false;
    char temp_apiUrlBase[64] = {0};
    char temp_apiKEY[64] = {0};
    char temp_apiUrlLocation[16] = {0};
    char temp_apiUrlForecast[16] = {0};
    char temp_apiLocation1[32] = {0};
    char temp_apiLocation2[32] = {0};
    temp_isResetDaily = doc["isResetDaily"] | false;
    temp_timeZone   = doc["timezone"]     | 0;
    temp_resetHour   = doc["resetHour"]     | 0;
    temp_resetMinute   = doc["resetMinute"]     | 0;
    //temp_colorBackground = doc["colorBack"] | false;
    temp_colorCurrentFrame = doc["colorFrame"] | 0;
    //temp_apiDaysForecast = doc["apiDaysForecast"] | 0;
    temp_apiForecastLocation = doc["apiForecastLoc"] | 1;
    temp_tempUnit     = doc["tempUnit"] | 0;
    temp_tempDigit    = doc["tempDigit"] | 0;
    temp_windUnit     = doc["windUnit"] | 0;
    temp_windDigit    = doc["windDigit"] | 0;
    temp_isDrawCurrentFrame= doc["isDrawFrame"] | false;
    const char* strurl = doc["apiUrlBase"] | "";
    strlcpy(temp_apiUrlBase, strurl, sizeof(temp_apiUrlBase));
    const char* strKey = doc["apiKey"] | "";
    strlcpy(temp_apiKEY, strKey, sizeof(temp_apiKEY));
    const char* strUrlLocation = doc["apiUrlLocation"] | "";
    strlcpy(temp_apiUrlLocation, strUrlLocation, sizeof(temp_apiUrlLocation));
    const char* strLocation1 = doc["apiLocation1"] | "";
    strlcpy(temp_apiLocation1, strLocation1, sizeof(temp_apiLocation1));
    const char* strLocation2 = doc["apiLocation2"] | "";
    strlcpy(temp_apiLocation2, strLocation2, sizeof(temp_apiLocation2));
    const char* strUrlForecast = doc["apiUrlForecast"] | "";
    strlcpy(temp_apiUrlForecast, strUrlForecast, sizeof(temp_apiUrlForecast));

    capInt8(temp_timeZone, 0, 23);
    capInt8(temp_resetHour, 0, 23);
    capInt8(temp_resetMinute, 0, 59);
    //capInt8(temp_apiDaysForecast, 1, 10);
    capInt8(temp_apiForecastLocation, 1, 2);
    capInt8(temp_tempUnit, 0, 1);
    capInt8(temp_tempDigit, 0, 1);
    capInt8(temp_windUnit, 0, 2);
    capInt8(temp_windDigit, 0, 1);

    

    DEBUG_PRINTLN("Data received succesfully. Now store into variables...");
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {//increase from 50 to 100 if too many 'Busy' response
      i_timeZone = temp_timeZone;
      reset_localHour = temp_resetHour;
      reset_localMinute = temp_resetMinute;
      isResetDaily = temp_isResetDaily;
      //colorBackground = temp_colorBackground;
      colorCurrentFrame = temp_colorCurrentFrame;
      //apiDaysForecast = temp_apiDaysForecast;
      apiForecastLocation = temp_apiForecastLocation;
      temperatureUnit = temp_tempUnit;
      temperatureDigit = temp_tempDigit;
      windUnit = temp_windUnit;
      windDigit = temp_windDigit;
      isDrawCurrentFrame= temp_isDrawCurrentFrame;
      strlcpy(apiUrlBase, temp_apiUrlBase, sizeof(apiUrlBase));
      strlcpy(apiKEY, temp_apiKEY, sizeof(apiKEY));
      strlcpy(apiUrlLocation, temp_apiUrlLocation, sizeof(apiUrlLocation));
      strlcpy(apiLocation1, temp_apiLocation1, sizeof(apiLocation1));
      strlcpy(apiLocation2, temp_apiLocation2, sizeof(apiLocation2));
      strlcpy(apiUrlForecast, temp_apiUrlForecast, sizeof(apiUrlForecast));
      xSemaphoreGive(xMutex);
      DEBUG_PRINTLN("Done!");

      

      DEBUG_PRINT("Refreshing Time Server...");
      InitTimeRTC();
      DEBUG_PRINTLN("Done!");

      triggerTaskSavePreference(1500);
      request->send(200, "text/plain", "Saved");

    }//mutex
    else {
      DEBUG_PRINTLN("Failed to acquire mutex");
      request->send(503, "text/plain", "Server busy, try again");
    }//else

    },
    nullptr,  // ? upload handler — not needed, leave nullptr
      // ? Body handler — called as chunks arrive
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
        size_t index, size_t total) {
          if (index == 0) {
              request->_tempObject = malloc(total + 1);
          }
          if (request->_tempObject) {
              memcpy((uint8_t*)request->_tempObject + index, data, len);
              if (index + len == total) {
                  ((char*)request->_tempObject)[total] = '\0'; // null-terminate
              }
          }
      }
  );



  // Handle disconnects/leaks
  server.onNotFound([](AsyncWebServerRequest *request) {
      // If the request is destroyed but _tempObject isn't null, free it
      if (request->_tempObject != nullptr) {
          free(request->_tempObject);
          request->_tempObject = nullptr;
      }
      request->redirect("/");
  });
  

  server.on("/", HTTP_GET, handleRoot);//the main page

  
}//SetupWifiNormalMode

//return true means it is connected to the internet, return false means it created the captive portal
//the hostname argument will contains either the captive portal name, or the ip address if connected
bool SetupWifiConnect(const char *cstr_basic, char* hostname, size_t hostnameLEN){
  static bool serverStarted = false;
  char strSSID[256]="";
  char strPWD[256]="";
  bool isConnected=false;

  initWiFi();

  //Preferences_deleteWifiCredential(false);//delete credential for test

  Preferences_readWifiCredential(strSSID,strPWD);
  DEBUG_PRINTF("Loaded SSID: [%s] (len=%u)\n", strSSID, strlen(strSSID));
  Wifi_connect(strSSID, strPWD);

  EventBits_t bits = xEventGroupWaitBits(
      wifiEventGroup,
      WIFI_CONNECTED_BIT,
      pdFALSE,
      pdTRUE,
      pdMS_TO_TICKS(20000)
  );

  if (WiFi.status() == WL_CONNECTED){
    SetupWifiNormalMode(cstr_basic, hostname, hostnameLEN);
    DEBUG_PRINT("Begin server: "); DEBUG_PRINTLN(hostname);
    isConnected=true;
  }
  else{
    DEBUG_PRINTLN("Connection to Wifi failed. Starting Captive Portal...");
    StartWifiCaptivePortal(cstr_basic,hostname,hostnameLEN);
    isConnected=false;
  }
  if (!serverStarted) {
    server.begin();//this should be called only once
    serverStarted = true;
  }
  return isConnected;
}//SetupWifiConnect
