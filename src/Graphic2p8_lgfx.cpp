#include "Graphic2p8_lgfx.h"


LGFX lcd;



LGFX::LGFX(void) {
    { // SPI bus config
        auto cfg = _bus_instance.config();
        cfg.spi_host = VSPI_HOST;  // was VSPI_HOST - this was wrong
        cfg.spi_mode = 0;
        cfg.freq_write = 80000000; //40000000 80000000
        cfg.freq_read  = 16000000;
        cfg.pin_sclk = 14;
        cfg.pin_mosi = 13;
        cfg.pin_miso = 12;
        cfg.pin_dc   = 2;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
    }
    { // panel config
        auto cfg = _panel_instance.config();
        cfg.pin_cs   = 15;
        cfg.pin_rst  = -1;
        cfg.pin_busy = -1;
        cfg.panel_width = 320;
        cfg.panel_height = 240;
        cfg.memory_width  = 320;  // add this - native GRAM width
        cfg.memory_height = 240;  // add this - native GRAM height
        cfg.offset_x = 0;
        cfg.offset_y = 0;
        cfg.offset_rotation = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits = 1;
        cfg.readable    = true;
        cfg.invert      = false;
        cfg.rgb_order   = true;   // your panel needs this
        cfg.dlen_16bit  = false;
        cfg.bus_shared  = false;
        _panel_instance.config(cfg);
    }
    { // backlight config
        auto cfg = _light_instance.config();
        cfg.pin_bl      = 21;
        cfg.invert      = false;
        cfg.freq        = 44100;
        cfg.pwm_channel = 7;
        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
    }
    { // touch config
        auto cfg = _touch_instance.config();
        cfg.x_min = 300;
        cfg.x_max = 3900;
        cfg.y_min = 200;
        cfg.y_max = 3700;
        cfg.pin_int    = 36;
        cfg.bus_shared = false;
        cfg.spi_host = HSPI_HOST;  // touch uses separate SPI bus
        cfg.freq     = 2500000;
        cfg.pin_sclk = 25;
        cfg.pin_mosi = 32;
        cfg.pin_miso = 39;
        cfg.pin_cs   = 33;
        //cfg.pin_sclk = 14;
        //cfg.pin_mosi = 13;
        //cfg.pin_miso = 12;
        //cfg.pin_cs   = 33;
        _touch_instance.config(cfg);
        _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
}


void DrawSplashScreen(LGFX &mylcd, const uint16_t* image, const uint16_t W, const uint16_t H)
{
    if (!image) return;
    const int mainW = mylcd.width();
    const int mainH = mylcd.height();
    int x = (mainW - W) / 2;
    int y = (mainH - H) / 2;
    mylcd.pushImage(x, y,
        W,
        H,
        (const lgfx::rgb565_t*)image  //StayOnTarget320 (const lgfx::rgb565_t*) (const lgfx::swap565_t*) bgr565_t
    );

}//DrawSplashScreen

void DrawSpriteText(LGFX_Sprite &sprite, const char* text, const GFXfont* font, int y, const xJustification xjust, const uint16_t color)
{
    if (!text) return;
    if (!font) return;
    sprite.setTextColor(color);
    sprite.setFont(font);//setFreeFont
    int x = 0;
    switch (xjust){
        case LEFT:
            x = 0;
            break;
        case CENTER:
            x = (sprite.width() - sprite.textWidth(text)) / 2;
            break;
        case RIGHT:
            x = sprite.width() - sprite.textWidth(text);
            break;
        default:
            x = 0;
            break;
    }//switch
    sprite.drawString(text, x, y);
}//DrawSpriteText


// Called on Core 1 BEFORE any main sprite is allocated so zlib has the best chance
// of finding the ~32 KB contiguous block it needs.  Derives the .rgb path from
// pngpath, skips if already cached, then decodes PNG → raw RGB565 and saves it.
void convertIconToRgb(LGFX &mylcd, const char* pngpath, const uint16_t colorBack)
{
    if (!pngpath || pngpath[0] == '\0') return;

    char rawpath[24] = {};
    strlcpy(rawpath, pngpath, sizeof(rawpath));
    char* dot = strrchr(rawpath, '.');
    if (!dot) return;
    strlcpy(dot, ".rgb", sizeof(rawpath) - (size_t)(dot - rawpath));

    // Skip if already converted
    bool rawExists = false;
    if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
        rawExists = LittleFS.exists(rawpath);
        xSemaphoreGive(filesystemMutex);
    }
    if (rawExists) return;

    // Load PNG bytes from LittleFS
    uint8_t* pngBuffer = nullptr;
    size_t   pngSize   = 0;
    if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        File f = LittleFS.open(pngpath, "r");
        if (f) {
            pngSize   = f.size();
            pngBuffer = (uint8_t*)malloc(pngSize);
            if (pngBuffer) { f.read(pngBuffer, pngSize); }
            f.close();
        }
        xSemaphoreGive(filesystemMutex);
    }
    if (!pngBuffer || !pngSize) { if (pngBuffer) free(pngBuffer); return; }

    // Decode PNG → sprite → save raw pixels.
    // No main sprite is allocated here, so zlib (~32 KB) has a clean heap.
    static const size_t kIconBytes = 64 * 64 * 2;
    LGFX_Sprite iconSprite(&mylcd);
    if (iconSprite.createSprite(64, 64)) {
        iconSprite.fillSprite(colorBack);
        bool decoded = iconSprite.drawPng(pngBuffer, pngSize, 0, 0);
        DEBUG_PRINTF("[icon] pre-decode %s -> %d  heap free=%u max=%u\n",
                     pngpath, decoded, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        if (decoded) {
            uint8_t* saveBuf = (uint8_t*)malloc(kIconBytes);
            if (saveBuf) {
                iconSprite.readRect(0, 0, 64, 64, (lgfx::rgb565_t*)saveBuf);
                if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    File fw = LittleFS.open(rawpath, "w");
                    if (fw) { fw.write(saveBuf, kIconBytes); fw.close(); }
                    xSemaphoreGive(filesystemMutex);
                }
                free(saveBuf);
                DEBUG_PRINTF("[icon] saved raw: %s\n", rawpath);
            } else {
                DEBUG_PRINTF("[icon] saveBuf malloc(%u) failed — heap free=%u max=%u\n",
                             kIconBytes, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
            }
        }
        iconSprite.deleteSprite();
    }
    free(pngBuffer);
}//convertIconToRgb


