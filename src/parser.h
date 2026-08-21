#ifndef SPIRE_PARSER_H
#define SPIRE_PARSER_H

#include "common.h"
#include "ast.h"

typedef enum {
    PARSE_OK,
    PARSE_INCOMPLETE, /* input ended mid-block; caller should read another line and retry */
    PARSE_ERROR,       /* syntax error, message in *err */
    PARSE_EMPTY        /* input was blank / comments only */
} ParseStatus;

/* Parses `src` (a full accumulated buffer, possibly multi-line) as a spire
 * program. On PARSE_OK, *out is a heap-allocated N_SEQ root node (caller
 * frees with node_free). On PARSE_ERROR, *err is a heap-allocated message
 * (caller frees). */
ParseStatus parse_program(const char *src, Node **out, char **err);

#endif
