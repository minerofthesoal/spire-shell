#ifndef SPIRE_COMPLETE_H
#define SPIRE_COMPLETE_H

#include "common.h"

/* Finds completions for the word touching `cursor` in `line`. Appends
 * candidate replacement words (the full word, not just the suffix) to
 * `out`. *out_word_start receives the byte offset in `line` where the
 * word-being-completed begins. */
void complete_line(const char *line, size_t cursor, strvec_t *out, size_t *out_word_start);

#endif
