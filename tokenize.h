/*
 * In-place string tokenizer, by Stefan Bruda.
 */

#ifndef __TOKENIZE_H
#define __TOKENIZE_H

#include <stddef.h>

/*
 * str_tokenize(s, t, n) receives a string s of length n and tokenize
 * the string at blanks putting the result in t and returning the
 * number of tokens thus obtained.  Continuous sequences of blanks are
 * treated as one blank.
 *
 * The function is very space efficient, but should be used with
 * extreme caution.  In particular, enough space should be allocated
 * for t; the function changes its argument s; if s is allocated
 * dynamically, it should NOT be deleted as long as t is in use (once
 * s is deleted t becomes full of dangling pointers).
 *
 * Example of use: The following piece of code
 *
 *    char str[100] = "I am  sick and    tired of string tokenizers.";
 *    char *toks[strlen(str)];
 *    int t = str_tokenize(str, toks, strlen(str));
 *    for (int i = 0; i < t; i++)
 *        cout << i << ": " << toks[i] << "\n";
 *
 * produces the following printout:
 *
 *    0: I
 *    1: am
 *    2: sick
 *    3: and
 *    4: tired
 *    5: of
 *    6: string
 *    7: tokenizers.
 *
 * This function comes with no waranty, use it at your own risk and
 * always keep in mind that the first argument is modified, and that
 * the function cannot be used on constant strings.
 */
size_t str_tokenize(char*, char**, const size_t);

#endif /* __TOKENIZE_H */
