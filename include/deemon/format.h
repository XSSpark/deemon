/* Copyright (c) 2018 Griefer@Work                                            *
 *                                                                            *
 * This software is provided 'as-is', without any express or implied          *
 * warranty. In no event will the authors be held liable for any damages      *
 * arising from the use of this software.                                     *
 *                                                                            *
 * Permission is granted to anyone to use this software for any purpose,      *
 * including commercial applications, and to alter it and redistribute it     *
 * freely, subject to the following restrictions:                             *
 *                                                                            *
 * 1. The origin of this software must not be misrepresented; you must not    *
 *    claim that you wrote the original software. If you use this software    *
 *    in a product, an acknowledgement in the product documentation would be  *
 *    appreciated but is not required.                                        *
 * 2. Altered source versions must be plainly marked as such, and must not be *
 *    misrepresented as being the original software.                          *
 * 3. This notice may not be removed or altered from any source distribution. *
 */
#ifndef GUARD_DEEMON_FORMAT_H
#define GUARD_DEEMON_FORMAT_H 1

#include "api.h"
#include "object.h"
#include <stddef.h>
#include <stdarg.h>

DECL_BEGIN

/* General-purpose printing of formatted data to the given `printer'.
 * These functions implement c's `printf()' standard (except for the
 * wide-string part) with the following extensions:
 *   - `%I...': Length prefix: sizeof(size_t)
 *   - `%I8...', `%I16...', `%I32...', `%I64...': Absolute length prefix.
 *   - `%q': Print quoted (escaped) string from a UTF-8 source. - Prefix with `%#q' to omit surrounding quotes.
 *   - `%I8q': Print quoted (escaped) string from a 1-byte-per-character source. - Prefix with `%#q' to omit surrounding quotes.
 *   - `%I16q': Print quoted (escaped) string from a 2-byte-per-character source. - Prefix with `%#q' to omit surrounding quotes.
 *   - `%I32q': Print quoted (escaped) string from a 4-byte-per-character source. - Prefix with `%#q' to omit surrounding quotes.
 *   - `%s': Print a UTF-8 string.
 *   - `%I8s': Print a 1-byte-per-character string.
 *   - `%I16s': Print a 2-byte-per-character string.
 *   - `%I32s': Print a 4-byte-per-character string.
 *   - `%I8c': Print a character from the range U+0000 - U+00FF (Same as the regular `%c')
 *   - `%I16c': Print a character from the range U+0000 - U+FFFF
 *   - `%I32c': Print a character from the range U+00000000 - U+FFFFFFFF
 *   - `%$s', `%$q': Take the absolute length of the string as a `size_t' (may also be combined with I* prefixes)
 *   - `%C': Print a character from the range U+0000 - U+00FF in its c-escaped form (using '\'' instead of '\"')
 *   - `%I8C': Print a character from the range U+0000 - U+00FF in its c-escaped form (using '\'' instead of '\"') (Same as the regular `%C')
 *   - `%I16C': Print a character from the range U+0000 - U+FFFF in its c-escaped form (using '\'' instead of '\"')
 *   - `%I32C': Print a character from the range U+00000000 - U+FFFFFFFF in its c-escaped form (using '\'' instead of '\"')
 *   - `%#C': Print a character from the range U+0000 - U+00FF in its c-escaped form (without surrounding '\''-characters)
 *   - `%#I8C': Print a character from the range U+0000 - U+00FF in its c-escaped form (without surrounding '\''-characters) (Same as the regular `%C')
 *   - `%#I16C': Print a character from the range U+0000 - U+FFFF in its c-escaped form (without surrounding '\''-characters)
 *   - `%#I32C': Print a character from the range U+00000000 - U+FFFFFFFF in its c-escaped form (without surrounding '\''-characters)
 *   - `%.?s', `%.?q': Take the max length of the string or precision as a `size_t' (may also be combined with I* prefixes)
 *   - `%:?s', `%:?q': Same as `%$s' / `%$q'
 *   - `%:DIGIT: Similar to `%.DIGIT', but set the absolute length of the string, rather than its maximum length
 *               This is to `%.DIGIT' what `%$s' is to `%.?s'
 *   - `%?...': Take the width of the output text as a `size_t'
 *   - `%b': Integer option: Same as `o', but output as a binary.
 *   - `%k': Taking a `DeeObject *', print `__str__' or `none' when `NULL' was passed.
 *   - `%r': Taking a `DeeObject *', print `__repr__' or `none' when `NULL' was passed.
 *   - `%K': Same as `%k', but decref() the object afterwards.
 *   - `%R': Same as `%r', but decref() the object afterwards.
 * HINT: To guaranty fulfillment of `K' and `R' operands,
 *       the format string is _always_ fully processed.
 * @return: * :  The sum of all return values from calls to `*printer'
 * @return: < 0: The first negative return value of a call to `*printer' */