void DrawWeatherCurrent(LGFX &mylcd, LGFX_Sprite &sprite, spriteGeom &geom, WeatherCurrent &localCurrent, const uint16_t colorBack, const uint16_t colorText, const uint16_t colorBad, const uint16_t colorFrame, const GFXfont* fontTime, const GFXfont* fontMain, const bool isDrawFrame, bool isForecastLoc)
{
    if (!fontTime) return;
    if (!fontMain) return;

    // Caller pre-allocates both sprites before any convertIconToRgb/zlib call (while
    // max=77KB), then passes them in already-allocated.  Skip createSprite here.
    // If pre-allocation failed (sprite.width()==0), fall back with a single attempt.
    if (sprite.width() == 0) {
        if (!sprite.createSprite(geom.width, geom.heigth)) {
            DEBUG_PRINTF("[draw] createSprite(%d,%d) OOM — heap free=%u max=%u\n",
                         geom.width, geom.heigth, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
            mylcd.fillRect(geom.x, geom.y, geom.width, geom.heigth, colorBad);
            return;
        }
    }
    sprite.fillSprite(colorBack);

    int posY=54;//sprite is drawn first

    // Fast path only — .rgb was pre-decoded by convertIconToRgb() before this call.
    // Read row-by-row into a 128-byte stack buffer: no heap malloc needed, immune
    // to the post-zlib fragmentation that prevents malloc(8192) from succeeding.
    char rawpath[24] = {};
    if (localCurrent.pathicon[0] != '\0') {
        strlcpy(rawpath, localCurrent.pathicon, sizeof(rawpath));
        char* dot = strrchr(rawpath, '.');
        if (dot) {
            strlcpy(dot, ".rgb", sizeof(rawpath) - (size_t)(dot - rawpath));
            bool rawExists = false;
            if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                rawExists = LittleFS.exists(rawpath);
                xSemaphoreGive(filesystemMutex);
            }
            if (rawExists) {
                int iconX = (sprite.width() - 64) / 2;
                if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    File f = LittleFS.open(rawpath, "r");
                    if (f) {
                        uint8_t rowBuf[128]; // 64 pixels × 2 bytes — stack only, no heap
                        for (int row = 0; row < 64; row++) {
                            if (f.read(rowBuf, sizeof(rowBuf)) == sizeof(rowBuf)) {
                                sprite.pushImage(iconX, posY + row, 64, 1, (const lgfx::rgb565_t*)rowBuf);
                            }
                        }
                        f.close();
                    }
                    xSemaphoreGive(filesystemMutex);
                }
            }
        }
    }

    // Show elapsed time since the API data was last updated ("2min ago", "1h4min ago").
    // Falls back to "HH:MM AM/PM" if NTP is not yet synced.
    char buffer[32];
    {
        time_t api_t = 0;
        int y=0, mo=0, dd=0, hour24=0, minute=0;
        if (localCurrent.localtime[0] != '\0' &&
            sscanf(localCurrent.localtime, "%d-%d-%d %d:%d", &y, &mo, &dd, &hour24, &minute) == 5) {
            struct tm t = {};
            t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = dd;
            t.tm_hour = hour24; t.tm_min = minute; t.tm_sec = 0;
            t.tm_isdst = -1;
            api_t = mktime(&t);
        }
        time_t now_t = time(nullptr);
        if (now_t > 1000000000L && api_t > 0) {
            long diff = (long)(now_t - api_t);
            if (diff < 60) {
                strlcpy(buffer, "just now", sizeof(buffer));
            } else if (diff < 3600) {
                snprintf(buffer, sizeof(buffer), "%ldmin ago", diff / 60);
            } else {
                long h = diff / 3600;
                long m = (diff % 3600) / 60;
                if (m == 0) snprintf(buffer, sizeof(buffer), "%ldh ago", h);
                else        snprintf(buffer, sizeof(buffer), "%ldh%ldmin ago", h, m);
            }
        } else {
            // NTP not synced yet — fall back to raw HH:MM AM/PM
            if (api_t > 0) {
                int h12 = hour24 % 12; if (h12 == 0) h12 = 12;
                snprintf(buffer, sizeof(buffer), "%d:%02d %s", h12, minute,
                         hour24 < 12 ? "AM" : "PM");
            } else {
                strlcpy(buffer, "--:-- --", sizeof(buffer));
            }
        }
    }

    char bufferTemp[16];
    char bufferWind[16];

    posY=2;

    DrawSpriteText(sprite, buffer, fontTime, posY, CENTER, colorText);
    posY += 16;

    DrawSpriteText(sprite, localCurrent.location, fontMain, posY, CENTER, colorText);
    posY += 20;

    formatTemeprature(bufferTemp, localCurrent.temp, localCurrent.t_digit, localCurrent.t_unit);
    formatWing(bufferWind, localCurrent.wind, localCurrent.w_digit, localCurrent.w_unit);
    snprintf(buffer, sizeof(buffer), "%s / %s", bufferTemp, bufferWind);

    DrawSpriteText(sprite, buffer, fontMain, posY, CENTER, colorText);

    if (!localCurrent.isFetchOK) {
        sprite.drawRect(0, 0, sprite.width(), sprite.height(), colorBad);
        sprite.drawLine(0, 0, sprite.width()-1, sprite.height()-1, colorBad);
        sprite.drawLine(sprite.width()-1, 0, 0, sprite.height()-1, colorBad);
    }

    if ((isDrawFrame)&&(isForecastLoc)) {
        sprite.drawRect(0, 0, sprite.width(), sprite.height(), colorFrame);
    }

    sprite.pushSprite(geom.x, geom.y);
    sprite.deleteSprite();

}//DrawWeatherCurrent

