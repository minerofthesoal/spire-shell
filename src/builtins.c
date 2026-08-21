#include "builtins.h"
#include "exec.h"
#include "vars.h"
#include "arrays.h"
#include "arith.h"
#include "aliases.h"
#include "funcs.h"
#include "jobs.h"
#include "config.h"
#include "history.h"
#include "colors.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

static int b_cd(int argc, char **argv) {
    const char *target;
    char resolved[4096];
    if (argc < 2) {
        target = var_get("HOME");
        if (!target) { fprintf(stderr, "spire: cd: HOME not set\n"); return 1; }
    } else if (strcmp(argv[1], "-") == 0) {
        target = var_get("OLDPWD");
        if (!target) { fprintf(stderr, "spire: cd: OLDPWD not set\n"); return 1; }
        printf("%s\n", target);
    } else {
        target = argv[1];
    }
    char *oldpwd = getcwd(resolved, sizeof(resolved));
    if (chdir(target) != 0) { fprintf(stderr, "spire: cd: %s: %s\n", target, strerror(errno)); return 1; }
    if (oldpwd) var_set("OLDPWD", oldpwd, true);
    char newpwd[4096];
    if (getcwd(newpwd, sizeof(newpwd))) var_set("PWD", newpwd, true);
    return 0;
}

static int b_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    char buf[4096];
    if (getcwd(buf, sizeof(buf))) { printf("%s\n", buf); return 0; }
    fprintf(stderr, "spire: pwd: %s\n", strerror(errno));
    return 1;
}

static int b_exit(int argc, char **argv) {
    g_should_exit = true;
    g_exit_code = argc > 1 ? atoi(argv[1]) : g_exit_code;
    return g_exit_code;
}

static int b_export(int argc, char **argv) {
    if (argc == 1) {
        strvec_t names; sv_init(&names);
        vars_dump(&names);
        for (size_t i = 0; i < names.count; i++)
            if (var_is_exported(names.items[i]))
                printf("export %s=%s\n", names.items[i], var_get(names.items[i]));
        sv_free(&names);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            var_set(argv[i], eq + 1, true);
            *eq = '=';
        } else {
            var_export(argv[i]);
        }
    }
    return 0;
}

static int b_unset(int argc, char **argv) {
    for (int i = 1; i < argc; i++) var_unset(argv[i]);
    return 0;
}

static int b_alias(int argc, char **argv) {
    if (argc == 1) {
        strvec_t names, values; sv_init(&names); sv_init(&values);
        alias_list(&names, &values);
        for (size_t i = 0; i < names.count; i++) printf("alias %s='%s'\n", names.items[i], values.items[i]);
        sv_free(&names); sv_free(&values);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) {
            const char *v = alias_get(argv[i]);
            if (v) printf("alias %s='%s'\n", argv[i], v);
            else fprintf(stderr, "spire: alias: %s: not found\n", argv[i]);
            continue;
        }
        *eq = '\0';
        char *val = eq + 1;
        size_t vl = strlen(val);
        if (vl >= 2 && ((val[0] == '\'' && val[vl-1] == '\'') || (val[0] == '"' && val[vl-1] == '"'))) {
            val[vl-1] = '\0'; val++;
        }
        alias_set(argv[i], val);
    }
    return 0;
}

static int b_unalias(int argc, char **argv) {
    for (int i = 1; i < argc; i++) alias_unset(argv[i]);
    return 0;
}

static int b_source(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "spire: source: filename required\n"); return 1; }
    int st = exec_source_file(argv[1]);
    if (st < 0) { fprintf(stderr, "spire: source: %s: %s\n", argv[1], strerror(errno)); return 1; }
    return st;
}

