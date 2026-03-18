#ifndef FILE_UTIL_h
#define FILE_UTIL_h

#include <Arduino.h>



bool mountLittleFS();

bool mkdirLittleFS(const char* dirname);

void deleteFile(const char * path);

void listDir(const char * dirname);

void clearFolder(const char* path);

bool checkLittleFS(size_t &total, size_t &used, size_t &free);

void checkRAM(size_t &total, size_t &used, size_t &free);
size_t checkRAMpercent(const size_t total, const size_t used);
size_t GetRAMfragmentation();
bool isRAMusageSafe();
size_t checkLittleFSpercent(const size_t total, const size_t free);
void formatBytes(size_t bytes, char* outBuffer);
void formatBytesPercent(size_t bytes, char* outBuffer);

void checkFlash(size_t &total);

void sanitizeFilename(char* str);

#endif