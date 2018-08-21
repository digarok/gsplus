enum {
  file_non,
  file_regular,
  file_resource,
  file_directory,
};

enum {
  translate_none,
  translate_crlf,
  translate_merlin,
};



struct file_info {

  time_t create_date;
  time_t modified_date;
  word16 access;
  word16 storage_type;
  word16 file_type;
  word32 aux_type;
  word32 eof;
  word32 blocks;
  word32 resource_eof;
  word32 resource_blocks;
  mode_t st_mode;
  int has_fi;
  byte finder_info[32];
};


extern Engine_reg engine;

#define SEC() engine.psr |= 0x01
#define CLC() engine.psr &= ~0x01
#define SEV() engine.psr |= 0x40
#define CLV() engine.psr &= ~0x40
#define SEZ() engine.psr |= 0x02
#define CLZ() engine.psr &= ~0x02
#define SEI() engine.psr |= 0x04
#define CLI() engine.psr &= ~0x04

enum {
  C = 0x01,
  Z = 0x02,
  I = 0x04,
  D = 0x08,
  X = 0x10,
  M = 0x20,
  V = 0x40,
  N = 0x80
};

extern int g_cfg_host_read_only;
extern int g_cfg_host_crlf;
extern int g_cfg_host_merlin;
extern char *host_root;

unsigned host_startup(void);
void host_shutdown(void);

int host_is_root(struct stat *);

/* garbage collected string routines */

void *host_gc_malloc(size_t size);
void host_gc_free(void);
char *host_gc_strdup(const char *src);
char *host_gc_append_path(const char *a, const char *b);
char *host_gc_append_string(const char *a, const char *b);

/* text conversion */
void host_cr_to_lf(byte *buffer, size_t size);
void host_lf_to_cr(byte *buffer, size_t size);
void host_merlin_to_text(byte *buffer, size_t size);
void host_text_to_merlin(byte *buffer, size_t size);

/* errno -> IIgs/mli error */
word32 host_map_errno(int xerrno);
word32 host_map_errno_path(int xerrno, const char *path);
const char *host_error_name(word16 error);

/* file info */

int host_file_type_to_finder_info(byte *buffer, word16 file_type, word32 aux_type);
void host_get_file_xinfo(const char *path, struct file_info *fi);
word32 host_get_file_info(const char *path, struct file_info *fi);
word32 host_set_file_info(const char *path, struct file_info *fi);


void host_set_date_time_rec(word32 ptr, time_t time);
void host_set_date_time(word32 ptr, time_t time);

time_t host_get_date_time(word32 ptr);
time_t host_get_date_time_rec(word32 ptr);

/* convert to prodos date/time */
word32 host_convert_date_time(time_t time);


void host_hexdump(word32 address, int size);
void host_hexdump_native(void *data, unsigned address, int size);

