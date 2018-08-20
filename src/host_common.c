#define _BSD_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

#include "defc.h"
#include "gsos.h"
#include "fst.h"

#include "host_common.h"

#if defined(__APPLE__) 
#include <sys/xattr.h>
#include <sys/attr.h>
#include <sys/paths.h>
#endif

#ifdef __linux__
#include <sys/xattr.h>

#endif


#if defined(_WIN32) || defined(WIN_SDL) 
#include <io.h>
#include <sys/xattr.h>
#endif

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/extattr.h>
#endif

#if defined(_AIX)
#include <sys/ea.h>
#endif

#ifndef XATTR_FINDERINFO_NAME
#define XATTR_FINDERINFO_NAME "com.apple.FinderInfo"
#endif

#ifndef XATTR_RESOURCEFORK_NAME
#define XATTR_RESOURCEFORK_NAME "com.apple.ResourceFork"
#endif


ino_t root_ino = 0;
dev_t root_dev = 0;
char *host_root = NULL;
int host_read_only = 0;

char *g_cfg_host_path = ""; // must not be null.
int g_cfg_host_read_only = 0;
int g_cfg_host_crlf = 1;
int g_cfg_host_merlin = 0;




unsigned host_startup(void) {

	struct stat st;

	if (!g_cfg_host_path) return invalidFSTop;
	if (!*g_cfg_host_path) return invalidFSTop;
	if (host_root) free(host_root);
	host_root = strdup(g_cfg_host_path);

	host_read_only = g_cfg_host_read_only;

	if (stat(host_root, &st) < 0) {
		fprintf(stderr, "%s does not exist\n", host_root);
		return invalidFSTop;
	}
	if (!S_ISDIR(st.st_mode)) {
		fprintf(stderr, "%s is not a directory\n", host_root);
		return invalidFSTop;
	}

	root_ino = st.st_ino;
	root_dev = st.st_dev;

	return 0;
}

void host_shutdown(void) {
	if (host_root) free(host_root);
	host_root = NULL;
	root_ino = 0;
	root_dev = 0;
}

int host_is_root(struct stat *st) {
	return st->st_ino == root_ino && st->st_dev == root_dev;
}


/*
 * simple malloc pool to simplify code.  Should never need > 4 allocations.
 *
 */
static void *gc[16];
static void **gc_ptr = &gc[0];

void *host_gc_malloc(size_t size) {
	if (gc_ptr == &gc[16]) {
		errno = ENOMEM;
		return NULL;
	}

	void *ptr = malloc(size);
	if (ptr) {
		*gc_ptr++ = ptr;
	}
	return ptr;
}


void host_gc_free(void) {

	while (gc_ptr > gc) free(*--gc_ptr);

}

char *host_gc_strdup(const char *src) {
	if (!src) return "";
	if (!*src) return "";
	int len = strlen(src) + 1;
	char *cp = host_gc_malloc(len);
	memcpy(cp, src, len);
	return cp;
}

char *host_gc_append_path(const char *a, const char *b) {

	/* strip all leading /s from b) */
	while (*b == '/') ++b;

	int aa = strlen(a);
	int bb = strlen(b);

	char *cp = host_gc_malloc(aa + bb + 2);
	if (!cp) return NULL;
	memcpy(cp, a, aa);
	int len = aa;

	/* strip all trailing /s from b */
	while (len > 2 && cp[len-1] == '/') --len;
	cp[len++] = '/';
	memcpy(cp + len, b, bb);
	len += bb;
	cp[len] = 0;
	return cp;
}


char *host_gc_append_string(const char *a, const char *b) {
	int aa = strlen(a);
	int bb = strlen(b);

	char *cp = host_gc_malloc(aa + bb + 2);
	if (!cp) return NULL;
	memcpy(cp, a, aa);
	int len = aa;
	memcpy(cp + len, b, bb);
	len += bb;
	cp[len] = 0;
	return cp;
}



/* 
 * text conversion.
 */

void host_cr_to_lf(byte *buffer, size_t size) {
	size_t i;
	for (i = 0; i < size; ++i) {
		if (buffer[i] == '\r') buffer[i] = '\n';
	}
}

void host_lf_to_cr(byte *buffer, size_t size) {
	size_t i;
	for (i = 0; i < size; ++i) {
		if (buffer[i] == '\n') buffer[i] = '\r';
	}
}

