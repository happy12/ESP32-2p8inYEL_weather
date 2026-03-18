#ifndef VARIABLES_h
#define VARIABLES_h

#pragma once

#include <Arduino.h>
#include <cstring>
#include <unordered_set>
#include <algorithm> // for std::remove_if
#include <inttypes.h>
#include <rom/rtc.h>
#include <LittleFS.h>//LittleFS file system
#include <time.h>
#include <esp_sntp.h> //some time status function utilities
#include <UtilTimeZone.h>
#include <UtilVector.h>


//#include <TFT_eSPI.h> do not use


#define MATDEBUG 0 // Change to 0 to disable all Serial output

#if MATDEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
  #define DEBUG_BEGIN(baud) Serial.begin(baud)
  #define DEBUG_FLUSH Serial.flush()
  #define DEBUG_VERBOSE //do nothing, meaning it will allow the normal debug verbose
  #define DEBUG_SERIALWAIT { unsigned long _sw=millis(); while(!Serial && (millis()-_sw)<3000){delay(10);} } // 3 s max — never blocks without a monitor
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...) // Becomes nothing when DEBUG is 0
  #define DEBUG_BEGIN(baud)
  #define DEBUG_FLUSH
   //silence system logs:
  #define DEBUG_VERBOSE esp_log_level_set("*", ESP_LOG_NONE)
  #define DEBUG_SERIALWAIT
#endif

extern SemaphoreHandle_t xMutex;
extern SemaphoreHandle_t filesystemMutex;
extern SemaphoreHandle_t xSpriteDoneSem; // Core 1 → Core 0: C1+C2 sprites freed, safe to do TAF HTTP

extern const char charFirmwareVersion[16];//this saves RAM instead of const String
extern const char* charDescription;//used for the wifi router
extern const char* charAuthor;//used for the wifi router
extern const char* charHostName; //name for captive portal access point 
extern char charDeviceName[32]; //will contain the hostname plus the EID appended to it for unique identifier

#define TFT_DEGREE "\xB0" //symbol for degree in ASCII  or "\xF7" 0xB0, try character 176(B0) 127 (7F) or 247 (F7)
#define TFT_PERCENT "\x25" //symbol for % in ascii
#define TFT_COPYRIGHT "\xA9" //symbol for (c) in ascii
#define TFT_PLUSMINUS "\xB1" //symbol for plus/minus in ascii
#define TFT_POWER2 "\xB2" //symbol for power 2 in ascii
#define TFT_POWER3 "\xB3" //symbol for power 3 in ascii
#define TFT_MU "\xB5" //symbol for mu in ascii
#define UTF8_DEGREE "\xC2\xB0" //symbol for degree in UTF-8  or 0xB0 "\xC2\xB0"     "\xB0" works

extern unsigned long timer_previousRestart;

extern bool shouldRestart;

extern uint8_t i_timeZone;
extern bool isResetDaily;
extern uint8_t reset_localHour;
extern uint8_t reset_localMinute;

void hasResetTimePassed(time_t &elapsed, uint8_t resetHour, uint8_t resetMinute, uint8_t tzoneIndex);

extern uint16_t colorBackground;
extern char apiUrlBase[64];
extern char apiKEY[64];
extern char apiUrlLocation[16];
extern char apiUrlForecast[16];
extern char apiLocation1[32];
extern char apiLocation2[32];
extern uint8_t apiForecastLocation;//1=location1, 2=location2
extern uint8_t temperatureUnit;//0=C, 1=F
extern uint8_t temperatureDigit;
extern uint8_t windUnit;//0=kt, 1=mph, 2=kph
extern uint8_t windDigit;
extern bool isDrawCurrentFrame;
extern uint16_t colorCurrentFrame;

