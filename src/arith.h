#ifndef SPIRE_ARITH_H
#define SPIRE_ARITH_H

/* Evaluates a C-like integer arithmetic expression, resolving bare and
 * $-prefixed identifiers as shell variables (writing back on assignment
 * forms: =, +=, -=, *=, /=, %=, ++, --). Unparsable/unset names evaluate
 * to 0 rather than erroring, so a bad expression degrades gracefully
 * instead of crashing the shell. */
long arith_eval(const char *expr);

#endif
