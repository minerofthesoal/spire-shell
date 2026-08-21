#include "lexer.h"
#include <ctype.h>

void lexer_init(Lexer *lx, const char *src) {
    lx->src = src;
    lx->pos = 0;
    lx->len = strlen(src);
}

static int peekc(Lexer *lx, size_t off) {
    size_t p = lx->pos + off;
    if (p >= lx->len) return -1;
    return (unsigned char)lx->src[p];
}

static void skip_spaces(Lexer *lx) {
    while (lx->pos < lx->len) {
        char c = lx->src[lx->pos];
        if (c == ' ' || c == '\t' || c == '\r') { lx->pos++; continue; }
        if (c == '\\' && peekc(lx, 1) == '\n') { lx->pos += 2; continue; } /* line continuation */
        break;
    }
}

static void skip_comment(Lexer *lx) {
    while (lx->pos < lx->len && lx->src[lx->pos] != '\n') lx->pos++;
}

/* Scan a single WORD token, preserving quote characters in the raw text.
 * Expansion of quotes/vars happens later, at execution time. */
static char *scan_word(Lexer *lx) {
    dstr_t buf;
    ds_init(&buf);
    bool in_sq = false, in_dq = false, in_bt = false;
    int paren_depth = 0;

    while (lx->pos < lx->len) {
        char c = lx->src[lx->pos];

        if (!in_sq && !in_dq && !in_bt && paren_depth == 0) {
            if (isspace((unsigned char)c)) break;
            if (c == '|' || c == '&' || c == ';' || c == '<' || c == '>' || c == '#' || c == ')') break;
        }

        if (c == '\\' && !in_sq) {
            ds_append_c(&buf, c);
            lx->pos++;
            if (lx->pos < lx->len) { ds_append_c(&buf, lx->src[lx->pos]); lx->pos++; }
            continue;
        }
        if (c == '\'' && !in_dq && !in_bt) { in_sq = !in_sq; ds_append_c(&buf, c); lx->pos++; continue; }
        if (c == '"' && !in_sq && !in_bt) { in_dq = !in_dq; ds_append_c(&buf, c); lx->pos++; continue; }
        if (c == '`' && !in_sq) { in_bt = !in_bt; ds_append_c(&buf, c); lx->pos++; continue; }
        if (c == '(' && !in_sq) { paren_depth++; ds_append_c(&buf, c); lx->pos++; continue; }
        if (c == ')' && !in_sq && paren_depth > 0) { paren_depth--; ds_append_c(&buf, c); lx->pos++; continue; }
        /* only treat '{' as depth-opening right after a '$' (i.e. "${...}")
         * so that a bare standalone '{' (function/command-group blocks)
         * still lexes as its own word token */
        if (c == '{' && !in_sq && buf.len > 0 && buf.data[buf.len-1] == '$') {
            paren_depth++; ds_append_c(&buf, c); lx->pos++; continue;
        }
        if (c == '}' && !in_sq && paren_depth > 0) { paren_depth--; ds_append_c(&buf, c); lx->pos++; continue; }

        ds_append_c(&buf, c);
        lx->pos++;
    }

    char *result = xstrdup(buf.data);
    ds_free(&buf);
    return result;
}

void lexer_next(Lexer *lx, Token *out) {
    skip_spaces(lx);

    if (lx->pos >= lx->len) { out->type = TOK_EOF; out->text = NULL; return; }

    char c = lx->src[lx->pos];

    if (c == '\n') { lx->pos++; out->type = TOK_NEWLINE; out->text = NULL; return; }

    if (c == '#') { skip_comment(lx); lexer_next(lx, out); return; }

    /* fd-prefixed redirection: bare digit run immediately followed by > or < */
    if (isdigit((unsigned char)c)) {
        size_t save = lx->pos;
        size_t p = lx->pos;
        while (p < lx->len && isdigit((unsigned char)lx->src[p])) p++;
        if (p < lx->len && (lx->src[p] == '>' || lx->src[p] == '<') && p > lx->pos) {
            char digits[16];
            size_t dn = p - lx->pos;
            if (dn > 15) dn = 15;
            memcpy(digits, lx->src + lx->pos, dn);
            digits[dn] = '\0';
            int fd = atoi(digits);
            lx->pos = p;
            if (fd == 2 && lx->src[lx->pos] == '>') {
                if (peekc(lx, 1) == '>') {
                    lx->pos += 2; out->type = TOK_DGTERR; out->text = NULL; return;
                }
                if (peekc(lx, 1) == '&' && peekc(lx, 2) == '1') {
                    lx->pos += 3; out->type = TOK_ERR2OUT; out->text = NULL; return;
                }
                lx->pos += 1; out->type = TOK_GTERR; out->text = NULL; return;
            }
            /* other fds: fall back to treating the digits as a plain word */
            lx->pos = save;
        } else {
            lx->pos = save;
        }
    }

    switch (c) {
        case '|':
            lx->pos++;
            if (peekc(lx, 0) == '|') { lx->pos++; out->type = TOK_OR; }
            else out->type = TOK_PIPE;
            out->text = NULL; return;
        case '&':
            lx->pos++;
            if (peekc(lx, 0) == '&') { lx->pos++; out->type = TOK_AND; }
            else if (peekc(lx, 0) == '>') { lx->pos++; out->type = TOK_GTAMP; }
            else out->type = TOK_BG;
            out->text = NULL; return;
        case ';':
            lx->pos++; out->type = TOK_SEMI; out->text = NULL; return;
        case ')':
            lx->pos++; out->type = TOK_RPAREN; out->text = NULL; return;
        case '<':
            lx->pos++;
            if (peekc(lx, 0) == '<') {
                lx->pos++;
                if (peekc(lx, 0) == '-') { lx->pos++; out->type = TOK_DLT_DASH; }
                else out->type = TOK_DLT;
            } else out->type = TOK_LT;
            out->text = NULL; return;
        case '>':
            lx->pos++;
            if (peekc(lx, 0) == '>') { lx->pos++; out->type = TOK_DGT; }
            else out->type = TOK_GT;
            out->text = NULL; return;
        default: break;
    }

    out->type = TOK_WORD;
    out->text = scan_word(lx);
}

void token_free(Token *t) {
    if (t->text) { free(t->text); t->text = NULL; }
}

bool input_needs_more(const char *s) {
    bool in_sq = false, in_dq = false, in_bt = false;
    int paren_depth = 0;
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\\' && !in_sq) { i++; continue; }
        if (c == '\'' && !in_dq && !in_bt) { in_sq = !in_sq; continue; }
        if (c == '"' && !in_sq && !in_bt) { in_dq = !in_dq; continue; }
        if (c == '`' && !in_sq) { in_bt = !in_bt; continue; }
        if (c == '(' && !in_sq) { paren_depth++; continue; }
        if (c == ')' && !in_sq && paren_depth > 0) { paren_depth--; continue; }
    }
    if (in_sq || in_dq || in_bt || paren_depth > 0) return true;
    /* trailing backslash line-continuation */
    size_t j = n;
    int bs = 0;
    while (j > 0 && s[j - 1] == '\\') { bs++; j--; }
    if (bs % 2 == 1) return true;
    return false;
}
