#ifndef SPIRE_BUILTINS_H
#define SPIRE_BUILTINS_H

#include "common.h"

typedef int (*BuiltinFn)(int argc, char **argv);

void builtins_init(void);
BuiltinFn builtin_lookup(const char *name);
bool builtin_exists(const char *name);
void builtins_list_names(strvec_t *out);

#endif
