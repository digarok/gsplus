/*
 * host_fst.c
 */

#define _WIN32_WINNT 0x0600 // vista+
#include <Windows.h>

#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>

#include "defc.h"
#include "gsos.h"
#include "fst.h"


extern Engine_reg engine;


/* direct page offsets */

#define dp_call_number 0x30
#define dp_param_blk_ptr 0x32
#define dp_dev1_num 0x36
#define dp_dev2_num 0x38
#define dp_path1_ptr 0x3a
#define dp_path2_ptr 0x3e
#define dp_path_flag 0x42

#define global_buffer 0x009a00

enum {
	regular_file,
	resource_file,
	directory_file,
};


struct directory {
	int displacement;
	int num_entries;
	char *entries[1];
};

struct fd_entry {
	struct fd_entry *next;
	char *path;
	int cookie;
	int type;
	int access;
	HANDLE handle;
	struct directory *dir;
};

#pragma pack(push, 2)
struct AFP_Info {
	uint32_t magic;
	uint32_t version;
	uint32_t file_id;
	uint32_t backup_date;
	uint8_t finder_info[32];
	uint16_t prodos_file_type;
	uint32_t prodos_aux_type;
	uint8_t reserved[6];
};
#pragma pack(pop)


static void free_directory(struct directory *dd);
static struct directory *read_directory(const char *path, word16 *error);

static int file_type_to_finder_info(byte *buffer, word16 file_type, word32 aux_type);
static int finder_info_to_filetype(const byte *buffer, word16 *file_type, word32 *aux_type);


static void afp_init(struct AFP_Info *info, word16 file_type, word32 aux_type) {
	//static_assert(sizeof(AFP_Info) == 60, "Incorrect AFP_Info size");
	memset(info, 0, sizeof(*info));
	info->magic = 0x00504641;
	info->version = 0x00010000;
	info->prodos_file_type = file_type;
	info->prodos_aux_type = aux_type;
	if (file_type || aux_type)
		file_type_to_finder_info(info->finder_info, file_type, aux_type);
}

static BOOL afp_verify(struct AFP_Info *info) {
	if (!info) return 0;

	if (info->magic != 0x00504641) return 0;
	if (info->version != 0x00010000) return 0;

	return 1;
}


static int afp_to_filetype(struct AFP_Info *info, word16 *file_type, word32 *aux_type) {
	// check for prodos ftype/auxtype...
	if (info->prodos_file_type || info->prodos_aux_type) {
		*file_type = info->prodos_file_type;
		*aux_type = info->prodos_aux_type;
		return 0;
	}
	int ok = finder_info_to_filetype(info->finder_info, file_type, aux_type);
	if (ok == 0) {
		info->prodos_file_type = *file_type;
		info->prodos_aux_type = *aux_type;
	}
	return 0;
}

enum { prefer_prodos, prefer_hfs };
static void afp_synchronize(struct AFP_Info *info, int preference) {
	// if ftype/auxtype is inconsistent between prodos and finder info, use
	// prodos as source of truth.
	word16 f;
	word32 a;
	if (finder_info_to_filetype(info->finder_info, &f, &a) != 0) return;
	if (f == info->prodos_file_type && a == info->prodos_aux_type) return;
	if (preference == prefer_prodos)
		file_type_to_finder_info(info->finder_info, info->prodos_file_type, info->prodos_aux_type);
	else {
		info->prodos_file_type = f;
		info->prodos_aux_type = a;
	}
}


#define COOKIE_BASE 0x8000

static word32 cookies[32] = {};

static int alloc_cookie() {
	for (int i = 0; i < 32; ++i) {
		word32 x = cookies[i];

		for (int j = 0; j < 32; ++j, x >>= 1) {
			if (x & 0x01) continue;

			cookies[i] |= (1 << j);
			return COOKIE_BASE + (i * 32 + j);
		}
	}
	return -1;
}

static int free_cookie(int cookie) {
	if (cookie < COOKIE_BASE) return -1;
	cookie -= COOKIE_BASE;
	if (cookie >= 32 * 32) return -1;

	int chunk = cookie / 32;
	int offset = 1 << (cookie % 32);

	word32 x = cookies[chunk];

	if ((x & offset) == 0) return -1;
	x &= ~offset;
	cookies[chunk] = x;
	return 0;
}

static struct fd_entry *fd_head = NULL;

static DWORD root_file_id[3] = {};
//static ino_t root_ino = 0;
//static dev_t root_dev = 0;
static char *root = NULL;
static int read_only = 0;

char *g_cfg_host_path = ""; // must not be null.
int g_cfg_host_read_only = 0;


/*
 * simple malloc pool to simplify code.  Should never need > 4 allocations.
 *
 */
