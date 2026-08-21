#include "funcs.h"

#define FUNC_BUCKETS 128

typedef struct FuncEntry {
    char *name;
    Node *body;
    struct FuncEntry *next;
} FuncEntry;

static FuncEntry *buckets[FUNC_BUCKETS];

static unsigned hash_name(const char *s) {
    unsigned h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++);
    return h % FUNC_BUCKETS;
}

void funcs_init(void) {
    for (int i = 0; i < FUNC_BUCKETS; i++) buckets[i] = NULL;
}

void func_define(const char *name, Node *body) {
    unsigned h = hash_name(name);
    for (FuncEntry *e = buckets[h]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            node_free(e->body);
            e->body = body;
            return;
        }
    }
    FuncEntry *e = xmalloc(sizeof(FuncEntry));
    e->name = xstrdup(name);
    e->body = body;
    e->next = buckets[h];
    buckets[h] = e;
}

Node *func_get(const char *name) {
    unsigned h = hash_name(name);
    for (FuncEntry *e = buckets[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e->body;
    return NULL;
}

void func_undefine(const char *name) {
    unsigned h = hash_name(name);
    FuncEntry **pp = &buckets[h];
    while (*pp) {
        FuncEntry *e = *pp;
        if (strcmp(e->name, name) == 0) {
            *pp = e->next;
            node_free(e->body);
            free(e->name);
            free(e);
            return;
        }
        pp = &e->next;
    }
}

void func_list_names(strvec_t *out) {
    for (int i = 0; i < FUNC_BUCKETS; i++)
        for (FuncEntry *e = buckets[i]; e; e = e->next)
            sv_push_dup(out, e->name);
}
