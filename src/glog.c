/*
   GSPLUS - Advanced Apple IIGS Emulator Environment
   Based on the KEGS emulator written by Kent Dickey
   See COPYRIGHT.txt for Copyright information
   See LICENSE.txt for license (GPL v2)
 */
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include "glog.h"

int glog(const char *s) {
  time_t timer;
  char buffer[26];
  struct tm* tm_info;

  time(&timer);
  tm_info = localtime(&timer);

  strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
  printf("%s - %s\n", buffer, s);

  return 0;
}


int glogf(const char *fmt, ...) {

  time_t timer;
  char buffer[26];
  struct tm* tm_info;

  time(&timer);
  tm_info = localtime(&timer);

  strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

  printf("%s - ", buffer);

  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  fputc('\n', stdout);
  return 0;
}