DFUNDEF dssize_t Dee_FormatPrintf(dformatprinter printer, void *arg, char const *__restrict format, ...);
DFUNDEF dssize_t DCALL Dee_VFormatPrintf(dformatprinter printer, void *arg, char const *__restrict format, va_list args);


#define Dee_FormatPRINT(printer,arg,str) \
        (*printer)(arg,str,COMPILER_STRLEN(str))

/* Quote (backslash-escape) the given text, printing the resulting text to `printer'.
 * NOTE: This function always generates pure ASCII, and is therefor safe to be used
 *       when targeting an `ascii_printer' */
DFUNDEF dssize_t DCALL
Dee_FormatQuote(/*ascii*/dformatprinter printer, void *arg,
                /*utf-8*/char const *__restrict text, size_t textlen,
                unsigned int flags);
#define FORMAT_QUOTE_FNORMAL   0x0000
#define FORMAT_QUOTE_FFORCEHEX 0x0002 /* Force hex encoding of all control characters without special strings (`"\n"', etc.). */
#define FORMAT_QUOTE_FFORCEOCT 0x0004 /* Force octal encoding of all non-ascii characters. */
#define FORMAT_QUOTE_FNOCTRL   0x0008 /* Disable special encoding strings such as `"\r"', `"\n"' or `"\e"' */
#define FORMAT_QUOTE_FNOASCII  0x0010 /* Disable regular ascii-characters and print everything using special encodings. */
#define FORMAT_QUOTE_FUPPERHEX 0x0020 /* Use uppercase characters for hex (e.g.: `"\xAB"'). */
#define FORMAT_QUOTE_FPRINTRAW 0x0080 /* Don't surround the quoted text with "..."; */


DFUNDEF dssize_t DCALL
Dee_FormatQuote8(/*ascii*/dformatprinter printer, void *arg,
                 uint8_t const *__restrict text, size_t textlen,
                 unsigned int flags);

/* Format-quote 16-bit, and 32-bit text. */
DFUNDEF dssize_t DCALL
Dee_FormatQuote16(/*ascii*/dformatprinter printer, void *arg,
                  uint16_t const *__restrict text, size_t textlen,
                  unsigned int flags);
DFUNDEF dssize_t DCALL
Dee_FormatQuote32(/*ascii*/dformatprinter printer, void *arg,
                  uint32_t const *__restrict text, size_t textlen,
                  unsigned int flags);

/* Repeat the given `ch' a total of `count' times. */
DFUNDEF dssize_t DCALL
Dee_FormatRepeat(/*ascii*/dformatprinter printer, void *arg,
                 char ch, size_t count);

/* Repeat `str...+=length' such that a total of `total_characters'
 * characters (not bytes, but characters) are printed. */
DFUNDEF dssize_t DCALL
Dee_FormatRepeatUtf8(dformatprinter printer, void *arg,
                     /*utf-8*/char const *__restrict str,
                     size_t length, size_t total_characters);

