#ifndef SPIRE_LEXER_H
#define SPIRE_LEXER_H

#include "common.h"

typedef enum {
    TOK_WORD,
    TOK_PIPE,       /* |    */
    TOK_AND,        /* &&   */
    TOK_OR,         /* ||   */
    TOK_SEMI,       /* ;    */
    TOK_BG,         /* &    */
    TOK_LT,         /* <    */
    TOK_DLT,        /* <<   */
    TOK_DLT_DASH,   /* <<-  */
    TOK_GT,         /* >    */
    TOK_DGT,        /* >>   */
    TOK_GTERR,      /* 2>   */
    TOK_DGTERR,     /* 2>>  */
    TOK_ERR2OUT,    /* 2>&1 */
    TOK_GTAMP,      /* &>   */
    TOK_RPAREN,     /* bare ')' - case-arm terminator / subshell close   */
    TOK_NEWLINE,
    TOK_EOF
} TokType;

typedef struct {
    TokType type;
    char *text; /* owned, only meaningful for TOK_WORD */
} Token;

typedef struct {
    const char *src;
    size_t pos;
    size_t len;
} Lexer;

void lexer_init(Lexer *lx, const char *src);
void lexer_next(Lexer *lx, Token *out);
void token_free(Token *t);

/* Quick pre-scan used by the REPL to know whether more input is needed
 * before attempting a real parse (unterminated quotes / trailing backslash). */
bool input_needs_more(const char *s);

#endif