struct WeatherCurrent{
    char condition[64];//things like overcast, sunny, etc
    char location[16];
    char urlicon[80];
    char pathicon[24];
    char localtime[20]; // API local time: "YYYY-MM-DD HH:MM"
    char TAFday[3][10]; //day of the week
    char TAFurlicon[3][80];
    char TAFpathicon[3][24];
    char TAFcondition[3][64];//things like overcast, sunny, etc
    float temp;
    uint8_t t_unit;
    uint8_t t_digit;
    uint8_t w_unit;
    uint8_t w_digit;
    float dewpoint_c;
    float wind;
    int wind_degree; //0-360
    float gust;
    float vis_miles;
    uint8_t cloud; //0-100 %
    float TAFmaxtemp[3];
    float TAFmintemp[3];
    float TAFmaxwind_kts[3];
    float TAFvis_miles[3];
    float TAFprecip_mm[3];
    float TAFsnow_cm[3];
    uint8_t TAFchance_of_rain[3]; //0-100 %
    uint8_t TAFchance_of_snow[3]; //0-100 %
    bool isFetchOK;
    WeatherCurrent(const char* c = "", const char* loc = "", const char* _urlicon = "", 
                    const char* _pathicon = "", const char* _localtime = "",
                    const float _t=0, const uint8_t _tunit=0, const uint8_t _tdigit=0, 
                    const uint8_t _wunit=0, const uint8_t _wdigit=0, 
                    const float _dc=0, const float _wind=0,
                    const int _wdeg=0, const float _gust=0,
                    const float _vis=0, const uint8_t _cloud=0,
                    const bool _isFetchOK=false) :
                    condition{}, location{}, urlicon{}, pathicon{}, localtime{}, 
                    TAFday{}, TAFurlicon{}, TAFpathicon{}, TAFcondition{},
                    temp(_t), t_unit(_tunit), t_digit(_tdigit), 
                    w_unit(_wunit), w_digit(_wdigit),
                    dewpoint_c(_dc), wind(_wind), wind_degree(_wdeg),
                    gust(_gust), vis_miles(_vis), cloud(_cloud),
                    TAFmaxtemp{}, TAFmintemp{},
                    TAFmaxwind_kts{}, TAFvis_miles{},
                    TAFprecip_mm{}, TAFsnow_cm{},
                    TAFchance_of_rain{}, TAFchance_of_snow{},
                    isFetchOK(_isFetchOK) {
        if (c) strlcpy(condition, c, sizeof(condition) );
        if (loc) strlcpy(location, loc, sizeof(location) );
        if (_urlicon) strlcpy(urlicon, _urlicon, sizeof(urlicon) );
        if (_pathicon) strlcpy(pathicon, _pathicon, sizeof(pathicon) );
        if (_localtime) strlcpy(localtime, _localtime, sizeof(localtime) );
    }
};
extern WeatherCurrent weathercurrent1;
extern WeatherCurrent weathercurrent2;

struct pointInt{
  int x;
  int y;
  pointInt(const int _x=0, const int _y=0): x(_x), y(_y) {}
};

struct spriteGeom{
  int width;
  int heigth;
  int x;
  int y;
  spriteGeom(const int _w=0, const int _h=0, const int _x=0, const int _y=0) :
          width(_w), heigth(_h), x(_x), y(_y) {}
};


inline void capInt8(uint8_t &val, const uint8_t imin, const uint8_t imax){
  if (val < imin) val = imin;
  if (val > imax) val = imax;
};
inline void capInt(int &val, const int imin, const int imax){
  if (val < imin) val = imin;
  if (val > imax) val = imax;
};
inline void capFloat(float &val, const float fmin, const float fmax){
  if (val < fmin) val = fmin;
  if (val > fmax) val = fmax;
};
inline void capDouble(double &val, const double fmin, const double fmax){
  if (val < fmin) val = fmin;
  if (val > fmax) val = fmax;
};
inline float mph2kts(const float &val){
  return val * 0.86897f;
};
inline float mph2kph(const float &val){
  return val * 1.6093f;
};