/* Print a unicode character `ch', encoded as UTF-8 into `printer' */
DFUNDEF dssize_t DCALL
Dee_FormatPutc(dformatprinter printer, void *arg, uint32_t ch);

/* Convert an 8, 16, or 32-bit character array to UTF-8 and write it to `printer'
 * NOTE: 8-bit here refers to the unicode range U+0000 - U+00FF */
DFUNDEF dssize_t DCALL Dee_FormatPrint8(dformatprinter printer, void *arg, uint8_t const *__restrict text, size_t textlen);
DFUNDEF dssize_t DCALL Dee_FormatPrint16(dformatprinter printer, void *arg, uint16_t const *__restrict text, size_t textlen);
DFUNDEF dssize_t DCALL Dee_FormatPrint32(dformatprinter printer, void *arg, uint32_t const *__restrict text, size_t textlen);


struct va_list_struct { va_list vl_ap; };

/* Pack a new value, given a special format string `string'.
 * Format language syntax:
 *     __main__ ::= object;
 *     object ::= ('n' | '-')         // `none'
 *              | ref_object          // `object' <-- `va_arg(DeeObject *)'
 *              | ref_int             // `int'    <-- `va_arg(...)'
 *              | ref_float           // `float'  <-- `va_arg(...)'
 *              | ref_bool            // `bool'   <-- `va_arg(...)'
 *              | ref_str             // `string' <-- `va_arg(...)'
 *              | '[' [objects] ']'   // `list'
 *              | '(' [objects] ')'   // `tuple'
 *              | '{' [objects] '}'   // `set'
 *              | '<' [object] '>'    // `cell' (When `object')
 *     ;
 *     objects ::= (object | ',')...  // `,' is simply ignored, but can be used to prevent ambiguity
 *     
 *     ref_object ::= 'o' | 'O'; // `DeeObject *' (Uppercase `O' inherits a reference from `va_arg' and causes `Dee_Packf' to propagate an error when `NULL')
 *     ref_int    ::= ref_intlen ('d' | 'u' | 'i' | 'x'); // `u' and `x' create unsigned integers
 *     ref_intlen ::= 'I' ['8' | '16' | '32' | '64'] // Fixed-length / sizeof(size_t)
 *                  | 'hh' // char
 *                  | 'h'  // short
 *                  | ''   // int (Default when nothing else was given)
 *                  | 'l'  // long
 *                  | 'll' // long long
 *     ;
 *     ref_float  ::= 'f'  // float / double
 *                  | 'LD' // long double
 *     ;
 *     ref_bool   ::= ref_intlen 'b';
 *     ref_str    ::= [ '.<int>'  // str = va_arg(char *); DeeString_NewSized(str,MIN(strlen(str),<int>));
 *                    | '.*'      // max = va_arg(unsigned int); str = va_arg(char *); DeeString_NewSized(str,MIN(strlen(str),max));
 *                    | '.?'      // max = va_arg(size_t); str = va_arg(char *); DeeString_NewSized(str,MIN(strlen(str),max));
 *                    | '$'       // len = va_arg(size_t); str = va_arg(char *); DeeString_NewSized(str,len);
 *                    ] 's';      // DeeString_New(va_arg(char *));
 *
 * Example:
 * >> Dee_Packf("(d<d>Os)",10,20,DeeInt_NewInt(42),"foobar");
 * >> // (10,<20>,42,"foobar")
 */
DFUNDEF DREF DeeObject *Dee_Packf(char const *__restrict format, ...);
DFUNDEF DREF DeeObject *DCALL Dee_VPackf(char const *__restrict format, va_list args);
DFUNDEF DREF DeeObject *DCALL Dee_VPPackf(char const **__restrict pformat, struct va_list_struct *__restrict pargs);
DFUNDEF void DCALL Dee_VPPackf_Cleanup(char const *__restrict format, va_list args);

