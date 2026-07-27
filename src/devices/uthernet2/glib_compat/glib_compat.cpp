/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Minimal GLib replacements for vendored libslirp (GPL-3.0-or-later).
 */

#include "glib.h"

#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

extern "C" {

void g_log_compat(const char *level, const char *fmt, ...) {
    // Skip routine debug chatter from libslirp; keep warnings and worse.
    if (level && std::strcmp(level, "DEBUG") == 0) {
        return;
    }
    fprintf(stderr, "[slirp/%s] ", level ? level : "?");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

gpointer g_malloc(gsize n_bytes) {
    if (n_bytes == 0) {
        n_bytes = 1;
    }
    void *p = std::malloc(n_bytes);
    if (!p) {
        abort();
    }
    return p;
}

gpointer g_malloc0(gsize n_bytes) {
    void *p = g_malloc(n_bytes);
    std::memset(p, 0, n_bytes);
    return p;
}

gpointer g_realloc(gpointer mem, gsize n_bytes) {
    if (n_bytes == 0) {
        g_free(mem);
        return nullptr;
    }
    void *p = std::realloc(mem, n_bytes);
    if (!p) {
        abort();
    }
    return p;
}

void g_free(gpointer mem) {
    std::free(mem);
}

gchar *g_strdup(const gchar *str) {
    if (!str) {
        return nullptr;
    }
    const size_t n = std::strlen(str) + 1;
    gchar *out = static_cast<gchar *>(g_malloc(n));
    std::memcpy(out, str, n);
    return out;
}

gint g_snprintf(gchar *string, gulong n, const gchar *format, ...) {
    va_list ap;
    va_start(ap, format);
    const gint r = g_vsnprintf(string, n, format, ap);
    va_end(ap);
    return r;
}

gint g_vsnprintf(gchar *string, gulong n, const gchar *format, va_list args) {
    if (!string || n == 0) {
        return 0;
    }
    return vsnprintf(string, n, format, args);
}

gint g_ascii_strcasecmp(const gchar *s1, const gchar *s2) {
    if (!s1) {
        return s2 ? -1 : 0;
    }
    if (!s2) {
        return 1;
    }
    while (*s1 && *s2) {
        const unsigned char c1 = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(*s1)));
        const unsigned char c2 = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(*s2)));
        if (c1 != c2) {
            return static_cast<gint>(c1) - static_cast<gint>(c2);
        }
        ++s1;
        ++s2;
    }
    return static_cast<gint>(static_cast<unsigned char>(*s1)) -
           static_cast<gint>(static_cast<unsigned char>(*s2));
}

gboolean g_str_has_prefix(const gchar *str, const gchar *prefix) {
    if (!str || !prefix) {
        return FALSE;
    }
    const size_t n = std::strlen(prefix);
    return std::strncmp(str, prefix, n) == 0 ? TRUE : FALSE;
}

gchar *g_strstr_len(const gchar *haystack, gssize haystack_len, const gchar *needle) {
    if (!haystack || !needle) {
        return nullptr;
    }
    if (haystack_len < 0) {
        return const_cast<gchar *>(std::strstr(haystack, needle));
    }
    const size_t nlen = std::strlen(needle);
    if (nlen == 0) {
        return const_cast<gchar *>(haystack);
    }
    for (gssize i = 0; i + static_cast<gssize>(nlen) <= haystack_len; ++i) {
        if (std::memcmp(haystack + i, needle, nlen) == 0) {
            return const_cast<gchar *>(haystack + i);
        }
    }
    return nullptr;
}

gsize g_strlcpy(gchar *dest, const gchar *src, gsize dest_size) {
    if (!dest || dest_size == 0) {
        return src ? std::strlen(src) : 0;
    }
    if (!src) {
        dest[0] = '\0';
        return 0;
    }
    const size_t slen = std::strlen(src);
    const size_t copy = (slen >= dest_size) ? (dest_size - 1) : slen;
    std::memcpy(dest, src, copy);
    dest[copy] = '\0';
    return slen;
}

const gchar *g_strerror(gint errnum) {
    return strerror(errnum);
}

guint g_strv_length(gchar **str_array) {
    if (!str_array) {
        return 0;
    }
    guint n = 0;
    while (str_array[n]) {
        ++n;
    }
    return n;
}

void g_strfreev(gchar **str_array) {
    if (!str_array) {
        return;
    }
    for (guint i = 0; str_array[i]; ++i) {
        g_free(str_array[i]);
    }
    g_free(str_array);
}

