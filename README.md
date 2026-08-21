warning this is ai generated 

# spire

**S**yntax-**P**olyglot **I**nteractive **R**untime **E**nvironment

A shell that merges the parts of fish, zsh, and bash people actually miss
when they switch between them: fish's live, colorized syntax highlighting
(but fully customizable) and readable `if ... end` blocks, bash/zsh's
`if/then/fi` and `name() { }` syntax so your muscle memory and existing
scripts still work, and a zsh-style module system for extending it.

This is a real, from-scratch shell — a hand-written lexer, recursive-descent
parser, tree-walking executor with proper `fork`/`exec`/`pipe`/job-control,
and a raw-terminal line editor. Not a wrapper around `readline` and not a
toy. It is, however, a big project built in one sitting, so see
[Limitations](#limitations) before you make it your only shell.

## Highlights

- **Both syntaxes, one parser.** Write `if`/`else if`/`end` or
  `if`/`then`/`elif`/`fi`. Write `for x in a b; do ... done` or
  `for x in a b; ... end`. Define functions with `function name ... end`
  or `name() { ... }`. Mix and match freely, even in the same file.
- **Math**: `$((expr))` and standalone `(( expr ))` support the full
  C-style operator set — `+ - * / % ** << >> < <= > >= == != & | ^ && ||
  ?: = += -= *= /= %= ++ --` — plus a `let` builtin.
- **Arrays**: bash-style `arr=(a b c)`, `arr+=(d)`, `arr[2]=x`,
  `${arr[i]}` (negative indices count from the end), `${arr[@]}`,
  `${arr[*]}`, `${#arr[@]}`, and fish-style `set arr a b c`.
- **case/esac and switch/case/end**, both wired to the same matcher
  (glob-style patterns via `fnmatch`, `|` to combine patterns in the
  bash form).
- **Subshells** `(cmds)` — run in an isolated forked child, so `cd` and
  variable changes inside don't leak out.
- **Here-docs**: `<<EOF`, `<<-EOF` (strips leading tabs), and
  `<<'EOF'`/`<<"EOF"` for a literal, non-expanding body.
- **Fish-style autosuggestions**: as you type, the rest of the most
  recent matching history entry appears inline in a dim color; accept it
  with → or End, or just keep typing to ignore it.
- **Live syntax highlighting**, computed and redrawn on every keystroke,
  with colors defined in a config file, not hardcoded — commands,
  invalid commands, strings, variables, operators, comments, keywords,
  numbers, filesystem paths, and array subscripts each get their own
  configurable color, plus a `set_color` builtin (à la fish) for
  building custom-colored prompts and output, and five built-in
  `colorscheme` presets (`dark`, `light`, `mono`, `solarized`, `nord`).
- **A real line editor**, built on raw termios, not GNU readline: arrow
  keys, Ctrl-A/E/U/K/W, history browsing, Tab completion for commands,
  files, and `$variables`, Ctrl-C to cancel a line without killing the
  shell, Ctrl-L to clear the screen.
- **Pipes, redirection, job control**: `|`, `&&`, `||`, `;`, `&`,
  `>`, `>>`, `<`, `2>`, `2>>`, `2>&1`, `&>`, plus `jobs`/`fg`/`bg`/`wait`.
  Foreground jobs get real terminal control via `tcsetpgrp`.
  Background jobs report `Done` the next time you get a prompt.
- **Variable expansion**: `$VAR`, `${VAR}`, `${VAR:-default}`,
  `${VAR:+alt}`, `${VAR:=default}`, `${#VAR}`, `$?`, `$$`, `$0`, `$1..$9`,
  `$#`, `$@`/`$*`, and fish's `$argv`.
- **Command substitution**, three ways: `$(cmd)`, `` `cmd` ``, and fish's
  bare `(cmd)`.
- **Aliases, functions, globbing, quoting** (`'literal'`, `"$expanded"`,
  `\escapes`), `~` expansion.
- **A module system**: drop a `.spire` file in `~/.config/spire/modules/`,
  list it in `modules = ...` in the config, and it's sourced at startup.
  Two starter modules ship in the box (`aliases`, `git`).
- **~28 builtins**: `cd pwd exit export unset alias unalias source echo
  printf type which jobs fg bg wait history set functions true false
  break continue return read test let set_color module colorscheme help`.

## Building & installing

```sh
./install.sh                 # build, install to /usr/local/bin (uses sudo if needed)
./install.sh --user          # install to ~/.local/bin, no sudo, no /etc/shells edit
./install.sh --prefix=/opt   # install into /opt/bin instead
```

This builds with `make` (plain C11, no dependencies beyond libc), installs
the binary, and — without ever clobbering files you already have — writes
a starter `~/.config/spire/spire.conf` and the two example modules into
`~/.config/spire/modules/`.

To make it your login shell:

```sh
chsh -s "$(command -v spire)"    # or the full path install.sh printed
```

To remove it:

```sh
./uninstall.sh            # removes the binary, keeps your config/history
./uninstall.sh --purge    # also removes ~/.config/spire and history
```

You can also just `make` and run `./spire` directly without installing
anything.

## Using it

```sh
spire                    # interactive shell
spire -c 'echo hi'       # run a one-liner and exit
spire script.spire a b   # run a script, with $1=a $2=b
```

### Control flow, either syntax

```sh
if test -f config.toml
    echo "found it"
else if test -d config.d
    echo "found a directory instead"
else
    echo "nothing"
end
```

```sh
if [ -f config.toml ]; then
    echo "found it"
elif [ -d config.d ]; then
    echo "found a directory instead"
else
    echo "nothing"
fi
```

### Functions, either syntax

```sh
function greet
    echo "hello $argv"
end
```

```sh
greet() {
    echo "hello $1"
}
```

### Loops

```sh
for f in *.txt
    echo $f
end

for f in *.txt; do echo $f; done

while test -f lockfile
    sleep 1
end
```

## Configuration

`~/.config/spire/spire.conf`, plain `key = value`:

```sh
prompt = "%F{cyan}%n%f@%F{cyan}%m%f %F{yellow}%~%f %F{brightblack}%g%f %F{green}spire❯%f "

color.command         = green
color.command_invalid = red
color.builtin         = blue
color.string          = yellow
color.variable        = cyan
color.operator        = magenta
color.comment         = brightblack
color.keyword         = blue
color.number          = magenta

history.size = 5000
modules = aliases, git
```

Prompt escapes: `%n` user, `%m` host, `%~` cwd (with `$HOME` shortened to
`~`), `%d` full cwd, `%g` git branch, `%#` `#`/`$` for root/non-root,
`%F{color}...%f` a colored span, `%%` a literal `%`.

Color values: `black red green yellow blue magenta cyan white default`,
their `bright*` variants, `bold-<name>`, or a raw 256-color number.

There's also a `colorscheme` builtin with a couple of built-in presets
(`colorscheme dark|light|mono`) if you'd rather not hand-pick every color.

## Writing a module

A module is just a `.spire` file, sourced into the shell at startup:

```sh
# ~/.config/spire/modules/mymodule.spire
function hello
    echo "hi from mymodule, argv = $argv"
end

alias reload='source ~/.config/spire/spire.conf'
```

Enable it with `modules = mymodule` in `spire.conf`, or load it on demand
with `module load mymodule`. `module list` shows what's enabled.

## Project layout

```
src/            all shell source (C11, no external dependencies)
  lexer.c/h       tokenizer (word/operator/comment scanning, quote-aware)
  parser.c/h      recursive-descent parser, both syntaxes -> one AST
  ast.c/h         AST node definitions
  expand.c/h      variable/command-substitution/glob/quote expansion
  exec.c/h        the tree-walking executor: fork/exec/pipe/redirect/jobs
  builtins.c/h    builtin commands
  vars.c/h        shell variable table (+ environment sync)
  funcs.c/h       user-defined function table
  aliases.c/h     alias table
  jobs.c/h        background job tracking
  history.c/h     command history (in-memory + append-only file)
  lineedit.c/h    raw-terminal line editor (the "not-readline" part)
  highlight.c/h   live syntax highlighter used by the line editor
  complete.c/h    tab completion (commands, files, variables)
  colors.c/h      ANSI color name/code resolution
  config.c/h      spire.conf loading
  prompt.c/h      prompt template rendering (%n %m %~ %F{}... etc)
  modules.c/h     module loader
  main.c          entry point, argument handling, REPL loop
config/
  spire.conf        starter config, copied to ~/.config/spire on install
  modules/          starter modules (aliases, git)
install.sh, uninstall.sh, Makefile
```

## Limitations

This was built to be genuinely useful, not to pass a POSIX conformance
suite. Known gaps, so nothing surprises you:

- No process substitution (`<(cmd)` / `>(cmd)`).
- No true nested arrays or associative arrays (maps) — only flat,
  integer-indexed arrays.
- UTF-8 in the line editor is passed through but cursor-column math
  assumes one byte = one column, so multi-byte characters can throw off
  cursor positioning during editing (the content itself is fine).
- Very long command lines that wrap past the terminal width can confuse
  the line editor's redraw; it isn't multi-row aware.
- Aliases are argument-prefix macros (`alias ll='ls -la'`), not full
  re-parsed command lines — an alias can't itself contain a pipe or `&&`.
- Word splitting/globbing is a close, pragmatic approximation of
  POSIX rules, not a byte-for-byte match to bash.
- `export`/`cd`/other state-changing builtins run with an I/O redirection
  attached (rare) execute in a forked child and so won't affect the
  parent shell's state — matches most real shells' subshell semantics
  closely but is a simplification worth knowing about.
- Job control works (`jobs`/`fg`/`bg`, real `tcsetpgrp` terminal-control
  handoff) but stopped-job/`SIGTSTP` handling is basic compared to a
  mature shell.
- Arithmetic (`$(( ))`) doesn't short-circuit `&&`/`||` (both sides
  always evaluate), which only matters if a side has a side effect like
  `i++`.
- Heredoc body capture assumes no pending two-token lookahead at the
  point it's parsed; this holds for ordinary use but is a known edge
  case in deeply unusual constructs.

Patches welcome in spirit, even though this is a from-scratch personal
project rather than a maintained package — read the source, it's meant
to be readable.
