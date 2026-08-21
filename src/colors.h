#ifndef SPIRE_COLORS_H
#define SPIRE_COLORS_H

#include "common.h"

/* Returns a heap string containing the ANSI escape sequence for a color
 * name ("green", "brightcyan", "default", or a raw 256-color number like
 * "208"). Caller frees. */
char *ansi_code_for(const char *name);

#define ANSI_RESET "\x1b[0m"

/* Appends `\x1b[..m` + text[0..len) + reset to *out. */
void color_wrap_n(dstr_t *out, const char *color_name, const char *text, size_t len);
void color_wrap(dstr_t *out, const char *color_name, const char *text);

#endif
