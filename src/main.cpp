#include <Arduino.h>

//https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/#extended-io

#ifndef ARDUINO_ARCH_ESP32
#error "This project requires ESP32"
#endif


//core 1 will be for all update, sendor, display
//core 0 will be for web page parameters, and time sync
TaskHandle_t MainCode1Task;
TaskHandle_t HeapCode0Task;
TaskHandle_t RSSICode0Task;
TaskHandle_t FetchWeatherCode0Task;

const unsigned long timerWifi_delay = (30UL * 1000UL);//30000UL;//30sec delay between wifi connection check, in ms (5UL * 60UL * 1000UL)
const unsigned long timerRestart_delay = 1500UL;//1.5sec delay to wait for a restart
const unsigned long timerWeatherFetch_delay = (31UL * 1000UL);//31 second delay between weather data fetch, in ms

#include "wifi_config_portal.h"
#include <DNSServer.h>
#include <ping/ping_sock.h> // Native ESP-IDF Ping headers
#include <lwip/inet.h>// Native ESP-IDF Ping headers
#include <lwip/netdb.h>// Native ESP-IDF Ping headers
#include <lwip/sockets.h>// Native ESP-IDF Ping headers

//prototyping
void MainCore1(void *pvParameters);
void heapMonitorTask(void *pvParameters);
void wifiRSSiMonitorTask(void *pvParameters);
void fetchWeatherTask(void *pvParameters);

#include "variables.h"
#include "utilCrash.h"
#include "utilFile.h"
#include "wifi_config_portal.h"

#include "Graphic2p8_lgfx.h"


#include "trebuc8pt8b.h"
#include "trebuc9pt8b.h"
#include "trebuc10pt8b.h"

//#include "SunsetFlyerSimpleWhite.h"
//#include "SunsetFlyerLogo16.h"
#include "StayOnTarget320.h"


