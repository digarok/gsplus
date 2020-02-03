#include <string.h>
#include <ctype.h>

#include "string_extra.h"

/* from MUSL/MIT */

#ifndef HAVE_STRCASESTR
char *strcasestr(const char *h, const char *n)
{
	size_t l = strlen(n);
	for (; *h; h++) if (!strncasecmp(h, n, l)) return (char *)h;
	return 0;
}
#endif

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *l, const char *r)
{
	for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++);
	return tolower(*l) - tolower(*r);
}
#endif

#ifndef HAVE_STRNCASECMP

int strncasecmp(const char *l, const char *r, size_t n)
{
	if (!n--) return 0;
	for (; *l && *r && n && (*l == *r || tolower(*l) == tolower(*r)); l++, r++, n--);
	return tolower(*l) - tolower(*r);
}
#endif