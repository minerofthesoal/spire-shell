#ifndef SPIRE_MODULES_H
#define SPIRE_MODULES_H

/* Sources every module named in the config's `modules = a, b, c` list from
 * ~/.config/spire/modules/<name>.spire. Missing files are reported but do
 * not stop the shell from starting. */
void modules_load_enabled(void);

#endif