GString *g_string_new(const gchar *init) {
    auto *s = static_cast<GString *>(g_malloc0(sizeof(GString)));
    const char *src = init ? init : "";
    s->len = std::strlen(src);
    s->allocated_len = s->len + 1;
    s->str = static_cast<gchar *>(g_malloc(s->allocated_len));
    std::memcpy(s->str, src, s->len + 1);
    return s;
}

static void g_string_ensure(GString *string, gsize need) {
    if (need <= string->allocated_len) {
        return;
    }
    gsize cap = string->allocated_len ? string->allocated_len : 16;
    while (cap < need) {
        cap *= 2;
    }
    string->str = static_cast<gchar *>(g_realloc(string->str, cap));
    string->allocated_len = cap;
}

GString *g_string_append_printf(GString *string, const gchar *format, ...) {
    if (!string) {
        return nullptr;
    }
    va_list ap;
    va_start(ap, format);
    va_list ap2;
    va_copy(ap2, ap);
    const int n = vsnprintf(nullptr, 0, format, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return string;
    }
    g_string_ensure(string, string->len + static_cast<gsize>(n) + 1);
    vsnprintf(string->str + string->len, static_cast<size_t>(n) + 1, format, ap2);
    va_end(ap2);
    string->len += static_cast<gsize>(n);
    return string;
}

gchar *g_string_free(GString *string, gboolean free_segment) {
    if (!string) {
        return nullptr;
    }
    gchar *result = nullptr;
    if (free_segment) {
        g_free(string->str);
    } else {
        result = string->str;
    }
    g_free(string);
    return result;
}

struct _GRand {
    std::mt19937 eng;
};

GRand *g_rand_new(void) {
    auto *r = new _GRand();
    std::random_device rd;
    r->eng.seed(rd());
    return r;
}

void g_rand_free(GRand *rand_) {
    delete rand_;
}

gint32 g_rand_int_range(GRand *rand_, gint32 begin, gint32 end) {
    if (!rand_ || begin >= end) {
        return begin;
    }
    std::uniform_int_distribution<gint32> dist(begin, end - 1);
    return dist(rand_->eng);
}

const gchar *g_getenv(const gchar *name) {
    if (!name) {
        return nullptr;
    }
    return std::getenv(name);
}

guint g_parse_debug_string(const gchar *string, const GDebugKey *keys, guint nkeys) {
    if (!string || !keys) {
        return 0;
    }
    guint result = 0;
    std::string s(string);
    size_t start = 0;
    while (start < s.size()) {
        while (start < s.size() &&
               (s[start] == ':' || s[start] == ',' || s[start] == ' ' || s[start] == ';')) {
            ++start;
        }
        if (start >= s.size()) {
            break;
        }
        size_t end = start;
        while (end < s.size() && s[end] != ':' && s[end] != ',' && s[end] != ' ' && s[end] != ';') {
            ++end;
        }
        const std::string token = s.substr(start, end - start);
        for (guint i = 0; i < nkeys; ++i) {
            if (keys[i].key && token == keys[i].key) {
                result |= keys[i].value;
            }
        }
        start = end;
    }
    return result;
}

void g_error_free(GError *error) {
    if (!error) {
        return;
    }
    g_free(error->message);
    g_free(error);
}

static void set_spawn_error(GError **error, const char *msg) {
    if (!error) {
        return;
    }
    auto *e = static_cast<GError *>(g_malloc0(sizeof(GError)));
    e->message = g_strdup(msg);
    *error = e;
}

gboolean g_shell_parse_argv(const gchar * /*command_line*/, gint * /*argcp*/, gchar *** /*argvp*/,
                            GError **error) {
    set_spawn_error(error, "g_shell_parse_argv not supported (glib_compat stub)");
    return FALSE;
}

gboolean g_spawn_async(const gchar * /*working_directory*/, gchar ** /*argv*/, gchar ** /*envp*/,
                       GSpawnFlags /*flags*/, GSpawnChildSetupFunc /*child_setup*/,
                       gpointer /*user_data*/, GPid * /*child_pid*/, GError **error) {
    set_spawn_error(error, "g_spawn_async not supported (glib_compat stub)");
    return FALSE;
}

gboolean g_spawn_async_with_fds(const gchar * /*working_directory*/, gchar ** /*argv*/,
                                gchar ** /*envp*/, GSpawnFlags /*flags*/,
                                GSpawnChildSetupFunc /*child_setup*/, gpointer /*user_data*/,
                                GPid * /*child_pid*/, gint /*stdin_fd*/, gint /*stdout_fd*/,
                                gint /*stderr_fd*/, GError **error) {
    set_spawn_error(error, "g_spawn_async_with_fds not supported (glib_compat stub)");
    return FALSE;
}

}  // extern "C"
