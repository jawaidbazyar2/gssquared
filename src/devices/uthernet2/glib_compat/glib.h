/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Minimal GLib API surface for compiling vendored libslirp without system GLib.
 * C-callable; implementations live in glib_compat.cpp (C++ / STL).
 */
#ifndef GS2_GLIB_COMPAT_H
#define GS2_GLIB_COMPAT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <signal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int gboolean;
typedef int gint;
typedef unsigned int guint;
typedef short gshort;
typedef unsigned short gushort;
typedef long glong;
typedef unsigned long gulong;
typedef char gchar;
typedef unsigned char guchar;
typedef void *gpointer;
typedef const void *gconstpointer;
typedef size_t gsize;
typedef ptrdiff_t gssize;
typedef int64_t gint64;
typedef uint64_t guint64;
typedef int32_t gint32;
typedef uint32_t guint32;
typedef int16_t gint16;
typedef uint16_t guint16;
typedef int8_t gint8;
typedef uint8_t guint8;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define G_N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define G_STATIC_ASSERT(expr) _Static_assert(expr, #expr)
#ifdef __cplusplus
#undef G_STATIC_ASSERT
#define G_STATIC_ASSERT(expr) static_assert(expr, #expr)
#endif

#ifndef G_GNUC_PRINTF
#if defined(__GNUC__) || defined(__clang__)
#define G_GNUC_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define G_GNUC_PRINTF(fmt, args)
#endif
#endif

#ifndef G_LIKELY
#if defined(__GNUC__) || defined(__clang__)
#define G_LIKELY(x) __builtin_expect(!!(x), 1)
#define G_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define G_LIKELY(x) (x)
#define G_UNLIKELY(x) (x)
#endif
#endif

/* Byte order */
#ifndef G_LITTLE_ENDIAN
#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN 4321
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define G_BYTE_ORDER G_BIG_ENDIAN
#elif defined(_WIN32) || defined(__LITTLE_ENDIAN__) || \
    (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#define G_BYTE_ORDER G_LITTLE_ENDIAN
#else
#define G_BYTE_ORDER G_LITTLE_ENDIAN
#endif

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || \
    defined(__powerpc64__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
#define GLIB_SIZEOF_VOID_P 8
#else
#define GLIB_SIZEOF_VOID_P 4
#endif
/* Pretend we are a modern-enough GLib so libslirp prefers the simple spawn path. */
#define GLIB_MAJOR_VERSION 2
#define GLIB_MINOR_VERSION 70
#define GLIB_MICRO_VERSION 0
#define GLIB_CHECK_VERSION(major, minor, micro) \
    ((GLIB_MAJOR_VERSION > (major)) || \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION > (minor)) || \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION == (minor) && \
      GLIB_MICRO_VERSION >= (micro)))

#ifndef G_OS_UNIX
#if !defined(_WIN32)
#define G_OS_UNIX 1
#endif
#endif

#ifndef G_OS_WIN32
#if defined(_WIN32)
#define G_OS_WIN32 1
#endif
#endif

#define GLIB_UNUSED_PARAM(x) ((void)(x))

#define G_SIZEOF_MEMBER(type, member) sizeof(((type *)0)->member)

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define GUINT16_TO_BE(v) ((guint16)((((guint16)(v) & 0x00FFu) << 8) | (((guint16)(v) & 0xFF00u) >> 8)))
#define GUINT16_FROM_BE(v) GUINT16_TO_BE(v)
#define GUINT32_TO_BE(v) \
    ((guint32)((((guint32)(v) & 0x000000FFu) << 24) | (((guint32)(v) & 0x0000FF00u) << 8) | \
               (((guint32)(v) & 0x00FF0000u) >> 8) | (((guint32)(v) & 0xFF000000u) >> 24)))
#define GUINT32_FROM_BE(v) GUINT32_TO_BE(v)
#define GINT16_TO_BE(v) ((gint16)GUINT16_TO_BE((guint16)(v)))
#define GINT16_FROM_BE(v) ((gint16)GUINT16_FROM_BE((guint16)(v)))
#define GINT32_TO_BE(v) ((gint32)GUINT32_TO_BE((guint32)(v)))
#define GINT32_FROM_BE(v) ((gint32)GUINT32_FROM_BE((guint32)(v)))
#else
#define GUINT16_TO_BE(v) ((guint16)(v))
#define GUINT16_FROM_BE(v) ((guint16)(v))
#define GUINT32_TO_BE(v) ((guint32)(v))
#define GUINT32_FROM_BE(v) ((guint32)(v))
#define GINT16_TO_BE(v) ((gint16)(v))
#define GINT16_FROM_BE(v) ((gint16)(v))
#define GINT32_TO_BE(v) ((gint32)(v))
#define GINT32_FROM_BE(v) ((gint32)(v))
#endif

/* --- memory --- */
gpointer g_malloc(gsize n_bytes);
gpointer g_malloc0(gsize n_bytes);
gpointer g_realloc(gpointer mem, gsize n_bytes);
void g_free(gpointer mem);
gchar *g_strdup(const gchar *str);

#define g_new(type, count) ((type *)g_malloc(sizeof(type) * (gsize)(count)))
#define g_new0(type, count) ((type *)g_malloc0(sizeof(type) * (gsize)(count)))

/* --- logging / asserts --- */
void g_log_compat(const char *level, const char *fmt, ...) G_GNUC_PRINTF(2, 3);

#define g_debug(...) g_log_compat("DEBUG", __VA_ARGS__)
#define g_warning(...) g_log_compat("WARNING", __VA_ARGS__)
#define g_critical(...) g_log_compat("CRITICAL", __VA_ARGS__)
#define g_error(...) \
    do { \
        g_log_compat("ERROR", __VA_ARGS__); \
        abort(); \
    } while (0)

#define g_assert(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "g_assert failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define g_assert_not_reached() \
    do { \
        fprintf(stderr, "g_assert_not_reached (%s:%d)\n", __FILE__, __LINE__); \
        abort(); \
    } while (0)

#define g_return_if_fail(expr) \
    do { \
        if (!(expr)) { \
            g_critical("g_return_if_fail: %s", #expr); \
            return; \
        } \
    } while (0)

#define g_return_val_if_fail(expr, val) \
    do { \
        if (!(expr)) { \
            g_critical("g_return_val_if_fail: %s", #expr); \
            return (val); \
        } \
    } while (0)

#define g_warn_if_fail(expr) \
    do { \
        if (!(expr)) { \
            g_warning("g_warn_if_fail: %s", #expr); \
        } \
    } while (0)

#define g_warn_if_reached() g_warning("g_warn_if_reached (%s:%d)", __FILE__, __LINE__)

#ifndef g_warning_once
#define g_warning_once(...) g_warning(__VA_ARGS__)
#endif

/* --- strings --- */
gint g_snprintf(gchar *string, gulong n, const gchar *format, ...) G_GNUC_PRINTF(3, 4);
gint g_vsnprintf(gchar *string, gulong n, const gchar *format, va_list args);
gint g_ascii_strcasecmp(const gchar *s1, const gchar *s2);
gboolean g_str_has_prefix(const gchar *str, const gchar *prefix);
gchar *g_strstr_len(const gchar *haystack, gssize haystack_len, const gchar *needle);
gsize g_strlcpy(gchar *dest, const gchar *src, gsize dest_size);
const gchar *g_strerror(gint errnum);

typedef gchar **GStrv;
guint g_strv_length(gchar **str_array);
void g_strfreev(gchar **str_array);

typedef struct _GString {
    gchar *str;
    gsize len;
    gsize allocated_len;
} GString;

GString *g_string_new(const gchar *init);
GString *g_string_append_printf(GString *string, const gchar *format, ...) G_GNUC_PRINTF(2, 3);
gchar *g_string_free(GString *string, gboolean free_segment);

/* --- random --- */
typedef struct _GRand GRand;
GRand *g_rand_new(void);
void g_rand_free(GRand *rand_);
gint32 g_rand_int_range(GRand *rand_, gint32 begin, gint32 end);

/* --- env / debug --- */
const gchar *g_getenv(const gchar *name);

typedef struct {
    const gchar *key;
    guint value;
} GDebugKey;

guint g_parse_debug_string(const gchar *string, const GDebugKey *keys, guint nkeys);

/* --- GError (minimal) --- */
typedef struct _GError {
    gint domain;
    gint code;
    gchar *message;
} GError;

void g_error_free(GError *error);

/* --- spawn stubs (slirp_add_exec only; unused by Uthernet) --- */
typedef enum {
    G_SPAWN_DEFAULT = 0,
    G_SPAWN_SEARCH_PATH = 1 << 2,
} GSpawnFlags;

typedef int GPid;
typedef void (*GSpawnChildSetupFunc)(gpointer user_data);

gboolean g_shell_parse_argv(const gchar *command_line, gint *argcp, gchar ***argvp, GError **error);
gboolean g_spawn_async(const gchar *working_directory, gchar **argv, gchar **envp, GSpawnFlags flags,
                       GSpawnChildSetupFunc child_setup, gpointer user_data, GPid *child_pid,
                       GError **error);
gboolean g_spawn_async_with_fds(const gchar *working_directory, gchar **argv, gchar **envp,
                                GSpawnFlags flags, GSpawnChildSetupFunc child_setup,
                                gpointer user_data, GPid *child_pid, gint stdin_fd, gint stdout_fd,
                                gint stderr_fd, GError **error);

/* Unused by libslirp but referenced in comments */
typedef struct GArray GArray;
typedef struct GPollFD GPollFD;

/* Windows GetAdaptersAddresses flag used by slirp.c — not a GLib symbol, but
 * some MinGW environments expect it from windows headers; define if missing. */
#ifndef GAA_FLAG_INCLUDE_PREFIX
#define GAA_FLAG_INCLUDE_PREFIX 0x0010
#endif

#ifdef __cplusplus
}
#endif

#endif /* GS2_GLIB_COMPAT_H */
