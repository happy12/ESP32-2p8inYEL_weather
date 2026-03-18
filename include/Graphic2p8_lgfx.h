#ifndef MATHIEU_GRAPHICLGFX_h
#define MATHIEU_GRAPHICLGFX_h
#pragma once

#include <LovyanGFX.hpp>
#include "variables.h"
#include "utilString.h"

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_XPT2046 _touch_instance;

public:
    LGFX(void);
};
extern LGFX lcd;


enum xJustification { LEFT=1, CENTER=2, RIGHT=3 };

void DrawSplashScreen(LGFX &mylcd, const uint16_t* image, const uint16_t W, const uint16_t H);

void DrawSpriteText(LGFX_Sprite &sprite, const char* text, const GFXfont* font, int y, const xJustification xjust, const uint16_t color);

// Call on Core 1 BEFORE any main sprite is allocated.
// Decodes the PNG at pngpath to a raw .rgb file so DrawWeatherCurrent never needs drawPng.
void convertIconToRgb(LGFX &mylcd, const char* pngpath, const uint16_t colorBack = 0);

void DrawWeatherCurrent(LGFX &mylcd, LGFX_Sprite &sprite, spriteGeom &geom, WeatherCurrent &localCurrent, const uint16_t colorBack, const uint16_t colorText, const uint16_t colorBad, const uint16_t colorFrame, const GFXfont* fontTime, const GFXfont* fontMain, const bool isDrawFrame, bool isForecastLoc = false);
void DrawWeatherForecast(LGFX &mylcd, LGFX_Sprite &sprite, spriteGeom &geom, WeatherCurrent &localForecast, const uint8_t iday, const uint16_t colorBack, const uint16_t colorText, const uint16_t colorBad, const GFXfont* fontTime, const GFXfont* fontMain);




#endif