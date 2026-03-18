#include "utilCrash.h"

#include <rom/rtc.h>
#include <LittleFS.h>//LittleFS file system

#include "variables.h"
#include "utilFile.h"

const char* filename_crash_report   = "/crash_log.txt";
static const uint8_t MAX_CRASH_LOG_RECORDS = 10; // file is capped at this many records; oldest is dropped when full
RTC_DATA_ATTR CrashData crashLog;
RTC_DATA_ATTR bool hasCrashFlag = false;//init once and never on soft-reset (esp.restart)
RTC_DATA_ATTR int bootCount = 0;//init once and never on soft-reset (esp.restart)
volatile uint32_t heapGuardHeartbeat = 0;//for main core checker
volatile uint32_t mainCoreHeartbeat = 0;//for heapguard core checker


bool writeCrashLog(CrashData &data, const char *filename) {

  const size_t recordSize = sizeof(CrashData);
  const size_t margin = 1024;
  size_t bytesWritten = 0;

  if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(30)) != pdTRUE) {
    DEBUG_PRINTLN("Write Crash Log ABORTED: filesystemMutex timeout");
    return false;
  }

  size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  if (freeSpace < (recordSize + margin)) {
    xSemaphoreGive(filesystemMutex);
    DEBUG_PRINTLN("Write Crash Log ABORTED: Not enough space on LittleFS!");
    return false;
  }

  // Count how many records exist already
  size_t existingCount = 0;
  if (LittleFS.exists(filename)) {
    File f = LittleFS.open(filename, "r");
    if (f) { existingCount = f.size() / recordSize; f.close(); }
  }

  if (existingCount < MAX_CRASH_LOG_RECORDS) {
    // File is not full yet: simple append
    File file = LittleFS.open(filename, "a");
    if (!file) {
      xSemaphoreGive(filesystemMutex);
      DEBUG_PRINTLN("Write Crash Log ABORTED: open failed");
      return false;
    }
    bytesWritten = file.write((const uint8_t*)&data, recordSize);
    file.close();
  } else {
    // File is full: read records 1..N-1 (skip oldest), overwrite file, append new
    DEBUG_PRINTF("Crash log full (%d records). Rotating oldest out.\n", MAX_CRASH_LOG_RECORDS);
    CrashData buffer[MAX_CRASH_LOG_RECORDS - 1];
    File rFile = LittleFS.open(filename, "r");
    if (!rFile) {
      xSemaphoreGive(filesystemMutex);
      DEBUG_PRINTLN("Write Crash Log ABORTED: rotation read failed");
      return false;
    }
    rFile.seek(recordSize); // skip the oldest record
    rFile.read((uint8_t*)buffer, recordSize * (MAX_CRASH_LOG_RECORDS - 1));
    rFile.close();

    File wFile = LittleFS.open(filename, "w"); // overwrite
    if (!wFile) {
      xSemaphoreGive(filesystemMutex);
      DEBUG_PRINTLN("Write Crash Log ABORTED: rotation write failed");
      return false;
    }
    wFile.write((const uint8_t*)buffer, recordSize * (MAX_CRASH_LOG_RECORDS - 1));
    bytesWritten = wFile.write((const uint8_t*)&data, recordSize);
    wFile.close();
  }

  xSemaphoreGive(filesystemMutex);

  if ((bytesWritten == recordSize) && (bytesWritten > 0)) {
    DEBUG_PRINT("Write Log Succesful! bytes: "); DEBUG_PRINTLN(bytesWritten);
    return true;
  } else {
    DEBUG_PRINTLN("Write Crash Log FAIL: Partial Write");
    return false;
  }
}//writeCrashLog

bool readCrashLog(CrashData &crashRecord, const char *filename) {
  CrashData tempRecord;
  int count = 0;
  if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
    if (!LittleFS.exists(filename)){
      xSemaphoreGive(filesystemMutex);
      DEBUG_PRINTF("Read Crash Log ABORTED: file not found: [%s]",filename);
      return false;
    }
    File file = LittleFS.open(filename, "r");
    if (!file) {
      xSemaphoreGive(filesystemMutex);
      DEBUG_PRINTLN("Read Crash Log ABORTED: open failed");
      return false;
    }
    while (file.available() >= sizeof(CrashData)) {
      size_t readBytes = file.read((uint8_t*)&tempRecord, sizeof(CrashData));
      if (readBytes == sizeof(CrashData)) {
        count++;
        /*DEBUG_PRINTF("Record #%d | Task: %s | Reason: %d | Heap: %u | Frag: %.1f | Message: %s\n",
                          count, tempRecord.taskName, tempRecord.reasonCode, tempRecord.freeHeap, tempRecord.fragmentation, tempRecord.message);*/
      }
    }//while
    file.close();
    xSemaphoreGive(filesystemMutex);
  } else{
    DEBUG_PRINTLN("Read Crash Log ABORTED: filesystemMutex timeout");
    return false;
  }
  crashRecord = tempRecord;
  if (count == 0) { DEBUG_PRINTLN("File exists but contains no valid records."); }
  else { DEBUG_PRINTF("------- END READING CRASH LOG HISTORY (log count %i) -------\n",count); }

  return true;

}//readCrashLog



