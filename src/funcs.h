#ifndef SPIRE_FUNCS_H
#define SPIRE_FUNCS_H

#include "common.h"
#include "ast.h"

void funcs_init(void);
void func_define(const char *name, Node *body); /* takes ownership of body */
Node *func_get(const char *name);                /* NULL if not defined */
void func_undefine(const char *name);
/* iterate defined function names into a strvec (for completion/`functions` builtin) */
void func_list_names(strvec_t *out);

#endif
