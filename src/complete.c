#include "complete.h"
#include "vars.h"
#include "funcs.h"
#include "aliases.h"
#include "builtins.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

static bool is_word_sep(char c) {
    return isspace((unsigned char)c) || c == '|' || c == '&' || c == ';' || c == '<' || c == '>';
}

static void complete_files(const char *word, strvec_t *out) {
    const char *slash = strrchr(word, '/');
    char dir[4096], prefix[1024];
    if (slash) {
        size_t dl = (size_t)(slash - word);
        if (dl == 0) snprintf(dir, sizeof(dir), "/");
        else { memcpy(dir, word, dl); dir[dl] = '\0'; }
        snprintf(prefix, sizeof(prefix), "%s", slash + 1);
    } else {
        snprintf(dir, sizeof(dir), ".");
        snprintf(prefix, sizeof(prefix), "%s", word);
    }
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    size_t plen = strlen(prefix);
    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (plen == 0 && ent->d_name[0] == '.') continue;
        if (strncmp(ent->d_name, prefix, plen) != 0) continue;

        dstr_t cand; ds_init(&cand);
        if (slash) { ds_append_n(&cand, word, (size_t)(slash - word)); ds_append_c(&cand, '/'); }
        ds_append(&cand, ent->d_name);

        char statpath[4352];
        snprintf(statpath, sizeof(statpath), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(statpath, &st) == 0 && S_ISDIR(st.st_mode)) ds_append_c(&cand, '/');

        sv_push(out, xstrdup(cand.data));
        ds_free(&cand);
    }
    closedir(d);
}

static void complete_commands(const char *prefix, strvec_t *out) {
    size_t plen = strlen(prefix);
    strvec_t names; sv_init(&names);
    builtins_list_names(&names);
    func_list_names(&names);
    strvec_t avnames, avvals; sv_init(&avnames); sv_init(&avvals);
    alias_list(&avnames, &avvals);
    for (size_t i = 0; i < avnames.count; i++) sv_push_dup(&names, avnames.items[i]);
    sv_free(&avnames); sv_free(&avvals);
    for (size_t i = 0; i < names.count; i++)
        if (strncmp(names.items[i], prefix, plen) == 0) sv_push_dup(out, names.items[i]);
    sv_free(&names);

    const char *path = var_get("PATH");
    if (!path) return;
    char *copy = xstrdup(path);
    for (char *dir = strtok(copy, ":"); dir; dir = strtok(NULL, ":")) {
        DIR *d = opendir(dir);
        if (!d) continue;
        struct dirent *ent;
        while ((ent = readdir(d))) {
            if (strncmp(ent->d_name, prefix, plen) != 0) continue;
            char full[4096];
            snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
            if (access(full, X_OK) == 0) sv_push_dup(out, ent->d_name);
        }
        closedir(d);
    }
    free(copy);
}

static void complete_vars(const char *prefix, strvec_t *out) {
    strvec_t names; sv_init(&names);
    vars_dump(&names);
    size_t plen = strlen(prefix);
    for (size_t i = 0; i < names.count; i++) {
        if (strncmp(names.items[i], prefix, plen) == 0) {
            dstr_t d; ds_init(&d); ds_append_c(&d, '$'); ds_append(&d, names.items[i]);
            sv_push(out, xstrdup(d.data));
            ds_free(&d);
        }
    }
    sv_free(&names);
}

static void dedup(strvec_t *v) {
    for (size_t i = 0; i < v->count; i++) {
        for (size_t j = v->count - 1; j > i; j--) {
            if (strcmp(v->items[i], v->items[j]) == 0) {
                free(v->items[j]);
                memmove(&v->items[j], &v->items[j+1], sizeof(char *) * (v->count - j - 1));
                v->count--;
            }
        }
    }
}

void complete_line(const char *line, size_t cursor, strvec_t *out, size_t *out_word_start) {
    size_t start = cursor;
    while (start > 0 && !is_word_sep(line[start - 1])) start--;
    *out_word_start = start;
    char *word = xstrndup(line + start, cursor - start);

    if (word[0] == '$') {
        complete_vars(word + 1, out);
        free(word);
        dedup(out);
        return;
    }

    bool cmd_pos = true;
    size_t back = start;
    while (back > 0 && isspace((unsigned char)line[back - 1])) back--;
    if (back > 0 && !(line[back-1]=='|'||line[back-1]=='&'||line[back-1]==';')) cmd_pos = false;

    if (cmd_pos && !strchr(word, '/')) complete_commands(word, out);
    else complete_files(word, out);

    free(word);
    dedup(out);
}