void setup() {
  DEBUG_BEGIN(115200);
  DEBUG_SERIALWAIT; // Wait for Serial to be ready (for native USB devices)
  delay(500);// Give the hardware a second to stabilize
  DEBUG_PRINTLN("\n");
  DEBUG_PRINTLN("* * * --> INITIALIZATION <-- * * *");
  DEBUG_VERBOSE; //this will silence the output monitor of extra bug messages from library like webserver and wifi

#if MATDEBUG
  DEBUG_PRINT(charDescription); DEBUG_PRINT(" "); DEBUG_PRINT(charFirmwareVersion); DEBUG_PRINT(" For "); DEBUG_PRINTLN(ESP.getChipModel());
  char cstrCompileDate[32];
  snprintf(cstrCompileDate, sizeof(cstrCompileDate), "%s, %s", __DATE__, __TIME__);
  DEBUG_PRINT("Compiled on "); DEBUG_PRINTLN(cstrCompileDate);
  DEBUG_PRINTLN("\n");
  DEBUG_PRINT("ESP-IDF version: ");
  DEBUG_PRINTLN(esp_get_idf_version());
#endif

  DEBUG_PRINTLN("Initializing Display...");

  lcd.init();
  lcd.setBrightness(0);
  lcd.setRotation(4); // try 0, 1, 2, 3, 4, 5, 6, 7
  lcd.fillScreen(lcd.color565(0, 0, 0));
  lcd.setBrightness(255);

  DEBUG_PRINTF("Display Width: %d Height: %d\n", lcd.width(), lcd.height());//320x240

  DrawSplashScreen(lcd, StayOnTarget320, STAYONTARGET320_WIDTH, STAYONTARGET320_HEIGHT);

//Serial.printf("CPU cores: %d\n", ESP.getChipCores()); //2
//Serial.printf("Chip model: %s\n", ESP.getChipModel()); //ESP32-D0WD-V3
//Serial.printf("CPU freq: %d MHz\n", ESP.getCpuFreqMHz()); //240 Mhz

  DEBUG_PRINTLN("Initializing Mutex...");
  xMutex = xSemaphoreCreateMutex();//initialize as soon as possible, but after the serial
  configASSERT(xMutex);
  if (xMutex == NULL) {
    DEBUG_PRINTLN("Error: xMutex could not be created");
    //ledStatus.SetColor(0,RGB_PURPLE);
    //ledStatus.setBrightness(255);
    //ledStatus.turnON();
    while(1) { delay(1000); }
  }

  // Binary semaphore: Core 1 gives it after C1+C2 sprites are freed; Core 0 takes
  // it before starting fetchWeatherTAFIcons to prevent concurrent HTTP heap fragmentation.
  xSpriteDoneSem = xSemaphoreCreateBinary();
  configASSERT(xSpriteDoneSem);

  filesystemMutex = xSemaphoreCreateMutex();//initialize as soon as possible, but after the serial
  configASSERT(filesystemMutex);
  if (filesystemMutex == NULL) {
    DEBUG_PRINTLN("Error: filesystemMutex could not be created");
    //ledStatus.SetColor(0,RGB_PURPLE);
    //ledStatus.setBrightness(255);
    //ledStatus.turnON();
    while(1) { delay(1000); }
  }

#if MATDEBUG
  esp_reset_reason_t reason = esp_reset_reason();
  DEBUG_PRINTF("(For debug) Reset reason: %d ", reason);
  if (reason == ESP_RST_TASK_WDT) DEBUG_PRINT("(ESP_RST_TASK_WDT)");
  else if (reason == ESP_RST_WDT) DEBUG_PRINT("(ESP_RST_WDT)");
  DEBUG_PRINT("\n");
#endif

  //init crash files
  setupCrashInitBoot();

  //adds a default HTTP response header to all outgoing HTTP responses sent by the ESP32 web server
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  DEBUG_PRINTLN("Mounting LittleFS");
  if (!mountLittleFS()){
    //ledStatus.SetColor(0,RGB_PURPLE);
    //ledStatus.setBrightness(255);
    //ledStatus.turnON();
    DEBUG_PRINTLN("Error mounting LittleFS");
    delay(10000);
    //ESP.restart();
  }


#if MATDEBUG
  listDir("/");//check files, chart.umd.min.js
#endif
  clearFolder("/cache");//clear cache every boot

  shouldRestart=false;
  timer_previousRestart=0L;

  //pinMode(PIN_DIMMER, INPUT_PULLUP);
  //pinMode(PIN_SWITCH, INPUT);
  //analogSetAttenuation(ADC_11db);//less accurate but also les noisy.
  

  //here load user preferences
  DEBUG_PRINT("Read Preferences...");
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {

  if (!loadPreferences()) {
    savePreferences();  // write defaults on first boot
  }
  xSemaphoreGive(xMutex);
  };//mutex
  DEBUG_PRINTLN("Done!");


  DEBUG_PRINT("Setup Wifi...");
  DEBUG_PRINTLN(charHostName);
  char devicename[64];//will contain values
  //ledStatus.SetColor(0,RGB_BLUE);
  //ledStatus.setBrightness(64);
  //ledStatus.turnON();
  if (!SetupWifiConnect(charHostName, devicename, sizeof(devicename))){
    DEBUG_PRINTLN("Wifi not succesful.. :(");
    //notify the user of the html link so they can change some parameters through the web interface.
    //matDisplay.glDraw_Text(devicename,matDisplay.GetMiddleX(),-9,ORIENTATION_0,HORIZ_JUSTIFY_CENTER,VERT_JUSTIFY_BOTTOM,col_white,1);
  }
  else{
    //ledStatus.turnOFF();
    DEBUG_PRINT("Wifi succesful! ["); DEBUG_PRINT(devicename); DEBUG_PRINTLN("]");
    //matDisplay.glDraw_Text(devicename,matDisplay.GetMiddleX(),-9,ORIENTATION_0,HORIZ_JUSTIFY_CENTER,VERT_JUSTIFY_BOTTOM,col_white,1);
  }

  DEBUG_PRINT("Init Timezone and time server...");
  InitTimeRTC();
  DEBUG_PRINTLN("Done");
  checkTimezone();

  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    snprintf(charDeviceName, sizeof(charDeviceName), "%s", devicename);
    xSemaphoreGive(xMutex);
  }

  // Create Core 0 tasks FIRST — MainCore1 (priority 2) will preempt setup() as soon
  // as it is created, so all handles it references must be valid before that point.
  DEBUG_PRINTLN("|||>->-> Creating Tasks for CORE-0...");
  xTaskCreatePinnedToCore(
    heapMonitorTask,
    "HeapGuard",
    4096,
    NULL,
    0,
    &HeapCode0Task,
    0);
  DEBUG_PRINTF("HeapCode0Task stack free/requested: %u/4096\n", uxTaskGetStackHighWaterMark(HeapCode0Task));

  xTaskCreatePinnedToCore(
    wifiRSSiMonitorTask,
    "RSSImonitor",
    4096,
    NULL,
    0,
    &RSSICode0Task,
    0);
  DEBUG_PRINTF("RSSICode0Task stack free/requested: %u/4096\n", uxTaskGetStackHighWaterMark(RSSICode0Task));

  xTaskCreatePinnedToCore(
    fetchWeatherTask,
    "FetchWeather",
    16384,           // WiFiClientSecure TLS handshake alone needs 6-8KB
    NULL,
    1,
    &FetchWeatherCode0Task,
    0);
  DEBUG_PRINTF("FetchWeatherCode0Task stack free/requested: %u/16384\n", uxTaskGetStackHighWaterMark(FetchWeatherCode0Task));

  // Create MainCore1 LAST — it has priority 2 and will preempt setup() immediately.
  // By this point FetchWeatherCode0Task is a valid handle.
  DEBUG_PRINTLN("|||>->-> Creating Task for CORE-1...");
  xTaskCreatePinnedToCore(
    MainCore1,
    "Core1Task",
    16384,           // drawPng zlib call chain needs ~4-6 KB on top of sprite/local vars
    NULL,
    2,
    &MainCode1Task,
    1);
  DEBUG_PRINTF("MainCode1Task stack free/requested: %u/16384\n", uxTaskGetStackHighWaterMark(MainCode1Task));


  DEBUG_PRINTLN("* * * --> INITIALIZATION COMPLETE <-- * * *");

}//setup