void host_merlin_to_text(byte *buffer, size_t size) {
	size_t i;
	for (i = 0; i < size; ++i) {
		byte b = buffer[i];
		if (b == 0xa0) b = '\t';
		b &= 0x7f;
		if (b == '\r') b = '\n';
		buffer[i] = b;
	}
}

void host_text_to_merlin(byte *buffer, size_t size) {
	size_t i;
	for (i = 0; i < size; ++i) {
		byte b = buffer[i];
		if (b == '\t') b = 0xa0;
		if (b == '\n') b = '\r';
		if (b != ' ') b |= 0x80;
		buffer[i] = b;
	}	
}


/*
 * error remapping.
 * NOTE - GS/OS errors are a superset of P8 errors
 */

static word32 enoent(const char *path) {
	/*
		some op on path return ENOENT. check if it's
		fileNotFound or pathNotFound
	 */
	char *p = (char *)path;
	for(;;) {
		struct stat st;
		p = dirname(p);
		if (p == NULL) break;
		if (p[0] == '.' && p[1] == 0) break;
		if (p[0] == '/' && p[1] == 0) break;
		if (stat(p, &st) < 0) return pathNotFound;
	}
	return fileNotFound;
}

word32 host_map_errno(int xerrno) {
	switch(xerrno) {
		case 0: return 0;
		case EBADF:
			return invalidAccess;
		case EDQUOT:
		case EFBIG:
			return volumeFull;
		case ENOENT:
			return fileNotFound;
		case ENOTDIR:
			return pathNotFound;
		case ENOMEM:
			return outOfMem;
		case EEXIST:
			return dupPathname;
		default:
			return drvrIOError;
	}
}

word32 host_map_errno_path(int xerrno, const char *path) {
	if (xerrno == ENOENT) return enoent(path);
	return host_map_errno(xerrno);
}

const char *host_error_name(word16 error) {
	static char *errors[] = {
		"",
		"badSystemCall",
		"",
		"",
		"invalidPcount",
		"",
		"",
		"gsosActive",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		// 0x10
		"devNotFound",
		"invalidDevNum",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		// 0x20
		"drvrBadReq",
		"drvrBadCode",
		"drvrBadParm",
		"drvrNotOpen",
		"drvrPriorOpen",
		"irqTableFull",
		"drvrNoResrc",
		"drvrIOError",
		"drvrNoDevice",
		"drvrBusy",
		"",
		"drvrWrtProt",
		"drvrBadCount",
		"drvrBadBlock",
		"drvrDiskSwitch",
		"drvrOffLine",
		// 0x30
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		// 0x40
		"badPathSyntax",
		"",
		"tooManyFilesOpen",
		"invalidRefNum",
		"pathNotFound",
		"volNotFound",
		"fileNotFound",
		"dupPathname",
		"volumeFull",
		"volDirFull",
		"badFileFormat",
		"badStoreType",
		"eofEncountered",
		"outOfRange",
		"invalidAccess",
		"buffTooSmall",
		// 0x50
		"fileBusy",
		"dirError",
		"unknownVol",
		"paramRangeErr",
		"outOfMem",
		"",
		"badBufferAddress", /* P8 MLI only */
		"dupVolume",
		"notBlockDev",
		"invalidLevel",
		"damagedBitMap",
		"badPathNames",
		"notSystemFile",
		"osUnsupported",
		"",
		"stackOverflow",
		// 0x60
		"dataUnavail",
		"endOfDir",
		"invalidClass",
		"resForkNotFound",
		"invalidFSTID",
		"invalidFSTop",
		"fstCaution",
		"devNameErr",
		"defListFull",
		"supListFull",
		"fstError",
		"",
		"",
		"",
		"",
		"",
		//0x70
		"resExistsErr",
		"resAddErr",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		//0x80
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"networkError"
	};

	if (error < sizeof(errors) / sizeof(errors[0]))
		return errors[error];
	return "";
}



/*
 * File info.
 */


static int hex(byte c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c + 10 - 'a';
	if (c >= 'A' && c <= 'F') return c + 10 - 'A';
	return 0;
}


