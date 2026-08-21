#include "expand.h"
#include "vars.h"
#include "exec.h"
#include "arith.h"
#include "arrays.h"
#include <ctype.h>
#include <glob.h>

static char *cmdsubst_run(const char *src) {
    char *out = NULL;
    exec_capture_stdout(src, &out);
    return out ? out : xstrdup("");
}

/* *pp points at '(' ; returns content between matching parens (exclusive),
 * advances *pp past the closing ')'. Respects nested parens/quotes. */
static char *scan_balanced_parens(const char **pp) {
    const char *p = *pp;
    p++;
    int depth = 1;
    dstr_t buf; ds_init(&buf);
    bool insq = false, indq = false;
    while (*p && depth > 0) {
        char c = *p;
        if (c == '\\' && !insq) {
            ds_append_c(&buf, c); p++;
            if (*p) { ds_append_c(&buf, *p); p++; }
            continue;
        }
        if (c == '\'' && !indq) { insq = !insq; ds_append_c(&buf, c); p++; continue; }
        if (c == '"' && !insq) { indq = !indq; ds_append_c(&buf, c); p++; continue; }
        if (c == '(' && !insq) { depth++; ds_append_c(&buf, c); p++; continue; }
        if (c == ')' && !insq) {
            depth--;
            p++;
            if (depth == 0) break;
            ds_append_c(&buf, c);
            continue;
        }
        ds_append_c(&buf, c); p++;
    }
    *pp = p;
    char *res = xstrdup(buf.data);
    ds_free(&buf);
    return res;
}

/* *pp points at '{'; non-nested content up to matching '}'. */
static char *scan_balanced_braces(const char **pp) {
    const char *p = *pp;
    p++;
    const char *start = p;
    while (*p && *p != '}') p++;
    char *res = xstrndup(start, (size_t)(p - start));
    if (*p == '}') p++;
    *pp = p;
    return res;
}

static char *scan_backtick(const char **pp) {
    const char *p = *pp;
    p++; /* opening ` */
    dstr_t buf; ds_init(&buf);
    while (*p && *p != '`') {
        if (*p == '\\' && (p[1] == '`' || p[1] == '\\' || p[1] == '$')) {
            ds_append_c(&buf, p[1]); p += 2; continue;
        }
        ds_append_c(&buf, *p); p++;
    }
    if (*p == '`') p++;
    *pp = p;
    char *res = xstrdup(buf.data);
    ds_free(&buf);
    return res;
}

static char *resolve_brace_expr(char *inner) {
    char *result;
    if (inner[0] == '#' && inner[1]) {
        const char *v = var_get(inner + 1);
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu", v ? strlen(v) : 0);
        result = xstrdup(buf);
        free(inner);
        return result;
    }
    char *colon = strchr(inner, ':');
    if (colon && colon[1]) {
        *colon = '\0';
        char op = colon[1];
        char *word = colon + 2;
        const char *v = var_get(inner);
        bool empty = (!v || v[0] == '\0');
        if (op == '-') result = xstrdup(empty ? word : v);
        else if (op == '+') result = xstrdup(empty ? "" : word);
        else if (op == '=') {
            if (empty) { var_set(inner, word, var_is_exported(inner)); result = xstrdup(word); }
            else result = xstrdup(v);
        } else {
            result = xstrdup(v ? v : "");
        }
        free(inner);
        return result;
    }
    const char *v = var_get(inner);
    result = xstrdup(v ? v : "");
    free(inner);
    return result;
}

typedef enum { ARR_NOT_ARRAY, ARR_SINGLE, ARR_MULTI } ArrBraceResult;

/* Parses a "name", "name[index]", "name[@|*]" or "#name[...]" brace-body
 * (as captured by scan_balanced_braces) as a possible array reference.
 * `inner` is always freed. */
