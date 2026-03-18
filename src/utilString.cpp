#include "utilString.h"


void cleanConditions(char* text, const size_t textLEN)
{
  //try to have the Condition string at 11 char or less
  if (!text) return;

  if (strstr(text, "loudy") != NULL) {
    if (textLEN>11)
      strlcpy(text, "Cloudy", textLEN);
    return;
  }

  if (strstr(text, "freezing rain") != NULL) {
    if (textLEN>11)
      strlcpy(text, "Ice/Rain", textLEN);
    return;
  }
  if (strstr(text, "freezing drizzle") != NULL) {
    if (textLEN>11)
      strlcpy(text, "Ice/Rain", textLEN);
    return;
  }
  if (strstr(text, "hunder") != NULL) {
    if (textLEN>11)
      strlcpy(text, "Thunder", textLEN);
    return;
  }
  if (strstr(text, "rain") != NULL) {
    if (textLEN>11)
      strlcpy(text, "Rain", textLEN);
    return;
  }
  if (strstr(text, "drizzle") != NULL) {
    if (textLEN>11)
      strlcpy(text, "Drizzle", textLEN);
    return;
  }
  if (strstr(text, "snow") != NULL) {
    if (textLEN>11)
      strlcpy(text, "Snow", textLEN);
    return;
  }
  if (strstr(text, "sleet") != NULL) {
    if (textLEN>11)
      strlcpy(text, "Ice/Rain", textLEN);
    return;
  }
  if (strcmp(text, "Freezing fog") == 0) {
    if (textLEN>11)
      strlcpy(text, "Ice/Fog", textLEN);
    return;
  }

}//cleanConditions