static int b_echo(int argc, char **argv) {
    bool nl = true, interpret = false;
    int i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1]) {
        if (strcmp(argv[i], "-n") == 0) { nl = false; i++; }
        else if (strcmp(argv[i], "-e") == 0) { interpret = true; i++; }
        else if (strcmp(argv[i], "-ne") == 0 || strcmp(argv[i], "-en") == 0) { nl = false; interpret = true; i++; }
        else break;
    }
    for (; i < argc; i++) {
        if (i > 1 && argv[i-1]) putchar(' ');
        if (!interpret) { fputs(argv[i], stdout); continue; }
        for (char *p = argv[i]; *p; p++) {
            if (*p == '\\' && p[1]) {
                p++;
                switch (*p) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    case '\\': putchar('\\'); break;
                    default: putchar('\\'); putchar(*p); break;
                }
            } else putchar(*p);
        }
    }
    if (nl) putchar('\n');
    return 0;
}

static int b_printf(int argc, char **argv) {
    if (argc < 2) return 0;
    const char *fmt = argv[1];
    int argi = 2;
    const char *p = fmt;
    while (*p) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) { case 'n': putchar('\n'); break; case 't': putchar('\t'); break; default: putchar(*p); }
            p++;
            continue;
        }
        if (*p == '%' && p[1]) {
            p++;
            if (*p == '%') { putchar('%'); p++; continue; }
            if (*p == 's') { fputs(argi < argc ? argv[argi++] : "", stdout); p++; continue; }
            if (*p == 'd' || *p == 'i') { printf("%ld", argi < argc ? strtol(argv[argi++], NULL, 10) : 0); p++; continue; }
            putchar('%'); continue;
        }
        putchar(*p); p++;
    }
    return 0;
}

static int b_type(int argc, char **argv) {
    if (argc < 2) return 0;
    int status = 0;
    for (int i = 1; i < argc; i++) {
        const char *name = argv[i];
        if (alias_get(name)) { printf("%s is an alias for '%s'\n", name, alias_get(name)); continue; }
        if (func_get(name)) { printf("%s is a function\n", name); continue; }
        if (builtin_exists(name)) { printf("%s is a shell builtin\n", name); continue; }
        const char *path = var_get("PATH");
        bool found = false;
        if (path) {
            char *pathcopy = xstrdup(path);
            for (char *dir = strtok(pathcopy, ":"); dir; dir = strtok(NULL, ":")) {
                char full[4096];
                snprintf(full, sizeof(full), "%s/%s", dir, name);
                if (access(full, X_OK) == 0) { printf("%s is %s\n", name, full); found = true; break; }
            }
            free(pathcopy);
        }
        if (!found) { fprintf(stderr, "spire: type: %s: not found\n", name); status = 1; }
    }
    return status;
}

static int b_jobs(int argc, char **argv) { (void)argc; (void)argv; jobs_print(); return 0; }

static int parse_job_id(const char *s) {
    if (s[0] == '%') s++;
    return atoi(s);
}

