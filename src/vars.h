#ifndef SPIRE_VARS_H
#define SPIRE_VARS_H

#include "common.h"

void vars_init(void);                       /* import process environ */
const char *var_get(const char *name);       /* NULL if unset */
void var_set(const char *name, const char *value, bool exported);
void var_export(const char *name);           /* mark existing var exported */
void var_unset(const char *name);
bool var_is_exported(const char *name);
void vars_dump(strvec_t *names_out); /* every currently-set variable name */

/* special vars: $?, $$, $0, positional handled by exec/main directly via var_set */

#endif
