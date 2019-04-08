/*
   GSPLUS - Advanced Apple IIGS Emulator Environment
   Based on the KEGS emulator written by Kent Dickey
   See COPYRIGHT.txt for Copyright information
   See LICENSE.txt for license (GPL v2)
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

#include "host_common.h"

#ifdef _MSC_VER
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif


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
  int translate;
  struct directory *dir;
};




static void free_directory(struct directory *dd);
static struct directory *read_directory(const char *path, word16 *error);


#define COOKIE_BASE 0x8000

static word32 cookies[32] = { 0 };

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



static struct fd_entry *find_fd(int cookie) {
  struct fd_entry *head = fd_head;

  while (head) {
    if (head->cookie == cookie) return head;
    head = head->next;
  }
  return NULL;
}


static void free_fd(struct fd_entry     *e) {
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
  char *str = host_gc_malloc(length + 1);
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
  int length = get_memory_c(ptr, 0);
  ptr += 1;
  char *str = host_gc_malloc(length + 1);
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



static word32 fst_shutdown(void) {

  // close any remaining files.
  struct fd_entry *head = fd_head;
  while (head) {
    struct fd_entry *next = head->next;

    free_fd(head);
    head = next;
  }
  host_shutdown();
  return 0;
}

static word32 fst_startup(void) {
  // if restart, close any previous files.

  fst_shutdown();

  memset(&cookies, 0, sizeof(cookies));

  return host_startup();
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
      afp_init(&fi.afp, fi.file_type, fi.aux_type);
      fi.has_fi = 1;
    }

  } else {

    fi.access = get_memory16_c(pb + CreateRec_fAccess, 0);
    fi.file_type = get_memory16_c(pb + CreateRec_fileType, 0);
    fi.aux_type = get_memory32_c(pb + CreateRec_auxType, 0);
    fi.storage_type = get_memory16_c(pb + CreateRec_storageType, 0);
    fi.create_date = host_get_date_time(pb + CreateRec_createDate);

    afp_init(&fi.afp, fi.file_type, fi.aux_type);
    fi.has_fi = 1;
  }
  int ok;

  if (fi.storage_type == 0 && fi.file_type == 0x0f)
    fi.storage_type = 0x0d;

  if (fi.storage_type == 0x0d) {
    ok = CreateDirectory(path, NULL);
    if (!ok) return host_map_win32_error(GetLastError());

    if (class) {
      if (pcount >= 5) set_memory16_c(pb + CreateRecGS_storageType, fi.storage_type, 0);
    } else {
      set_memory16_c(pb + CreateRec_storageType, fi.storage_type, 0);
    }

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
    if (h == INVALID_HANDLE_VALUE) return host_map_win32_error(GetLastError());
    // set ftype, auxtype...
    host_set_file_info(path, &fi);
    CloseHandle(h);

    fi.storage_type = 1;

    if (class) {
      if (pcount >= 5) set_memory16_c(pb + CreateRecGS_storageType, fi.storage_type, 0);
    } else {
      set_memory16_c(pb + CreateRec_storageType, fi.storage_type, 0);
    }

    return 0;
  }


  if (fi.storage_type == 0x8005) {
    // convert an existing file to an extended file.
    // this checks that the file exists and has a 0-sized resource.
    word32 rv = host_get_file_info(path, &fi);
    if (rv) return rv;
    if (fi.storage_type == extendedFile) return resExistsErr;
    if (fi.storage_type < seedling || fi.storage_type > tree) return resAddErr;
    return 0;
  }

  // other storage types?
  return badStoreType;
}



static word32 fst_destroy(int class, const char *path) {

  word16 rv = 0;
  BOOL ok = 0;
  word16 type = host_storage_type(path, &rv);
  if (rv) return rv;
  if (g_cfg_host_read_only) return drvrWrtProt;

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

  if (!ok) return host_map_win32_error(GetLastError());
  return 0;
}

static word32 fst_set_file_info(int class, const char *path) {

  word16 rv = 0;
  word16 type = host_storage_type(path, &rv);
  if (rv) return rv;
  if (g_cfg_host_read_only) return drvrWrtProt;


  word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

  struct file_info fi;
  memset(&fi, 0, sizeof(fi));


  // load up existing file types / finder info.
  host_get_file_xinfo(path, &fi);

  word32 option_list = 0;
  if (class) {
    word16 pcount = get_memory16_c(pb, 0);

    if (pcount >= 2) fi.access = get_memory16_c(pb + FileInfoRecGS_access, 0);
    if (pcount >= 3) fi.file_type = get_memory16_c(pb + FileInfoRecGS_fileType, 0);
    if (pcount >= 4) fi.aux_type = get_memory32_c(pb + FileInfoRecGS_auxType, 0);
    // reserved.
    //if (pcount >= 5) fi.storage_type = get_memory16_c(pb + FileInfoRecGS_storageType, 0);
    if (pcount >= 6) fi.create_date = host_get_date_time_rec(pb + FileInfoRecGS_createDateTime);
    if (pcount >= 7) fi.modified_date = host_get_date_time_rec(pb + FileInfoRecGS_modDateTime);
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
    fi.create_date = host_get_date_time(pb + FileRec_createDate);
    fi.modified_date = host_get_date_time(pb + FileRec_modDate);
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

  return host_set_file_info(path, &fi);
}

static word32 fst_get_file_info(int class, const char *path) {

  word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

  struct file_info fi;
  int rv = 0;

  rv = host_get_file_info(path, &fi);
  if (rv) return rv;

  if (class) {

    word16 pcount = get_memory16_c(pb, 0);

    if (pcount >= 2) set_memory16_c(pb + FileInfoRecGS_access, fi.access, 0);
    if (pcount >= 3) set_memory16_c(pb + FileInfoRecGS_fileType, fi.file_type, 0);
    if (pcount >= 4) set_memory32_c(pb + FileInfoRecGS_auxType, fi.aux_type, 0);
    if (pcount >= 5) set_memory16_c(pb + FileInfoRecGS_storageType, fi.storage_type, 0);

    if (pcount >= 6) host_set_date_time_rec(pb + FileInfoRecGS_createDateTime, fi.create_date);
    if (pcount >= 7) host_set_date_time_rec(pb + FileInfoRecGS_modDateTime, fi.modified_date);

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

    host_set_date_time(pb + FileRec_createDate, fi.create_date);
    host_set_date_time(pb + FileRec_modDate, fi.modified_date);

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
  word16 type = host_storage_type(path, &rv);
  if (rv) return rv;
  if (g_cfg_host_read_only) return drvrWrtProt;

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

  char *p = host_gc_append_path(path, "*");
  h = FindFirstFile(p, &data);
  if (h == INVALID_HANDLE_VALUE) {
    *error = host_map_win32_error(GetLastError());
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
		  *error = outOfMem;
        free_directory(dd);
        FindClose(h);
        return NULL;
      }
      dd = tmp;
    }
    dd->entries[dd->num_entries++] = strdup(data.cFileName);
  } while (FindNextFile(h, &data) != 0);

  //?
  DWORD e = GetLastError();
  if (e != ERROR_NO_MORE_FILES) *error = host_map_win32_error(e);

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
    *error = host_map_win32_error(GetLastError());
  }
  return h;
}

static HANDLE open_resource_fork(const char *path, word16 *access, word16 *error) {

  char *rpath = host_gc_append_string(path, ":AFP_Resource");

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
    *error = host_map_win32_error(GetLastError());
  }
  return h;
}



static word32 fst_open(int class, const char *path) {


  word32 pb = get_memory24_c(engine.direct + dp_param_blk_ptr, 0);

  struct file_info fi;
  word16 rv = 0;

  rv = host_get_file_info(path, &fi);
  if (rv) return rv;


  HANDLE h = INVALID_HANDLE_VALUE;
  int type = file_regular;
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
    type = file_resource;
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
    type = file_directory;
  }


  if (g_cfg_host_read_only) {
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
    case file_regular:
      h = open_data_fork(path, &access, &rv);
      break;
    case file_resource:
      h = open_resource_fork(path, &access, &rv);
      break;
    case file_directory:
      dd = read_directory(path, &rv);
      break;
  }

  if (rv) return rv;


  if (class) {
    if (pcount >= 5) set_memory16_c(pb + OpenRecGS_access, access, 0);
    if (pcount >= 6) set_memory16_c(pb + OpenRecGS_fileType, fi.file_type, 0);
    if (pcount >= 7) set_memory32_c(pb + OpenRecGS_auxType, fi.aux_type, 0);
    if (pcount >= 8) set_memory16_c(pb + OpenRecGS_storageType, fi.storage_type, 0);

    if (pcount >= 9) host_set_date_time_rec(pb + OpenRecGS_createDateTime, fi.create_date);
    if (pcount >= 10) host_set_date_time_rec(pb + OpenRecGS_modDateTime, fi.modified_date);

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

  if (type == file_regular) {

    if (g_cfg_host_crlf) {
      if (fi.file_type == 0x04 || fi.file_type == 0xb0)
        e->translate = translate_crlf;
    }

    if (g_cfg_host_merlin && fi.file_type == 0x04) {
      int n = strlen(path);
      if (n >= 3 && path[n-1] == 'S' && path[n-2] == '.')
        e->translate = translate_merlin;
    }
  }



  e->access = access;
  e->path = strdup(path);
  e->type = type;


  // insert it in the linked list.
  e->next = fd_head;
  fd_head = e;

  engine.xreg = e->cookie;
  engine.yreg = access;       // actual access, needed in fcr.

  return rv;
}

static word32 fst_read(int class) {

  int cookie = engine.yreg;
  struct fd_entry *e = find_fd(cookie);

  if (!e) return invalidRefNum;

  switch (e->type) {
    case file_directory:
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

    for (word32 i = 0; i < request_count; ++i) {
      byte b;
      DWORD read_count;
      ok = ReadFile(e->handle, &b, 1, &read_count, NULL);
      if (!ok) return host_map_win32_error(GetLastError());
      if (read_count == 0) break;
      transfer_count++;

      switch(e->translate) {
        case translate_crlf:
          host_lf_to_cr(&b, 1);
          break;
        case translate_merlin:
          host_text_to_merlin(&b, 1);
          break;
      }

      set_memory_c(data_buffer++, b, 0);
      if (newline_table[b & newline_mask]) break;
    }
    if (transfer_count == 0) rv = eofEncountered;
  }
  else {
    DWORD read_count;
    byte *data = host_gc_malloc(request_count);
    if (!data) return outOfMem;

    ok = ReadFile(e->handle, data, request_count, &read_count, NULL);
    if (!ok) rv = host_map_win32_error(GetLastError());
    else if (read_count == 0) rv = eofEncountered;
    if (read_count > 0) {
      transfer_count = read_count;
      switch(e->translate) {
        case translate_crlf:
          host_lf_to_cr(data, transfer_count);
          break;
        case translate_merlin:
          host_text_to_merlin(data, transfer_count);
          break;
      }
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
    case file_directory:
      return badStoreType;
  }
  if (g_cfg_host_read_only) return drvrWrtProt;

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
  byte *data = host_gc_malloc(request_count);
  if (!data) return outOfMem;

  for (word32 i = 0; i < request_count; ++i) {
    data[i] = get_memory_c(data_buffer + i,0);
  }

  switch (e->translate) {
    case translate_crlf:
      host_cr_to_lf(data, request_count);
      break;
    case translate_merlin:
      host_merlin_to_text(data, request_count);
      break;
  }
  word32 rv = 0;
  DWORD write_count = 0;
  int ok = WriteFile(e->handle, data, request_count, &write_count, NULL);
  if (!ok) rv = host_map_win32_error(GetLastError());
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
    case file_directory:
      return badStoreType;
  }

  if (!FlushFileBuffers(e->handle)) return host_map_win32_error(GetLastError());
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
      d.QuadPart = -(LONGLONG)displacement;
      break;
    case eofMinus:
      m = FILE_END;
      d.QuadPart = -(LONGLONG)displacement;
      break;

    default:
      return paramRangeErr;

  }
  if (!SetFilePointerEx(h, d, position, m)) return host_map_win32_error(GetLastError());
  return 0;
}

static word32 fst_set_mark(int class) {

  int cookie = engine.yreg;

  struct fd_entry *e = find_fd(cookie);
  if (!e) return invalidRefNum;

  switch (e->type) {
    case file_directory:
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
    case file_directory:
      return badStoreType;
  }
  if (g_cfg_host_read_only) return drvrWrtProt;

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
  if (!SetEndOfFile(e->handle)) return host_map_win32_error(GetLastError());

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
    case file_directory:
      return badStoreType;
  }

  // get the current mark
  LARGE_INTEGER pos;
  LARGE_INTEGER zero; zero.QuadPart = 0;
  if (!SetFilePointerEx(e->handle, zero, &pos, FILE_CURRENT)) return host_map_win32_error(GetLastError());

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
    case file_directory:
      return badStoreType;
  }

  LARGE_INTEGER eof;

  if (!GetFileSizeEx(e->handle, &eof)) return host_map_win32_error(GetLastError());

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


  if (e->type != file_directory) return badFileFormat;

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
    case 0:             // displacement is absolute entry number.
      break;
    case 1:             // displacement is added to the current displacement.
      displacement = dir_displacement + displacement;
      break;
    case 2:             // displacement is substracted from current displacement.
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
  char *fullpath = host_gc_append_path(e->path, dname);
  struct  file_info fi;
  rv = host_get_file_info(fullpath, &fi);

  if (dname) fprintf(stderr, " - %s", dname);

  // p16 and gs/os both use truncating c1 output string.
  rv = set_gsstr_truncate(name, dname);

  if (class) {

    if (pcount > 2) set_memory16_c(pb + DirEntryRecGS_flags, fi.storage_type == 0x05 ? 0x8000 : 0, 0);

    if (pcount >= 6) set_memory16_c(pb + DirEntryRecGS_entryNum, displacement, 0);
    if (pcount >= 7) set_memory16_c(pb + DirEntryRecGS_fileType, fi.file_type, 0);
    if (pcount >= 8) set_memory32_c(pb + DirEntryRecGS_eof, fi.eof, 0);
    if (pcount >= 9) set_memory32_c(pb + DirEntryRecGS_blockCount, fi.blocks, 0);

    if (pcount >= 10) host_set_date_time_rec(pb + DirEntryRecGS_createDateTime, fi.create_date);
    if (pcount >= 11) host_set_date_time_rec(pb + DirEntryRecGS_modDateTime, fi.modified_date);

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

    host_set_date_time_rec(pb + DirEntryRec_createTime, fi.create_date);
    host_set_date_time_rec(pb + DirEntryRec_modTime, fi.modified_date);

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
  word16 type = host_storage_type(path1, &rv);
  if (rv) return rv;
  if (type == 0x0f) return badStoreType;
  if (g_cfg_host_read_only) return drvrWrtProt;

  if (!MoveFile(path1, path2)) return host_map_win32_error(GetLastError());
  return 0;
}

static word32 fst_format(int class) {
  return notBlockDev;
}

static word32 fst_erase(int class) {
  return notBlockDev;
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
  return host_gc_strdup(buffer);
}


#else
#define expand_path(x, y) (x)
#endif

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

  fprintf(stderr, "Host FST: %04x %s", call, call_name(call));

  if (call & 0x8000) {
    fputs("\n", stderr);
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

    if (!host_root) {
      acc = networkError;
      engine.acc =  acc;
      SEC();
      fprintf(stderr, "          %02x   %s\n", acc, host_error_name(acc));

      return;
    }


    int class = call >> 13;
    call &= 0x1fff;

    if (class > 1) {
      acc = invalidClass;
      engine.acc = acc;
      SEC();
      fprintf(stderr, "          %02x   %s\n", acc, host_error_name(acc));

      return;
    }

    char *path1 = NULL;
    char *path2 = NULL;
    char *path3 = NULL;
    char *path4 = NULL;
    const char *cp;

    switch(call & 0xff) {
      case 0x01:
      case 0x02:
      case 0x05:
      case 0x06:
      case 0x0b:
      case 0x10:
        path1 = get_path1();
        break;
      case 0x04:
        path1 = get_path1();
        path2 = get_path2();
        break;
    }

    if (path1) fprintf(stderr, " - %s", path1);
    if (path2) fprintf(stderr, " - %s", path2);


    switch(call & 0xff) {
      case 0x01:
        cp = check_path(path1, &acc);
        if (acc) break;
        path3 = host_gc_append_path(host_root, cp);
        path3 = expand_path(path3, &acc);
        if (acc) break;

        acc = fst_create(class, path3);
        break;
      case 0x02:
        cp = check_path(path1, &acc);
        if (acc) break;
        path3 = host_gc_append_path(host_root, cp);
        path3 = expand_path(path3, &acc);
        if (acc) break;

        acc = fst_destroy(class, path3);
        break;
      case 0x04:

        cp = check_path(path1, &acc);
        if (acc) break;
        path3 = host_gc_append_path(host_root, cp);
        path3 = expand_path(path3, &acc);
        if (acc) break;

        cp = check_path(path2, &acc);
        if (acc) break;
        path4 = host_gc_append_path(host_root, cp);
        path4 = expand_path(path4, &acc);
        if (acc) break;

        acc = fst_change_path(class, path3, path4);
        break;
      case 0x05:
        cp = check_path(path1, &acc);
        if (acc) break;
        path3 = host_gc_append_path(host_root, cp);
        path3 = expand_path(path3, &acc);
        if (acc) break;

        acc = fst_set_file_info(class, path3);
        break;
      case 0x06:
        cp = check_path(path1, &acc);
        if (acc) break;
        path3 = host_gc_append_path(host_root, cp);
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
        path3 = host_gc_append_path(host_root, cp);
        path3 = expand_path(path3, &acc);
        if (acc) break;

        acc = fst_clear_backup(class, path3);
        break;
      case 0x10:
        cp = check_path(path1, &acc);
        if (acc) break;
        path3 = host_gc_append_path(host_root, cp);
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
    fputs("\n", stderr);
  }

  if (acc) fprintf(stderr, "          %02x   %s\n", acc, host_error_name(acc));

  host_gc_free();

  engine.acc = acc;
  if (acc) SEC();
  else CLC();
}