static int b_fg(int argc, char **argv) {
    Job *j = argc > 1 ? job_find(parse_job_id(argv[1])) : NULL;
    if (!j) { fprintf(stderr, "spire: fg: no such job\n"); return 1; }
    printf("%s\n", j->cmdline);
    kill(-j->pgid, SIGCONT);
    j->state = JOB_RUNNING;
    if (g_shell_is_interactive) tcsetpgrp(STDIN_FILENO, j->pgid);
    int status;
    waitpid(j->pgid, &status, WUNTRACED);
    if (g_shell_is_interactive) tcsetpgrp(STDIN_FILENO, g_shell_pgid);
    if (WIFSTOPPED(status)) { j->state = JOB_STOPPED; return 128 + SIGTSTP; }
    job_remove(j);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static int b_bg(int argc, char **argv) {
    Job *j = argc > 1 ? job_find(parse_job_id(argv[1])) : NULL;
    if (!j) { fprintf(stderr, "spire: bg: no such job\n"); return 1; }
    kill(-j->pgid, SIGCONT);
    j->state = JOB_RUNNING;
    printf("[%d] %s &\n", j->id, j->cmdline);
    return 0;
}

static int b_wait(int argc, char **argv) {
    (void)argv;
    if (argc > 1) { Job *j = job_find(parse_job_id(argv[1])); if (j) waitpid(j->pgid, NULL, 0); return 0; }
    int status;
    while (waitpid(-1, &status, 0) > 0) {}
    return 0;
}

static int b_history(int argc, char **argv) { (void)argc; (void)argv; history_print(); return 0; }

static int b_set(int argc, char **argv) {
    if (argc == 1) {
        strvec_t names; sv_init(&names);
        vars_dump(&names);
        for (size_t i = 0; i < names.count; i++) printf("%s %s\n", names.items[i], var_get(names.items[i]));
        sv_free(&names);
        strvec_t anames; sv_init(&anames);
        arrays_dump(&anames);
        for (size_t i = 0; i < anames.count; i++) {
            strvec_t elems; sv_init(&elems);
            array_get_all(anames.items[i], &elems);
            printf("%s = (", anames.items[i]);
            for (size_t k = 0; k < elems.count; k++) printf("%s%s", k ? " " : "", elems.items[k]);
            printf(")\n");
            sv_free(&elems);
        }
        sv_free(&anames);
        return 0;
    }
    int i = 1;
    bool exported = false, erase = false;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "-x") == 0) exported = true;
        else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--erase") == 0) erase = true;
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "-g") == 0) { /* scope hints: accepted, not distinguished */ }
        else break;
        i++;
    }
    if (i >= argc) return 0;
    const char *name = argv[i++];
    if (erase) { var_unset(name); array_unset(name); return 0; }
    if (argc - i >= 2) {
        /* fish semantics: `set name a b c` makes a list, not a joined string */
        strvec_t elems; sv_init(&elems);
        for (; i < argc; i++) sv_push_dup(&elems, argv[i]);
        array_set(name, &elems);
        var_unset(name);
        sv_free(&elems);
        return 0;
    }
    dstr_t val; ds_init(&val);
    for (; i < argc; i++) { if (val.len) ds_append_c(&val, ' '); ds_append(&val, argv[i]); }
    var_set(name, val.data, exported || var_is_exported(name));
    array_unset(name);
    ds_free(&val);
    return 0;
}

static int b_functions(int argc, char **argv) {
    (void)argv;
    strvec_t names; sv_init(&names);
    func_list_names(&names);
    if (argc == 1) { for (size_t i = 0; i < names.count; i++) printf("%s\n", names.items[i]); }
    sv_free(&names);
    return 0;
}

static int b_true(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int b_false(int argc, char **argv) { (void)argc; (void)argv; return 1; }

static int b_let(int argc, char **argv) {
    if (argc < 2) return 1;
    long last = 0;
    for (int i = 1; i < argc; i++) last = arith_eval(argv[i]);
    return last != 0 ? 0 : 1;
}

/* fish's set_color: prints the raw escape code for a name (or "normal" to
 * reset), with no trailing newline, so it can be embedded via command
 * substitution in prompts/echo: `echo (set_color red)hi(set_color normal)`.
 * -b NAME sets the background instead. */
static int b_set_color(int argc, char **argv) {
    if (argc < 2) return 0;
    bool bg = false;
    int i = 1;
    if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) { bg = true; i++; }
    const char *name = argv[i];
    if (strcmp(name, "normal") == 0 || strcmp(name, "reset") == 0) { fputs(ANSI_RESET, stdout); return 0; }
    char *code = ansi_code_for(name);
    if (bg && code[0]) {
        /* crude fg->bg shift: 3x/9x -> 4x/10x */
        dstr_t d; ds_init(&d);
        for (char *p = code; *p; p++) {
            if (*p == '3' && p[1] != ';' ) { ds_append_c(&d, '4'); continue; }
            if (*p == '9' && isdigit((unsigned char)p[1])) { ds_append(&d, "10"); continue; }
            ds_append_c(&d, *p);
        }
        fputs(d.data, stdout);
        ds_free(&d);
    } else {
        fputs(code, stdout);
    }
    free(code);
    return 0;
}

static int b_break(int argc, char **argv) { (void)argc; (void)argv; exec_signal_break(); return 0; }
static int b_continue(int argc, char **argv) { (void)argc; (void)argv; exec_signal_continue(); return 0; }
static int b_return(int argc, char **argv) { exec_signal_return(argc > 1 ? atoi(argv[1]) : 0); return g_exit_code; }

