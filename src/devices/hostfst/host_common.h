#pragma once

#include "defc_shim.h"

#ifdef _WIN32

#include <stdint.h>

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

void afp_init(struct AFP_Info *info, word16 file_type, word32 aux_type);
BOOL afp_verify(struct AFP_Info *info);
int afp_to_filetype(struct AFP_Info *info, word16 *file_type, word32 *aux_type);
enum { prefer_prodos, prefer_hfs };
void afp_synchronize(struct AFP_Info *info, int preference);

#endif

#ifdef _WIN32
typedef FILETIME host_time_t;
typedef struct AFP_Info host_finder_info_t;
#else
typedef time_t host_time_t;
typedef unsigned char host_finder_info_t[32];
#endif



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

  host_time_t create_date;
  host_time_t modified_date;
  word16 access;
  word16 storage_type;
  word16 file_type;
  word32 aux_type;
  word32 eof;
  word32 blocks;
  word32 resource_eof;
  word32 resource_blocks;
  int has_fi;
  #ifdef _WIN32
  struct AFP_Info afp;
  #else
  byte finder_info[32];
  #endif
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

#ifdef __cplusplus
extern "C" {
#endif

extern char *g_cfg_host_path;
extern int g_cfg_host_read_only;
extern int g_cfg_host_crlf;
extern int g_cfg_host_merlin;
extern char *host_root;

unsigned host_startup(void);
void host_shutdown(void);

#ifdef _WIN32
int host_is_root(const BY_HANDLE_FILE_INFORMATION *info);
#else
int host_is_root(struct stat *);
#endif

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

#ifdef _WIN32
word32 host_map_win32_error(DWORD);
word32 host_map_win32_error_path(DWORD, const char *path);
#endif

/* file info */
int host_finder_info_to_filetype(const byte *buffer, word16 *file_type, word32 *aux_type);
int host_file_type_to_finder_info(byte *buffer, word16 file_type, word32 aux_type);

void host_get_file_xinfo(const char *path, struct file_info *fi);
word32 host_get_file_info(const char *path, struct file_info *fi);
word32 host_set_file_info(const char *path, struct file_info *fi);

/* guesses filetype/auxtype from extension */
void host_synthesize_file_xinfo(const char *path, struct file_info *fi);

void host_set_date_time_rec(word32 ptr, host_time_t time);
void host_set_date_time(word32 ptr, host_time_t time);

host_time_t host_get_date_time(word32 ptr);
host_time_t host_get_date_time_rec(word32 ptr);

/* convert to prodos date/time */
word32 host_convert_date_time(host_time_t time);


/* scan a directory, return array of char * */
unsigned host_scan_directory(const char *path, char ***out, size_t *entries, unsigned p8);
void host_free_directory(char **data, size_t count);


/* 0x01, 0x0d, 0x0f, 0 on error */ 
unsigned host_storage_type(const char *path, word16 *error_ptr);

void host_hexdump(word32 address, int size);
void host_hexdump_native(void *data, unsigned address, int size);

#ifdef __cplusplus
}
#endif

