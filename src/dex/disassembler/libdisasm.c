/* Copyright (c) 2019 Griefer@Work                                            *
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
#ifndef GUARD_DEX_FS_LIBDISASM_C
#define GUARD_DEX_FS_LIBDISASM_C 1
#define DEE_SOURCE 1
#define _KOS_SOURCE 1

#include <deemon/api.h>
#include <deemon/arg.h>
#include <deemon/code.h>
#include <deemon/dex.h>
#include <deemon/error.h>
#include <deemon/file.h>
#include <deemon/int.h>
#include <deemon/none.h>
#include <deemon/object.h>
#include <deemon/objmethod.h>
#include <deemon/string.h>

/**/
#include "libdisasm.h"
/**/

#include <string.h>

#ifndef __USE_KOS
#define strend(x) ((x) + strlen(x))
#endif

DECL_BEGIN

struct disasm_flag {
	char         name[11]; /* The name of this flag. */
	bool         invert;   /* Invert the logical meaning of this flag. */
	unsigned int flag;     /* The actual flag. */
};

PRIVATE struct disasm_flag const disasm_flags[] = {
	{ "ddi",        0, PCODE_FDDI },
	{ "coflags",    1, PCODE_FNOCOFLAGS },
	{ "except",     1, PCODE_FNOEXCEPT },
	{ "jmparrow",   1, PCODE_FNOJUMPARROW },
	{ "badcomment", 1, PCODE_FNOBADCOMMENT },
	{ "argcomment", 1, PCODE_FNOARGCOMMENT },
	{ "altcomment", 0, PCODE_FALTCOMMENT },
	{ "depth",      1, PCODE_FNODEPTH },
	{ "skipdelop",  1, PCODE_FNOSKIPDELOP },
	{ "address",    1, PCODE_FNOADDRESS },
	{ "bytes",      1, PCODE_FNOBYTES },
	{ "inner",      1, PCODE_FNOINNER },
	{ "label",      1, PCODE_FNOLABELS },
};

PRIVATE int DCALL
parse_asm_flags(char *__restrict str,
                unsigned int *__restrict presult) {
	unsigned int i;
	for (;;) {
		char *flag_start = str;
		char *flag_end   = strchr(str, ',');
		if (!flag_end)
			flag_end = strend(str);
		if (flag_start < flag_end) {
			bool remove_flag = false;
			size_t optlen;
			if (*flag_start == 'n') {
				++flag_start;
				if (flag_start[0] == 'o' &&
				    flag_start[1] == '-')
					flag_start += 2;
				remove_flag = true;
			}
			optlen = (size_t)(flag_end - flag_start);
			if likely(optlen < COMPILER_STRLEN(disasm_flags[0].name)) {
				for (i = 0; i < COMPILER_LENOF(disasm_flags); ++i) {
					if (disasm_flags[i].name[optlen])
						continue;
					if (memcmp(disasm_flags[i].name, flag_start, optlen * sizeof(char)) != 0)
						continue;
					/* Found it! (update the resulting set of flags) */
					if (remove_flag ^ disasm_flags[i].invert)
						*presult &= ~disasm_flags[i].flag;
					else {
						*presult |= disasm_flags[i].flag;
					}
					goto next_opt;
				}
			}
			DeeError_Throwf(&DeeError_ValueError,
			                "Unknown disassembler option %$q",
			                (size_t)(flag_end - flag_start), flag_start);
			return -1;
		}
next_opt:
		if (!*flag_end)
			break;
		str = flag_end + 1;
	}
	return 0;
}