static int finder_info_to_filetype(const byte *buffer, word16 *file_type, word32 *aux_type) {

	if (!memcmp("pdos", buffer + 4, 4))
	{
		if (buffer[0] == 'p') {
			*file_type = buffer[1];
			*aux_type = (buffer[2] << 8) | buffer[3];
			return 0;
		}
		if (!memcmp("PSYS", buffer, 4)) {
			*file_type = 0xff;
			*aux_type = 0x0000;
			return 0;
		}
		if (!memcmp("PS16", buffer, 4)) {
			*file_type = 0xb3;
			*aux_type = 0x0000;
			return 0;
		}

		// old mpw method for encoding.
		if (!isxdigit(buffer[0]) && isxdigit(buffer[1]) && buffer[2] == ' ' && buffer[3] == ' ')
		{
			*file_type = (hex(buffer[0]) << 8) | hex(buffer[1]);
			*aux_type = 0;
			return 0;
		}
	}
	if (!memcmp("TEXT", buffer, 4)) {
		*file_type = 0x04;
		*aux_type = 0x0000;
		return 0;
	}
	if (!memcmp("BINA", buffer, 4)) {
		*file_type = 0x00;
		*aux_type = 0x0000;
		return 0;
	}
	if (!memcmp("dImgdCpy", buffer, 8)) {
		*file_type = 0xe0;
		*aux_type = 0x0005;
		return 0;
	}

	if (!memcmp("MIDI", buffer, 4)) {
		*file_type = 0xd7;
		*aux_type = 0x0000;
		return 0;
	}

	if (!memcmp("AIFF", buffer, 4)) {
		*file_type = 0xd8;
		*aux_type = 0x0000;
		return 0;
	}

	if (!memcmp("AIFC", buffer, 4)) {
		*file_type = 0xd8;
		*aux_type = 0x0001;
		return 0;
	}

	return -1;
}

int host_file_type_to_finder_info(byte *buffer, word16 file_type, word32 aux_type) {
	if (file_type > 0xff || aux_type > 0xffff) return -1;

	if (!file_type && aux_type == 0x0000) {
		memcpy(buffer, "BINApdos", 8);
		return 0;
	}

	if (file_type == 0x04 && aux_type == 0x0000) {
		memcpy(buffer, "TEXTpdos", 8);
		return 0;
	}

	if (file_type == 0xff && aux_type == 0x0000) {
		memcpy(buffer, "PSYSpdos", 8);
		return 0;
	}

	if (file_type == 0xb3 && aux_type == 0x0000) {
		memcpy(buffer, "PS16pdos", 8);
		return 0;
	}

	if (file_type == 0xd7 && aux_type == 0x0000) {
		memcpy(buffer, "MIDIpdos", 8);
		return 0;
	}
	if (file_type == 0xd8 && aux_type == 0x0000) {
		memcpy(buffer, "AIFFpdos", 8);
		return 0;
	}
	if (file_type == 0xd8 && aux_type == 0x0001) {
		memcpy(buffer, "AIFCpdos", 8);
		return 0;
	}
	if (file_type == 0xe0 && aux_type == 0x0005) {
		memcpy(buffer, "dImgdCpy", 8);
		return 0;
	}


	memcpy(buffer, "p   pdos", 8);
	buffer[1] = (file_type) & 0xff;
	buffer[2] = (aux_type >> 8) & 0xff;
	buffer[3] = (aux_type) & 0xff;
	return 0;
}


#if defined(__APPLE__)
void host_get_file_xinfo(const char *path, struct file_info *fi) {

	ssize_t tmp;
	tmp = getxattr(path, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, 0);
	if (tmp < 0) tmp = 0;
	fi->resource_eof = tmp;
	fi->resource_blocks = (tmp + 511) / 512;

	tmp = getxattr(path, XATTR_FINDERINFO_NAME, fi->finder_info, 32, 0, 0);
	if (tmp == 16 || tmp == 32){
		fi->has_fi = 1;

		finder_info_to_filetype(fi->finder_info, &fi->file_type, &fi->aux_type);
	}
}
#elif defined(__sun)
void host_get_file_xinfo(const char *path, struct file_info *fi) {

	struct stat st;

	// can't stat an xattr directly?
	int fd;
	fd = attropen(path, XATTR_RESOURCEFORK_NAME, O_RDONLY);
	if (fd >= 0) {
		if (fstat(fd, &st) == 0) {
			fi->resource_eof = st.st_size;
			fi->resource_blocks = st.st_blocks;
		}
		close(fd);
	}

	fd = attropen(path, XATTR_FINDERINFO_NAME, O_RDONLY);
	if (fd >= 0) {
		int tmp = read(fd, fi->finder_info, 32);
		if (tmp == 16 || tmp == 32) {
			fi->has_fi = 1;
			finder_info_to_filetype(fi->finder_info, &fi->file_type, &fi->aux_type);
		}
		close(fd);
	}
}
#elif defined(__linux__)
void host_get_file_xinfo(const char *path, struct file_info *fi) {

	ssize_t tmp;
	tmp = getxattr(path, "user.com.apple.ResourceFork", NULL, 0);
	if (tmp < 0) tmp = 0;
	fi->resource_eof = tmp;
	fi->resource_blocks = (tmp + 511) / 512;

	tmp = getxattr(path, "user.com.apple.FinderInfo", fi->finder_info, 32);
	if (tmp == 16 || tmp == 32){
		fi->has_fi = 1;

		finder_info_to_filetype(fi->finder_info, &fi->file_type, &fi->aux_type);
	}
}
#else
void host_get_file_xinfo(const char *path, struct file_info *fi) {
}
#endif

