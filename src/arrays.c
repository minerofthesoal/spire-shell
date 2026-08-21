#include "arrays.h"

#define ARR_BUCKETS 128

typedef struct ArrEntry {
    char *name;
    strvec_t elems;
    struct ArrEntry *next;
} ArrEntry;

static ArrEntry *buckets[ARR_BUCKETS];

static unsigned hash_name(const char *s) {
    unsigned h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++);
    return h % ARR_BUCKETS;
}

static ArrEntry *find(const char *name) {
    unsigned h = hash_name(name);
    for (ArrEntry *e = buckets[h]; e; e = e->next) if (strcmp(e->name, name) == 0) return e;
    return NULL;
}

static ArrEntry *find_or_create(const char *name) {
    ArrEntry *e = find(name);
    if (e) return e;
    unsigned h = hash_name(name);
    e = xmalloc(sizeof(ArrEntry));
    e->name = xstrdup(name);
    sv_init(&e->elems);
    e->next = buckets[h];
    buckets[h] = e;
    return e;
}

void arrays_init(void) { for (int i = 0; i < ARR_BUCKETS; i++) buckets[i] = NULL; }

bool array_exists(const char *name) { return find(name) != NULL; }

void array_set(const char *name, strvec_t *elems) {
    ArrEntry *e = find_or_create(name);
    sv_clear(&e->elems);
    for (size_t i = 0; i < elems->count; i++) sv_push_dup(&e->elems, elems->items[i]);
}

void array_append(const char *name, strvec_t *elems) {
    ArrEntry *e = find_or_create(name);
    for (size_t i = 0; i < elems->count; i++) sv_push_dup(&e->elems, elems->items[i]);
}

size_t array_len(const char *name) {
    ArrEntry *e = find(name);
    return e ? e->elems.count : 0;
}

static bool resolve_index(size_t count, long idx, size_t *out) {
    if (idx < 0) idx += (long)count;
    if (idx < 0 || (size_t)idx >= count) return false;
    *out = (size_t)idx;
    return true;
}

const char *array_get_index(const char *name, long idx) {
    ArrEntry *e = find(name);
    if (!e) return NULL;
    size_t i;
    if (!resolve_index(e->elems.count, idx, &i)) return NULL;
    return e->elems.items[i];
}

void array_set_index(const char *name, long idx, const char *value) {
    ArrEntry *e = find_or_create(name);
    if (idx < 0) {
        size_t i;
        if (resolve_index(e->elems.count, idx, &i)) { free(e->elems.items[i]); e->elems.items[i] = xstrdup(value); }
        return;
    }
    while ((size_t)idx >= e->elems.count) sv_push_dup(&e->elems, "");
    free(e->elems.items[idx]);
    e->elems.items[idx] = xstrdup(value);
}

void array_get_all(const char *name, strvec_t *out) {
    ArrEntry *e = find(name);
    if (!e) return;
    for (size_t i = 0; i < e->elems.count; i++) sv_push_dup(out, e->elems.items[i]);
}

void array_unset(const char *name) {
    unsigned h = hash_name(name);
    ArrEntry **pp = &buckets[h];
    while (*pp) {
        ArrEntry *e = *pp;
        if (strcmp(e->name, name) == 0) {
            *pp = e->next;
            sv_free(&e->elems);
            free(e->name);
            free(e);
            return;
        }
        pp = &e->next;
    }
}

void arrays_dump(strvec_t *names_out) {
    for (int i = 0; i < ARR_BUCKETS; i++)
        for (ArrEntry *e = buckets[i]; e; e = e->next)
            sv_push_dup(names_out, e->name);
}