static ArrBraceResult try_array_brace(char *inner, strvec_t *multi, char **single) {
    bool has_hash = (inner[0] == '#' && inner[1] != '\0');
    const char *namestart = has_hash ? inner + 1 : inner;
    size_t nl = 0;
    while (namestart[nl] && namestart[nl] != '[' && namestart[nl] != ':') nl++;
    char *name = xstrndup(namestart, nl);

    if (!array_exists(name)) { free(name); free(inner); return ARR_NOT_ARRAY; }

    if (namestart[nl] != '[') {
        /* bare ${arr} / ${#arr} where arr is an array: bash treats this as
         * element 0 (or its length). */
        const char *el = array_get_index(name, 0);
        if (has_hash) { char buf[32]; snprintf(buf, sizeof(buf), "%zu", el ? strlen(el) : 0); *single = xstrdup(buf); }
        else *single = xstrdup(el ? el : "");
        free(name); free(inner);
        return ARR_SINGLE;
    }

    const char *ip = namestart + nl + 1;
    const char *istart = ip;
    while (*ip && *ip != ']') ip++;
    char *idxtext = xstrndup(istart, (size_t)(ip - istart));
    ArrBraceResult result = ARR_SINGLE;

    if (has_hash) {
        long len;
        if (strcmp(idxtext, "@") == 0 || strcmp(idxtext, "*") == 0) len = (long)array_len(name);
        else { const char *el = array_get_index(name, arith_eval(idxtext)); len = el ? (long)strlen(el) : 0; }
        char buf[32]; snprintf(buf, sizeof(buf), "%ld", len);
        *single = xstrdup(buf);
    } else if (strcmp(idxtext, "@") == 0) {
        array_get_all(name, multi);
        result = ARR_MULTI;
    } else if (strcmp(idxtext, "*") == 0) {
        strvec_t tmp; sv_init(&tmp);
        array_get_all(name, &tmp);
        dstr_t d; ds_init(&d);
        for (size_t k = 0; k < tmp.count; k++) { if (k) ds_append_c(&d, ' '); ds_append(&d, tmp.items[k]); }
        *single = xstrdup(d.data);
        ds_free(&d); sv_free(&tmp);
    } else {
        const char *el = array_get_index(name, arith_eval(idxtext));
        *single = xstrdup(el ? el : "");
    }
    free(idxtext); free(name); free(inner);
    return result;
}

/* *pp points just after '$'. Returns true for a single-value result (with
 * *single set, never NULL), or false with *multi populated instead (one
 * entry per array element — the "${arr[@]}" case). Advances *pp past the
 * construct either way; if '$' wasn't followed by anything expandable, *pp
 * is left unchanged and *single = "$". */
static bool expand_dollar(const char **pp, strvec_t *multi, char **single) {
    const char *p = *pp;
    if (p[0] == '(' && p[1] == '(') {
        char *src = scan_balanced_parens(&p);
        *pp = p;
        long v = arith_eval(src);
        free(src);
        char buf[32]; snprintf(buf, sizeof(buf), "%ld", v);
        *single = xstrdup(buf);
        return true;
    }
    if (*p == '(') {
        char *src = scan_balanced_parens(&p);
        *pp = p;
        *single = cmdsubst_run(src);
        free(src);
        return true;
    }
    if (*p == '{') {
        char *inner = scan_balanced_braces(&p);
        *pp = p;
        char *inner_copy = xstrdup(inner);
        ArrBraceResult ar = try_array_brace(inner, multi, single);
        if (ar == ARR_SINGLE) { free(inner_copy); return true; }
        if (ar == ARR_MULTI) { free(inner_copy); return false; }
        *single = resolve_brace_expr(inner_copy);
        return true;
    }
    if (*p == '?' || *p == '$' || *p == '#' || *p == '@' || *p == '*' || (*p >= '0' && *p <= '9')) {
        char name[2] = { *p, '\0' };
        p++;
        *pp = p;
        const char *v = var_get(name);
        *single = xstrdup(v ? v : "");
        return true;
    }
    if (isalpha((unsigned char)*p) || *p == '_') {
        const char *start = p;
        while (isalnum((unsigned char)*p) || *p == '_') p++;
        char *name = xstrndup(start, (size_t)(p - start));
        *pp = p;
        if (array_exists(name)) {
            const char *el = array_get_index(name, 0);
            *single = xstrdup(el ? el : "");
        } else {
            const char *v = var_get(name);
            *single = xstrdup(v ? v : "");
        }
        free(name);
        return true;
    }
    *single = xstrdup("$"); /* literal '$', *pp left unchanged */
    return true;
}