void DrawWeatherForecast(LGFX &mylcd, LGFX_Sprite &sprite, spriteGeom &geom, WeatherCurrent &localForecast, const uint8_t iday, uint16_t colorBack, const uint16_t colorText, const uint16_t colorBad, const GFXfont* fontTime, const GFXfont* fontMain)
{
    if (!fontTime) return;
    if (!fontMain) return;
    if (iday>2) return;

    // Allocate sprite here — Phase 1.5 (Core 0) has already cached all .rgb icons so
    // convertIconToRgb() in Phase 2 is always a no-op.  No zlib runs after this point,
    // so allocating sequentially inside each DrawWeatherForecast call is safe.
    if (!sprite.createSprite(geom.width, geom.heigth)) {
        DEBUG_PRINTF("[draw] createSprite(%d,%d) OOM — heap free=%u max=%u\n",
                     geom.width, geom.heigth, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        mylcd.fillRect(geom.x, geom.y, geom.width, geom.heigth, colorBad);
        return;
    }
    sprite.fillSprite(colorBack);

    int posY=52;//sprite is drawn first

    // Fast path only — .rgb was pre-decoded by convertIconToRgb() before this call.
    // Read row-by-row into a 128-byte stack buffer: no heap malloc needed, immune
    // to the post-zlib fragmentation that prevents malloc(8192) from succeeding.
    char rawpath[24] = {};
    if (localForecast.TAFpathicon[iday][0] != '\0') {
        strlcpy(rawpath, localForecast.TAFpathicon[iday], sizeof(rawpath));
        char* dot = strrchr(rawpath, '.');
        if (dot) {
            strlcpy(dot, ".rgb", sizeof(rawpath) - (size_t)(dot - rawpath));
            bool rawExists = false;
            if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                rawExists = LittleFS.exists(rawpath);
                xSemaphoreGive(filesystemMutex);
            }
            if (rawExists) {
                int iconX = (sprite.width() - 64) / 2;
                if (xSemaphoreTake(filesystemMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    File f = LittleFS.open(rawpath, "r");
                    if (f) {
                        uint8_t rowBuf[128]; // 64 pixels × 2 bytes — stack only, no heap
                        for (int row = 0; row < 64; row++) {
                            if (f.read(rowBuf, sizeof(rowBuf)) == sizeof(rowBuf)) {
                                sprite.pushImage(iconX, posY + row, 64, 1, (const lgfx::rgb565_t*)rowBuf);
                            }
                        }
                        f.close();
                    }
                    xSemaphoreGive(filesystemMutex);
                }
            }
        }
    }

    char buffer[32], bufMax[32], bufMin[32];

    posY=2;

    DrawSpriteText(sprite, localForecast.TAFday[iday], fontTime, posY, CENTER, colorText);
    posY += 18;

    formatFloatDigit(bufMax, localForecast.TAFmaxtemp[iday], localForecast.t_digit);
    formatFloatDigit(bufMin, localForecast.TAFmintemp[iday], localForecast.t_digit);

    snprintf(buffer, sizeof(buffer), "%s / %s", bufMax, bufMin);
    DrawSpriteText(sprite, buffer, fontMain, posY, CENTER, colorText);
    posY += 20;

    if ((localForecast.TAFchance_of_rain[iday] == 0)&&(localForecast.TAFchance_of_snow[iday] == 0)){
        char text[64];
        strlcpy(text, localForecast.TAFcondition[iday], sizeof(text));
        cleanConditions(text, sizeof(text));
        snprintf(buffer, sizeof(buffer), "%s", text);
    }
    else if (localForecast.TAFchance_of_snow[iday] > 0){//some snow
        snprintf(buffer, sizeof(buffer), "Snow: %d%%", (int)localForecast.TAFchance_of_snow[iday]);
    }
    else {//some rain
        snprintf(buffer, sizeof(buffer), "Rain: %d%%", (int)localForecast.TAFchance_of_rain[iday]);
    }
    DrawSpriteText(sprite, buffer, fontMain, posY, CENTER, colorText);

    if (!localForecast.isFetchOK) {
        sprite.drawRect(0, 0, sprite.width(), sprite.height(), colorBad);
        sprite.drawLine(0, 0, sprite.width()-1, sprite.height()-1, colorBad);
        sprite.drawLine(sprite.width()-1, 0, 0, sprite.height()-1, colorBad);
    }

    sprite.pushSprite(geom.x, geom.y);
    sprite.deleteSprite();

}//DrawWeatherForecast