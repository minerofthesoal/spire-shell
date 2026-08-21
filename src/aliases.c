#include "aliases.h"

#define ALIAS_BUCKETS 64

typedef struct AliasEntry {
    char *name;
    char *value;
    struct AliasEntry *next;
} AliasEntry;

static AliasEntry *buckets[ALIAS_BUCKETS];

static unsigned hash_name(const char *s) {
    unsigned h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++);
    return h % ALIAS_BUCKETS;
}

void aliases_init(void) {
    for (int i = 0; i < ALIAS_BUCKETS; i++) buckets[i] = NULL;
}

void alias_set(const char *name, const char *value) {
    unsigned h = hash_name(name);
    for (AliasEntry *e = buckets[h]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            free(e->value);
            e->value = xstrdup(value);
            return;
        }
    }
    AliasEntry *e = xmalloc(sizeof(AliasEntry));
    e->name = xstrdup(name);
    e->value = xstrdup(value);
    e->next = buckets[h];
    buckets[h] = e;
}

const char *alias_get(const char *name) {
    unsigned h = hash_name(name);
    for (AliasEntry *e = buckets[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e->value;
    return NULL;
}

void alias_unset(const char *name) {
    unsigned h = hash_name(name);
    AliasEntry **pp = &buckets[h];
    while (*pp) {
        AliasEntry *e = *pp;
        if (strcmp(e->name, name) == 0) {
            *pp = e->next;
            free(e->name); free(e->value); free(e);
            return;
        }
        pp = &e->next;
    }
}

void alias_list(strvec_t *names_out, strvec_t *values_out) {
    for (int i = 0; i < ALIAS_BUCKETS; i++)
        for (AliasEntry *e = buckets[i]; e; e = e->next) {
            sv_push_dup(names_out, e->name);
            sv_push_dup(values_out, e->value);
        }
}