/* Splits `val` on IFS whitespace, merging the first fragment into *cur and
 * pushing completed words to *out. Used only for UNQUOTED expansions. */
static void append_split(dstr_t *cur, strvec_t *out, const char *val) {
    if (!*val) return;
    if (isspace((unsigned char)val[0]) && cur->len > 0) {
        sv_push(out, xstrdup(cur->data));
        ds_clear(cur);
    }
    const char *p = val;
    bool first_seg = true;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (!first_seg) { sv_push(out, xstrdup(cur->data)); ds_clear(cur); }
        ds_append_n(cur, start, (size_t)(p - start));
        first_seg = false;
    }
    size_t vlen = strlen(val);
    if (vlen > 0 && isspace((unsigned char)val[vlen - 1])) {
        sv_push(out, xstrdup(cur->data));
        ds_clear(cur);
    }
}

/* Splices array elements in: element 0 merges into *cur, each subsequent
 * element flushes *cur as its own word and starts a fresh one. Used for
 * both quoted and unquoted "${arr[@]}" (each element stays atomic either
 * way, which matches the overwhelmingly common real-world usage). */
static void append_array_multi(dstr_t *cur, strvec_t *out, strvec_t *elems) {
    for (size_t i = 0; i < elems->count; i++) {
        if (i > 0) { sv_push(out, xstrdup(cur->data)); ds_clear(cur); }
        ds_append(cur, elems->items[i]);
    }
}

static void expand_word_impl(const char *raw, strvec_t *out, bool do_glob) {
    dstr_t cur; ds_init(&cur);
    bool force_emit = false;
    bool glob_eligible = false;
    size_t n = strlen(raw);
    size_t i = 0;

    if (n > 0 && raw[0] == '~' && (n == 1 || raw[1] == '/')) {
        const char *home = var_get("HOME");
        ds_append(&cur, home ? home : "~");
        force_emit = true;
        i = 1;
    }

    while (i < n) {
        char c = raw[i];

        if (c == '\'') {
            force_emit = true;
            i++;
            while (i < n && raw[i] != '\'') { ds_append_c(&cur, raw[i]); i++; }
            if (i < n) i++;
            continue;
        }
        if (c == '"') {
            force_emit = true;
            i++;
            while (i < n && raw[i] != '"') {
                if (raw[i] == '\\' && i + 1 < n &&
                    (raw[i+1]=='"' || raw[i+1]=='\\' || raw[i+1]=='$' || raw[i+1]=='`')) {
                    ds_append_c(&cur, raw[i+1]); i += 2; continue;
                }
                if (raw[i] == '$') {
                    const char *p = raw + i + 1;
                    strvec_t multi; sv_init(&multi);
                    char *val = NULL;
                    bool single = expand_dollar(&p, &multi, &val);
                    if (single) { ds_append(&cur, val); free(val); }
                    else append_array_multi(&cur, out, &multi);
                    sv_free(&multi);
                    i = (size_t)(p - raw);
                    continue;
                }
                if (raw[i] == '`') {
                    const char *p = raw + i;
                    char *src = scan_backtick(&p);
                    char *val = cmdsubst_run(src);
                    free(src);
                    ds_append(&cur, val);
                    free(val);
                    i = (size_t)(p - raw);
                    continue;
                }
                ds_append_c(&cur, raw[i]); i++;
            }
            if (i < n) i++;
            continue;
        }
        if (c == '\\') {
            force_emit = true;
            i++;
            if (i < n) { ds_append_c(&cur, raw[i]); i++; }
            continue;
        }
        if (c == '$') {
            const char *p = raw + i + 1;
            size_t before = (size_t)(p - raw);
            strvec_t multi; sv_init(&multi);
            char *val = NULL;
            bool single = expand_dollar(&p, &multi, &val);
            size_t after = (size_t)(p - raw);
            if (single && after == before) {
                force_emit = true;
                ds_append_c(&cur, '$');
                free(val);
            } else if (single) {
                append_split(&cur, out, val);
                free(val);
            } else {
                append_array_multi(&cur, out, &multi);
            }
            sv_free(&multi);
            i = after;
            continue;
        }
        if (c == '`') {
            const char *p = raw + i;
            char *src = scan_backtick(&p);
            char *val = cmdsubst_run(src);
            free(src);
            append_split(&cur, out, val);
            free(val);
            i = (size_t)(p - raw);
            continue;
        }
        if (c == '(') {
            const char *p = raw + i;
            char *src = scan_balanced_parens(&p);
            char *val = cmdsubst_run(src);
            free(src);
            append_split(&cur, out, val);
            free(val);
            i = (size_t)(p - raw);
            continue;
        }
        if (c == '*' || c == '?' || c == '[') glob_eligible = true;
        force_emit = true;
        ds_append_c(&cur, c);
        i++;
    }

    if (force_emit || cur.len > 0) {
        if (do_glob && glob_eligible && cur.len > 0) {
            glob_t g;
            int r = glob(cur.data, GLOB_NOCHECK, NULL, &g);
            if (r == 0 && g.gl_pathc > 0) {
                for (size_t k = 0; k < g.gl_pathc; k++) sv_push_dup(out, g.gl_pathv[k]);
                globfree(&g);
                ds_free(&cur);
                return;
            }
            globfree(&g);
        }
        sv_push_dup(out, cur.data);
    }
    ds_free(&cur);
}

