#ifndef UTIL_CRASH_h
#define UTIL_CRASH_h

#include <Arduino.h>


// to use, put setupCrashInitBoot() in setup
//then in a checker, call these:
//
// checkMemoryHealth() on a timer (every 1h perhaps)
// checkCorruptionHealth() on a timer (every 1h perhaps), If you are using FreeRTOS, put the integrity check in a separate task with a low priority. This ensures it only runs when the CPU has nothing more important to do.
//  triggerCrashTimeout(..) when reading a sensor or something that can hang

struct CrashData {
  uint32_t magic;// Set this to something unique like 0xDEADBEEF
  uint32_t uptimeMillis;
  uint8_t reasonCode; // 1 = Frag, 2 = Corruption, 3 = Timeout, 4 = heaptask checker loop
  size_t freeHeap;
  float fragmentation;
  char message[32];
  char taskName[16];
    CrashData() {
      magic = 0;
      uptimeMillis = 0;
      reasonCode = 0;
      freeHeap = 0;
      fragmentation = 0.0f;
      memset(message, 0, sizeof(message));
      memset(taskName, 0, sizeof(taskName));
    }
};

#define CRASH_MAGIC 0xDEADC0DE  // Unique ID to verify RTC data

extern const char* filename_crash_report;
extern RTC_DATA_ATTR CrashData crashLog;
extern RTC_DATA_ATTR bool hasCrashFlag;
extern RTC_DATA_ATTR int bootCount;
extern volatile uint32_t heapGuardHeartbeat;
extern volatile uint32_t mainCoreHeartbeat;

bool writeCrashLog(CrashData &data, const char *filename);
bool readCrashLog(CrashData &crashRecord, const char *filename);
void setupCrashInitBoot();
void triggerCrashReport(uint8_t code, const char* logMsg);

//frag check+trigger
void checkMemoryHealth(const float &maxFrag);

void checkCorruptionHealth();
//timeout trigger
void triggerCrashTimeout(const char* origin);

//checker for the main core, call in core 1 loop
void checkHeapGuardianHealth(const unsigned long &idleTreshold);
void checkMainCoreHealth(const unsigned long &idleTreshold);

#endif