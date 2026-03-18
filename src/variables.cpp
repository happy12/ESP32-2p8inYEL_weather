#include "variables.h"

SemaphoreHandle_t xMutex = nullptr;
SemaphoreHandle_t filesystemMutex = nullptr;
SemaphoreHandle_t xSpriteDoneSem = nullptr;

unsigned long timer_previousRestart=0;


bool shouldRestart=false;

const char charFirmwareVersion[16] = "1.0.0";//this saves RAM instead of const String
const char* charDescription = "Generic Weather Fetcher";//used for the wifi router
const char* charAuthor = "Mathieu Fregeau";//used for the wifi router
const char* charHostName = "esp32-yellow"; //name for captive portal access point 
char charDeviceName[32]; //will contain the hostname plus the EID appended to it for unique identifier


uint8_t i_timeZone=0;
bool isResetDaily;
uint8_t reset_localHour;
uint8_t reset_localMinute;

uint16_t colorBackground;
char apiUrlBase[64];
char apiKEY[64];
char apiUrlLocation[16];
char apiUrlForecast[16];
char apiLocation1[32];
char apiLocation2[32];
uint8_t apiForecastLocation=1;//1=location1, 2=location2
uint8_t temperatureUnit=1;//0=C, 1=F
uint8_t temperatureDigit=0;
uint8_t windUnit;//0=kt, 1=mph, 2=kph
uint8_t windDigit;
bool isDrawCurrentFrame=false;
uint16_t colorCurrentFrame = 0;

WeatherCurrent weathercurrent1;
WeatherCurrent weathercurrent2;

void hasResetTimePassed(time_t &elapsed, uint8_t hour, uint8_t minute, uint8_t tzoneIndex)
{
  time_t now;
  time(&now);

  // Find today's midnight UTC
  time_t midnight_utc = now - (now % 86400);

  // Calculate the reset moment specifically for TODAY
  int32_t timezone_offset_seconds = GetOffsetTimezone(tzoneIndex) * 3600;
  int32_t reset_offset_seconds = (hour * 3600) + (minute * 60);
  
  time_t today_reset = midnight_utc + reset_offset_seconds - timezone_offset_seconds;

  if (now > (today_reset + 120)) { 
      today_reset += 86400; 
  }
  elapsed = now - today_reset;
}//hasResetTimePassed


void InitTimeRTC()
{
    // This starts a background system task on Core 0
    // that automatically syncs time periodically.
    //configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);//UTC, gmtOffset_sec=0
    //this is safe to call early in the code even before the wifi is established, even can be recall later with new time zone
    char tzone[32];
    char tserver[32];
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      GetCharTimezone(tzone, i_timeZone);
      GetCharTimeServer(tserver, i_timeZone);
      xSemaphoreGive(xMutex);
    }
    configTzTime(tzone, tserver, "pool.ntp.org", "time.nist.gov");//2 backups "time.google.com"

    // Optional: Adjust the sync interval (default is 1 hour)
    // sntp_set_sync_interval(12 * 60 * 60 * 1000); // 12 hours in ms
}//InitTimeRTC

void checkSyncStatus() {
  sntp_sync_status_t status = sntp_get_sync_status();
  if (status == SNTP_SYNC_STATUS_COMPLETED) {
        DEBUG_PRINTLN("Sync Successful!");
    } else if (status == SNTP_SYNC_STATUS_RESET) {
        DEBUG_PRINTLN("Sync Failed/Not Started: Check server or Wi-Fi.");
    } else {
        DEBUG_PRINTLN("Sync in progress...");
    }
}//checkSyncStatus
void checkTimezone() {
  
  char timeStringLocal[32]="Error Local time";
  char timeStringUTC[32]="Error UTC time";
  struct tm timeinfoLocal;
  if (getLocalTime(&timeinfoLocal)){// Get time with TZ applied
    strftime(timeStringLocal, sizeof(timeStringLocal), "%A, %b %d %Y, %I:%M %p", &timeinfoLocal);
  }
  time_t now;
  time(&now); // Get the raw Unix timestamp, and need to be done after getLocalTime call, to ensure synchronization
  struct tm gm_infoUTC;
  if (gmtime_r(&now, &gm_infoUTC)!= nullptr){ // Get raw UTC/GMT time
    strftime(timeStringUTC, sizeof(timeStringUTC), "%A, %b %d %Y, %I:%M %p", &gm_infoUTC);
  }

  DEBUG_PRINTLN("--- Timezone Verification ---");
  DEBUG_PRINTF("Local Hour: %s\n", timeStringLocal);
  DEBUG_PRINTF("UTC Hour:   %s\n", timeStringUTC);
  // If tm_isdst > 0, the ESP32 successfully applied your DST rule
  DEBUG_PRINTF("Local DST Active: %s\n", timeinfoLocal.tm_isdst > 0 ? "Yes" : "No");
}//checkTimezone
bool checkIsTimeValid(){
  struct tm timeinfoTEMP;
  if (getLocalTime(&timeinfoTEMP, 0)) { // 0ms wait - just check and move on
        return true;
    }
  return false;
}//checkIsTimeValid