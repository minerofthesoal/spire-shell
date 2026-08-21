#ifndef SPIRE_ARRAYS_H
#define SPIRE_ARRAYS_H

#include "common.h"

void arrays_init(void);
bool array_exists(const char *name);

/* Replaces (or creates) the array, copying `elems`. */
void array_set(const char *name, strvec_t *elems);
/* Appends copies of `elems` to the array (creating it if needed). */
void array_append(const char *name, strvec_t *elems);

size_t array_len(const char *name); /* 0 if undefined */
/* Negative indices count from the end (-1 = last element), fish/python
 * style. Returns NULL if undefined or out of range. */
const char *array_get_index(const char *name, long idx);
/* Grows the array with empty strings if `idx` is past the current end. */
void array_set_index(const char *name, long idx, const char *value);

/* Appends copies of every element (in order) to *out*. */
void array_get_all(const char *name, strvec_t *out);

void array_unset(const char *name);
void arrays_dump(strvec_t *names_out);

#endif