void setupCrashInitBoot() {
  RESET_REASON reason = rtc_get_reset_reason(0);// Check if this was a cold start or a warm reboot
  if (reason == POWERON_RESET) {//normal hard reset
    DEBUG_PRINTLN("Initial Power-on: RTC memory is now fully wiped.");
    bootCount = 0;// Manually ensuring a clean slate if needed
  } else {
    DEBUG_PRINTF("Reboot/Wake: RTC memory preserved. Boot count: %d\n", bootCount);
  }
  bootCount++;


  if (hasCrashFlag) {
    DEBUG_PRINTLN("|-<<-- CRASH REPORT -->>-|");
    DEBUG_PRINTF("Reason Code: %u\n", crashLog.reasonCode);
    DEBUG_PRINTF("Heap at Crash: %u bytes\n", crashLog.freeHeap);
    DEBUG_PRINTF("Fragmentation at Crash: %.2f%%\n", crashLog.fragmentation);
    DEBUG_PRINTF("Message at Crash: %s\n", crashLog.message);
    if (crashLog.magic == CRASH_MAGIC) {
      DEBUG_PRINTF("\n--- RECOVERY REPORT ---");
      DEBUG_PRINTF("Offending Task: %s\n", crashLog.taskName);
      DEBUG_PRINTF("Uptime at Crash:         %u ms\n", crashLog.uptimeMillis);
      crashLog.magic = 0; // Clear it
    }
    CrashData tempRecord;//so it can be read again, perhaps?
    readCrashLog(tempRecord, filename_crash_report);

    DEBUG_PRINTLN("|<--- CRASH REPORT END --->|");

    // Clear the flag so we don't read it again next time
    hasCrashFlag = false;
  } else {
    DEBUG_PRINTLN("No crash report (Normal Boot).");
  }


}//setupInitBoot

void triggerCrashReport(uint8_t code, const char* logMsg) {
  hasCrashFlag = true;//this is a global bool that should survive a crash and be available
  crashLog.magic = CRASH_MAGIC;
  crashLog.reasonCode = code;

  // Get the name of the current running task
  char* currentTask = pcTaskGetName(nullptr);
  //memset(crashLog.taskName, 0, sizeof(crashLog.taskName));
  strlcpy(crashLog.taskName, currentTask ? currentTask : "Unknown", sizeof(crashLog.taskName));

  crashLog.freeHeap = ESP.getFreeHeap();
  //crashLog.fragmentation = 100.0f * ((float)ESP.getMaxAllocHeap() / ESP.getFreeHeap());
  crashLog.fragmentation = 100.0f *  (1.0f - ((float)ESP.getMaxAllocHeap() / ESP.getFreeHeap()));
  crashLog.uptimeMillis = millis();

  //memset(crashLog.message, 0, sizeof(crashLog.message));
  strlcpy(crashLog.message, logMsg ? logMsg : "", sizeof(crashLog.message));

  DEBUG_PRINTLN("Saving crash report and restarting...");

  writeCrashLog(crashLog, filename_crash_report);
  delay(50);
  DEBUG_PRINTLN("bye");
  DEBUG_FLUSH; // Ensure the serial message is sent

  ESP.restart();
}//triggerCrashReport

//frag check+trigger
void checkMemoryHealth(const float &maxFrag) {
  size_t totalFree = ESP.getFreeHeap();
  size_t largestBlock = ESP.getMaxAllocHeap();
  //float fragmentation = 100.0 * ((float)largestBlock / totalFree);
  float fragmentation = 100.0f * (1.0f - ((float)largestBlock / (float)totalFree));//large value (higher than 80.0) is bad

  if (fragmentation > maxFrag) {
    triggerCrashReport(1,"High Fragmentation");
  }
}//checkMemoryHealth

void checkCorruptionHealth() {
  if (!heap_caps_check_integrity_all(true)) {
    triggerCrashReport(2, "Memory Integrity Error. Corruption");
  }
}
//timeout trigger
void triggerCrashTimeout(const char* origin) {//origin would be a comment to identify what triggers this action, like a sensor
  char action[32];
  //memset(action, 0, sizeof(action));
  snprintf(action, sizeof(action), "Time Out [%s]", origin ? origin : "Unknown");
  triggerCrashReport(3,action);
}

//checker for the main core, call in core 1 loop
void checkHeapGuardianHealth(const unsigned long &idleTreshold) {
    static uint32_t lastHeartbeat_Heap = 0;
    static unsigned long lastChangeTime_Heap = 0;
    uint32_t now = millis();

    if (heapGuardHeartbeat != lastHeartbeat_Heap) {
        lastHeartbeat_Heap = heapGuardHeartbeat;
        lastChangeTime_Heap = now;
        return;
    }
    if (lastChangeTime_Heap == 0) {
      lastChangeTime_Heap = now;
      return;
    }

    // Heartbeat hasn't changed. Check if it has been too long.
    uint32_t idleTime = now - lastChangeTime_Heap;//How long has it been?

    if (idleTime > idleTreshold) {
        // No mutex, no print, no delay
        triggerCrashReport(4, "heapMonitorTask Guardian Freeze");
    }
}


void checkMainCoreHealth(const unsigned long &idleTreshold) {
    static uint32_t lastHeartbeat_Main = 0;
    static unsigned long lastChangeTime_Main = 0;
    uint32_t now = millis();

    if (mainCoreHeartbeat != lastHeartbeat_Main) {
      lastHeartbeat_Main = mainCoreHeartbeat;
      lastChangeTime_Main = now;
      return;
    }
    if (lastChangeTime_Main == 0) {
      lastChangeTime_Main = now;
      return;
    }

    // Heartbeat hasn't changed. Check if it has been too long.
    uint32_t idleTime = now - lastChangeTime_Main;//How long has it been?

    if (idleTime > idleTreshold) {
        // No mutex, no print, no delay
        triggerCrashReport(5, "MainCore1 Guardian Freeze");
    }
}
