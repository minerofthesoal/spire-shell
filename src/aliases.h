#ifndef SPIRE_ALIASES_H
#define SPIRE_ALIASES_H

#include "common.h"

void aliases_init(void);
void alias_set(const char *name, const char *value);
const char *alias_get(const char *name); /* NULL if not defined */
void alias_unset(const char *name);
void alias_list(strvec_t *names_out, strvec_t *values_out);

#endif
