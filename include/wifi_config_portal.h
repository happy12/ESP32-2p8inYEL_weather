
#ifndef WIFI_CONFIG_PORTAL_h
#define WIFI_CONFIG_PORTAL_h

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
//#include <NetworkClientSecure.h> //only for v3.xx of arduino core, for v2.0.0+ use WiFiClientSecure
#include <UtilColor.h>

#include "variables.h"
#include "utilCrash.h"
#include "utilFile.h"
#include "contentHTMLportal.h"
#include "contentHTMLroot.h"


extern DNSServer dnsServer;
extern WiFiUDP udp;
extern const uint16_t localPort;



//for wifi management
void setWifiHostname(const char* name);
void initWifiEventGroup();
void initWiFi();
void startServer();
void startUDP();
void startMDNS();
void stopServer();
void stopUDP();
void stopMDNS();
void triggerTaskStartMDNS(uint32_t delayMS);

//handle event such as disconnect
void WiFiEvent(WiFiEvent_t event);
void Wifi_connect(const char* str_ssid, const char* str_pwd);
void reconnectWifi();
void GetESPid(char *cname, size_t nameLEN);


//for the user preferences
bool savePreferences();
void triggerTaskSavePreference(uint32_t delayMS);
bool loadPreferences();
void printStreamToSerial(HTTPClient &http);

void fetchWeatherData(WeatherCurrent &wcurrent, const char *apiLoc, const uint8_t locationIndex, bool currentOnly = false);
void fetchWeatherTAFIcons(WeatherCurrent &wcurrent);
WeatherCurrent fetchWeatherDataCurrent(const char*baseurl, const char*apikey, const char*urllocation, const char*location, const char*urlforecast, const uint8_t tUnit, const uint8_t tDigit, const uint8_t wUnit, const uint8_t wDigit);
void fetchWeatherIcon(char*pathicon, const size_t sizepath, const char*baseurl, const char*urlicon, const char*foldercache="/cache");


//captive portal
template <size_t N>
void dbm_to_SignalBars(char (&buffer)[N], const int rssi) {
  // Define 4 levels of signal strength
  if (rssi >= -55)      snprintf(buffer, N, "▂▄▆█");// Full Strength
  else if (rssi >= -65) snprintf(buffer, N, "▂▄▆");
  else if (rssi >= -75) snprintf(buffer, N, "▂▄");
  else                  snprintf(buffer, N, "▂");
}//dbm_to_SignalBars
void SsidScanner(AsyncResponseStream *stream);
void handleSaveWifiCredentials(AsyncWebServerRequest *request);
void Preferences_deleteWifiCredential(const bool isRestart);
void StartWifiCaptivePortal(const char *cstr_basic, char* hostname, size_t hostnameLEN);

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  bool canHandle(__unused AsyncWebServerRequest *request) const override { return true; }

  void handleRequest(AsyncWebServerRequest *request);
};//class CaptiveRequestHandler


//normal wifi mode
void SetupWifiNormalMode(const char *cstr_basic, char* hostname, size_t hostnameLEN);

///dashboard main webpage, can be ommitted if the device does not need to show data or take input from/to user
void handleRoot(AsyncWebServerRequest *request);

//return true means it is connected to the internet, return false means it created the captive portal
//the hostname argument will contains either the captive portal name, or the ip address if connected
bool SetupWifiConnect(const char *cstr_basic, char* hostname, size_t hostnameLEN);

#endif