/* Unpack values from an object.
 * Format language syntax:
 *     __main__   ::= object;
 *     object     ::= ('n' | '-')          // Ignore / skip this object. (Do not advance `va_arg')
 *                  | ref_object           // `va_arg(DeeObject **)'
 *                  | ref_int              //
 *                  | ref_float            //
 *                  | ref_bool             //
 *                  | ref_str              // `char **'
 *                  | '(' [objects...] ')' // Enumerate elements of a sequence
 *     ;
 *     objects    ::= [(object   // Parse some object from the sequence.
 *                    | ','      // `,' is simply ignored, but can be used to prevent ambiguity.
 *                    | '|'      // Any following objects are optional (va_arg() is still invoked, but non-present elements are skipped)
 *                      )...];
 *     ref_object ::= 'o'; // `va_arg(DeeObject **)'
 *     ref_int    ::= [ref_intlen] ('d' | 'u' | 'i' | 'x'); // `u' and `x' read unsigned integers
 *     ref_str    ::= ['$']      // `va_arg(size_t *)' (Store the length of the string)
 *                  | 'l' 's'    // `va_arg(wchar_t **)'
 *                  | 'U16' 's'  // `va_arg(uint16_t **)'
 *                  | 'U32' 's'  // `va_arg(uint32_t **)'
 *                  | 's'        // `va_arg(char **)'
 *                  ;
 *     ref_intlen ::= 'I' ['8' | '16' | '32' | '64'] // Fixed-length / sizeof(size_t)
 *                  | 'hh' // `va_arg(char *)'
 *                  | 'h'  // `va_arg(short *)'
 *                  | ''   // `va_arg(int *)' (Default when nothing else was given)
 *                  | 'l'  // `va_arg(long *)'
 *                  | 'll' // `va_arg(long long *)'
 *     ;
 *     ref_float  ::= 'f'  // `va_arg(float *)'
 *                  | 'D'  // `va_arg(double *)'
 *                  | 'LD' // `va_arg(long double *)'
 *     ;
 *     ref_bool   ::= 'b'; // `va_arg(bool)'
 */
DFUNDEF int Dee_Unpackf(DeeObject *__restrict self, char const *__restrict format, ...);
DFUNDEF int DCALL Dee_VUnpackf(DeeObject *__restrict self, char const *__restrict format, va_list args);
DFUNDEF int DCALL Dee_VPUnpackf(DeeObject *__restrict self, char const **__restrict pformat, struct va_list_struct *__restrict pargs);


/* Both of these functions return a pointer to the target address where
 * printing has/would have stopped (excluding a terminating \0-character).
 * In the event that the buffer provided to `Dee_snprintf' is insufficient,
 * at most `bufsize' characters will have been written, and the exact required
 * size can be determined by `((return - buffer)+1)*sizeof(char)'.
 * In the event of an error (only possible when `format' contains
 * something like `%k' or `%r'), `NULL' will be returned. */
DFUNDEF char *Dee_sprintf(char *__restrict buffer, char const *__restrict format, ...);
DFUNDEF char *Dee_snprintf(char *__restrict buffer, size_t bufsize, char const *__restrict format, ...);
DFUNDEF char *DCALL Dee_vsprintf(char *__restrict buffer, char const *__restrict format, va_list args);
DFUNDEF char *DCALL Dee_vsnprintf(char *__restrict buffer, size_t bufsize, char const *__restrict format, va_list args);

#ifndef __INTELLISENSE__
#ifndef __NO_builtin_expect
#define Dee_Unpackf(self,...)                   __builtin_expect(Dee_Unpackf(self,__VA_ARGS__),0)
#define Dee_VUnpackf(self,format,args)          __builtin_expect(Dee_VUnpackf(self,format,args),0)
#define Dee_VPUnpackf(self,pformat,pargs)       __builtin_expect(Dee_VPUnpackf(self,pformat,pargs),0)
#endif /* !__NO_builtin_expect */
#endif


DECL_END

#endif /* !GUARD_DEEMON_FORMAT_H */