static void *gc[16];
static void **gc_ptr = &gc[0];
static void *gc_malloc(size_t size) {
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


static void gc_free(void) {

	while (gc_ptr > gc) free(*--gc_ptr);

}

static char *append_path(const char *a, const char *b) {
	int aa = strlen(a);
	int bb = strlen(b);

	char *cp = gc_malloc(aa + bb + 2);
	if (!cp) return NULL;
	memcpy(cp, a, aa);
	int len = aa;
	if (len && cp[len-1] != '/' && b[0] != '/') cp[len++] = '/';
	memcpy(cp + len, b, bb);
	len += bb;
	cp[len] = 0;
	while (len > 2 && cp[len - 1] == '/') cp[--len] = 0;
	return cp;
}


static char *append_string(const char *a, const char *b) {
	int aa = strlen(a);
	int bb = strlen(b);

	char *cp = gc_malloc(aa + bb + 2);
	if (!cp) return NULL;
	memcpy(cp, a, aa);
	int len = aa;
	memcpy(cp + len, b, bb);
	len += bb;
	cp[len] = 0;
	return cp;
}

static char *gc_strdup(const char *src) {
	if (!src) return "";
	if (!*src) return "";
	int len = strlen(src) + 1;
	char *cp = gc_malloc(len);
	memcpy(cp, src, len);
	return cp;
}


static word32 map_errno() {
	switch(errno) {
		case 0: return 0;
		case EBADF:
			return invalidAccess;
#ifdef EDQUOT
		case EDQUOT:
#endif
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

static word32 map_last_error() {
	DWORD e = GetLastError();
	switch (e) {
		case ERROR_NO_MORE_FILES:
			return endOfDir;
		case ERROR_FILE_NOT_FOUND:
			return fileNotFound;
		case ERROR_PATH_NOT_FOUND:
			return pathNotFound;
		case ERROR_INVALID_ACCESS:
			return invalidAccess;
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:
			return dupPathname;
		case ERROR_DISK_FULL:
			return volumeFull;
		case ERROR_INVALID_PARAMETER:
			return paramRangeErr;
		case ERROR_DRIVE_LOCKED:
			return drvrWrtProt;
		case ERROR_NEGATIVE_SEEK:
			return outOfRange;
		case ERROR_SHARING_VIOLATION:
			return fileBusy; // destroy open file, etc.
		case ERROR_DIR_NOT_EMPTY:
			// ...
		default:
			fprintf(stderr, "GetLastError: %08x - %d\n", (int)e, (int)e);
			return drvrIOError;
	}
}

static struct fd_entry *find_fd(int cookie) {
	struct fd_entry *head = fd_head;

	while (head) {
		if (head->cookie == cookie) return head;
		head = head->next;
	}
	return NULL;
}


static void free_fd(struct fd_entry	*e) {
	if (!e) return;
	if (e->cookie) free_cookie(e->cookie);
	if (e->handle != INVALID_HANDLE_VALUE) CloseHandle(e->handle);
	if (e->dir) free_directory(e->dir);
	free(e->path);
	free(e);
}

static struct fd_entry *alloc_fd() {
	struct fd_entry *e = calloc(sizeof(struct fd_entry), 1);
	e->handle = INVALID_HANDLE_VALUE;
	return e;
}

static word32 remove_fd(int cookie) {
	word32 rv = invalidRefNum;

	struct fd_entry *prev = NULL;
	struct fd_entry *head = fd_head;
	while (head) {
		if (head->cookie == cookie) {
			if (prev) prev->next = head->next;
			else fd_head = head->next;

			free_fd(head);
			rv = 0;
			break;
		}
		prev = head;
		head = head->next;
	}
	return rv;
}


struct file_info {

	FILETIME create_date;
	FILETIME modified_date;

	word16 access;
	word16 storage_type;
	word16 file_type;
	word32 aux_type;
	word32 eof;
	word32 blocks;
	word32 resource_eof;
	word32 resource_blocks;
	int has_fi;
	//byte finder_info[32];
	struct AFP_Info afp;
};

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

static int file_type_to_finder_info(byte *buffer, word16 file_type, word32 aux_type) {
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



static void get_file_xinfo(const char *path, struct file_info *fi) {

	HANDLE h;

	char *p = append_string(path, ":AFP_Resource");
	LARGE_INTEGER size = { 0 };


	h = CreateFile(p, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h != INVALID_HANDLE_VALUE) {
		GetFileSizeEx(h, &size);
		CloseHandle(h);
	}
	fi->resource_eof = size.LowPart;
	fi->resource_blocks = (size.LowPart + 511) / 512;


	p = append_string(path, ":AFP_AfpInfo");

	h = CreateFile(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (h != INVALID_HANDLE_VALUE) {
		DWORD read = 0;
		if (ReadFile(h, &fi->afp, sizeof(struct AFP_Info), &read, NULL) && read == sizeof(struct AFP_Info)) {
			if (afp_verify(&fi->afp)) fi->has_fi = 1;
			afp_to_filetype(&fi->afp, &fi->file_type, &fi->aux_type);
		}
		CloseHandle(h);
	}
}

static word16 map_attributes(DWORD dwFileAttributes) {

	// 0x01 = read enable
	// 0x02 = write enable
	// 0x04 = invisible
	// 0x08 = reserved
	// 0x10 = reserved
	// 0x20 = backup needed
	// 0x40 = rename enable
	// 0x80 = destroy enable

	word16 access = 0;

	if (dwFileAttributes & FILE_ATTRIBUTE_READONLY)
		access = readEnable;
	else
		access = readEnable | writeEnable | renameEnable | destroyEnable;

	if (dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
		access |= fileInvisible;

	// map FILE_ATTRIBUTE_ARCHIVE to backup needed bit?

	return access;
}

static word32 get_file_info(const char *path, struct file_info *fi) {


	HANDLE h = CreateFile(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h == INVALID_HANDLE_VALUE) return map_last_error();

	//FILE_BASIC_INFO fbi;
	//FILE_STANDARD_INFO fsi;
	//FILE_ID_INFO fii;
 	BY_HANDLE_FILE_INFORMATION info;


	memset(fi, 0, sizeof(*fi));
	//memset(&fbi, 0, sizeof(fbi));
	//memset(&fsi, 0, sizeof(fsi));

	GetFileInformationByHandle(h, &info);
	//GetFileInformationByHandleEx(h, FileBasicInfo, &fbi, sizeof(fbi));
	//GetFileInformationByHandleEx(h, FileStandardInfo, &fsi, sizeof(fsi));
 	//GetFileInformationByHandleEx(h, FileIdInfo, &fii, sizeof(fii));

	word32 size = info.nFileSizeLow;

	fi->eof = size;
	fi->blocks = (size + 511) / 512;

	fi->create_date = info.ftCreationTime;
	fi->modified_date = info.ftLastWriteTime;

	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		fi->storage_type = directoryFile;
		fi->file_type = 0x0f;

	 	DWORD id[3] = { info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow };
 		if (memcmp(&id, &root_file_id, sizeof(root_file_id)) == 0) {
			fi->storage_type = 0x0f;
 		}
	} else {
		fi->file_type = 0x06;
		if (size < 0x200) fi->storage_type = seedling;
		else if (size < 0x20000) fi->storage_type = sapling;
		else fi->storage_type = tree;

		get_file_xinfo(path, fi);
		if (fi->resource_eof) fi->storage_type = extendedFile;
	}
	// 0x01 = read enable
	// 0x02 = write enable
	// 0x04 = invisible
	// 0x08 = reserved
	// 0x10 = reserved
	// 0x20 = backup needed
	// 0x40 = rename enable
	// 0x80 = destroy enable

	word16 access = 0;

	if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
		access = 0x01;
	else
		access = 0x01 | 0x02 | 0x40 | 0x80;

	if (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
		access |= 0x04;

	// map FILE_ATTRIBUTE_ARCHIVE to backup needed bit?

	fi->access = map_attributes(info.dwFileAttributes);


	CloseHandle(h);
	return 0;
}


static word32 set_file_info(const char *path, struct file_info *fi) {

	if (fi->has_fi && fi->storage_type != 0x0d && fi->storage_type != 0x0f) {
		char *rpath = append_string(path, ":AFP_AfpInfo");

		HANDLE h = CreateFile(rpath, GENERIC_WRITE, 
			FILE_SHARE_READ , NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) return map_last_error();

		WriteFile(h, &fi->afp, sizeof(struct AFP_Info), NULL, NULL);
		CloseHandle(h);
	}


	if (fi->create_date.dwLowDateTime || fi->create_date.dwHighDateTime 
		|| fi->modified_date.dwLowDateTime || fi->modified_date.dwHighDateTime) {
		// SetFileInformationByHandle can modify dates.
		HANDLE h;
		h = CreateFile(path, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) return map_last_error();

		FILE_BASIC_INFO fbi;
		FILE_BASIC_INFO newfbi;
		memset(&fbi, 0, sizeof(fbi));
		memset(&newfbi, 0, sizeof(newfbi));

		BOOL ok;
		ok = GetFileInformationByHandleEx(h, FileBasicInfo, &fbi, sizeof(fbi));

		int delta = 0;

		word16 old_access = map_attributes(fbi.FileAttributes);
		if (fi->access && fi->access != old_access) {
			newfbi.FileAttributes = fbi.FileAttributes;

			if (fi->access & fileInvisible) {
				delta = 1;
				newfbi.FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
			}
			// hfs fst only marks it read enable if all are clear.
			word16 locked = writeEnable | destroyEnable | renameEnable;
			if ((fi->access & locked) == 0) {
				delta = 1;
				newfbi.FileAttributes |= FILE_ATTRIBUTE_READONLY;
			}
		}

		// todo -- compare against nt file time to see if it's actually changed.
		// to prevent time stamp truncation.

		if (fi->create_date.dwLowDateTime || fi->create_date.dwHighDateTime) {
			delta = 1;
			newfbi.CreationTime.LowPart = fi->create_date.dwLowDateTime;
			newfbi.CreationTime.HighPart = fi->create_date.dwHighDateTime;
		}
		if (fi->modified_date.dwLowDateTime || fi->modified_date.dwHighDateTime) {
			delta = 1;
			newfbi.LastWriteTime.LowPart = fi->modified_date.dwLowDateTime;
			newfbi.LastWriteTime.HighPart = fi->modified_date.dwHighDateTime;
			//newfbi.ChangeTime.LowPart = fi->modified_date.dwLowDateTime; //?
			//newfbi.ChangeTime.HighPart = fi->modified_date.dwHighDateTime; //?
		}

		if (delta)
			ok = SetFileInformationByHandle(h, FileBasicInfo, &newfbi, sizeof(newfbi));
		CloseHandle(h);
	}
	return 0;
}

/*
 * if this is an absolute path, verify and skip past :Host:
 *
 */
static const char *check_path(const char *in, word32 *error) {

	word32 tmp;
	if (!error) error = &tmp;

	*error = 0;
	if (!in) return "";

	/* check for .. */
	const char *cp = in;
	do {
		while (*cp == '/') ++cp;
		if (cp[0] == '.' && cp[1] == '.' && (cp[2] == '/' || cp[2] == 0))
		{
			*error = badPathSyntax;
			return NULL;
		}
		cp = strchr(cp, '/');
	} while(cp);


	if (in[0] != '/') return in;

	if (strncasecmp(in, "/Host", 5) == 0 && (in[5] == '/' || in[5] == 0)) {
		in += 5;
		while (*in == '/') ++in;
		return in;
	}
	*error = volNotFound;
	return NULL;
}


static char * get_gsstr(word32 ptr) {
	if (!ptr) return NULL;
	int length = get_memory16_c(ptr, 0);
	ptr += 2;
	char *str = gc_malloc(length + 1);
	for (int i = 0; i < length; ++i) {
		char c = get_memory_c(ptr+i, 0);
		if (c == ':') c = '/';
		str[i] = c;
	}
	str[length] = 0;
	return str;
}

static char * get_pstr(word32 ptr) {
	if (!ptr) return NULL;
	int length = get_memory16_c(ptr, 0);
	ptr += 2;
	char *str = gc_malloc(length + 1);
	for (int i = 0; i < length; ++i) {
		char c = get_memory_c(ptr+i, 0);
		if (c == ':') c = '/';
		str[i] = c;
	}
	str[length] = 0;
	return str;
}

static word32 set_gsstr(word32 ptr, const char *str) {

	if (!ptr) return paramRangeErr;

	int l = str ? strlen(str) : 0;


	word32 cap = get_memory16_c(ptr, 0);
	ptr += 2;

	if (cap < 4) return paramRangeErr;

	set_memory16_c(ptr, l, 0);
	ptr += 2;

	if (cap < l + 4) return buffTooSmall;


	for (int i = 0; i < l; ++i) {
		char c = *str++;
		if (c == '/') c = ':';
		set_memory_c(ptr++, c, 0);
	}
	return 0;
}

static word32 set_gsstr_truncate(word32 ptr, const char *str) {

	if (!ptr) return paramRangeErr;

	int l = str ? strlen(str) : 0;


	word32 cap = get_memory16_c(ptr, 0);
	ptr += 2;

	if (cap < 4) return paramRangeErr;

	set_memory16_c(ptr, l, 0);
	ptr += 2;

	// get dir entry copies data even
	// if buffTooSmall...
	int rv = 0;
	if (cap < l + 4) {
		l = cap - 4;
		rv = buffTooSmall;
	}

	for (int i = 0; i < l; ++i) {
		char c = *str++;
		if (c == '/') c = ':';
		set_memory_c(ptr++, c, 0);
	}
	return rv;
}

static word32 set_pstr(word32 ptr, const char *str) {

	if (!ptr) return paramRangeErr;

	int l = str ? strlen(str) : 0;

	if (l > 255) return buffTooSmall;
	// / is the pascal separator.
	set_memory_c(ptr++, l, 0);
	for (int i = 0; i < l; ++i) {
		set_memory_c(ptr++, *str++, 0);
	}
	return 0;
}


static word32 set_option_list(word32 ptr, word16 fstID, const byte *data, int size) {
	if (!ptr) return 0;

	// totalSize
	word32 cap = get_memory16_c(ptr, 0);
	ptr += 2;

	if (cap < 4) return paramRangeErr;

	// reqSize
	set_memory16_c(ptr, size + 2, 0);
	ptr += 2;

	if (cap < size + 6) return buffTooSmall;


	// fileSysID.
	set_memory16_c(ptr, fstID, 0);
	ptr += 2;

	for (int i = 0; i < size; ++i)
		set_memory_c(ptr++, *data++, 0);

	return 0;
}


static int dayofweek(int y, int m, int d)	/* 1 <= m <= 12,  y > 1752 (in the U.K.) */
    {
        static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        y -= m < 3;
        return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    }
/*
 * converts time_t to a gs/os readhextime date/time record.
 */

static void set_date_time_rec(word32 ptr, FILETIME utc) {

	if (utc.dwLowDateTime == 0 && utc.dwHighDateTime == 0) {
		for (int i = 0; i < 8; ++i) set_memory_c(ptr++, 0, 0);
		return;
	}


	SYSTEMTIME tmLocal;
	SYSTEMTIME tmUTC;

	FileTimeToSystemTime(&utc, &tmUTC);
	SystemTimeToTzSpecificLocalTime(NULL, &tmUTC, &tmLocal);

	if (tmLocal.wYear < 1900 || tmLocal.wYear > 1900 + 255) {
		for (int i = 0; i < 8; ++i) set_memory_c(ptr++, 0, 0);
		return;
	}
	if (tmLocal.wSecond == 60) tmLocal.wSecond = 59; /* leap second */

	int dow = dayofweek(tmLocal.wYear, tmLocal.wMonth, tmLocal.wDay);
	set_memory_c(ptr++, tmLocal.wSecond, 0);
	set_memory_c(ptr++, tmLocal.wMinute, 0);
	set_memory_c(ptr++, tmLocal.wHour, 0);
	set_memory_c(ptr++, tmLocal.wYear - 1900, 0);
	set_memory_c(ptr++, tmLocal.wDay - 1, 0); // 1 = sunday
	set_memory_c(ptr++, tmLocal.wMonth - 1, 0);
	set_memory_c(ptr++, 0, 0);
	set_memory_c(ptr++, dow + 1, 0);
}

/*
 * converts time_t to a prodos16 date/time record.
 */
static void set_date_time(word32 ptr, FILETIME utc) {

	if (utc.dwLowDateTime == 0 && utc.dwHighDateTime == 0) {
		for (int i = 0; i < 4; ++i) set_memory_c(ptr++, 0, 0);
		return;
	}

	SYSTEMTIME tmLocal;
	SYSTEMTIME tmUTC;

	FileTimeToSystemTime(&utc, &tmUTC);
	SystemTimeToTzSpecificLocalTime(NULL, &tmUTC, &tmLocal);

	if (tmLocal.wYear < 1940 || tmLocal.wYear > 2039) {
		for (int i = 0; i < 4; ++i) set_memory_c(ptr++, 0, 0);
		return;		
	}
	if (tmLocal.wSecond == 60) tmLocal.wSecond = 59; /* leap second */

	word16 tmp = 0;
	tmp |= (tmLocal.wYear % 100) << 9;
	tmp |= tmLocal.wMonth << 5;
	tmp |= tmLocal.wDay;

	set_memory16_c(ptr, tmp, 0);
	ptr += 2;

	tmp = 0;
	tmp |= tmLocal.wHour << 8;
	tmp |= tmLocal.wMinute;
	set_memory16_c(ptr, tmp, 0);
}


static FILETIME get_date_time(word32 ptr) {

	FILETIME utc = {0, 0};

	word16 a = get_memory16_c(ptr + 0, 0);
	word16 b = get_memory16_c(ptr + 2, 0);
	if (!a && !b) return utc;


	SYSTEMTIME tmLocal;
	SYSTEMTIME tmUTC;
	memset(&tmLocal, 0, sizeof(tmLocal));
	memset(&tmUTC, 0, sizeof(tmUTC));

	tmLocal.wYear = ((a >> 9) & 0x7f) + 1900;
	tmLocal.wMonth = ((a >> 5) & 0x0f);
	tmLocal.wDay = (a >> 0) & 0x1f;

	tmLocal.wHour = (b >> 8) & 0x1f;
	tmLocal.wMinute = (b >> 0) & 0x3f;
	tmLocal.wSecond = 0;


	// 00 - 39 => 2000-2039
	// 40 - 99 => 1940-1999
	if (tmLocal.wYear < 40) tmLocal.wYear += 100;


	TzSpecificLocalTimeToSystemTime(NULL, &tmLocal, &tmUTC);
	if (!SystemTimeToFileTime(&tmUTC, &utc)) utc =(FILETIME){0, 0};

	return utc;
}

static FILETIME get_date_time_rec(word32 ptr) {

	FILETIME utc = {0, 0};

	byte buffer[8];
	for (int i = 0; i < 8; ++i) buffer[i] = get_memory_c(ptr++, 0);

	if (!memcmp(buffer, "\x00\x00\x00\x00\x00\x00\x00\x00", 8)) return utc;

	SYSTEMTIME tmLocal;
	SYSTEMTIME tmUTC;
	memset(&tmLocal, 0, sizeof(tmLocal));
	memset(&tmUTC, 0, sizeof(tmUTC));

	tmLocal.wSecond = buffer[0];
	tmLocal.wMinute = buffer[1];
	tmLocal.wHour = buffer[2];
	tmLocal.wYear = 1900 + buffer[3];
	tmLocal.wDay = buffer[4] + 1;
	tmLocal.wMonth = buffer[5] + 1;

	TzSpecificLocalTimeToSystemTime(NULL, &tmLocal, &tmUTC);
	if (!SystemTimeToFileTime(&tmUTC, &utc)) utc =(FILETIME){0, 0};

	return utc;
}


static char *get_path1(void) {
	word32 direct = engine.direct;
	word16 flags = get_memory16_c(direct + dp_path_flag, 0);
	if (flags & (1 << 14))
		return get_gsstr( get_memory24_c(direct + dp_path1_ptr, 0));
	return NULL;
}

static char *get_path2(void) {
	word32 direct = engine.direct;
	word16 flags = get_memory16_c(direct + dp_path_flag, 0);
	if (flags & (1 << 6))
		return get_gsstr( get_memory24_c(direct + dp_path2_ptr, 0));
	return NULL;
}


#define SEC() engine.psr |= 1
#define CLC() engine.psr &= ~1


static word32 fst_shutdown(void) {

	// close any remaining files.
	struct fd_entry *head = fd_head;
	while (head) {
		struct fd_entry *next = head->next;

		free_fd(head);
		head = next;
	}
	return 0;
}

static word32 fst_startup(void) {
	// if restart, close any previous files.


	if (!g_cfg_host_path) return invalidFSTop;
	if (!*g_cfg_host_path) return invalidFSTop;
	if (root) free(root);
	root = strdup(g_cfg_host_path);

	read_only = g_cfg_host_read_only;

	fst_shutdown();

	memset(&cookies, 0, sizeof(cookies));

	HANDLE h = CreateFile(root, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "%s does not exist or is not accessible\n", root);
		return invalidFSTop;
	}
	FILE_BASIC_INFO fbi;
 	BY_HANDLE_FILE_INFORMATION info;

 	GetFileInformationByHandle(h, &info);
	GetFileInformationByHandleEx(h, FileBasicInfo, &fbi, sizeof(fbi));
	// can't delete volume root. 
 	CloseHandle(h);

 	root_file_id[0] = info.dwVolumeSerialNumber;
 	root_file_id[1] = info.nFileIndexHigh;
 	root_file_id[2] = info.nFileIndexLow;


	if (!(fbi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		fprintf(stderr, "%s is not a directory\n", root);
		CloseHandle(h);
		return invalidFSTop;
	}
	CloseHandle(h);

	//root_ino = st.st_ino;
	//root_dev = st.st_dev;

	return 0;
}


static word32 fst_create(int class, const char *path) {

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	struct file_info fi;
	memset(&fi, 0, sizeof(fi));

	word16 pcount = 0;
	if (class) {
		pcount = get_memory16_c(pb, 0);
		if (pcount >= 2) fi.access = get_memory16_c(pb + CreateRecGS_access, 0);
		if (pcount >= 3) fi.file_type = get_memory16_c(pb + CreateRecGS_fileType, 0);
		if (pcount >= 4) fi.aux_type = get_memory32_c(pb + CreateRecGS_auxType, 0);
		if (pcount >= 5) fi.storage_type = get_memory16_c(pb + CreateRecGS_storageType, 0);
		if (pcount >= 6) fi.eof = get_memory32_c(pb + CreateRecGS_eof, 0);
		if (pcount >= 7) fi.resource_eof = get_memory32_c(pb + CreateRecGS_resourceEOF, 0);


		if (pcount >= 4) {
			afp_init
		(&fi.afp, fi.file_type, fi.aux_type);
			fi.has_fi = 1;
		}

	} else {

		fi.access = get_memory16_c(pb + CreateRec_fAccess, 0);
		fi.file_type = get_memory16_c(pb + CreateRec_fileType, 0);
		fi.aux_type = get_memory32_c(pb + CreateRec_auxType, 0);
		fi.storage_type = get_memory16_c(pb + CreateRec_storageType, 0);
		fi.create_date = get_date_time(pb + CreateRec_createDate);

		afp_init
	(&fi.afp, fi.file_type, fi.aux_type);
		fi.has_fi = 1;
	}
	int ok;

	if (fi.storage_type == 0x0d || fi.storage_type == 0x0f) {
		ok = CreateDirectory(path, NULL);
		if (!ok) return map_last_error();
		return 0;
	}
	if (fi.storage_type <= 3 || fi.storage_type == 0x05) {
		// normal file.
		// 0x05 is an extended/resource file but we don't do anything special.
		HANDLE h = CreateFile(path, 
			GENERIC_WRITE, 
			FILE_SHARE_READ | FILE_SHARE_WRITE, 
			NULL, 
			CREATE_NEW, 
			FILE_ATTRIBUTE_NORMAL, 
			NULL);
		if (h == INVALID_HANDLE_VALUE) return map_last_error();
		// set ftype, auxtype...
		set_file_info(path, &fi); // set_file_info_handle(...);
		CloseHandle(h);

		if (class) {
			if (pcount >= 5) set_memory16_c(pb + CreateRecGS_storageType, 1, 0);
		} else {
			set_memory16_c(pb + CreateRec_storageType, 1, 0);
		}

		return 0;
	}


	if (fi.storage_type == 0x8005) {
		// convert an existing file to an extended file.
		// this checks that the file exists and has a 0-sized resource.
		word32 rv = get_file_info(path, &fi);
		if (rv) return rv;
		if (fi.storage_type == extendedFile) return resExistsErr;
		if (fi.storage_type < seedling || fi.storage_type > tree) return resAddErr;
		return 0;
	}

	// other storage types?
	return badStoreType;
}


static word16 storage_type(const char *path, word16 *error) {
	if (!path) {
		*error = badPathSyntax; 
		return 0; // ?
	}
	HANDLE h;
	h = CreateFile(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		*error = map_last_error();
		return 0;
	}

 	BY_HANDLE_FILE_INFORMATION info;
 	GetFileInformationByHandle(h, &info);
 	CloseHandle(h);

 	DWORD id[3] = { info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow };
 	if (memcmp(&id, &root_file_id, sizeof(root_file_id)) == 0) {
  		return 0x0f;
 	}

 	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
 		return directoryFile;

 	return standardFile;
}

static word32 fst_destroy(int class, const char *path) {

	word16 rv = 0;
	BOOL ok = 0;
	word16 type = storage_type(path, &rv);
	if (rv) return rv;
	if (read_only) return drvrWrtProt;

	switch(type) {
		case 0: return fileNotFound;
		case 0x0f: return badStoreType;
		default: return badStoreType;

		case directoryFile:
			ok = RemoveDirectory(path);
			break;
		case sapling:
		case seedling:
		case tree:
		case extendedFile:
			ok = DeleteFile(path);
			break;
	}

	if (!ok) return map_last_error();
	return 0;
}

static word32 fst_set_file_info(int class, const char *path) {

	word16 rv = 0;
	word16 type = storage_type(path, &rv);
	if (rv) return rv;
	if (read_only) return drvrWrtProt;


	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	struct file_info fi;
	memset(&fi, 0, sizeof(fi));


	// load up existing file types / finder info.
	get_file_xinfo(path, &fi);

	word32 option_list = 0;
	if (class) {
		word16 pcount = get_memory16_c(pb, 0);

		if (pcount >= 2) fi.access = get_memory16_c(pb + FileInfoRecGS_access, 0);
		if (pcount >= 3) fi.file_type = get_memory16_c(pb + FileInfoRecGS_fileType, 0);
		if (pcount >= 4) fi.aux_type = get_memory32_c(pb + FileInfoRecGS_auxType, 0);
		// reserved.
		//if (pcount >= 5) fi.storage_type = get_memory16_c(pb + FileInfoRecGS_storageType, 0);
		if (pcount >= 6) fi.create_date = get_date_time_rec(pb + FileInfoRecGS_createDateTime);
		if (pcount >= 7) fi.modified_date = get_date_time_rec(pb + FileInfoRecGS_modDateTime);
		if (pcount >= 8) option_list = get_memory24_c(pb + FileInfoRecGS_optionList, 0);
		// remainder reserved

		if (fi.has_fi) {
			if (pcount < 4) fi.aux_type = fi.afp.prodos_aux_type;
			if (pcount < 3) fi.file_type = fi.afp.prodos_file_type;
		}

	} else {
		fi.access = get_memory16_c(pb + FileRec_fAccess, 0);
		fi.file_type = get_memory16_c(pb + FileRec_fileType, 0);
		fi.aux_type = get_memory32_c(pb + FileRec_auxType, 0);
		// reserved.
		//fi.storage_type = get_memory32_c(pb + FileRec_storageType, 0);
		fi.create_date = get_date_time(pb + FileRec_createDate);
		fi.modified_date = get_date_time(pb + FileRec_modDate);
	}


	if (fi.has_fi) {
		fi.afp.prodos_file_type = fi.file_type;
		fi.afp.prodos_aux_type = fi.aux_type;
	}
	else {
		afp_init(&fi.afp, fi.file_type, fi.aux_type);
		fi.has_fi = 1;
	}

	if (option_list) {
		// total size, req size, fst id, data...
		int total_size = get_memory16_c(option_list + 0, 0); 
		int req_size = get_memory16_c(option_list + 2, 0); 
		int fst_id = get_memory16_c(option_list + 4, 0);

		int size = req_size - 6;
		if ((fst_id == proDOSFSID || fst_id == hfsFSID || fst_id == appleShareFSID) && size >= 32) {

			// this should never happen...
			if (!fi.has_fi) afp_init(&fi.afp, fi.file_type, fi.aux_type);
			fi.has_fi = 1;

			for (int i = 0; i <32; ++i) 
				fi.afp.finder_info[i] = get_memory_c(option_list + 6 + i, 0);
		}
	}

	// one more check... if ftype/auxtype doesn't match the ftype/auxtype in finder info
	// update finder info
	if (fi.has_fi) afp_synchronize(&fi.afp, prefer_prodos);

	return set_file_info(path, &fi);
}

static word32 fst_get_file_info(int class, const char *path) {

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	struct file_info fi;
	int rv = 0;

	rv = get_file_info(path, &fi);
	if (rv) return rv;

	if (class) {

		word16 pcount = get_memory16_c(pb, 0);

		if (pcount >= 2) set_memory16_c(pb + FileInfoRecGS_access, fi.access, 0);
		if (pcount >= 3) set_memory16_c(pb + FileInfoRecGS_fileType, fi.file_type, 0);
		if (pcount >= 4) set_memory32_c(pb + FileInfoRecGS_auxType, fi.aux_type, 0);
		if (pcount >= 5) set_memory16_c(pb + FileInfoRecGS_storageType, fi.storage_type, 0);

		if (pcount >= 6) set_date_time_rec(pb + FileInfoRecGS_createDateTime, fi.create_date);
		if (pcount >= 7) set_date_time_rec(pb + FileInfoRecGS_modDateTime, fi.modified_date);

		if (pcount >= 8) {
			word16 fst_id = hfsFSID;
			//if (fi.storage_type == 0x0f) fst_id = mfsFSID;
			word32 option_list = get_memory24_c(pb + FileInfoRecGS_optionList, 0);
			rv = set_option_list(option_list,  fst_id, fi.afp.finder_info, fi.has_fi ? 32 : 0);
		}
		if (pcount >= 9) set_memory32_c(pb + FileInfoRecGS_eof, fi.eof, 0);
		if (pcount >= 10) set_memory32_c(pb + FileInfoRecGS_blocksUsed, fi.blocks, 0);
		if (pcount >= 11) set_memory32_c(pb + FileInfoRecGS_resourceEOF, fi.resource_eof, 0);
		if (pcount >= 12) set_memory32_c(pb + FileInfoRecGS_resourceBlocks, fi.resource_blocks, 0);

	} else {

		set_memory16_c(pb + FileRec_fAccess, fi.access, 0);
		set_memory16_c(pb + FileRec_fileType, fi.file_type, 0);
		set_memory32_c(pb + FileRec_auxType, fi.aux_type, 0);
		set_memory16_c(pb + FileRec_storageType, fi.storage_type, 0);

		set_date_time(pb + FileRec_createDate, fi.create_date);
		set_date_time(pb + FileRec_modDate, fi.modified_date);

		set_memory32_c(pb + FileRec_blocksUsed, fi.blocks, 0);
	}

	return rv;
}

static word32 fst_judge_name(int class, char *path) {

	if (class == 0) return invalidClass;


	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);
	word16 pcount = get_memory16_c(pb, 0);
	word16 name_type = get_memory16_c(pb + JudgeNameRecGS_nameType, 0);
	word32 name = pcount >= 5 ? get_memory24_c(pb + JudgeNameRecGS_name, 0) : 0;

	// 255 max length.
	if (pcount >= 4) set_memory16_c(pb + JudgeNameRecGS_maxLen, 255, 0);
	if (pcount >= 6) set_memory16_c(pb + JudgeNameRecGS_nameFlags, 0, 0);

	word16 nameFlags = 0;
	word16 rv = 0;

	if (name) {
		word16 cap = get_memory16_c(name, 0);
		if (cap < 4) return buffTooSmall;
		word16 length = get_memory16_c(name + 2, 0);
		if (length == 0) {
			nameFlags |= 1 << 13;
			rv = set_gsstr(name, "A");
		} else {
			// if volume name, only allow "Host" ?
			if (length > 255) nameFlags |= 1 << 14;
			for (int i = 0; i < length; ++i) {
				char c = get_memory_c(name + 4 + i, 0);
				if (c == 0 || c == ':' || c == '/' || c == '\\') {
					nameFlags |= 1 << 15;
					set_memory_c(name + 4 + i, '.', 0);
				}
			}
		}
	}
	if (pcount >= 6) set_memory16_c(pb + JudgeNameRecGS_nameFlags, nameFlags, 0);
	return rv;
}

static word32 fst_volume(int class) {

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	word32 rv = 0;
	if (class) {
		word16 pcount = get_memory16_c(pb, 0);
		if (pcount >= 2) rv = set_gsstr(get_memory24_c(pb + VolumeRecGS_volName, 0), ":Host");
		// finder bug -- if used blocks is 0, doesn't display header.
		if (pcount >= 3) set_memory32_c(pb + VolumeRecGS_totalBlocks, 0x007fffff, 0);
		if (pcount >= 4) set_memory32_c(pb + VolumeRecGS_freeBlocks, 0x007fffff-1, 0);
		if (pcount >= 5) set_memory16_c(pb + VolumeRecGS_fileSysID, mfsFSID, 0);
		if (pcount >= 6) set_memory16_c(pb + VolumeRecGS_blockSize, 512, 0);
		// handled via gs/os
		//if (pcount >= 7) set_memory16_c(pb + VolumeRecGS_characteristics, 0, 0);
		//if (pcount >= 8) set_memory16_c(pb + VolumeRecGS_deviceID, 0, 0);
	} else {

		// prodos 16 uses / sep.
		rv = set_pstr(get_memory24_c(pb + VolumeRec_volName, 0), "/Host");
		set_memory32_c(pb + VolumeRec_totalBlocks, 0x007fffff, 0);
		set_memory32_c(pb + VolumeRec_freeBlocks, 0x007fffff-1, 0);
		set_memory16_c(pb + VolumeRec_fileSysID, mfsFSID, 0);
	}

	return rv;

}

static word32 fst_clear_backup(int class, const char *path) {

	word16 rv = 0;
	word16 type = storage_type(path, &rv);
	if (rv) return rv;
	if (read_only) return drvrWrtProt;

	return invalidFSTop;
}

static void free_directory(struct directory *dd) {
	if (!dd) return;
	for (int i = 0; i < dd->num_entries; ++i) {
		free(dd->entries[i]);
	}
	free(dd);
}

static int qsort_callback(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}

/*
 * exclude files from get_dir_entries.
 *
 */
static int filter_directory_entry(const char *name) {
	if (!name[0]) return 1;
	if (name[0] == '.') {
		return 1;
		/*
		if (!strcmp(name, ".")) return 1;
		if (!strcmp(name, "..")) return 1;
		if (!strncmp(name, "._", 2)) return 1; // ._ resource fork
		if (!strcmp(name, ".fseventsd")) return 1;
		*/
	}
	return 0;
}

static struct directory *read_directory(const char *path, word16 *error) {
	HANDLE h;
	WIN32_FIND_DATA data;
	struct directory *dd;
	int capacity = 100;
	int size = sizeof(struct directory) + capacity * sizeof(char *);

	memset(&data, 0, sizeof(data));

	char *p = append_path(path, "*");
	h = FindFirstFile(p, &data);
	if (h == INVALID_HANDLE_VALUE) {
		*error = map_last_error();
		return NULL;
	}

	dd = (struct directory *)malloc(size);
	if (!dd) {
		FindClose(h);
		*error = outOfMem;
		return NULL;
	}
	memset(dd, 0, size);
	do {
		if (filter_directory_entry(data.cFileName)) continue;
		if (dd->num_entries >= capacity) {
			capacity += capacity;
			size = sizeof(struct directory) + capacity * sizeof(char *);
			struct directory * tmp = realloc(dd, size);
			if (!tmp) {
				*error = map_errno();
				free_directory(dd);
				FindClose(h);
				return NULL;
			}
			dd = tmp;
		}
		dd->entries[dd->num_entries++] = strdup(data.cFileName);
	} while (FindNextFile(h, &data) != 0);

	//?
	if (GetLastError() != ERROR_NO_MORE_FILES) *error = map_last_error();

	FindClose(h);

	// sort them....
	qsort(dd->entries, dd->num_entries, sizeof(char *), qsort_callback);

	return dd;
}



static HANDLE open_data_fork(const char *path, word16 *access, word16 *error) {

	HANDLE h = INVALID_HANDLE_VALUE;
	for (;;) {

		DWORD wAccess = 0;
		DWORD wShare = FILE_SHARE_READ;
		DWORD wCreate = OPEN_EXISTING;

		switch(*access) {
			case readEnableAllowWrite:
			case readWriteEnable:
				wAccess = GENERIC_READ | GENERIC_WRITE;
				break;
			case readEnable:
				wAccess = GENERIC_READ;
				break;
			case writeEnable:
				wAccess = GENERIC_WRITE;
				break;
		}

		h = CreateFile(path, wAccess, wShare, NULL, wCreate, FILE_ATTRIBUTE_NORMAL, NULL);


		if (*access == readEnableAllowWrite) {
			if (h == INVALID_HANDLE_VALUE) {
				*access = readEnable;
				continue;
			}
			*access = readWriteEnable;
		}
		break;
	}
	if (h == INVALID_HANDLE_VALUE) {
		*error = map_last_error();
	}
	return h;
}

static HANDLE open_resource_fork(const char *path, word16 *access, word16 *error) {

	char *rpath = append_string(path, ":AFP_Resource");

	HANDLE h = INVALID_HANDLE_VALUE;
	for (;;) {

		DWORD wAccess = 0;
		DWORD wShare = FILE_SHARE_READ;
		DWORD wCreate = 0;

		switch(*access) {
			case readEnableAllowWrite:
			case readWriteEnable:
				wAccess = GENERIC_READ | GENERIC_WRITE;
				wCreate = OPEN_ALWAYS;
				break;
			case readEnable:
				wAccess = GENERIC_READ;
				wCreate = OPEN_EXISTING;
				break;
			case writeEnable:
				wAccess = GENERIC_WRITE;
				wCreate = OPEN_ALWAYS;
				break;
		}

		h = CreateFile(rpath, wAccess, wShare, NULL, wCreate, FILE_ATTRIBUTE_NORMAL, NULL);


		if (*access == readEnableAllowWrite) {
			if (h == INVALID_HANDLE_VALUE) {
				*access = readEnable;
				continue;
			}
			*access = readWriteEnable;
		}
		break;
	}
	if (h == INVALID_HANDLE_VALUE) {
		*error = map_last_error();
	}
	return h;
}



static word32 fst_open(int class, const char *path) {


	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	struct file_info fi;
	word16 rv = 0;

	rv = get_file_info(path, &fi);
	if (rv) return rv;


	HANDLE h = INVALID_HANDLE_VALUE;
	int type = regular_file;
	struct directory *dd = NULL;

	word16 pcount = 0;
	word16 request_access = readEnableAllowWrite;
	word16 access = 0;
	word16 resource_number = 0;
	if (class) {
		pcount = get_memory16_c(pb, 0);
		if (pcount >= 3) request_access = get_memory16_c(pb + OpenRecGS_requestAccess, 0); 
		if (pcount >= 4) resource_number = get_memory16_c(pb + OpenRecGS_resourceNumber, 0); 
	}

	if (resource_number) {
		if (resource_number > 1) return paramRangeErr;
		type = resource_file;
	}

	if (access > 3) return paramRangeErr;

	// special access checks for directories.
	if (fi.storage_type == 0x0f || fi.storage_type == 0x0d) {
		if (resource_number) return resForkNotFound;
		switch (request_access) {
			case readEnableAllowWrite:
				request_access = readEnable;
				break;
			case writeEnable:
			case readWriteEnable:
				return invalidAccess;
		}
		type = directory_file;
	}


	if (read_only) {
		switch (request_access) {
			case readEnableAllowWrite:
				request_access = readEnable;
				break;
			case readWriteEnable:
			case writeEnable:
				return invalidAccess;
				break;
		}
	}

	access = request_access;
	switch(type) {
		case regular_file:
			h = open_data_fork(path, &access, &rv);
			break;
		case resource_file:
			h = open_resource_fork(path, &access, &rv);
			break;
		case directory_file:
			dd = read_directory(path, &rv);
			break;
	}

	if (rv) return rv;


	if (class) {
		if (pcount >= 5) set_memory16_c(pb + OpenRecGS_access, access, 0);
		if (pcount >= 6) set_memory16_c(pb + OpenRecGS_fileType, fi.file_type, 0);
		if (pcount >= 7) set_memory32_c(pb + OpenRecGS_auxType, fi.aux_type, 0);
		if (pcount >= 8) set_memory16_c(pb + OpenRecGS_storageType, fi. storage_type, 0);

		if (pcount >= 9) set_date_time_rec(pb + OpenRecGS_createDateTime, fi.create_date);
		if (pcount >= 10) set_date_time_rec(pb + OpenRecGS_modDateTime, fi.modified_date);

		if (pcount >= 11) {
			word16 fst_id = hfsFSID;
			//if (fi.storage_type == 0x0f) fst_id = mfsFSID;

			word32 option_list = get_memory24_c(pb + OpenRecGS_optionList, 0); 
			word32 tmp = set_option_list(option_list, fst_id, fi.afp.finder_info, fi.has_fi ? 32 : 0);
			if (!rv) rv = tmp; 
		}

		if (pcount >= 12) set_memory32_c(pb + OpenRecGS_eof, fi.eof, 0);
		if (pcount >= 13) set_memory32_c(pb + OpenRecGS_blocksUsed, fi.blocks, 0);
		if (pcount >= 14) set_memory32_c(pb + OpenRecGS_resourceEOF, fi.resource_eof, 0);
		if (pcount >= 15) set_memory32_c(pb + OpenRecGS_resourceBlocks, fi.resource_blocks, 0);


	}
	// prodos 16 doesn't return anything in the parameter block.

	struct fd_entry *e = alloc_fd();
	if (!e) {
		if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
		if (dd) free_directory(dd);
		return outOfMem;
	}
	e->handle = h;
	e->dir = dd;

	e->cookie = alloc_cookie();
	if (!e->cookie) {
		free_fd(e);
		return tooManyFilesOpen;
	}

	e->access = access;
	e->path = strdup(path);
	e->type = type;


	// insert it in the linked list.
	e->next = fd_head;
	fd_head = e;

	engine.xreg = e->cookie;
	engine.yreg = access; // actual access, needed in fcr.

	return rv;
}

static word32 fst_read(int class) {

	int cookie = engine.yreg;
	struct fd_entry *e = find_fd(cookie);

	if (!e) return invalidRefNum;

	switch (e->type) {
		case directory_file:
			return badStoreType;
	}

	if (!(e->access & readEnable))
		return invalidAccess;

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	word32 data_buffer = 0;
	word32 request_count = 0;
	word32 transfer_count = 0;

	if (class) {
		data_buffer = get_memory24_c(pb + IORecGS_dataBuffer, 0);
		request_count = get_memory24_c(pb + IORecGS_requestCount, 0);
		// pre-zero transfer count
		set_memory32_c(pb + IORecGS_transferCount, 0, 0);
	} else {
		data_buffer = get_memory24_c(pb + FileIORec_dataBuffer, 0);
		request_count = get_memory24_c(pb + FileIORec_requestCount, 0);
		set_memory32_c(pb + FileIORec_transferCount, 0, 0);
	}

	if (request_count == 0) return 0;


	word16 newline_mask;
	word32 rv = 0;
	int ok;

	newline_mask = get_memory16_c(global_buffer, 0);
	if (newline_mask) {
		byte newline_table[256];
		for (int i = 0; i < 256; ++i)
			newline_table[i] = get_memory_c(global_buffer + 2 + i, 0);

		for (word32 i = 0 ; i < request_count; ++i) {
			byte b;
			DWORD read_count;
			ok = ReadFile(e->handle, &b, 1, &read_count, NULL);
			if (!ok) return map_last_error();
			if (read_count == 0) break;
			transfer_count++;
			set_memory_c(data_buffer++, b, 0);
			if (newline_table[b & newline_mask]) break;
		}
		if (transfer_count == 0) rv = eofEncountered;
	}
	else {
		DWORD read_count;
		byte *data = gc_malloc(request_count);
		if (!data) return outOfMem;

		ok = ReadFile(e->handle, data, request_count, &read_count, NULL);
		if (!ok) rv = map_last_error();
		else if (read_count == 0) rv = eofEncountered;
		if (read_count > 0) {
			transfer_count = read_count;
			for (size_t i = 0; i < transfer_count; ++i) {
				set_memory_c(data_buffer + i, data[i], 0);
			}
		}
	}

	if (transfer_count) {
		if (class)
			set_memory32_c(pb + IORecGS_transferCount, transfer_count, 0);
		else
			set_memory32_c(pb + FileIORec_transferCount, transfer_count, 0);
	}

	return rv;
}

static word32 fst_write(int class) {

	int cookie = engine.yreg;
	struct fd_entry *e = find_fd(cookie);

	if (!e) return invalidRefNum;

	switch (e->type) {
		case directory_file:
			return badStoreType;
	}
	if (read_only) return drvrWrtProt;

	if (!(e->access & writeEnable))
		return invalidAccess;

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	word32 data_buffer = 0;
	word32 request_count = 0;

	if (class) {
		data_buffer = get_memory24_c(pb + IORecGS_dataBuffer, 0);
		request_count = get_memory24_c(pb + IORecGS_requestCount, 0);
		// pre-zero transfer count
		set_memory32_c(pb + IORecGS_transferCount, 0, 0);
	} else {
		data_buffer = get_memory24_c(pb + FileIORec_dataBuffer, 0);
		request_count = get_memory24_c(pb + FileIORec_requestCount, 0);
		set_memory32_c(pb + FileIORec_transferCount, 0, 0);
	}

	if (request_count == 0) return 0;
	byte *data = gc_malloc(request_count);
	if (!data) return outOfMem;

	for (word32 i = 0; i < request_count; ++i) {
		data[i] = get_memory_c(data_buffer + i,0);
	}

	word32 rv = 0;
	DWORD write_count = 0;
	int ok = WriteFile(e->handle, data, request_count, &write_count, NULL);
	if (!ok) rv = map_last_error();
	if (write_count > 0) {
		if (class)
			set_memory32_c(pb + IORecGS_transferCount, write_count, 0);
		else
			set_memory32_c(pb + FileIORec_transferCount, write_count, 0);
	}
	return rv;
}

static word32 fst_close(int class) {

	int cookie = engine.yreg;

	return remove_fd(cookie);

}

static word32 fst_flush(int class) {

	int cookie = engine.yreg;
	struct fd_entry *e = find_fd(cookie);

	if (!e) return invalidRefNum;

	switch (e->type) {
		case directory_file:
			return badStoreType;
	}

	if (!FlushFileBuffers(e->handle)) return map_last_error();
	return 0;
}


static word16 win_seek(HANDLE h, word16 base, word32 displacement, LARGE_INTEGER *position) {
	LARGE_INTEGER d;
	int m = 0;
	switch (base) {
		case startPlus:
			m = FILE_BEGIN;
			d.QuadPart = displacement;
			break;
		case markPlus:
			m = FILE_CURRENT;
			d.QuadPart = displacement;
			break;
		case markMinus:
			m = FILE_CURRENT;
			d.QuadPart = -displacement;
			break;
		case eofMinus:
			m = FILE_END;
			d.QuadPart = -displacement;
			break;

		default:
			return paramRangeErr;

	}
	if (!SetFilePointerEx(h, d, position, m)) return map_last_error();
	return 0;
}

static word32 fst_set_mark(int class) {

	int cookie = engine.yreg;

	struct fd_entry *e = find_fd(cookie);
	if (!e) return invalidRefNum;

	switch (e->type) {
		case directory_file:
			return badStoreType;
	}

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	word16 base = 0;
	word32 displacement = 0;
	if (class) {
		base = get_memory16_c(pb + SetPositionRecGS_base, 0);
		displacement = get_memory32_c(pb + SetPositionRecGS_displacement, 0);
	} else {
		displacement = get_memory32_c(pb + MarkRec_position, 0);
	}
	if (base > markMinus) return paramRangeErr;

	return win_seek(e->handle, base, displacement, NULL);
}

static word32 fst_set_eof(int class) {

	int cookie = engine.yreg;

	struct fd_entry *e = find_fd(cookie);
	if (!e) return invalidRefNum;

	switch (e->type) {
		case directory_file:
			return badStoreType;
	}
	if (read_only) return drvrWrtProt;

	if (!(e->access & writeEnable))
		return invalidAccess;

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	word16 base = 0;
	word32 displacement = 0;
	if (class) {
		base = get_memory16_c(pb + SetPositionRecGS_base, 0);
		displacement = get_memory32_c(pb + SetPositionRecGS_displacement, 0);
	} else {
		displacement = get_memory32_c(pb + EOFRec_eofPosition, 0);
	}

	if (base > markMinus) return paramRangeErr;

	// get the current mark
	LARGE_INTEGER mark;
	LARGE_INTEGER zero; zero.QuadPart = 0;
	SetFilePointerEx(e->handle, zero, &mark, FILE_CURRENT);

	word16 rv = win_seek(e->handle, base, displacement, NULL);
	if (rv) return rv;

	// SetEndOfFile sets the current positions as eof.
	if (!SetEndOfFile(e->handle)) return map_last_error();

	// restore old mark. ???
	SetFilePointerEx(e->handle, mark, NULL, FILE_BEGIN);
	return 0;

}

static word32 fst_get_mark(int class) {

	int cookie = engine.yreg;

	struct fd_entry *e = find_fd(cookie);
	if (!e) return invalidRefNum;

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	switch (e->type) {
		case directory_file:
			return badStoreType;
	}

	// get the current mark
	LARGE_INTEGER pos;
	LARGE_INTEGER zero; zero.QuadPart = 0;
	if (!SetFilePointerEx(e->handle, zero, &pos, FILE_CURRENT)) return map_last_error();

	if (class) {
		set_memory32_c(pb + PositionRecGS_position, pos.LowPart, 0);
	} else {
		set_memory32_c(pb + MarkRec_position, pos.LowPart, 0);
	}

	return 0;
}

static word32 fst_get_eof(int class) {

	int cookie = engine.yreg;

	struct fd_entry *e = find_fd(cookie);
	if (!e) return invalidRefNum;

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);


	switch (e->type) {
		case directory_file:
			return badStoreType;
	}

	LARGE_INTEGER eof;

	if (!GetFileSizeEx(e->handle, &eof)) return map_last_error();

	// what if > 32 bits?

	if (class) {
		set_memory32_c(pb + PositionRecGS_position, eof.LowPart, 0);
	} else {
		set_memory32_c(pb + MarkRec_position, eof.LowPart, 0);
	}

	return 0;
}




static word32 fst_get_dir_entry(int class) {

	int cookie = engine.yreg;

	struct fd_entry *e = find_fd(cookie);
	if (!e) return invalidRefNum;


	if (e->type != directory_file) return badFileFormat;

	word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

	word16 base = 0;
	word16 pcount = 0;
	word32 displacement = 0;
	word32 name = 0;

	if (class) {
		pcount = get_memory16_c(pb, 0);
		base = get_memory16_c(pb + DirEntryRecGS_base, 0);
		displacement = get_memory16_c(pb + DirEntryRecGS_displacement, 0);
		name = get_memory24_c(pb + DirEntryRecGS_name, 0);
	} else {
		base = get_memory16_c(pb + DirEntryRec_base, 0);
		displacement = get_memory16_c(pb + DirEntryRec_displacement, 0);
		name = get_memory24_c(pb + DirEntryRec_nameBuffer, 0);
	}

	if (base == 0 && displacement == 0) {
		// count them up.
		int count = e->dir->num_entries;
		e->dir->displacement = 0;

		if (class) {
			if (pcount >= 6) set_memory16_c(pb + DirEntryRecGS_entryNum, count, 0);
		}
		else {
			set_memory16_c(pb + DirEntryRec_entryNum, count, 0);
		}

		return 0;
	}

	int dir_displacement = e->dir->displacement;
	switch (base) {
		case 0: // displacement is absolute entry number.
			break;
		case 1: // displacement is added to the current displacement.
			displacement = dir_displacement + displacement;
			break;
		case 2: // displacement is substracted from current displacement.
			displacement = dir_displacement - displacement;
			break;
		default:
			return paramRangeErr;
	}
	//if (displacement) --displacement;
	--displacement;
	if (displacement < 0) return endOfDir;
	if (displacement >= e->dir->num_entries) return endOfDir;


	word32 rv = 0;
	const char *dname = e->dir->entries[displacement++];
	e->dir->displacement = displacement;
	char *fullpath = append_path(e->path, dname);
	struct 	file_info fi;
	rv = get_file_info(fullpath, &fi);


	// p16 and gs/os both use truncating c1 output string.
	rv = set_gsstr_truncate(name, dname);

	if (class) {

		if (pcount > 2) set_memory16_c(pb + DirEntryRecGS_flags, fi.storage_type == 0x05 ? 0x8000 : 0, 0);

		if (pcount >= 6) set_memory16_c(pb + DirEntryRecGS_entryNum, displacement, 0);
		if (pcount >= 7) set_memory16_c(pb + DirEntryRecGS_fileType, fi.file_type, 0);
		if (pcount >= 8) set_memory32_c(pb + DirEntryRecGS_eof, fi.eof, 0);
		if (pcount >= 9) set_memory32_c(pb + DirEntryRecGS_blockCount, fi.blocks, 0);

		if (pcount >= 10) set_date_time_rec(pb + DirEntryRecGS_createDateTime, fi.create_date);
		if (pcount >= 11) set_date_time_rec(pb + DirEntryRecGS_modDateTime, fi.modified_date);

		if (pcount >= 12) set_memory16_c(pb + DirEntryRecGS_access, fi.access, 0);
		if (pcount >= 13) set_memory32_c(pb + DirEntryRecGS_auxType, fi.aux_type, 0);
		if (pcount >= 14) set_memory16_c(pb + DirEntryRecGS_fileSysID, mfsFSID, 0);

		if (pcount >= 15) {
			word16 fst_id = hfsFSID;
			//if (fi.storage_type == 0x0f) fst_id = mfsFSID;
			word32 option_list = get_memory24_c(pb + DirEntryRecGS_optionList, 0);
			word32 tmp = set_option_list(option_list, fst_id, fi.afp.finder_info, fi.has_fi ? 32 : 0);
			if (!rv) rv = tmp;
		}

		if (pcount >= 16) set_memory32_c(pb + DirEntryRecGS_resourceEOF, fi.resource_eof, 0);
		if (pcount >= 17) set_memory32_c(pb + DirEntryRecGS_resourceBlocks, fi.resource_blocks, 0);
	}
	else {
		set_memory16_c(pb + DirEntryRec_flags, fi.storage_type == 0x05 ? 0x8000 : 0, 0);

		set_memory16_c(pb + DirEntryRec_entryNum, displacement, 0);
		set_memory16_c(pb + DirEntryRec_fileType, fi.file_type, 0);
		set_memory32_c(pb + DirEntryRec_endOfFile, fi.eof, 0);
		set_memory32_c(pb + DirEntryRec_blockCount, fi.blocks, 0);

		set_date_time_rec(pb + DirEntryRec_createTime, fi.create_date);
		set_date_time_rec(pb + DirEntryRec_modTime, fi.modified_date);

		set_memory16_c(pb + DirEntryRec_access, fi.access, 0);
		set_memory32_c(pb + DirEntryRec_auxType, fi.aux_type, 0);
		set_memory16_c(pb + DirEntryRec_fileSysID, mfsFSID, 0);

	}

	return rv;
}

static word32 fst_change_path(int class, const char *path1, const char *path2) {

	/* make sure they're not trying to rename the volume... */
	BOOL ok = 0;

	word16 rv = 0;
	word16 type = storage_type(path1, &rv);
	if (rv) return rv;
	if (type == 0x0f) return badStoreType;
	if (read_only) return drvrWrtProt;

	if (!MoveFile(path1, path2)) return map_last_error();	
	return 0;
}

static word32 fst_format(int class) {
	return notBlockDev;
}

static word32 fst_erase(int class) {
	return notBlockDev;
}

static const char *call_name(word16 call) {
	static char* class1[] = {
		// 0x00
		"",
		"CreateGS",
		"DestroyGS",
		"",
		"ChangePathGS",
		"SetFileInfoGS",
		"GetFileInfoGS",
		"JudgeNameGS",
		"VolumeGS",
		"",
		"",
		"ClearBackupGS",
		"",
		"",
		"",
		"",

		// 0x10
		"OpenGS",
		"",
		"ReadGS",
		"WriteGS",
		"CloseGS",
		"FlushGS",
		"SetMarkGS",
		"GetMarkGS",
		"SetEOFGS",
		"GetEOFGS",
		"",
		"",
		"GetDirEntryGS",
		"",
		"",
		"",

		// 0x20
		"",
		"",
		"",
		"",
		"FormatGS",
		"EraseDiskGS",
	};


	static char* class0[] = {
		// 0x00
		"",
		"CREATE",
		"DESTROY",
		"",
		"CHANGE_PATH",
		"SET_FILE_INFO",
		"GET_FILE_INFO",
		"",
		"VOLUME",
		"",
		"",
		"CLEAR_BACKUP_BIT",
		"",
		"",
		"",
		"",

		// 0X10
		"OPEN",
		"",
		"READ",
		"WRITE",
		"CLOSE",
		"FLUSH",
		"SET_MARK",
		"GET_MARK",
		"SET_EOF",
		"GET_EOF",
		"",
		"",
		"GET_DIR_ENTRY",
		"",
		"",
		"",

		// 0X20
		"",
		"",
		"",
		"",
		"FORMAT",
		"ERASE_DISK",
	};



	if (call & 0x8000) {
		static char *sys[] = {
			"",
			"fst_startup",
			"fst_shutdown",
			"fst_remove_vcr",
			"fst_deferred_flush"
		};
		call &= ~0x8000;

		if (call < sizeof(sys) / sizeof(sys[0]))
			return sys[call];

		return "";
	}

	int class = call >> 13;
	call &= 0x1fff;
	switch(class) {
		case 0:
			if (call < sizeof(class0) / sizeof(class0[0]))
				return class0[call];
			break;
		case 1:
			if (call < sizeof(class1) / sizeof(class1[0]))
				return class1[call];
			break;

	}
	return "";
}

static const char *error_name(word16 error) {
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
		"",
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

#ifdef __CYGWIN__

#include <sys/cygwin.h>

static char *expand_path(const char *path, word32 *error) {

	char buffer[PATH_MAX];
	ssize_t ok = cygwin_conv_path(CCP_POSIX_TO_WIN_A, path, buffer, sizeof(buffer));
	if (ok < 0) {
		*error = fstError;
		return NULL;
	}
	return gc_strdup(buffer);
}


#else
#define expand_path(x, y) (x)
#endif


void host_fst(void) {
	
	/*
	 * input:
	 * c = set
	 * a = default error code
	 * x = gs/os callnum
	 * y = [varies]
	 *
	 * output:
	 * c = set/clear
	 * a = error code/0
	 * x = varies
	 * y = varies
	 */


	word32 acc = 0;
	word16 call = engine.xreg;

	fprintf(stderr, "Host FST: %04x %s\n", call, call_name(call));


	if (call & 0x8000) {
		// system level.
		switch(call) {
			case 0x8001:
				acc = fst_startup();
				break;
			case 0x8002:
				acc = fst_shutdown();
				break;
			default:
				acc = badSystemCall;
				break;
		}
	} else {

		int class = call >> 13;
		call &= 0x1fff;

		if (class > 1) {
			SEC();
			engine.acc = invalidClass;
			return;
		}
		char *path1 = get_path1();
		char *path2 = get_path2();
		char *path3 = NULL;
		char *path4 = NULL;
		const char *cp;

		switch(call & 0xff) {
			case 0x01:
				cp = check_path(path1, &acc);
				if (acc) break;
				path3 = append_path(root, cp);
				path3 = expand_path(path3, &acc);
				if (acc) break;

				acc = fst_create(class, path3);
				break;
			case 0x02:
				cp = check_path(path1, &acc);
				if (acc) break;
				path3 = append_path(root, cp);
				path3 = expand_path(path3, &acc);
				if (acc) break;

				acc = fst_destroy(class, path3);
				break;
			case 0x04:

				cp = check_path(path1, &acc);
				if (acc) break;
				path3 = append_path(root, cp);
				path3 = expand_path(path3, &acc);
				if (acc) break;

				cp = check_path(path2, &acc);
				if (acc) break;
				path4 = append_path(root, cp);
				path4 = expand_path(path4, &acc);
				if (acc) break;

				acc = fst_change_path(class, path3, path4);
				break;
			case 0x05:
				cp = check_path(path1, &acc);
				if (acc) break;
				path3 = append_path(root, cp);
				path3 = expand_path(path3, &acc);
				if (acc) break;

				acc = fst_set_file_info(class, path3);
				break;
			case 0x06:
				cp = check_path(path1, &acc);
				if (acc) break;
				path3 = append_path(root, cp);
				path3 = expand_path(path3, &acc);
				if (acc) break;

				acc = fst_get_file_info(class, path3);
				break;
			case 0x07:
				acc = fst_judge_name(class, path1);
				break;
			case 0x08:
				acc = fst_volume(class);
				break;
			case 0x0b:
				cp = check_path(path1, &acc);
				if (acc) break;
				path3 = append_path(root, cp);
				path3 = expand_path(path3, &acc);
				if (acc) break;

				acc = fst_clear_backup(class, path3);
				break;
			case 0x10:
				cp = check_path(path1, &acc);
				if (acc) break;
				path3 = append_path(root, cp);
				path3 = expand_path(path3, &acc);
				if (acc) break;

				acc = fst_open(class, path3);
				break;
			case 0x012:
				acc = fst_read(class);
				break;
			case 0x013:
				acc = fst_write(class);
				break;
			case 0x14:
				acc = fst_close(class);
				break;
			case 0x15:
				acc = fst_flush(class);
				break;
			case 0x16:
				acc = fst_set_mark(class);
				break;
			case 0x17:
				acc = fst_get_mark(class);
				break;
			case 0x18:
				acc = fst_set_eof(class);
				break;
			case 0x19:
				acc = fst_get_eof(class);
				break;
			case 0x1c:
				acc = fst_get_dir_entry(class);
				break;
			case 0x24:
				acc = fst_format(class);
				break;
			case 0x25:
				acc = fst_erase(class);
				break;

			default:
				acc = invalidFSTop;
				break;
		}


	}

 	if (acc) fprintf(stderr, "          %02x   %s\n", acc, error_name(acc));

	gc_free();

	engine.acc = acc;
	if (acc) SEC();
	else CLC();
}