PRIVATE DREF DeeObject *DCALL
libdisasm_public_printcode_f(size_t argc,
                             DeeObject **__restrict argv,
                             DeeObject *kw) {
	DeeObject *fp = NULL;
	DeeCodeObject *code;
	dssize_t error;
	DeeObject *flags_ob = NULL;
	unsigned int flags  = PCODE_FNORMAL /*|PCODE_FDDI*/;
	PRIVATE DEFINE_KWLIST(kwlist, { K(code), K(out), K(flags), KEND });
	if (DeeArg_UnpackKw(argc, argv, kw, kwlist, "o|oo:printcode", &code, &fp, &flags_ob))
		goto err;
	if (DeeFunction_Check(code))
		code = DeeFunction_CODE(code);
	else {
		if (DeeObject_AssertTypeExact((DeeObject *)code, &DeeCode_Type))
			goto err;
	}
	if (fp && DeeString_Check(fp)) {
		flags_ob = fp;
		fp       = NULL;
	}
	if (flags_ob &&
	    (DeeObject_AssertTypeExact(flags_ob, &DeeString_Type) ||
	     parse_asm_flags(DeeString_STR(flags_ob), &flags)))
		goto err;
	if (!fp) {
		struct unicode_printer printer = UNICODE_PRINTER_INIT;
		error = libdisasm_printcode(&unicode_printer_print,
		                            &printer,
		                            code->co_code,
		                            code->co_code + code->co_codebytes,
		                            code, NULL, flags);
		DBG_ALIGNMENT_ENABLE();
		if unlikely(error < 0) {
			unicode_printer_fini(&printer);
			goto err;
		}
		return unicode_printer_pack(&printer);
	}
	error = libdisasm_printcode((dformatprinter)&DeeFile_WriteAll, fp,
	                            code->co_code,
	                            code->co_code + code->co_codebytes,
	                            code, NULL, flags);
	DBG_ALIGNMENT_ENABLE();
	if unlikely(error < 0)
		goto err;
	return DeeInt_NewSize((size_t)error);
err:
	return NULL;
}

PRIVATE DEFINE_KWCMETHOD(libdisasm_public_printcode, libdisasm_public_printcode_f);

PRIVATE struct dex_symbol symbols[] = {
	{ "printcode", (DeeObject *)&libdisasm_public_printcode, MODSYM_FNORMAL,
	  DOC("(co:?X2?Dfunction?Ert:Code,flags=!0)->?Dstring\n"
	      "(co:?X2?Dfunction?Ert:Code,flags=!P{})->?Dstring\n"
	      "(co:?X2?Dfunction?Ert:Code,out:?DFile,flags=!0)->?Dint\n"
	      "(co:?X2?Dfunction?Ert:Code,out:?DFile,flags=!P{})->?Dint\n"
	      "@throw ValueError The given @flags string contains an unknown flag\n"
	      "@param flags Either an integer set of internal flags, or a comma-separated options list (see below)\n"
	      "Print the given code object @co, either to file @out, or into a string that is then returned\n"
	      "When @flags is given as a string, it must be a comma-separated list of the following options:\n"
	      "%{table Name|Behavior\n"
	      "$\"ddi\"|Include DDI directives in output\n"
	      "$\"coflags\"|Include code flag directions in output\n"
	      "$\"except\"|Include exception handler labels and directives\n"
	      "$\"jmparrow\"|Draw arrows indicating the paths taken by jump instructions\n"
	      "$\"badcomment\"|Include comments about invalid instructions/operands\n"
	      "$\"argcomment\"|Include comments about the typing of operands\n"
	      "$\"altcomment\"|Include comments about alternate operand representations\n"
	      "$\"depth\"|Include stack-depth and transition information\n"
	      "$\"skipdelop\"|Skip $ASM_DELOP instruction\n"
	      "$\"address\"|Print instruction addresses\n"
	      "$\"bytes\"|Include raw text bytes in output\n"
	      "$\"inner\"|Also print the code of inner assembly recursively\n"
	      "$\"label\"|Assign label names for jumps\n"
	      "$\"\"|Empty options are simply ignored}\n"
	      //"$\"RAW\": Alias for $\"ddi,except,no-jmparrow,no-depth,no-address,no-bytes\". "
	      //          "Used to direct the disassembler to produce output as close to what "
	      //          "user-defined input assembly may have looked like\n"
	      "Any option may be prefixed with $\"n\" or $\"no-\" to disable that feature.\n"
	      "The initial options state is identical to $\"no-ddi,coflags,except,jmparrow,"
	      "badcomment,argcomment,no-altcomment,depth,skipdelop,address,bytes,inner,label\"") },
	/* TODO: API for enumerate code instructions, analyzing & grouping their effect,
	 *       as well as query available instructions by id, and by name. */
	{ NULL }
};

PUBLIC struct dex DEX = {
	/* .d_symbols = */ symbols
};

DECL_END

#endif /* !GUARD_DEX_FS_LIBDISASM_C */
