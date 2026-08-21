#include "colors.h"
#include <ctype.h>

typedef struct { const char *name; int code; } NamedColor;

static const NamedColor g_colors[] = {
    {"black", 30}, {"red", 31}, {"green", 32}, {"yellow", 33},
    {"blue", 34}, {"magenta", 35}, {"cyan", 36}, {"white", 37},
    {"brightblack", 90}, {"brightred", 91}, {"brightgreen", 92}, {"brightyellow", 93},
    {"brightblue", 94}, {"brightmagenta", 95}, {"brightcyan", 96}, {"brightwhite", 97},
    {"gray", 90}, {"grey", 90},
    {NULL, 0}
};

char *ansi_code_for(const char *name) {
    if (!name || strcmp(name, "default") == 0 || name[0] == '\0') return xstrdup("");
    bool all_digit = true;
    for (const char *p = name; *p; p++) if (!isdigit((unsigned char)*p)) { all_digit = false; break; }
    if (all_digit) {
        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[38;5;%sm", name);
        return xstrdup(buf);
    }
    bool bold = false;
    const char *n = name;
    if (str_has_prefix(n, "bold-")) { bold = true; n += 5; }
    for (int i = 0; g_colors[i].name; i++) {
        if (strcmp(g_colors[i].name, n) == 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "\x1b[%s%dm", bold ? "1;" : "", g_colors[i].code);
            return xstrdup(buf);
        }
    }
    return xstrdup("");
}

void color_wrap_n(dstr_t *out, const char *color_name, const char *text, size_t len) {
    if (len == 0) return;
    char *code = ansi_code_for(color_name);
    if (code[0]) { ds_append(out, code); ds_append_n(out, text, len); ds_append(out, ANSI_RESET); }
    else ds_append_n(out, text, len);
    free(code);
}

void color_wrap(dstr_t *out, const char *color_name, const char *text) {
    color_wrap_n(out, color_name, text, strlen(text));
}
