/*
 * This file is a consolidation of functions required for tfe
 * emulation taken from the following files
 *
 * lib.c   - Library functions.
 * util.c  - Miscellaneous utility functions.
 * crc32.c
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Matthies <andreas.matthies@gmx.net>
 *  Tibor Biczo <crown@mail.matav.hu>
 *  Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>*
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rawnetsupp.h"



#define CRC32_POLY  0xedb88320
static unsigned long crc32_table[256];
static int crc32_is_initialized = 0;

void lib_free(void *ptr) {
  free(ptr);
}

void *lib_malloc(size_t size) {

  void *ptr = malloc(size);

  if (ptr == NULL && size > 0) {
    perror("lib_malloc");
    exit(-1);
  }

  return ptr;
}

/*-----------------------------------------------------------------------*/

/* Malloc enough space for `str', copy `str' into it and return its
   address.  */
char *lib_stralloc(const char *str) {
  char *ptr;

  if (str == NULL) {
    fprintf(stderr, "lib_stralloc\n");
    exit(-1);
  }

  ptr = strdup(str);
  if (!ptr) {
    perror("lib_stralloc");
    exit(-1);
  }

  return ptr;
}



/* Like realloc, but abort if not enough memory is available.  */
void *lib_realloc(void *ptr, size_t size) {


  void *new_ptr = realloc(ptr, size);

  if (new_ptr == NULL) {
    perror("lib_realloc");
    exit(-1);
  }

  return new_ptr;
}

// Util Stuff

/* Set a new value to the dynamically allocated string *str.
   Returns `-1' if nothing has to be done.  */
int util_string_set(char **str, const char *new_value) {
  if (*str == NULL) {
    if (new_value != NULL)
      *str = lib_stralloc(new_value);
  } else {
    if (new_value == NULL) {
      lib_free(*str);
      *str = NULL;
    } else {
      /* Skip copy if src and dest are already the same.  */
      if (strcmp(*str, new_value) == 0)
        return -1;

      *str = (char *)lib_realloc(*str, strlen(new_value) + 1);
      strcpy(*str, new_value);
    }
  }
  return 0;
}


// crc32 Stuff

unsigned long crc32_buf(const char *buffer, unsigned int len) {
  int i, j;
  unsigned long crc, c;
  const char *p;

  if (!crc32_is_initialized) {
    for (i = 0; i < 256; i++) {
      c = (unsigned long) i;
      for (j = 0; j < 8; j++)
        c = c & 1 ? CRC32_POLY ^ (c >> 1) : c >> 1;
      crc32_table[i] = c;
    }
    crc32_is_initialized = 1;
  }

  crc = 0xffffffff;
  for (p = buffer; len > 0; ++p, --len)
    crc = (crc >> 8) ^ crc32_table[(crc ^ *p) & 0xff];

  return ~crc;
}

void rawnet_hexdump(const void *what, int count) {
  static const char hex[] = "0123456789abcdef";
  char buffer1[16 * 3 + 1];
  char buffer2[16 + 1];
  unsigned offset;
  unsigned char *cp = (unsigned char *)what;


  offset = 0;
  while (count > 0) {
    unsigned char x = *cp++;

    buffer1[offset * 3] = hex[x >> 4];
    buffer1[offset * 3 + 1] = hex[x & 0x0f];
    buffer1[offset * 3 + 2] = ' ';

    buffer2[offset] = (x < 0x80) && isprint(x) ? x : '.';


    --count;
    ++offset;
    if (offset == 16 || count == 0) {
      buffer1[offset * 3] = 0;
      buffer2[offset] = 0;
      fprintf(stderr, "%-50s %s\n", buffer1, buffer2);
      offset = 0;
    }
  }
}
