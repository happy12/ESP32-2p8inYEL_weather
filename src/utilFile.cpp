#include "utilFile.h"

#include <LittleFS.h>//LittleFS file system
#include <esp_partition.h>//to see what is usable in flash

#include "variables.h"

bool mountLittleFS() {//should not require mutex at all
  if(!LittleFS.begin(true)) {//true will format on failure
    DEBUG_PRINTLN("Mounting LittleFS FAILED!");
    return false;
  }
  DEBUG_PRINTLN("LittleFS Mounted!");
  return true;
}//mountLittleFS

bool mkdirLittleFS(const char* dirname) {
  DEBUG_PRINTF("Checking directory: [%s] ", dirname);
  if(!LittleFS.exists(dirname)){
    DEBUG_PRINT("Creating directory...");
    if(!LittleFS.mkdir(dirname)){
        DEBUG_PRINTLN("Failed");
        return false;
    }
    else{ 
      DEBUG_PRINTLN("Success!"); 
      return true; 
    }
  }
  else{ 
    DEBUG_PRINTLN("directory exist!"); 
    return true; 
  }
  return true; 
}//mountLittleFS

void deleteFile(const char * path) {

  DEBUG_PRINTF("Deleting file: [%s]...", path);
  if(LittleFS.exists(path)) {
    if(LittleFS.remove(path)) {
      DEBUG_PRINTLN("file successfully deleted");
    } else {
      DEBUG_PRINTLN("Error: Could not delete file");
    }
  } else {
    DEBUG_PRINTLN("file does not exist");
  }
}//deleteFile

void listDir(const char * dirname) {
  DEBUG_PRINTF("Listing directory: %s\n", dirname);
  File root = LittleFS.open(dirname);
  if (!root) {
    DEBUG_PRINTLN("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    DEBUG_PRINTLN(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      DEBUG_PRINT("  DIR : ");
      DEBUG_PRINTLN(file.name());
    } else {
      DEBUG_PRINT("  FILE: ");
      DEBUG_PRINT(file.name());
      DEBUG_PRINT("\tSIZE: ");
      DEBUG_PRINTLN(file.size());
    }
    file = root.openNextFile();
  }
}//listDir

//usage: clearFolder("/mycache");
void clearFolder(const char* path) {
  DEBUG_PRINTF("Clearing folder: [%s]...", path);
  if (!LittleFS.exists(path)) {
      LittleFS.mkdir(path);
      DEBUG_PRINTLN("Clearing folder: folder created");
      return; // nothing to delete, just created
  }

  // Delete all files in folder
  File dir = LittleFS.open(path);
  if (!dir || !dir.isDirectory()) {
    DEBUG_PRINTF("Clearing folder error. Not a directory: [%s]...", path);
    return;
  }

  File file = dir.openNextFile();
  while (file) {
      if (!file.isDirectory()) {
          char filePath[64];
          snprintf(filePath, sizeof(filePath), "%s/%s", path, file.name());
          file.close();
          LittleFS.remove(filePath);
           DEBUG_PRINTF("Removed file: [%s]", filePath);
      } else {
          file.close();
      }
      file = dir.openNextFile();
  }
  dir.close();
}//clearFolder

bool checkLittleFS(size_t &total, size_t &used, size_t &free)
{
  free = 0;
  total = 0;
  used = 0;
  if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
    total = LittleFS.totalBytes();
    used = LittleFS.usedBytes();
    xSemaphoreGive(filesystemMutex);
  }
  else {
    return false;
  }

  if (used > total) free = 0;
  else free = total - used;
  return true;
}//checkLittleFS

void checkRAM(size_t &total, size_t &used, size_t &free)
{
  total = ESP.getHeapSize();//in bytes
  //free = ESP.getMinFreeHeap();//in bytes returns the historical minimum, not the current free heap
  free = ESP.getFreeHeap();//in bytes, current free heap state
  if (free > total) used = 0;
  else used = total - free;
}
size_t checkRAMpercent(const size_t total, const size_t used)
{
  return (size_t)ceil(( (float)used / ((float)total) ) * 100.0f);//ceil because the higher the more risk of failure
}
size_t GetRAMfragmentation()
{
  size_t free = ESP.getFreeHeap();
  size_t largestBlock = ESP.getMaxAllocHeap();
  // If largest block is much smaller than total free, fragmentation is high.
  // 100% = No fragmentation (Perfect)
  // < 50% = High fragmentation (Risk of crash)
  return (size_t)floor(( (float)largestBlock / ((float)free) ) * 100.0f);//floor because the lower the riskier
}
bool isRAMusageSafe()
{
  size_t freeHeap = ESP.getFreeHeap();
  size_t maxBlock = ESP.getMaxAllocHeap();
  // DANGER ZONE: Less than 60KB total or no single 15KB holes
  if (freeHeap < 60000 || maxBlock < 15000) return false;
  return true;
}


size_t checkLittleFSpercent(const size_t total, const size_t free)
{
  return (size_t)(floor((float(free) / float(total))*100.0f));//floor because lower than 20% is bad
}
void formatBytes(size_t bytes, char* outBuffer) {
    if (bytes < 1024) {
        snprintf(outBuffer, 16, "%u B", (unsigned int)bytes);
    } else if (bytes < (1024 * 1024)) {
        snprintf(outBuffer, 16, "%d KB", (int)round(bytes / 1024.0));//rounding and casting into integer
    } else {
        snprintf(outBuffer, 16, "%.1f MB", bytes / 1024.0 / 1024.0);
    }
}
void formatBytesPercent(size_t bytes, char* outBuffer) {
  snprintf(outBuffer, 16, "%u", (unsigned int)bytes);
}

void sanitizeFilename(char* str) {
  while (*str) {
    // If character is NOT a letter or number, make it an underscore, remove characters like \ / * or space, etc
    if (!isalnum(*str)) {
      *str = '_';
    }
    str++;
  }
}//sanitizeFilename


void checkFlash(size_t &total) {

  const esp_partition_t* part = esp_partition_find_first( ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL  );
  if (part) {
    total = part->size;
    char outBuffer[16];
    formatBytes(total, outBuffer);
    DEBUG_PRINTF("App partition size: %s\n", outBuffer);

    total = ESP.getFlashChipSize();
    outBuffer[0] = '\0';
    formatBytes(total, outBuffer);
    DEBUG_PRINTF("getFlashChipSize: %s\n", outBuffer);
  }

}