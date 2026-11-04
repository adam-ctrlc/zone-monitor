#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

#include "app_log.h"

void appLog(const char* format, ...) {

  char line[96];

  va_list args;

  va_start(args, format);

  vsnprintf(line, sizeof(line), format, args);

  va_end(args);

  Serial.println(line);
}