//loop of core1, for sensors and display
void MainCore1(void * pvParameters) {
  const unsigned long idleCheckMainCore1 = (15UL * 60UL * 1000UL);//15 minute, was 15UL * 60UL * 1000UL
  const unsigned long timer1ResetChecker_delay = (1UL * 60UL * 1000UL);//1 minute
  const unsigned long timerWeatherFetch_delay = (1UL * 60UL * 1000UL) + 3500L;//1 minute
  unsigned long timer1_previousResetChecker = millis();
  unsigned long timer_previousWeatherFetch = millis() - timerWeatherFetch_delay; // triggers immediately on first iteration
  time_t last_reset_time = 0;//initial time of zero
  bool gotCurrentData = false;
  bool gotForecastData = false;
  bool waitingForForecast = false; // true after C1+C2 done; next notify = Phase 2, not Phase 1
  WeatherCurrent localCurrent1, localCurrent2;
  uint8_t localForecastLoc = 1;
  bool temp_isDrawFrame = false;
  

  LGFX_Sprite spriteC1(&lcd);
  LGFX_Sprite spriteC2(&lcd);
  LGFX_Sprite spriteF1(&lcd);
  LGFX_Sprite spriteF2(&lcd);
  LGFX_Sprite spriteF3(&lcd);
  spriteGeom geomC1;
  geomC1.width = 159;
  geomC1.heigth = 118;
  geomC1.x=1;
  geomC1.y=0;
  spriteGeom geomC2;
  geomC2.width = 160;
  geomC2.heigth = 118;
  geomC2.x=160;
  geomC2.y=0;

  spriteGeom geomF1;
  geomF1.width = 106;
  geomF1.heigth = 110;//leaving room for bottom info bar
  geomF1.x=1;
  geomF1.y=120;

  spriteGeom geomF2;
  geomF2.width = 106;
  geomF2.heigth = 110;//leaving room for bottom info bar
  geomF2.x=106;
  geomF2.y=120;

  spriteGeom geomF3;
  geomF3.width = 106;
  geomF3.heigth = 110;//leaving room for bottom info bar
  geomF3.x=212;
  geomF3.y=120;

  // spriteC1 buffer (38 KB) is created/deleted per weather update so it's
  // free during the TLS fetch, giving mbedTLS a larger contiguous heap block.

  colorBackground = lcd.color565(0, 0, 0);//black
  uint16_t colorWhite = lcd.color565(255, 255, 255);
  uint16_t colorGrey = lcd.color565(128, 128, 128);
  uint16_t colorRed = lcd.color565(255, 0, 0);
  uint16_t colorGreen = lcd.color565(0, 255, 0);
  uint16_t colorBlue = lcd.color565(0, 0, 255);
  uint16_t colorCyan = lcd.color565(0, 255, 255);//0x07FFu; // RGB565 cyan

  lcd.fillScreen(colorBackground);

  for(;;) {
    unsigned long timer1_now = millis();

    //check if needs daily reset
    if ( (unsigned long)(timer1_now - timer1_previousResetChecker) >= timer1ResetChecker_delay){
      timer1_previousResetChecker = timer1_now;
      //alright let's check if we have to reset
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        bool temp_isResetDaily = isResetDaily;
        uint8_t temp_timezone = i_timeZone;
        uint8_t temp_resetMinute = reset_localMinute;
        uint8_t temp_resetHour = reset_localHour;
        xSemaphoreGive(xMutex);
        time_t elapsedTime=0;
        hasResetTimePassed(elapsedTime, temp_resetHour, temp_resetMinute, temp_timezone);
        //DEBUG_PRINTF("is Reset daily?: %d (%02dH:%02dM)\n", temp_isResetDaily, temp_resetHour, temp_resetMinute);
        if (elapsedTime<0){
          int32_t seconds_until = -elapsedTime;
          int hours = seconds_until / 3600;
          int minutes = (seconds_until % 3600) / 60;
          if (temp_isResetDaily&&(seconds_until<61)){//turn on the led for a minute before reset.
            //ledStatus.SetColor(0,RGB_BLUE); ledStatus.turnON();
          }
          DEBUG_PRINTF("Time before reset: T minus %ld seconds (%d hour, %ld minutes)\n", seconds_until, hours, minutes);
        } else {//reset should have happened, now is the time if it was not reset yet
          int hours = elapsedTime / 3600;
          int minutes = (elapsedTime % 3600) / 60;
          DEBUG_PRINTF("Ellapsed time since last reset: %ld seconds (%d hour, %d minutes)\n", elapsedTime, hours, minutes);
          if (temp_isResetDaily && elapsedTime < 65){//do it
            DEBUG_PRINTLN("Gonna reset myself up. Tomorrow is a new day. See you soon.");
            vTaskDelay(pdMS_TO_TICKS(500));
            ESP.restart();
          }
          else{
            DEBUG_PRINTLN("Skipping reset.");
            //ledStatus.turnOFF();
          }
        }
      }//mutex
    }//timer check for restart


    //fetch weather data
  if ((unsigned long)(timer1_now - timer_previousWeatherFetch) >= timerWeatherFetch_delay){
    timer_previousWeatherFetch = timer1_now;
    DEBUG_PRINTLN("Good time for a fetch ain't it...");
    //fetchWeatherData();
    //gotFreshWeatherData=true;
    xTaskNotifyGive(FetchWeatherCode0Task); // fire and forget, Core 1 stays free
    //DEBUG_PRINTLN("Done fetching weather data.");
  }//weather fetch delay check

  // non-blocking check: did the fetch task send a notification?
  if (ulTaskNotifyTake(pdTRUE, 0)) {
    if (waitingForForecast) {
      // We already drew C1+C2 and signalled Core 0 to start TAF fetch;
      // the next notification is Phase 2 (forecast icons ready), not Phase 1.
      gotForecastData = true;
      waitingForForecast = false;
    } else {
      gotCurrentData = true;
    }
  }

  // Phase 1: current weather data is ready — draw C1 + C2
  if (gotCurrentData) {
    gotCurrentData = false;
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      localCurrent1 = weathercurrent1;
      localCurrent2 = weathercurrent2;
      localForecastLoc = apiForecastLocation;
      temp_isDrawFrame = isDrawCurrentFrame;
      xSemaphoreGive(xMutex);
    }

    // Wait for heap to recover from TLS fragmentation before the first zlib run.
    // At startup the heap may need extra time to coalesce after WiFi/TLS init.
    for (int _t = 0; _t < 40 && ESP.getMaxAllocHeap() < 40000; _t++) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    DEBUG_PRINTF("[C1+C2] pre-draw: free=%u max=%u\n",
                 ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    // Interleave convert+draw: zlib runs first (heap is cleanest), then sprite.
    // After zlib frees its allocs the heap may be slightly fragmented; the wait
    // below gives the allocator a chance to coalesce before createSprite needs 38KB.
    convertIconToRgb(lcd, localCurrent1.pathicon, colorBackground);
    for (int _t = 0; _t < 20 && ESP.getMaxAllocHeap() < 38500; _t++) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    DEBUG_PRINTF("[C1 post-cvt] free=%u max=%u\n",
                 ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    DrawWeatherCurrent(lcd, spriteC1, geomC1, localCurrent1, colorBackground, colorWhite, colorRed, colorCurrentFrame, &trebuc8pt8b, &trebuc10pt8b, temp_isDrawFrame, localForecastLoc == 1);

    convertIconToRgb(lcd, localCurrent2.pathicon, colorBackground);
    for (int _t = 0; _t < 20 && ESP.getMaxAllocHeap() < 38500; _t++) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    DEBUG_PRINTF("[C2 post-cvt] free=%u max=%u\n",
                 ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    DrawWeatherCurrent(lcd, spriteC2, geomC2, localCurrent2, colorBackground, colorWhite, colorRed, colorCurrentFrame, &trebuc8pt8b, &trebuc10pt8b, temp_isDrawFrame, localForecastLoc == 2);

    // If either location failed, retry sooner instead of waiting the full cycle.
    if (!localCurrent1.isFetchOK || !localCurrent2.isFetchOK) {
      const unsigned long retryDelay = 10UL * 1000UL; // 10 seconds
      timer_previousWeatherFetch = timer1_now - timerWeatherFetch_delay + retryDelay;
      DEBUG_PRINTF("[C1+C2] fetch failed (loc1=%d loc2=%d) — retry in %lus\n",
                   localCurrent1.isFetchOK, localCurrent2.isFetchOK, retryDelay / 1000);
    }

    // Signal Core 0 that the 38KB sprites are freed; safe to start TAF HTTP connections.
    waitingForForecast = true;
    xSemaphoreGive(xSpriteDoneSem);
  }//gotCurrentData

  // Phase 2: TAF icons are ready — draw F1 + F2 + F3
  if (gotForecastData) {
    gotForecastData = false;
    // Re-copy locals so TAFpathicon[] reflects the freshly fetched icons
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      localCurrent1 = weathercurrent1;
      localCurrent2 = weathercurrent2;
      localForecastLoc = apiForecastLocation;
      xSemaphoreGive(xMutex);
    }
    WeatherCurrent &localForecastSrc = (localForecastLoc == 1) ? localCurrent1 : localCurrent2;

    // Override forecast day labels with the ESP32's NTP clock so they are always
    // correct — even during the ~15 min window after midnight when WeatherAPI's
    // cached response still reports the previous day's date.
    {
      time_t now_t = time(nullptr);
      if (now_t > 1000000000L) { // NTP synced (any date after ~2001)
        DEBUG_PRINTF("[TAF] API day0=%s → overriding with ESP32 clock\n",
                     localForecastSrc.TAFday[0]);
        for (int i = 0; i < 3; i++) {
          time_t day_t = now_t + (time_t)i * 86400;
          struct tm tm_buf;
          localtime_r(&day_t, &tm_buf);
          strftime(localForecastSrc.TAFday[i], sizeof(localForecastSrc.TAFday[i]), "%A", &tm_buf);
        }
      }
    }

    convertIconToRgb(lcd, localForecastSrc.TAFpathicon[0], colorBackground);
    DrawWeatherForecast(lcd, spriteF1, geomF1, localForecastSrc, 0, colorBackground, colorWhite, colorRed, &trebuc9pt8b, &trebuc10pt8b);

    convertIconToRgb(lcd, localForecastSrc.TAFpathicon[1], colorBackground);
    DrawWeatherForecast(lcd, spriteF2, geomF2, localForecastSrc, 1, colorBackground, colorWhite, colorRed, &trebuc9pt8b, &trebuc10pt8b);

    convertIconToRgb(lcd, localForecastSrc.TAFpathicon[2], colorBackground);
    DrawWeatherForecast(lcd, spriteF3, geomF3, localForecastSrc, 2, colorBackground, colorWhite, colorRed, &trebuc9pt8b, &trebuc10pt8b);

    DEBUG_PRINTF("[Core1] stack min=%u heap free=%u max=%u\n",
                 uxTaskGetStackHighWaterMark(NULL), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }//gotForecastData

    mainCoreHeartbeat++;// SIGNAL: "Core 1 is alive"
    checkHeapGuardianHealth(idleCheckMainCore1);//function to check if the heapMonitorTask task has frozen, this is like the supervisor to check if the task is asleep

    vTaskDelay(pdMS_TO_TICKS(10));//Always yield to let the system/watchdog breathe
  }//for
}//MainCore1

//the fetchWeatherTask is a task to fetch the weather, making it non blocking
void fetchWeatherTask(void *pvParameters) {
  //const TickType_t xDelayFetchWeather = pdMS_TO_TICKS(1UL * 60UL * 1000UL);//1 minute
  //delay is handled in the main core, where this task is set to run at certain time

    for (;;) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // sleep until Core 1 wakes us
      DEBUG_PRINTF("[fetch] heap free=%u  max block=%u  stack min=%u\n",
                   ESP.getFreeHeap(), ESP.getMaxAllocHeap(),
                   uxTaskGetStackHighWaterMark(NULL));

      // Phase 1: fetch weather data + current icons for both locations
      fetchWeatherData(weathercurrent1, apiLocation1, 1);
      fetchWeatherData(weathercurrent2, apiLocation2, 2);

      // Startup guard: if both locations failed (WiFi/DNS not fully ready), retry once
      // after a short delay before telling Core 1 to draw red squares.
      if (!weathercurrent1.isFetchOK) {
        DEBUG_PRINTLN("[fetch] location 1 failed — retry in 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        fetchWeatherData(weathercurrent1, apiLocation1, 1);
      }
      if (!weathercurrent2.isFetchOK) {
        DEBUG_PRINTLN("[fetch] location 2 failed — retry in 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        fetchWeatherData(weathercurrent2, apiLocation2, 2);
      }

      xTaskNotifyGive(MainCode1Task); // Phase 1: current data ready — Core 1 draws C1+C2

      // Phase 2: wait until Core 1 has finished C1+C2 sprites (frees 38KB heap blocks)
      // THEN fetch TAF icons. This prevents HTTP connections from fragmenting the heap
      // exactly when createSprite needs a large contiguous block.
      xSemaphoreTake(xSpriteDoneSem, pdMS_TO_TICKS(30000)); // waits for Core 1's signal

      uint8_t temp_forecastLoc = 1;
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        temp_forecastLoc = apiForecastLocation;
        xSemaphoreGive(xMutex);
      }
      if (temp_forecastLoc == 1) fetchWeatherTAFIcons(weathercurrent1);
      else                       fetchWeatherTAFIcons(weathercurrent2);
      xTaskNotifyGive(MainCode1Task); // Phase 2: TAF icons ready — Core 1 draws F1+F2+F3
    }//for
}//fetchWeatherTask

//the heapMonitorTask is a task to check for memory integrity, etc and reboot if passed a tresholh
void heapMonitorTask(void *pvParameters) {
  const TickType_t xDelayHEAP = pdMS_TO_TICKS(5UL * 60UL * 1000UL);//5 minute, was 5UL * 60UL * 1000UL
  const unsigned long idleCheckHEAP = (15UL * 60UL * 1000UL);//15 minute, was 15UL * 60UL * 1000UL
    for (;;) {
        //check fragmentation
        checkMemoryHealth(86.f);//85 or 88 is recommended, beyond 90 the esp32 can freeze

        //check heap fragmentation
        if (!heap_caps_check_integrity_all(false)) {
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
              DEBUG_PRINTLN("!!! HEALTH MONITOR: Corruption Detected !!!");
              //heap_caps_check_integrity_all(true); // Re-run with true to print specific debug info to Serial
              xSemaphoreGive(xMutex);
            }

            // Trigger crash report and reset
            triggerCrashReport(2, "Background Corruption");
        }//failed check

        // Delay for 10 seconds. 
        heapGuardHeartbeat++;
        checkMainCoreHealth(idleCheckHEAP);
        vTaskDelay(xDelayHEAP); // This puts the task into a "Blocked" state, using 0% CPU.
    }//for
}//heapMonitorTask

