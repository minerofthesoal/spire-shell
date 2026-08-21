#ifndef SPIRE_EXPAND_H
#define SPIRE_EXPAND_H

#include "common.h"

/* Expands one raw (lexer) word into zero or more argv strings, appended to
 * *out*. Handles: single/double quoting, backslash escapes, $VAR / ${VAR}
 * (with :-  :+  # length forms), $(...) and `...` and bare (...) command
 * substitution, ~ expansion, IFS word-splitting of unquoted expansions, and
 * globbing of literal (unquoted, unexpanded) glob characters. */
void expand_word(const char *raw, strvec_t *out);

/* Expand a word that must yield exactly one string (e.g. a redirection
 * target or a for-loop list word). Returns a newly allocated string. */
char *expand_word_single(const char *raw);

/* Like expand_word_single, but skips filesystem globbing — for case/switch
 * patterns, where "*.txt" is a match pattern, not a file lookup. */
char *expand_pattern_single(const char *raw);

/* Expands $VAR / $(...) / `...` within a heredoc body, treating everything
 * else (including quote characters) as literal text -- no word splitting,
 * no quote parsing, no globbing. */
char *expand_heredoc_body(const char *raw);

#endif

