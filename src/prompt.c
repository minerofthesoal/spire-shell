#include "prompt.h"
#include "colors.h"
#include "vars.h"
#include <unistd.h>
#include <pwd.h>

static void append_shortened_cwd(dstr_t *out) {
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) { ds_append(out, "?"); return; }
    const char *home = var_get("HOME");
    if (home && str_has_prefix(cwd, home)) {
        size_t hl = strlen(home);
        if (cwd[hl] == '\0') { ds_append_c(out, '~'); return; }
        if (cwd[hl] == '/') { ds_append_c(out, '~'); ds_append(out, cwd + hl); return; }
    }
    ds_append(out, cwd);
}

static void append_git_branch(dstr_t *out) {
    FILE *f = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (!f) return;
    char buf[256];
    if (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        if (n > 0) { ds_append_c(out, '('); ds_append(out, buf); ds_append_c(out, ')'); }
    }
    pclose(f);
}

char *prompt_render(const char *fmt) {
    dstr_t out; ds_init(&out);
    const char *p = fmt;
    while (*p) {
        if (*p != '%') { ds_append_c(&out, *p); p++; continue; }
        p++;
        if (!*p) { ds_append_c(&out, '%'); break; }
        switch (*p) {
            case '%': ds_append_c(&out, '%'); p++; break;
            case 'n': {
                const char *u = var_get("USER");
                if (!u) { struct passwd *pw = getpwuid(getuid()); u = pw ? pw->pw_name : "user"; }
                ds_append(&out, u); p++; break;
            }
            case 'm': {
                char host[256];
                if (gethostname(host, sizeof(host)) == 0) {
                    char *dot = strchr(host, '.');
                    if (dot) *dot = '\0';
                    ds_append(&out, host);
                } else ds_append(&out, "host");
                p++; break;
            }
            case '~': append_shortened_cwd(&out); p++; break;
            case 'd': {
                char cwd[4096];
                if (getcwd(cwd, sizeof(cwd))) ds_append(&out, cwd);
                p++; break;
            }
            case 'g': append_git_branch(&out); p++; break;
            case '#': ds_append_c(&out, getuid() == 0 ? '#' : '$'); p++; break;
            case 'F': {
                p++;
                if (*p == '{') {
                    p++;
                    const char *start = p;
                    while (*p && *p != '}') p++;
                    char *name = xstrndup(start, (size_t)(p - start));
                    if (*p == '}') p++;
                    char *code = ansi_code_for(name);
                    ds_append(&out, code);
                    free(code); free(name);
                }
                break;
            }
            case 'f': ds_append(&out, ANSI_RESET); p++; break;
            default: ds_append_c(&out, '%'); ds_append_c(&out, *p); p++; break;
        }
    }
    char *r = xstrdup(out.data);
    ds_free(&out);
    return r;
}

char *prompt_render_continuation(void) {
    return xstrdup("spire\x1b[90m...\x1b[0m> ");
}
