#ifndef SPIRE_LINEEDIT_H
#define SPIRE_LINEEDIT_H

#include "common.h"

void lineedit_init(void);
void lineedit_shutdown(void);

/* Reads one logical line from the terminal in raw mode, with live
 * highlighting, history browsing, and tab completion. Returns a newly
 * allocated string (no trailing newline), or NULL on EOF (Ctrl-D on an
 * empty line). `prompt_fmt` is the *unrendered* prompt template (re-rendered
 * fresh so %~ / %g stay current); pass NULL to use the continuation
 * prompt instead. */
char *lineedit_read(const char *prompt_fmt);

#endif