template <size_t N>
void formatFloatDigit(char (&formattednumber)[N], const float val, const uint8_t tdigit)
{
  if (tdigit == 1){
    float rounded = (val >= 0.f) ? (int)(val + 0.05f) : (int)(val - 0.05f);//trick to round without using cmath library, making sure to account for negative values
    snprintf(formattednumber, N, "%.1f", rounded);
  }
  else{//no digit
    int rounded = (val >= 0.f) ? (int)(val + 0.5f) : (int)(val - 0.5f);//trick to round without using cmath library, making sure to account for negative values
    snprintf(formattednumber, N, "%d", rounded);
  }
}
template <size_t N>
void formatTemeprature(char (&formattedtemp)[N], const float temp, const uint8_t digit, const uint8_t unit)
{
    char tsymbol = '\0';
    if (unit==0) tsymbol = 'C';
    else if (unit==1) tsymbol = 'F';
    else tsymbol = '?';

    if (digit == 1){
      float rounded = (temp >= 0.f) ? (int)(temp + 0.05f) : (int)(temp - 0.05f);//trick to round without using cmath library, making sure to account for negative values
      snprintf(formattedtemp, N, "%.1f%s%c", rounded,TFT_DEGREE,tsymbol);
    }
    else{//no digit
      int rounded = (temp >= 0.f) ? (int)(temp + 0.5f) : (int)(temp - 0.5f);//trick to round without using cmath library, making sure to account for negative values
      snprintf(formattedtemp, N, "%d%s%c", rounded,TFT_DEGREE,tsymbol);
    }
}//formatTemeprature

template <size_t N>
void formatWing(char (&formattedwind)[N], const float wind, const uint8_t digit, const uint8_t unit)
{
    char wsymbol[4] = {};
    if (unit==0) strlcpy(wsymbol,"kt", sizeof(wsymbol));
    else if (unit==1) strlcpy(wsymbol,"mph", sizeof(wsymbol));
    else if (unit==2) strlcpy(wsymbol,"kph", sizeof(wsymbol));
    else strlcpy(wsymbol,"?", sizeof(wsymbol));

    if (digit == 1){
      float rounded = (wind >= 0.f) ? (int)(wind + 0.05f) : (int)(wind - 0.05f);//trick to round without using cmath library, making sure to account for negative values
      snprintf(formattedwind, N, "%.1f %s", rounded,wsymbol);
    }
    else{//no digit
      int rounded = (wind >= 0.f) ? (int)(wind + 0.5f) : (int)(wind - 0.5f);//trick to round without using cmath library, making sure to account for negative values
      snprintf(formattedwind, N, "%d %s", rounded,wsymbol);
    }
}//formatTemeprature

// %H	Hour (00-23)
// %M	Minute (00-59)
// %S	Second (00-59)
// %d	Day of month
// %y	Year (last two digits)
// %p	AM or PM
template <size_t N>
void formatTime(char (&formattedtime)[N], time_t rawTime, const char *format) //"%Y-%m-%d %H:%M:%S" = Format: "YYYY-MM-DD HH:MM:SS" 
{
    struct tm ts;
    char buf[28];
    ts = *localtime(&rawTime);
    strftime(buf, sizeof(buf), format, &ts);
    snprintf(formattedtime, N, "%s", buf);//this is the output char
}

template <size_t N>
void GetCharTimezone(char (&buffer)[N], const uint8_t index){
  size_t len = sizeof(TZone) / sizeof(Timezone_t);
  snprintf(buffer, N, "UTC0");
  if ((index >= 0)&&(index < len)){
    snprintf(buffer, N, "%s", TZone[index].tzString);
  }
}//GetCharTimezone
template <size_t N>
void GetCharTimeServer(char (&buffer)[N], const uint8_t index){
  size_t len = sizeof(TZone) / sizeof(Timezone_t);
  snprintf(buffer, N, "time.google.com");
  if ((index >= 0)&&(index < len)){
    snprintf(buffer, N, "%s", TZone[index].ntpServer);
  }
}//GetCharTimeServer
inline int8_t GetOffsetTimezone(const uint8_t index){
  size_t len = sizeof(TZone) / sizeof(Timezone_t);
  if ((index >= 0)&&(index < len)){
    return TZone[index].tzoff;
  }
  return 0;//default UTC
}//GetOffsetTimezone

void InitTimeRTC();

void checkSyncStatus();
void checkTimezone();
bool checkIsTimeValid();

#endif