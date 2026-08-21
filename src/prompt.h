#ifndef SPIRE_PROMPT_H
#define SPIRE_PROMPT_H

#include "common.h"

/* Renders the configured prompt format into an ANSI-colored string.
 * Recognizes: %n user  %m host  %~ cwd(~-shortened)  %d full cwd
 *             %g git branch (blank if not in a repo)  %#  prompt char
 *             %F{color}...%f  foreground color span   %% literal percent
 * Caller frees the result. */
char *prompt_render(const char *fmt);

/* secondary "still typing a block" prompt, e.g. "spire... > " */
char *prompt_render_continuation(void);

#endif
