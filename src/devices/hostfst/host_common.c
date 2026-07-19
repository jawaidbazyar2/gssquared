#define _BSD_SOURCE

#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "defc_shim.h"
#include "gsos.h"
#include "fst.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef _MSC_VER
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif


#include "host_common.h"


char *host_root = NULL;

char *g_cfg_host_path = ""; // must not be null.
int g_cfg_host_read_only = 0;
int g_cfg_host_crlf = 1;
int g_cfg_host_merlin = 0;






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
    "badBufferAddress",             /* P8 MLI only */
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


static int hex(byte c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c + 10 - 'a';
  if (c >= 'A' && c <= 'F') return c + 10 - 'A';
  return 0;
}


int host_finder_info_to_filetype(const byte *buffer, word16 *file_type, word32 *aux_type) {

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

void host_synthesize_file_xinfo(const char *path, struct file_info *fi) {

  /* guess the file type / auxtype based on extension */
  int n;
  const char *dot = NULL;
  const char *slash = NULL;

  for(n = 0;; ++n) {
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
        return;
      }
    }
  }
  return;
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



void host_free_directory(char **data, size_t count) {
  for (int i = 0; i < count; ++i) free(data[i]);
  free(data);
}
