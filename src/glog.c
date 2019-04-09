/*
   GSPLUS - Advanced Apple IIGS Emulator Environment
   Based on the KEGS emulator written by Kent Dickey
   See COPYRIGHT.txt for Copyright information
   See LICENSE.txt for license (GPL v2)
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "glog.h"

extern int x_show_alert(int fatal, const char *str);

#define MAX_FATAL_LOGS          20

int g_fatal_log = 0;
char *g_fatal_log_strs[MAX_FATAL_LOGS];

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


int fatal_printf(const char *fmt, ...)     {
  static char buffer[4096];
  va_list ap;
  int ret;

  va_start(ap, fmt);

  if(g_fatal_log < 0) {
    g_fatal_log = 0;
  }

  ret = vsnprintf(buffer, sizeof(buffer), fmt, ap);

  glog(buffer);

  x_show_alert(1, buffer);
  /*
  if (g_fatal_log < MAX_FATAL_LOGS) {
    g_fatal_log_strs[g_fatal_log++] = strdup(buffer);
  }
  */

  va_end(ap);
  return ret;
}



void clear_fatal_logs()      {
  int i;

  for(i = 0; i < g_fatal_log; i++) {
    free(g_fatal_log_strs[i]);
    g_fatal_log_strs[i] = 0;
  }
  g_fatal_log = 0;
}