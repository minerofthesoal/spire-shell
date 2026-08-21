#include "vars.h"

extern char **environ;

#define VAR_BUCKETS 256

typedef struct VarEntry {
    char *name;
    char *value;
    bool exported;
    struct VarEntry *next;
} VarEntry;

static VarEntry *buckets[VAR_BUCKETS];

static unsigned hash_name(const char *s) {
    unsigned h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++);
    return h % VAR_BUCKETS;
}

static VarEntry *find_entry(const char *name) {
    unsigned h = hash_name(name);
    for (VarEntry *e = buckets[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e;
    return NULL;
}

void vars_init(void) {
    for (int i = 0; i < VAR_BUCKETS; i++) buckets[i] = NULL;
    for (char **e = environ; e && *e; e++) {
        char *eq = strchr(*e, '=');
        if (!eq) continue;
        char *name = xstrndup(*e, (size_t)(eq - *e));
        var_set(name, eq + 1, true);
        free(name);
    }
}

const char *var_get(const char *name) {
    VarEntry *e = find_entry(name);
    return e ? e->value : NULL;
}

void var_set(const char *name, const char *value, bool exported) {
    unsigned h = hash_name(name);
    VarEntry *e = find_entry(name);
    if (!e) {
        e = xmalloc(sizeof(VarEntry));
        e->name = xstrdup(name);
        e->value = NULL;
        e->exported = false;
        e->next = buckets[h];
        buckets[h] = e;
    }
    free(e->value);
    e->value = xstrdup(value ? value : "");
    if (exported) e->exported = true;
    if (e->exported) setenv(e->name, e->value, 1);
}

void var_export(const char *name) {
    VarEntry *e = find_entry(name);
    if (!e) { var_set(name, "", true); return; }
    e->exported = true;
    setenv(e->name, e->value, 1);
}

void var_unset(const char *name) {
    unsigned h = hash_name(name);
    VarEntry **pp = &buckets[h];
    while (*pp) {
        VarEntry *e = *pp;
        if (strcmp(e->name, name) == 0) {
            *pp = e->next;
            if (e->exported) unsetenv(e->name);
            free(e->name);
            free(e->value);
            free(e);
            return;
        }
        pp = &e->next;
    }
}

bool var_is_exported(const char *name) {
    VarEntry *e = find_entry(name);
    return e && e->exported;
}

void vars_dump(strvec_t *names_out) {
    for (int i = 0; i < VAR_BUCKETS; i++)
        for (VarEntry *e = buckets[i]; e; e = e->next)
            sv_push_dup(names_out, e->name);
}