// Carries ping result + semaphore between nativePing() and its callbacks.
struct PingResult {
  SemaphoreHandle_t done;
  bool success;
  uint32_t responseTimeMS;
};
// RTT exposed to wifiRSSiMonitorTask after nativePing() returns.
static uint32_t lastResponseTimeMS = 0;

static void on_ping_success(esp_ping_handle_t hdl, void *args) {
  PingResult* r = (PingResult*)args;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &r->responseTimeMS, sizeof(r->responseTimeMS));
  r->success = true;
  xSemaphoreGive(r->done); // wake nativePing() immediately
}
static void on_ping_timeout(esp_ping_handle_t hdl, void *args) {
  PingResult* r = (PingResult*)args;
  r->responseTimeMS = 0;
  r->success = false;
  xSemaphoreGive(r->done); // wake nativePing() immediately
}
bool nativePing(const char* host) {
    ip_addr_t target_addr;
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) { return false; }
    struct in_addr addr4 = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ip_addr_set_ip4_u32(&target_addr, addr4.s_addr);
    freeaddrinfo(res);

    PingResult result = { xSemaphoreCreateBinary(), false, 0 };
    if (!result.done) { return false; }

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 1;

    esp_ping_callbacks_t callbacks = {
        .cb_args = &result,
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL
    };

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &callbacks, &ping);
    esp_ping_start(ping);

    // Wake up as soon as the callback fires; 2 s is a hard safety ceiling.
    xSemaphoreTake(result.done, pdMS_TO_TICKS(2000));

    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
    vSemaphoreDelete(result.done);

    lastResponseTimeMS = result.responseTimeMS;
    return result.success;
}//nativePing