static int b_read(int argc, char **argv) {
    const char *prompt = NULL;
    int i = 1;
    if (i < argc && strcmp(argv[i], "-p") == 0 && i + 1 < argc) { prompt = argv[i+1]; i += 2; }
    if (prompt) { fputs(prompt, stdout); fflush(stdout); }
    char line[4096];
    if (!fgets(line, sizeof(line), stdin)) return 1;
    size_t n = strlen(line);
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
    const char *varname = i < argc ? argv[i] : "REPLY";
    var_set(varname, line, false);
    return 0;
}

/* minimal `test` / `[` supporting common unary and binary operators */
static int b_test(int argc, char **argv) {
    int n = argc - 1;
    char **a = argv + 1;
    if (n > 0 && strcmp(a[n-1], "]") == 0) n--;
    if (n == 0) return 1;
    if (n == 1) return a[0][0] == '\0' ? 1 : 0;
    if (n == 2 && strcmp(a[0], "!") == 0) return !b_test(3, (char *[]){"test", a[1], NULL});
    if (n == 2) {
        struct stat st;
        const char *op = a[0], *arg = a[1];
        if (strcmp(op, "-f") == 0) return (stat(arg, &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
        if (strcmp(op, "-d") == 0) return (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
        if (strcmp(op, "-e") == 0) return (stat(arg, &st) == 0) ? 0 : 1;
        if (strcmp(op, "-r") == 0) return access(arg, R_OK) == 0 ? 0 : 1;
        if (strcmp(op, "-w") == 0) return access(arg, W_OK) == 0 ? 0 : 1;
        if (strcmp(op, "-x") == 0) return access(arg, X_OK) == 0 ? 0 : 1;
        if (strcmp(op, "-z") == 0) return arg[0] == '\0' ? 0 : 1;
        if (strcmp(op, "-n") == 0) return arg[0] != '\0' ? 0 : 1;
    }
    if (n == 3) {
        const char *l = a[0], *op = a[1], *r = a[2];
        if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) return strcmp(l, r) == 0 ? 0 : 1;
        if (strcmp(op, "!=") == 0) return strcmp(l, r) != 0 ? 0 : 1;
        long li = strtol(l, NULL, 10), ri = strtol(r, NULL, 10);
        if (strcmp(op, "-eq") == 0) return li == ri ? 0 : 1;
        if (strcmp(op, "-ne") == 0) return li != ri ? 0 : 1;
        if (strcmp(op, "-lt") == 0) return li < ri ? 0 : 1;
        if (strcmp(op, "-le") == 0) return li <= ri ? 0 : 1;
        if (strcmp(op, "-gt") == 0) return li > ri ? 0 : 1;
        if (strcmp(op, "-ge") == 0) return li >= ri ? 0 : 1;
    }
    return 1;
}

static int b_module(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: module list | module load NAME\n"); return 1; }
    if (strcmp(argv[1], "list") == 0) {
        strvec_t *mods = config_modules();
        for (size_t i = 0; i < mods->count; i++) printf("%s\n", mods->items[i]);
        return 0;
    }
    if (strcmp(argv[1], "load") == 0 && argc > 2) {
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s.spire", config_module_dir(), argv[2]);
        int st = exec_source_file(path);
        if (st < 0) { fprintf(stderr, "spire: module: %s: not found\n", argv[2]); return 1; }
        return st;
    }
    fprintf(stderr, "usage: module list | module load NAME\n");
    return 1;
}

static int b_colorscheme(int argc, char **argv) {
    if (argc < 2) {
        printf("current color scheme (see ~/.config/spire/spire.conf to customize):\n");
        const char *classes[] = {"command","command_invalid","builtin","string","variable","operator","comment","keyword","number","path","array", NULL};
        for (int i = 0; classes[i]; i++) printf("  %-16s %s\n", classes[i], config_color(classes[i]));
        return 0;
    }
    if (strcmp(argv[1], "dark") == 0) {
        config_set("color.command", "green"); config_set("color.string", "yellow");
        config_set("color.variable", "cyan"); config_set("color.operator", "magenta");
        config_set("color.keyword", "blue"); config_set("color.comment", "brightblack");
        config_set("color.path", "white"); config_set("color.array", "brightmagenta");
    } else if (strcmp(argv[1], "light") == 0) {
        config_set("color.command", "blue"); config_set("color.string", "green");
        config_set("color.variable", "magenta"); config_set("color.operator", "red");
        config_set("color.keyword", "cyan"); config_set("color.comment", "black");
        config_set("color.path", "black"); config_set("color.array", "magenta");
    } else if (strcmp(argv[1], "mono") == 0) {
        config_set("color.command", "white"); config_set("color.string", "white");
        config_set("color.variable", "white"); config_set("color.operator", "white");
        config_set("color.keyword", "white"); config_set("color.comment", "brightblack");
        config_set("color.path", "white"); config_set("color.array", "white");
    } else if (strcmp(argv[1], "solarized") == 0) {
        config_set("color.command", "green"); config_set("color.string", "83");
        config_set("color.variable", "37"); config_set("color.operator", "yellow");
        config_set("color.keyword", "blue"); config_set("color.comment", "brightblack");
        config_set("color.path", "37"); config_set("color.array", "magenta");
        config_set("color.number", "166");
    } else if (strcmp(argv[1], "nord") == 0) {
        config_set("color.command", "109"); config_set("color.string", "150");
        config_set("color.variable", "111"); config_set("color.operator", "145");
        config_set("color.keyword", "111"); config_set("color.comment", "brightblack");
        config_set("color.path", "145"); config_set("color.array", "175");
        config_set("color.number", "175");
    } else {
        fprintf(stderr, "spire: colorscheme: unknown scheme '%s' (try dark, light, mono, solarized, nord)\n", argv[1]);
        return 1;
    }
    return 0;
}

static int b_help(int argc, char **argv) {
    (void)argc; (void)argv;
    printf(
        "spire - a shell that blends fish, zsh and bash\n\n"
        "control flow (fish or bash syntax both work):\n"
        "  if / else if / else / end        if / elif / else / fi\n"
        "  while ... end                    while ... do ... done\n"
        "  for X in ...; ... end            for X in ...; do ... done\n"
        "  function name ... end            name() { ... }\n\n"
        "builtins: cd pwd exit export unset alias unalias source echo printf\n"
        "          type jobs fg bg wait history set functions true false\n"
        "          break continue return read test module colorscheme help\n\n"
        "run `module list` to see enabled modules, and edit\n"
        "~/.config/spire/spire.conf to customize colors and the prompt.\n");
    return 0;
}

/* ---------------- table ---------------- */

typedef struct { const char *name; BuiltinFn fn; } BuiltinEntry;

static BuiltinEntry g_table[] = {
    {"cd", b_cd}, {"pwd", b_pwd}, {"exit", b_exit}, {"export", b_export},
    {"unset", b_unset}, {"alias", b_alias}, {"unalias", b_unalias},
    {"source", b_source}, {".", b_source}, {"echo", b_echo}, {"printf", b_printf},
    {"type", b_type}, {"which", b_type}, {"jobs", b_jobs}, {"fg", b_fg}, {"bg", b_bg},
    {"wait", b_wait}, {"history", b_history}, {"set", b_set}, {"functions", b_functions},
    {"true", b_true}, {"false", b_false}, {"break", b_break}, {"continue", b_continue},
    {"return", b_return}, {"read", b_read}, {"test", b_test}, {"[", b_test},
    {"let", b_let}, {"set_color", b_set_color},
    {"module", b_module}, {"colorscheme", b_colorscheme}, {"help", b_help},
    {NULL, NULL}
};

void builtins_init(void) { /* table is static; nothing to do */ }

BuiltinFn builtin_lookup(const char *name) {
    for (int i = 0; g_table[i].name; i++) if (strcmp(g_table[i].name, name) == 0) return g_table[i].fn;
    return NULL;
}

bool builtin_exists(const char *name) { return builtin_lookup(name) != NULL; }

void builtins_list_names(strvec_t *out) {
    for (int i = 0; g_table[i].name; i++) sv_push_dup(out, g_table[i].name);
}
