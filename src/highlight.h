#ifndef SPIRE_HIGHLIGHT_H
#define SPIRE_HIGHLIGHT_H

#include "common.h"

/* Returns a newly allocated, ANSI-colorized rendering of `buf` (a possibly
 * incomplete/partial command line as the user is typing it). Always
 * succeeds, tolerant of unterminated quotes etc. Caller frees. */
char *highlight_line(const char *buf);

bool command_exists(const char *name);

#endif