void adjustPower(float avgRssi) {
  // We use "Hysteresis" logic here to prevent bouncing between states
  static wifi_power_t currentPower = WIFI_POWER_19_5dBm;
  wifi_power_t nextPower = currentPower;
  const float HYSTERESIS_THRESHOLD = 5.0;

  // Hysteresis constants (the "buffer" zones)
  const float thresholdHigh = -50 + HYSTERESIS_THRESHOLD; // Must be better than -48 to drop power
  const float thresholdLow  = -50 - HYSTERESIS_THRESHOLD; // Must be worse than -52 to raise power

  if (avgRssi > thresholdHigh) {
    nextPower = WIFI_POWER_8_5dBm; 
  } else if (avgRssi < thresholdLow && avgRssi > -72) {
    nextPower = WIFI_POWER_13dBm;
  } else if (avgRssi < -78) { //else if (avgRssi < -78)
    nextPower = WIFI_POWER_19_5dBm;//max power
  }

  if (nextPower != currentPower) {
    currentPower = nextPower;
    DEBUG_PRINT("Avg RSSI: "); DEBUG_PRINT(avgRssi);
    DEBUG_PRINT(" -> Adjusting TX Power...");
    WiFi.setTxPower(currentPower);
    DEBUG_PRINTF("Power changed to %d dBm\n", currentPower);
  }//power
  
}//adjustPower