#undef _
#define _(a, b, c) { a, sizeof(a) - 1, b, c }
struct ftype_entry {
	char *ext;
	unsigned length;
	unsigned file_type;
	unsigned aux_type;
};

static struct ftype_entry suffixes[] = {
	_("c",    0xb0, 0x0008),
	_("cc",   0xb0, 0x0008),
	_("h",    0xb0, 0x0008),
	_("rez",  0xb0, 0x0015),
	_("asm",  0xb0, 0x0003),
	_("mac",  0xb0, 0x0003),
	_("pas",  0xb0, 0x0005),
	_("txt",  0x04, 0x0000),
	_("text", 0x04, 0x0000),
	_("s",    0x04, 0x0000),
	{ 0, 0, 0, 0}
};

static struct ftype_entry prefixes[] = {
	_("m16.",  0xb0, 0x0003),
	_("e16.",  0xb0, 0x0003),
	{ 0, 0, 0, 0}
};

#undef _

word32 host_get_file_info(const char *path, struct file_info *fi) {
	struct stat st;
	memset(fi, 0, sizeof(*fi));

	int ok = stat(path, &st);
	if (ok < 0) return host_map_errno(errno);

	fi->eof = st.st_size;
	fi->blocks = st.st_blocks;

	fi->create_date = st.st_ctime;
	fi->modified_date = st.st_mtime;

#if defined(__APPLE__) 
	fi->create_date = st.st_birthtime;
#endif


	fi->st_mode = st.st_mode;

	if (S_ISDIR(st.st_mode)) {
		fi->storage_type = directoryFile;
		fi->file_type = 0x0f;
		if (host_is_root(&st))
			fi->storage_type = 0x0f;
	} else if (S_ISREG(st.st_mode)) {
		fi->file_type = 0x06;
		if (st.st_size < 0x200) fi->storage_type = seedling;
		else if (st.st_size < 0x20000) fi->storage_type = sapling;
		else fi->storage_type = tree;
	} else {
		fi->storage_type = st.st_mode & S_IFMT;
		fi->file_type = 0;
	}
	// 0x01 = read enable
	// 0x02 = write enable
	// 0x04 = invisible
	// 0x08 = reserved
	// 0x10 = reserved
	// 0x20 = backup needed
	// 0x40 = rename enable
	// 0x80 = destroy enable

	fi->access = 0xc3; // placeholder...

	if (S_ISREG(st.st_mode)) {
		host_get_file_xinfo(path, fi);

		if (!fi->has_fi) {
			/* guess the file type / auxtype based on extension */
			int n;
			const char *dot = NULL;
			const char *slash = NULL;

			for(n = 0; ; ++n) {
				char c = path[n];
				if (c == 0) break;
				else if (c == '/') { slash = path + n + 1; dot = NULL; }
				else if (c == '.') dot = path + n + 1;
			}

			if (dot && *dot) {
				for (n = 0; n < sizeof(suffixes) / sizeof(suffixes[0]); ++n) {
					if (!suffixes[n].ext) break;
					if (!strcasecmp(dot, suffixes[n].ext)) {
						fi->file_type = suffixes[n].file_type;
						fi->aux_type = suffixes[n].aux_type;
						break;
					}
				}
			}
		}
	}

	// get file type/aux type

	if (fi->resource_eof) fi->storage_type = extendedFile;

	return 0;
}




