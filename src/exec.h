#ifndef SPIRE_EXEC_H
#define SPIRE_EXEC_H

#include "common.h"
#include "ast.h"
#include <sys/types.h>

void exec_init(void);

/* Execute a parsed top-level program (an N_SEQ root). Returns the exit
 * status of the last command run ($?). */
int exec_program(Node *root);

/* Execute a block Node (used for function bodies / control-flow bodies). */
int exec_block(Node *block);

/* Parse+execute `src` with stdout captured into a malloc'd, newline-trimmed
 * buffer (*out_buf, caller frees). Used for $(...) / `...` / (...) command
 * substitution. Returns the exit status. */
int exec_capture_stdout(const char *src, char **out_buf);

/* Parse+execute the contents of a file in the current shell (source/.), or
 * as a module. Returns exit status; -1 if the file could not be opened. */
int exec_source_file(const char *path);

/* Parse+execute a raw string as if typed at the prompt (used by -c). */
int exec_source_string(const char *src);

/* Invoke a user-defined function body with the given argv (argv[0] is the
 * function name). Sets up $1.., $#, $@, $argv for the duration of the call. */
int exec_call_function(Node *body, int argc, char **argv);

/* current recursion depth of function calls / command substitutions, used
 * to guard against runaway recursion */
extern int g_exec_depth;

/* set by `exit`/loop control builtins via longjmp-free flags checked between
 * statements */
extern volatile bool g_should_exit;
extern int g_exit_code;

/* whether the shell owns the terminal (real tty + interactive REPL) */
extern bool g_shell_is_interactive;
extern pid_t g_shell_pgid;

/* called by the `break` / `continue` / `return` builtins */
void exec_signal_break(void);
void exec_signal_continue(void);
void exec_signal_return(int code);

#endif