//the wifiRSSiMonitorTask is a task to check for wifi signal strenght, and adjust the ESP32 power to adequate level (to save power/battery)
void wifiRSSiMonitorTask(void *pvParameters) {
  const TickType_t xDelayRSSI = pdMS_TO_TICKS(1UL * 60UL * 1000UL);//1 minute
  static uint32_t disconnectedMinutes = 0;// a "stuck disconnected" counter
  const int SAMPLE_SIZE = 10;
  int rssiSamples[SAMPLE_SIZE];
  int sampleIndex = 0;
  int pingFailCount = 0;
  const char* host_for_pingCheck = "8.8.8.8"; // Google DNS to check internet
  int initialRSSI = WiFi.RSSI();

  for(int i = 0; i < SAMPLE_SIZE; i++) rssiSamples[i] = initialRSSI;//initialize the buffer to -70 which means weak

    for (;;) {
        
        if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {//make sure it is not in Access Point mode, and we have connection
          rssiSamples[sampleIndex] = WiFi.RSSI();//check signal strenght
          sampleIndex = (sampleIndex + 1) % SAMPLE_SIZE;

          float sumRSSI = 0;
          for(int i = 0; i < SAMPLE_SIZE; i++) {
            sumRSSI += rssiSamples[i];
          }
          float avgRssi = sumRSSI / SAMPLE_SIZE;//Calculate average

          if (avgRssi < -78) {//low rssi, initiate the ping
            bool pingSuccess = nativePing(host_for_pingCheck);
            if (!pingSuccess) pingFailCount++;
            else pingFailCount = 0;
            if (pingFailCount >= 3) { // EMERGENCY: We have signal but no data flow. Max out power!
              WiFi.setTxPower(WIFI_POWER_19_5dBm);
              DEBUG_PRINTLN("Ping failed 3x! Boosting power to MAX for stability.");
            }
            else {
              if (lastResponseTimeMS > 150) {//good ping but high latency
                DEBUG_PRINTLN("High Latency detected. Boosting power for stability.");
                WiFi.setTxPower(WIFI_POWER_17dBm);
              }//if lastResponseTimeMS
            }//good ping, proceed to adjust wifi power
          } else{
            pingFailCount = 0;
            adjustPower(avgRssi);//Adjust based on average
          }
        }//if connected
        else if (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED) {
          disconnectedMinutes++;
          if (disconnectedMinutes >= 3) { // disconnected for 3+ minutes
            disconnectedMinutes = 0;
            WiFi.disconnect();      // reset the driver state cleanly
            reconnectWifi();        // re-attempt using NVS-backed credentials
          } 
        }//if not connected
        else if (WiFi.getMode() == WIFI_AP){
          pingFailCount = 0;
          disconnectedMinutes = 0;
          WiFi.setTxPower(WIFI_POWER_19_5dBm);//max power since we are in AP mode, or we are disconnected and the reconnect will have better chanve if higher power
          if (WiFi.scanComplete() < 0) {//-2 = WIFI_SCAN_RUNNING, -1= fail; just so that we scan new wifi network and be ready to go for the portal page
            WiFi.scanNetworks(true); // async
          }
        }//if in AP mode
        else{
          pingFailCount = 0;
          disconnectedMinutes = 0;
        }

        vTaskDelay(xDelayRSSI); // This puts the task into a "Blocked" state, using 0% CPU.
    }//for
}//wifiRSSiMonitorTask




void loop() {
  if (WiFi.getMode() & WIFI_AP) {
    dnsServer.processNextRequest();
  }
  unsigned long timer_now = millis();//milliseconds

  //check if needs restart
  if (shouldRestart && ( (unsigned long)(timer_now - timer_previousRestart) >= timerRestart_delay)){
    ESP.restart();
  }//restart delay check

  

}//loop