#if defined(__APPLE__)
word32 host_set_file_info(const char *path, struct file_info *fi) {

	int ok;
	struct attrlist list;
	unsigned i = 0;
	struct timespec dates[2];

	if (fi->has_fi && fi->storage_type != 0x0d) {
		ok = setxattr(path, XATTR_FINDERINFO_NAME, fi->finder_info, 32, 0, 0);
		if (ok < 0) return host_map_errno(errno); 
	}	


	memset(&list, 0, sizeof(list));
	memset(dates, 0, sizeof(dates));

	list.bitmapcount = ATTR_BIT_MAP_COUNT;
	list.commonattr  = 0;

	if (fi->create_date)
	{
		dates[i++].tv_sec = fi->create_date;
		list.commonattr |= ATTR_CMN_CRTIME;
	}

	if (fi->modified_date)
	{
		dates[i++].tv_sec = fi->modified_date;
		list.commonattr |= ATTR_CMN_MODTIME;
	}

	ok = 0;
	if (i) ok = setattrlist(path, &list, dates, i * sizeof(struct timespec), 0);
	return 0;
}
#elif defined(__sun) 
word32 host_set_file_info(const char *path, struct file_info *fi) {

	if (fi->has_fi && fi->storage_type != 0x0d) {
		int fd = attropen(path, XATTR_FINDERINFO_NAME, O_WRONLY | O_CREAT, 0666);
		if (fd < 0) return host_map_errno(errno);
		write(fd, fi->finder_info, 32);
		close(fd);
	}

	if (fi->modified_date) {
		struct timeval times[2];

		memset(times, 0, sizeof(times));

		//times[0] = 0; // access
		times[1].tv_sec = fi.modified_date; // modified
		int ok = utimes(path, times);
		if (ok < 0) return host_map_errno(errno);
	}
	return 0;
}
#elif defined(__linux__)
word32 host_set_file_info(const char *path, struct file_info *fi) {

	if (fi->has_fi && fi->storage_type != 0x0d) {
		int ok = setxattr(path, "user.apple.FinderInfo", fi->finder_info, 32, 0);
		if (ok < 0) return host_map_errno(errno);
	}

	if (fi->modified_date) {
		struct timeval times[2];

		memset(times, 0, sizeof(times));

		//times[0] = 0; // access
		times[1].tv_sec = fi->modified_date; // modified
		int ok = utimes(path, times);
		if (ok < 0) return host_map_errno(errno);
	}
	return 0;
}

#else

word32 host_set_file_info(const char *path, struct file_info *fi) {

	if (fi->modified_date) {

		struct timeval times[2];

		memset(times, 0, sizeof(times));

		times[0] = 0; // access
		times[1].tv_sec = fi->modified_date; // modified

		int ok = utimes(path, times);
		if (ok < 0) return host_map_errno(errno);
	}
	return 0;
}

#endif


/*
 * Date/time conversion
 */

/*
 * converts time_t to a gs/os readhextime date/time record.
 */

void host_set_date_time_rec(word32 ptr, time_t time) {

	if (time == 0) {
		for (int i = 0; i < 8; ++i) set_memory_c(ptr++, 0, 0);
		return;
	}

	struct tm *tm = localtime(&time);
	if (tm->tm_sec == 60) tm->tm_sec = 59; /* leap second */

	set_memory_c(ptr++, tm->tm_sec, 0);
	set_memory_c(ptr++, tm->tm_min, 0);
	set_memory_c(ptr++, tm->tm_hour, 0);
	set_memory_c(ptr++, tm->tm_year, 0);
	set_memory_c(ptr++, tm->tm_mday - 1, 0);
	set_memory_c(ptr++, tm->tm_mon, 0);
	set_memory_c(ptr++, 0, 0);
	set_memory_c(ptr++, tm->tm_wday + 1, 0);
}

/*
 * converts time_t to a prodos16 date/time record.
 */
void host_set_date_time(word32 ptr, time_t time) {

	if (time == 0) {
		for (int i = 0; i < 4; ++i) set_memory_c(ptr++, 0, 0);
		return;
	}

	struct tm *tm = localtime(&time);

	word16 tmp = 0;
	tmp |= (tm->tm_year % 100) << 9;
	tmp |= tm->tm_mon << 5;
	tmp |= tm->tm_mday;

	set_memory16_c(ptr, tmp, 0);
	ptr += 2;

	tmp = 0;
	tmp |= tm->tm_hour << 8;
	tmp |= tm->tm_min;
	set_memory16_c(ptr, tmp, 0);
}