void expand_word(const char *raw, strvec_t *out) { expand_word_impl(raw, out, true); }

char *expand_word_single(const char *raw) {
    strvec_t tmp; sv_init(&tmp);
    expand_word(raw, &tmp);
    char *r;
    if (tmp.count == 0) r = xstrdup("");
    else if (tmp.count == 1) r = xstrdup(tmp.items[0]);
    else {
        dstr_t d; ds_init(&d);
        for (size_t i = 0; i < tmp.count; i++) {
            if (i) ds_append_c(&d, ' ');
            ds_append(&d, tmp.items[i]);
        }
        r = xstrdup(d.data);
        ds_free(&d);
    }
    sv_free(&tmp);
    return r;
}

/* Like expand_word_single but never touches the filesystem glob()-wise —
 * used for `case`/`switch` patterns, where "*.txt" is a match pattern
 * (fed to fnmatch), not a file lookup. */
char *expand_pattern_single(const char *raw) {
    strvec_t tmp; sv_init(&tmp);
    expand_word_impl(raw, &tmp, false);
    char *r;
    if (tmp.count == 0) r = xstrdup("");
    else if (tmp.count == 1) r = xstrdup(tmp.items[0]);
    else {
        dstr_t d; ds_init(&d);
        for (size_t i = 0; i < tmp.count; i++) {
            if (i) ds_append_c(&d, ' ');
            ds_append(&d, tmp.items[i]);
        }
        r = xstrdup(d.data);
        ds_free(&d);
    }
    sv_free(&tmp);
    return r;
}

char *expand_heredoc_body(const char *raw) {
    dstr_t cur; ds_init(&cur);
    size_t n = strlen(raw);
    size_t i = 0;
    while (i < n) {
        if (raw[i] == '\\' && i + 1 < n && (raw[i+1] == '$' || raw[i+1] == '`' || raw[i+1] == '\\')) {
            ds_append_c(&cur, raw[i+1]);
            i += 2;
            continue;
        }
        if (raw[i] == '$') {
            const char *p = raw + i + 1;
            strvec_t multi; sv_init(&multi);
            char *val = NULL;
            bool single = expand_dollar(&p, &multi, &val);
            if (single) { ds_append(&cur, val); free(val); }
            else {
                for (size_t k = 0; k < multi.count; k++) {
                    if (k) ds_append_c(&cur, ' ');
                    ds_append(&cur, multi.items[k]);
                }
            }
            sv_free(&multi);
            i = (size_t)(p - raw);
            continue;
        }
        if (raw[i] == '`') {
            const char *p = raw + i;
            char *src = scan_backtick(&p);
            char *val = cmdsubst_run(src);
            free(src);
            ds_append(&cur, val);
            free(val);
            i = (size_t)(p - raw);
            continue;
        }
        ds_append_c(&cur, raw[i]);
        i++;
    }
    char *r = xstrdup(cur.data);
    ds_free(&cur);
    return r;
}
