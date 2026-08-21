#include "common.h"
#include "vars.h"
#include "arrays.h"
#include "aliases.h"
#include "funcs.h"
#include "builtins.h"
#include "jobs.h"
#include "config.h"
#include "history.h"
#include "modules.h"
#include "lineedit.h"
#include "exec.h"
#include "parser.h"
#include "ast.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#define SPIRE_VERSION "1.0.0"

static void init_shell_process(bool interactive) {
    if (interactive) {
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        if (getpgrp() != getpid()) setpgid(0, 0);
        g_shell_pgid = getpid();
        tcsetpgrp(STDIN_FILENO, g_shell_pgid);
    }
    g_shell_is_interactive = interactive;
}

static void init_subsystems(void) {
    vars_init();
    exec_init();
    arrays_init();
    aliases_init();
    funcs_init();
    builtins_init();
    jobs_init();
    config_init();
    config_load_default();
    history_init(config_history_size(), config_history_path());
    lineedit_init();
    modules_load_enabled();

    var_set("SPIRE_VERSION", SPIRE_VERSION, true);
    if (!var_get("PATH")) var_set("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", true);
}

static void repl(void) {
    init_shell_process(true);
    while (!g_should_exit) {
        jobs_reap(true);
        char *line = lineedit_read(config_prompt());
        if (!line) { printf("\n"); break; }

        dstr_t acc; ds_init(&acc);
        ds_append(&acc, line);
        free(line);

        Node *root = NULL;
        char *err = NULL;
        ParseStatus st;
        for (;;) {
            st = parse_program(acc.data, &root, &err);
            if (st != PARSE_INCOMPLETE) break;
            char *cont = lineedit_read(NULL);
            if (!cont) break;
            ds_append_c(&acc, '\n');
            ds_append(&acc, cont);
            free(cont);
        }

        if (acc.len > 0) history_add(acc.data);

        if (st == PARSE_OK) {
            exec_program(root);
            node_free(root);
        } else if (st == PARSE_ERROR) {
            fprintf(stderr, "spire: %s\n", err);
            free(err);
        }
        ds_free(&acc);
    }
    lineedit_shutdown();
}

static void set_positional(int argc, char **argv, int start) {
    char buf[16];
    int n = argc - start;
    dstr_t joined; ds_init(&joined);
    for (int i = start; i < argc; i++) {
        snprintf(buf, sizeof(buf), "%d", i - start + 1);
        var_set(buf, argv[i], false);
        if (i > start) ds_append_c(&joined, ' ');
        ds_append(&joined, argv[i]);
    }
    snprintf(buf, sizeof(buf), "%d", n > 0 ? n : 0);
    var_set("#", buf, false);
    var_set("@", joined.data, false);
    var_set("*", joined.data, false);
    var_set("argv", joined.data, false);
    ds_free(&joined);
}

static void print_help(void) {
    printf(
        "spire " SPIRE_VERSION " - Syntax-Polyglot Interactive Runtime Environment\n\n"
        "usage:\n"
        "  spire                    start an interactive shell\n"
        "  spire -c 'commands'      run a command string and exit\n"
        "  spire script.sp [args]   run a script file\n"
        "  spire --version          print the version\n"
        "  spire --help             this message\n");
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    init_subsystems();

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_help();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("spire %s\n", SPIRE_VERSION);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        init_shell_process(false);
        set_positional(argc, argv, 3);
        var_set("0", "spire", false);
        int st = exec_source_string(argv[2]);
        return g_should_exit ? g_exit_code : st;
    }
    if (argc >= 2 && argv[1][0] != '-') {
        init_shell_process(false);
        var_set("0", argv[1], false);
        set_positional(argc, argv, 2);
        int st = exec_source_file(argv[1]);
        if (st < 0) { fprintf(stderr, "spire: %s: no such file\n", argv[1]); return 127; }
        return g_should_exit ? g_exit_code : st;
    }

    if (!isatty(STDIN_FILENO)) {
        init_shell_process(false);
        var_set("0", "spire", false);
        dstr_t buf; ds_init(&buf);
        char chunk[4096]; size_t r;
        while ((r = fread(chunk, 1, sizeof(chunk), stdin)) > 0) ds_append_n(&buf, chunk, r);
        int st = exec_source_string(buf.data);
        ds_free(&buf);
        return g_should_exit ? g_exit_code : st;
    }

    repl();
    return g_exit_code;
}