word32 host_convert_date_time(time_t time) {

	if (time == 0) return 0;

	struct tm *tm = localtime(&time);

	word16 dd = 0;
	dd |= (tm->tm_year % 100) << 9;
	dd |= tm->tm_mon << 5;
	dd |= tm->tm_mday;

	word16 tt = 0;
	tt |= tm->tm_hour << 8;
	tt |= tm->tm_min;


	return (tt << 16) | dd;
}


time_t host_get_date_time(word32 ptr) {

	word16 a = get_memory16_c(ptr + 0, 0);
	word16 b = get_memory16_c(ptr + 2, 0);
	if (!a && !b) return 0;

	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	tm.tm_year = (a >> 9) & 0x7f;
	tm.tm_mon = ((a >> 5) & 0x0f) - 1;
	tm.tm_mday = (a >> 0) & 0x1f;

	tm.tm_hour = (b >> 8) & 0x1f;
	tm.tm_min = (b >> 0) & 0x3f;
	tm.tm_sec = 0;

    tm.tm_isdst = -1;

	// 00 - 39 => 2000-2039
	// 40 - 99 => 1940-1999
	if (tm.tm_year < 40) tm.tm_year += 100;


	return mktime(&tm);
}

time_t host_get_date_time_rec(word32 ptr) {

	byte buffer[8];
	for (int i = 0; i < 8; ++i) buffer[i] = get_memory_c(ptr++, 0);

	if (!memcmp(buffer, "\x00\x00\x00\x00\x00\x00\x00\x00", 8)) return 0;

	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	tm.tm_sec = buffer[0];
	tm.tm_min = buffer[1];
	tm.tm_hour = buffer[2];
	tm.tm_year = buffer[3];
	tm.tm_mday = buffer[4] + 1;
	tm.tm_mon = buffer[5];
    tm.tm_isdst = -1;

	return mktime(&tm);
}


void host_hexdump(word32 address, int size) {
	const char *HexMap = "0123456789abcdef";

	char buffer1[16 * 3 + 1 + 1];
	char buffer2[16 + 1];
	unsigned i, j;

	printf("\n");
	while (size > 0) {
		memset(buffer1, ' ', sizeof(buffer1));
		memset(buffer2, ' ', sizeof(buffer2));

		int linelen = size;
		if (linelen > 16) linelen = 16;

		for (i = 0, j = 0; i < linelen; i++) {
			unsigned x = get_memory_c(address + i, 0);
			buffer1[j++] = HexMap[x >> 4];
			buffer1[j++] = HexMap[x & 0x0f];
			j++;
			if (i == 7) j++;
			x &= 0x7f;
			// isascii not part of std:: and may be a macro.
			buffer2[i] = isascii(x) && isprint(x) ? x : '.';
		}

		buffer1[sizeof(buffer1) - 1] = 0;
		buffer2[sizeof(buffer2) - 1] = 0;

		printf("%06x:\t%s\t%s\n",
			address, buffer1, buffer2);
		address += 16;
		size -= 16;
	}
	printf("\n");
}


void host_hexdump_native(void *data, unsigned address, int size) {
	const char *HexMap = "0123456789abcdef";

	char buffer1[16 * 3 + 1 + 1];
	char buffer2[16 + 1];
	unsigned i, j;

	printf("\n");
	while (size > 0) {
		memset(buffer1, ' ', sizeof(buffer1));
		memset(buffer2, ' ', sizeof(buffer2));

		int linelen = size;
		if (linelen > 16) linelen = 16;

		for (i = 0, j = 0; i < linelen; i++) {
			unsigned x = ((byte *)data)[address + i];
			buffer1[j++] = HexMap[x >> 4];
			buffer1[j++] = HexMap[x & 0x0f];
			j++;
			if (i == 7) j++;
			x &= 0x7f;
			// isascii not part of std:: and may be a macro.
			buffer2[i] = isascii(x) && isprint(x) ? x : '.';
		}

		buffer1[sizeof(buffer1) - 1] = 0;
		buffer2[sizeof(buffer2) - 1] = 0;

		printf("%06x:\t%s\t%s\n",
			address, buffer1, buffer2);
		address += 16;
		size -= 16;
	}
	printf("\n");
}
