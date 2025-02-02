/* Copyright (c) 2018-2023 Griefer@Work                                       *
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
 *    in a product, an acknowledgement (see the following) in the product     *
 *    documentation is required:                                              *
 *    Portions Copyright (c) 2018-2023 Griefer@Work                           *
 * 2. Altered source versions must be plainly marked as such, and must not be *
 *    misrepresented as being the original software.                          *
 * 3. This notice may not be removed or altered from any source distribution. *
 */
/*[[[deemon
#include <file>
#include <util>
#include <fs>

fs::chdir(fs::path::head(__FILE__));

local codes = list([none] * 256);
local codes_f0 = list([none] * 256);

for (local l: file.open("../../../include/deemon/asm.h")) {
	local name, code;
	try name, code = l.scanf(" # define ASM%[^ ] 0x%[0-9a-fA-F]")...;
	catch (...) continue;
	code = (int)("0x" + code);
	if (code <= 256) {
		if (codes[code] is none && name !in ["_INC", "_DEC", "_PREFIXMIN", "_PREFIXMAX"])
			codes[code] = name;
	} else if (code >= 0xf000 && code <= 0xf0ff) {
		if (codes_f0[code - 0xf000] is none && name !in ["_INCPOST", "_DECPOST"])
			codes_f0[code - 0xf000] = name;
	}
}
print "static void *const basic_targets[256] = {";
for (local i, name: util::enumerate(codes)) {
	if (name is none || name.startswith("RESERVED")) {
		print ("\t/" "* 0x%.2I8x *" "/" % i), "&&unknown_instruction,";
	} else {
		print ("\t/" "* 0x%.2I8x *" "/" % i), "&&target_ASM" + name + ",";
	}
}
print "};";
print "static void *const f0_targets[256] = {";
for (local i, name: util::enumerate(codes_f0)) {
	if (name is none || name.startswith("RESERVED")) {
		print ("\t/" "* 0x%.2I8x *" "/" % i), "&&unknown_instruction,";
	} else {
		print ("\t/" "* 0x%.2I8x *" "/" % i), "&&target_ASM" + name + ",";
	}
}
print "};";

]]]*/
static void *const basic_targets[256] = {
	/* 0x00 */ &&target_ASM_RET_NONE,
	/* 0x01 */ &&target_ASM_RET,
	/* 0x02 */ &&target_ASM_YIELDALL,
	/* 0x03 */ &&target_ASM_THROW,
	/* 0x04 */ &&target_ASM_RETHROW,
	/* 0x05 */ &&target_ASM_SETRET,
	/* 0x06 */ &&target_ASM_ENDCATCH,
	/* 0x07 */ &&target_ASM_ENDFINALLY,
	/* 0x08 */ &&target_ASM_CALL_KW,
	/* 0x09 */ &&target_ASM_CALL_TUPLE_KW,
	/* 0x0a */ &&unknown_instruction,
	/* 0x0b */ &&target_ASM_PUSH_BND_ARG,
	/* 0x0c */ &&target_ASM_PUSH_BND_EXTERN,
	/* 0x0d */ &&unknown_instruction,
	/* 0x0e */ &&target_ASM_PUSH_BND_GLOBAL,
	/* 0x0f */ &&target_ASM_PUSH_BND_LOCAL,
	/* 0x10 */ &&target_ASM_JF,
	/* 0x11 */ &&target_ASM_JF16,
	/* 0x12 */ &&target_ASM_JT,
	/* 0x13 */ &&target_ASM_JT16,
	/* 0x14 */ &&target_ASM_JMP,
	/* 0x15 */ &&target_ASM_JMP16,
	/* 0x16 */ &&target_ASM_FOREACH,
	/* 0x17 */ &&target_ASM_FOREACH16,
	/* 0x18 */ &&target_ASM_JMP_POP,
	/* 0x19 */ &&target_ASM_OPERATOR,
	/* 0x1a */ &&target_ASM_OPERATOR_TUPLE,
	/* 0x1b */ &&target_ASM_CALL,
	/* 0x1c */ &&target_ASM_CALL_TUPLE,
	/* 0x1d */ &&unknown_instruction,
	/* 0x1e */ &&target_ASM_DEL_GLOBAL,
	/* 0x1f */ &&target_ASM_DEL_LOCAL,
	/* 0x20 */ &&target_ASM_SWAP,
	/* 0x21 */ &&target_ASM_LROT,
	/* 0x22 */ &&target_ASM_RROT,
	/* 0x23 */ &&target_ASM_DUP,
	/* 0x24 */ &&target_ASM_DUP_N,
	/* 0x25 */ &&target_ASM_POP,
	/* 0x26 */ &&target_ASM_POP_N,
	/* 0x27 */ &&target_ASM_ADJSTACK,
	/* 0x28 */ &&target_ASM_SUPER,
	/* 0x29 */ &&target_ASM_SUPER_THIS_R,
	/* 0x2a */ &&unknown_instruction,
	/* 0x2b */ &&target_ASM_POP_STATIC,
	/* 0x2c */ &&target_ASM_POP_EXTERN,
	/* 0x2d */ &&unknown_instruction,
	/* 0x2e */ &&target_ASM_POP_GLOBAL,
	/* 0x2f */ &&target_ASM_POP_LOCAL,
	/* 0x30 */ &&unknown_instruction,
	/* 0x31 */ &&unknown_instruction,
	/* 0x32 */ &&unknown_instruction,
	/* 0x33 */ &&target_ASM_PUSH_NONE,
	/* 0x34 */ &&target_ASM_PUSH_VARARGS,
	/* 0x35 */ &&target_ASM_PUSH_VARKWDS,
	/* 0x36 */ &&target_ASM_PUSH_MODULE,
	/* 0x37 */ &&target_ASM_PUSH_ARG,
	/* 0x38 */ &&target_ASM_PUSH_CONST,
	/* 0x39 */ &&target_ASM_PUSH_REF,
	/* 0x3a */ &&unknown_instruction,
	/* 0x3b */ &&target_ASM_PUSH_STATIC,
	/* 0x3c */ &&target_ASM_PUSH_EXTERN,
	/* 0x3d */ &&unknown_instruction,
	/* 0x3e */ &&target_ASM_PUSH_GLOBAL,
	/* 0x3f */ &&target_ASM_PUSH_LOCAL,
	/* 0x40 */ &&target_ASM_CAST_TUPLE,
	/* 0x41 */ &&target_ASM_CAST_LIST,
	/* 0x42 */ &&target_ASM_PACK_TUPLE,
	/* 0x43 */ &&target_ASM_PACK_LIST,
	/* 0x44 */ &&unknown_instruction,
	/* 0x45 */ &&unknown_instruction,
	/* 0x46 */ &&target_ASM_UNPACK,
	/* 0x47 */ &&target_ASM_CONCAT,
	/* 0x48 */ &&target_ASM_EXTEND,
	/* 0x49 */ &&target_ASM_TYPEOF,
	/* 0x4a */ &&target_ASM_CLASSOF,
	/* 0x4b */ &&target_ASM_SUPEROF,
	/* 0x4c */ &&target_ASM_INSTANCEOF,
	/* 0x4d */ &&target_ASM_IMPLEMENTS,
	/* 0x4e */ &&target_ASM_STR,
	/* 0x4f */ &&target_ASM_REPR,
	/* 0x50 */ &&target_ASM_BOOL,
	/* 0x51 */ &&target_ASM_NOT,
	/* 0x52 */ &&target_ASM_ASSIGN,
	/* 0x53 */ &&target_ASM_MOVE_ASSIGN,
	/* 0x54 */ &&target_ASM_COPY,
	/* 0x55 */ &&target_ASM_DEEPCOPY,
	/* 0x56 */ &&target_ASM_GETATTR,
	/* 0x57 */ &&target_ASM_DELATTR,
	/* 0x58 */ &&target_ASM_SETATTR,
	/* 0x59 */ &&target_ASM_BOUNDATTR,
	/* 0x5a */ &&target_ASM_GETATTR_C,
	/* 0x5b */ &&target_ASM_DELATTR_C,
	/* 0x5c */ &&target_ASM_SETATTR_C,
	/* 0x5d */ &&target_ASM_GETATTR_THIS_C,
	/* 0x5e */ &&target_ASM_DELATTR_THIS_C,
	/* 0x5f */ &&target_ASM_SETATTR_THIS_C,
	/* 0x60 */ &&target_ASM_CMP_EQ,
	/* 0x61 */ &&target_ASM_CMP_NE,
	/* 0x62 */ &&target_ASM_CMP_GE,
	/* 0x63 */ &&target_ASM_CMP_LO,
	/* 0x64 */ &&target_ASM_CMP_LE,
	/* 0x65 */ &&target_ASM_CMP_GR,
	/* 0x66 */ &&target_ASM_CLASS_C,
	/* 0x67 */ &&target_ASM_CLASS_GC,
	/* 0x68 */ &&target_ASM_CLASS_EC,
	/* 0x69 */ &&target_ASM_DEFCMEMBER,
	/* 0x6a */ &&target_ASM_GETCMEMBER_R,
	/* 0x6b */ &&target_ASM_CALLCMEMBER_THIS_R,
	/* 0x6c */ &&unknown_instruction,
	/* 0x6d */ &&unknown_instruction,
	/* 0x6e */ &&target_ASM_FUNCTION_C,
	/* 0x6f */ &&target_ASM_FUNCTION_C_16,
	/* 0x70 */ &&target_ASM_CAST_INT,
	/* 0x71 */ &&target_ASM_INV,
	/* 0x72 */ &&target_ASM_POS,
	/* 0x73 */ &&target_ASM_NEG,
	/* 0x74 */ &&target_ASM_ADD,
	/* 0x75 */ &&target_ASM_SUB,
	/* 0x76 */ &&target_ASM_MUL,
	/* 0x77 */ &&target_ASM_DIV,
	/* 0x78 */ &&target_ASM_MOD,
	/* 0x79 */ &&target_ASM_SHL,
	/* 0x7a */ &&target_ASM_SHR,
	/* 0x7b */ &&target_ASM_AND,
	/* 0x7c */ &&target_ASM_OR,
	/* 0x7d */ &&target_ASM_XOR,
	/* 0x7e */ &&target_ASM_POW,
	/* 0x7f */ &&unknown_instruction,
	/* 0x80 */ &&unknown_instruction,
	/* 0x81 */ &&target_ASM_ADD_SIMM8,
	/* 0x82 */ &&target_ASM_ADD_IMM32,
	/* 0x83 */ &&target_ASM_SUB_SIMM8,
	/* 0x84 */ &&target_ASM_SUB_IMM32,
	/* 0x85 */ &&target_ASM_MUL_SIMM8,
	/* 0x86 */ &&target_ASM_DIV_SIMM8,
	/* 0x87 */ &&target_ASM_MOD_SIMM8,
	/* 0x88 */ &&target_ASM_SHL_IMM8,
	/* 0x89 */ &&target_ASM_SHR_IMM8,
	/* 0x8a */ &&target_ASM_AND_IMM32,
	/* 0x8b */ &&target_ASM_OR_IMM32,
	/* 0x8c */ &&target_ASM_XOR_IMM32,
	/* 0x8d */ &&target_ASM_ISNONE,
	/* 0x8e */ &&unknown_instruction,
	/* 0x8f */ &&target_ASM_DELOP,
	/* 0x90 */ &&target_ASM_NOP,
	/* 0x91 */ &&target_ASM_PRINT,
	/* 0x92 */ &&target_ASM_PRINT_SP,
	/* 0x93 */ &&target_ASM_PRINT_NL,
	/* 0x94 */ &&target_ASM_PRINTNL,
	/* 0x95 */ &&target_ASM_PRINTALL,
	/* 0x96 */ &&target_ASM_PRINTALL_SP,
	/* 0x97 */ &&target_ASM_PRINTALL_NL,
	/* 0x98 */ &&unknown_instruction,
	/* 0x99 */ &&target_ASM_FPRINT,
	/* 0x9a */ &&target_ASM_FPRINT_SP,
	/* 0x9b */ &&target_ASM_FPRINT_NL,
	/* 0x9c */ &&target_ASM_FPRINTNL,
	/* 0x9d */ &&target_ASM_FPRINTALL,
	/* 0x9e */ &&target_ASM_FPRINTALL_SP,
	/* 0x9f */ &&target_ASM_FPRINTALL_NL,
	/* 0xa0 */ &&unknown_instruction,
	/* 0xa1 */ &&target_ASM_PRINT_C,
	/* 0xa2 */ &&target_ASM_PRINT_C_SP,
	/* 0xa3 */ &&target_ASM_PRINT_C_NL,
	/* 0xa4 */ &&target_ASM_RANGE_0_I16,
	/* 0xa5 */ &&unknown_instruction,
	/* 0xa6 */ &&target_ASM_ENTER,
	/* 0xa7 */ &&target_ASM_LEAVE,
	/* 0xa8 */ &&unknown_instruction,
	/* 0xa9 */ &&target_ASM_FPRINT_C,
	/* 0xaa */ &&target_ASM_FPRINT_C_SP,
	/* 0xab */ &&target_ASM_FPRINT_C_NL,
	/* 0xac */ &&target_ASM_RANGE,
	/* 0xad */ &&target_ASM_RANGE_DEF,
	/* 0xae */ &&target_ASM_RANGE_STEP,
	/* 0xaf */ &&target_ASM_RANGE_STEP_DEF,
	/* 0xb0 */ &&target_ASM_CONTAINS,
	/* 0xb1 */ &&target_ASM_CONTAINS_C,
	/* 0xb2 */ &&target_ASM_GETITEM,
	/* 0xb3 */ &&target_ASM_GETITEM_I,
	/* 0xb4 */ &&target_ASM_GETITEM_C,
	/* 0xb5 */ &&target_ASM_GETSIZE,
	/* 0xb6 */ &&target_ASM_SETITEM,
	/* 0xb7 */ &&target_ASM_SETITEM_I,
	/* 0xb8 */ &&target_ASM_SETITEM_C,
	/* 0xb9 */ &&target_ASM_ITERSELF,
	/* 0xba */ &&target_ASM_DELITEM,
	/* 0xbb */ &&target_ASM_GETRANGE,
	/* 0xbc */ &&target_ASM_GETRANGE_PN,
	/* 0xbd */ &&target_ASM_GETRANGE_NP,
	/* 0xbe */ &&target_ASM_GETRANGE_PI,
	/* 0xbf */ &&target_ASM_GETRANGE_IP,
	/* 0xc0 */ &&target_ASM_GETRANGE_NI,
	/* 0xc1 */ &&target_ASM_GETRANGE_IN,
	/* 0xc2 */ &&target_ASM_GETRANGE_II,
	/* 0xc3 */ &&target_ASM_DELRANGE,
	/* 0xc4 */ &&target_ASM_SETRANGE,
	/* 0xc5 */ &&target_ASM_SETRANGE_PN,
	/* 0xc6 */ &&target_ASM_SETRANGE_NP,
	/* 0xc7 */ &&target_ASM_SETRANGE_PI,
	/* 0xc8 */ &&target_ASM_SETRANGE_IP,
	/* 0xc9 */ &&target_ASM_SETRANGE_NI,
	/* 0xca */ &&target_ASM_SETRANGE_IN,
	/* 0xcb */ &&target_ASM_SETRANGE_II,
	/* 0xcc */ &&target_ASM_BREAKPOINT,
	/* 0xcd */ &&target_ASM_UD,
	/* 0xce */ &&target_ASM_CALLATTR_C_KW,
	/* 0xcf */ &&target_ASM_CALLATTR_C_TUPLE_KW,
	/* 0xd0 */ &&target_ASM_CALLATTR,
	/* 0xd1 */ &&target_ASM_CALLATTR_TUPLE,
	/* 0xd2 */ &&target_ASM_CALLATTR_C,
	/* 0xd3 */ &&target_ASM_CALLATTR_C_TUPLE,
	/* 0xd4 */ &&target_ASM_CALLATTR_THIS_C,
	/* 0xd5 */ &&target_ASM_CALLATTR_THIS_C_TUPLE,
	/* 0xd6 */ &&target_ASM_CALLATTR_C_SEQ,
	/* 0xd7 */ &&target_ASM_CALLATTR_C_MAP,
	/* 0xd8 */ &&unknown_instruction,
	/* 0xd9 */ &&target_ASM_GETMEMBER_THIS_R,
	/* 0xda */ &&target_ASM_DELMEMBER_THIS_R,
	/* 0xdb */ &&target_ASM_SETMEMBER_THIS_R,
	/* 0xdc */ &&target_ASM_BOUNDMEMBER_THIS_R,
	/* 0xdd */ &&target_ASM_CALL_EXTERN,
	/* 0xde */ &&target_ASM_CALL_GLOBAL,
	/* 0xdf */ &&target_ASM_CALL_LOCAL,
	/* 0xe0 */ &&unknown_instruction,
	/* 0xe1 */ &&unknown_instruction,
	/* 0xe2 */ &&unknown_instruction,
	/* 0xe3 */ &&unknown_instruction,
	/* 0xe4 */ &&unknown_instruction,
	/* 0xe5 */ &&unknown_instruction,
	/* 0xe6 */ &&unknown_instruction,
	/* 0xe7 */ &&unknown_instruction,
	/* 0xe8 */ &&unknown_instruction,
	/* 0xe9 */ &&unknown_instruction,
	/* 0xea */ &&unknown_instruction,
	/* 0xeb */ &&unknown_instruction,
	/* 0xec */ &&unknown_instruction,
	/* 0xed */ &&unknown_instruction,
	/* 0xee */ &&unknown_instruction,
	/* 0xef */ &&unknown_instruction,
	/* 0xf0 */ &&target_ASM_EXTENDED1,
	/* 0xf1 */ &&target_ASM_RESERVED1,
	/* 0xf2 */ &&target_ASM_RESERVED2,
	/* 0xf3 */ &&target_ASM_RESERVED3,
	/* 0xf4 */ &&target_ASM_RESERVED4,
	/* 0xf5 */ &&target_ASM_RESERVED5,
	/* 0xf6 */ &&target_ASM_RESERVED6,
	/* 0xf7 */ &&target_ASM_RESERVED7,
	/* 0xf8 */ &&unknown_instruction,
	/* 0xf9 */ &&unknown_instruction,
	/* 0xfa */ &&target_ASM_STACK,
	/* 0xfb */ &&target_ASM_STATIC,
	/* 0xfc */ &&target_ASM_EXTERN,
	/* 0xfd */ &&unknown_instruction,
	/* 0xfe */ &&target_ASM_GLOBAL,
	/* 0xff */ &&target_ASM_LOCAL,
};
static void *const f0_targets[256] = {
	/* 0x00 */ &&unknown_instruction,
	/* 0x01 */ &&unknown_instruction,
	/* 0x02 */ &&unknown_instruction,
	/* 0x03 */ &&unknown_instruction,
	/* 0x04 */ &&unknown_instruction,
	/* 0x05 */ &&unknown_instruction,
	/* 0x06 */ &&target_ASM_ENDCATCH_N,
	/* 0x07 */ &&target_ASM_ENDFINALLY_N,
	/* 0x08 */ &&target_ASM16_CALL_KW,
	/* 0x09 */ &&target_ASM16_CALL_TUPLE_KW,
	/* 0x0a */ &&unknown_instruction,
	/* 0x0b */ &&target_ASM16_PUSH_BND_ARG,
	/* 0x0c */ &&target_ASM16_PUSH_BND_EXTERN,
	/* 0x0d */ &&unknown_instruction,
	/* 0x0e */ &&target_ASM16_PUSH_BND_GLOBAL,
	/* 0x0f */ &&target_ASM16_PUSH_BND_LOCAL,
	/* 0x10 */ &&unknown_instruction,
	/* 0x11 */ &&unknown_instruction,
	/* 0x12 */ &&unknown_instruction,
	/* 0x13 */ &&unknown_instruction,
	/* 0x14 */ &&target_ASM32_JMP,
	/* 0x15 */ &&unknown_instruction,
	/* 0x16 */ &&unknown_instruction,
	/* 0x17 */ &&unknown_instruction,
	/* 0x18 */ &&target_ASM_JMP_POP_POP,
	/* 0x19 */ &&target_ASM16_OPERATOR,
	/* 0x1a */ &&target_ASM16_OPERATOR_TUPLE,
	/* 0x1b */ &&target_ASM_CALL_SEQ,
	/* 0x1c */ &&target_ASM_CALL_MAP,
	/* 0x1d */ &&target_ASM_THISCALL_TUPLE,
	/* 0x1e */ &&target_ASM16_DEL_GLOBAL,
	/* 0x1f */ &&target_ASM16_DEL_LOCAL,
	/* 0x20 */ &&target_ASM_CALL_TUPLE_KWDS,
	/* 0x21 */ &&target_ASM16_LROT,
	/* 0x22 */ &&target_ASM16_RROT,
	/* 0x23 */ &&unknown_instruction,
	/* 0x24 */ &&target_ASM16_DUP_N,
	/* 0x25 */ &&unknown_instruction,
	/* 0x26 */ &&target_ASM16_POP_N,
	/* 0x27 */ &&target_ASM16_ADJSTACK,
	/* 0x28 */ &&unknown_instruction,
	/* 0x29 */ &&target_ASM16_SUPER_THIS_R,
	/* 0x2a */ &&unknown_instruction,
	/* 0x2b */ &&target_ASM16_POP_STATIC,
	/* 0x2c */ &&target_ASM16_POP_EXTERN,
	/* 0x2d */ &&unknown_instruction,
	/* 0x2e */ &&target_ASM16_POP_GLOBAL,
	/* 0x2f */ &&target_ASM16_POP_LOCAL,
	/* 0x30 */ &&unknown_instruction,
	/* 0x31 */ &&unknown_instruction,
	/* 0x32 */ &&target_ASM_PUSH_EXCEPT,
	/* 0x33 */ &&target_ASM_PUSH_THIS,
	/* 0x34 */ &&target_ASM_PUSH_THIS_MODULE,
	/* 0x35 */ &&target_ASM_PUSH_THIS_FUNCTION,
	/* 0x36 */ &&target_ASM16_PUSH_MODULE,
	/* 0x37 */ &&target_ASM16_PUSH_ARG,
	/* 0x38 */ &&target_ASM16_PUSH_CONST,
	/* 0x39 */ &&target_ASM16_PUSH_REF,
	/* 0x3a */ &&unknown_instruction,
	/* 0x3b */ &&target_ASM16_PUSH_STATIC,
	/* 0x3c */ &&target_ASM16_PUSH_EXTERN,
	/* 0x3d */ &&unknown_instruction,
	/* 0x3e */ &&target_ASM16_PUSH_GLOBAL,
	/* 0x3f */ &&target_ASM16_PUSH_LOCAL,
	/* 0x40 */ &&target_ASM_CAST_HASHSET,
	/* 0x41 */ &&target_ASM_CAST_DICT,
	/* 0x42 */ &&target_ASM16_PACK_TUPLE,
	/* 0x43 */ &&target_ASM16_PACK_LIST,
	/* 0x44 */ &&unknown_instruction,
	/* 0x45 */ &&unknown_instruction,
	/* 0x46 */ &&target_ASM16_UNPACK,
	/* 0x47 */ &&unknown_instruction,
	/* 0x48 */ &&unknown_instruction,
	/* 0x49 */ &&unknown_instruction,
	/* 0x4a */ &&unknown_instruction,
	/* 0x4b */ &&unknown_instruction,
	/* 0x4c */ &&unknown_instruction,
	/* 0x4d */ &&unknown_instruction,
	/* 0x4e */ &&unknown_instruction,
	/* 0x4f */ &&unknown_instruction,
	/* 0x50 */ &&target_ASM_PUSH_TRUE,
	/* 0x51 */ &&target_ASM_PUSH_FALSE,
	/* 0x52 */ &&target_ASM_PACK_HASHSET,
	/* 0x53 */ &&target_ASM_PACK_DICT,
	/* 0x54 */ &&unknown_instruction,
	/* 0x55 */ &&unknown_instruction,
	/* 0x56 */ &&unknown_instruction,
	/* 0x57 */ &&unknown_instruction,
	/* 0x58 */ &&unknown_instruction,
	/* 0x59 */ &&target_ASM_BOUNDITEM,
	/* 0x5a */ &&target_ASM16_GETATTR_C,
	/* 0x5b */ &&target_ASM16_DELATTR_C,
	/* 0x5c */ &&target_ASM16_SETATTR_C,
	/* 0x5d */ &&target_ASM16_GETATTR_THIS_C,
	/* 0x5e */ &&target_ASM16_DELATTR_THIS_C,
	/* 0x5f */ &&target_ASM16_SETATTR_THIS_C,
	/* 0x60 */ &&target_ASM_CMP_SO,
	/* 0x61 */ &&target_ASM_CMP_DO,
	/* 0x62 */ &&target_ASM16_PACK_HASHSET,
	/* 0x63 */ &&target_ASM16_PACK_DICT,
	/* 0x64 */ &&target_ASM16_GETCMEMBER,
	/* 0x65 */ &&target_ASM_CLASS,
	/* 0x66 */ &&target_ASM16_CLASS_C,
	/* 0x67 */ &&target_ASM16_CLASS_GC,
	/* 0x68 */ &&target_ASM16_CLASS_EC,
	/* 0x69 */ &&target_ASM16_DEFCMEMBER,
	/* 0x6a */ &&target_ASM16_GETCMEMBER_R,
	/* 0x6b */ &&target_ASM16_CALLCMEMBER_THIS_R,
	/* 0x6c */ &&unknown_instruction,
	/* 0x6d */ &&unknown_instruction,
	/* 0x6e */ &&target_ASM16_FUNCTION_C,
	/* 0x6f */ &&target_ASM16_FUNCTION_C_16,
	/* 0x70 */ &&target_ASM_SUPERGETATTR_THIS_RC,
	/* 0x71 */ &&target_ASM16_SUPERGETATTR_THIS_RC,
	/* 0x72 */ &&target_ASM_SUPERCALLATTR_THIS_RC,
	/* 0x73 */ &&target_ASM16_SUPERCALLATTR_THIS_RC,
	/* 0x74 */ &&unknown_instruction,
	/* 0x75 */ &&unknown_instruction,
	/* 0x76 */ &&unknown_instruction,
	/* 0x77 */ &&unknown_instruction,
	/* 0x78 */ &&unknown_instruction,
	/* 0x79 */ &&unknown_instruction,
	/* 0x7a */ &&unknown_instruction,
	/* 0x7b */ &&unknown_instruction,
	/* 0x7c */ &&unknown_instruction,
	/* 0x7d */ &&unknown_instruction,
	/* 0x7e */ &&unknown_instruction,
	/* 0x7f */ &&unknown_instruction,
	/* 0x80 */ &&unknown_instruction,
	/* 0x81 */ &&unknown_instruction,
	/* 0x82 */ &&unknown_instruction,
	/* 0x83 */ &&unknown_instruction,
	/* 0x84 */ &&unknown_instruction,
	/* 0x85 */ &&unknown_instruction,
	/* 0x86 */ &&unknown_instruction,
	/* 0x87 */ &&unknown_instruction,
	/* 0x88 */ &&unknown_instruction,
	/* 0x89 */ &&unknown_instruction,
	/* 0x8a */ &&unknown_instruction,
	/* 0x8b */ &&unknown_instruction,
	/* 0x8c */ &&unknown_instruction,
	/* 0x8d */ &&unknown_instruction,
	/* 0x8e */ &&unknown_instruction,
	/* 0x8f */ &&target_ASM16_DELOP,
	/* 0x90 */ &&target_ASM16_NOP,
	/* 0x91 */ &&unknown_instruction,
	/* 0x92 */ &&unknown_instruction,
	/* 0x93 */ &&unknown_instruction,
	/* 0x94 */ &&target_ASM_REDUCE_MIN,
	/* 0x95 */ &&target_ASM_REDUCE_MAX,
	/* 0x96 */ &&target_ASM_REDUCE_SUM,
	/* 0x97 */ &&target_ASM_REDUCE_ANY,
	/* 0x98 */ &&target_ASM_REDUCE_ALL,
	/* 0x99 */ &&unknown_instruction,
	/* 0x9a */ &&unknown_instruction,
	/* 0x9b */ &&unknown_instruction,
	/* 0x9c */ &&unknown_instruction,
	/* 0x9d */ &&unknown_instruction,
	/* 0x9e */ &&unknown_instruction,
	/* 0x9f */ &&unknown_instruction,
	/* 0xa0 */ &&unknown_instruction,
	/* 0xa1 */ &&target_ASM16_PRINT_C,
	/* 0xa2 */ &&target_ASM16_PRINT_C_SP,
	/* 0xa3 */ &&target_ASM16_PRINT_C_NL,
	/* 0xa4 */ &&target_ASM_RANGE_0_I32,
	/* 0xa5 */ &&unknown_instruction,
	/* 0xa6 */ &&target_ASM_VARARGS_UNPACK,
	/* 0xa7 */ &&target_ASM_PUSH_VARKWDS_NE,
	/* 0xa8 */ &&unknown_instruction,
	/* 0xa9 */ &&target_ASM16_FPRINT_C,
	/* 0xaa */ &&target_ASM16_FPRINT_C_SP,
	/* 0xab */ &&target_ASM16_FPRINT_C_NL,
	/* 0xac */ &&target_ASM_VARARGS_CMP_EQ_SZ,
	/* 0xad */ &&target_ASM_VARARGS_CMP_GR_SZ,
	/* 0xae */ &&unknown_instruction,
	/* 0xaf */ &&unknown_instruction,
	/* 0xb0 */ &&unknown_instruction,
	/* 0xb1 */ &&target_ASM16_CONTAINS_C,
	/* 0xb2 */ &&target_ASM_VARARGS_GETITEM,
	/* 0xb3 */ &&target_ASM_VARARGS_GETITEM_I,
	/* 0xb4 */ &&target_ASM16_GETITEM_C,
	/* 0xb5 */ &&target_ASM_VARARGS_GETSIZE,
	/* 0xb6 */ &&unknown_instruction,
	/* 0xb7 */ &&unknown_instruction,
	/* 0xb8 */ &&target_ASM16_SETITEM_C,
	/* 0xb9 */ &&target_ASM_ITERNEXT,
	/* 0xba */ &&unknown_instruction,
	/* 0xbb */ &&unknown_instruction,
	/* 0xbc */ &&unknown_instruction,
	/* 0xbd */ &&unknown_instruction,
	/* 0xbe */ &&target_ASM_GETMEMBER,
	/* 0xbf */ &&target_ASM16_GETMEMBER,
	/* 0xc0 */ &&target_ASM_DELMEMBER,
	/* 0xc1 */ &&target_ASM16_DELMEMBER,
	/* 0xc2 */ &&target_ASM_SETMEMBER,
	/* 0xc3 */ &&target_ASM16_SETMEMBER,
	/* 0xc4 */ &&target_ASM_BOUNDMEMBER,
	/* 0xc5 */ &&target_ASM16_BOUNDMEMBER,
	/* 0xc6 */ &&target_ASM_GETMEMBER_THIS,
	/* 0xc7 */ &&target_ASM16_GETMEMBER_THIS,
	/* 0xc8 */ &&target_ASM_DELMEMBER_THIS,
	/* 0xc9 */ &&target_ASM16_DELMEMBER_THIS,
	/* 0xca */ &&target_ASM_SETMEMBER_THIS,
	/* 0xcb */ &&target_ASM16_SETMEMBER_THIS,
	/* 0xcc */ &&target_ASM_BOUNDMEMBER_THIS,
	/* 0xcd */ &&target_ASM16_BOUNDMEMBER_THIS,
	/* 0xce */ &&target_ASM16_CALLATTR_C_KW,
	/* 0xcf */ &&target_ASM16_CALLATTR_C_TUPLE_KW,
	/* 0xd0 */ &&target_ASM_CALLATTR_KWDS,
	/* 0xd1 */ &&target_ASM_CALLATTR_TUPLE_KWDS,
	/* 0xd2 */ &&target_ASM16_CALLATTR_C,
	/* 0xd3 */ &&target_ASM16_CALLATTR_C_TUPLE,
	/* 0xd4 */ &&target_ASM16_CALLATTR_THIS_C,
	/* 0xd5 */ &&target_ASM16_CALLATTR_THIS_C_TUPLE,
	/* 0xd6 */ &&target_ASM16_CALLATTR_C_SEQ,
	/* 0xd7 */ &&target_ASM16_CALLATTR_C_MAP,
	/* 0xd8 */ &&unknown_instruction,
	/* 0xd9 */ &&target_ASM16_GETMEMBER_THIS_R,
	/* 0xda */ &&target_ASM16_DELMEMBER_THIS_R,
	/* 0xdb */ &&target_ASM16_SETMEMBER_THIS_R,
	/* 0xdc */ &&target_ASM16_BOUNDMEMBER_THIS_R,
	/* 0xdd */ &&target_ASM16_CALL_EXTERN,
	/* 0xde */ &&target_ASM16_CALL_GLOBAL,
	/* 0xdf */ &&target_ASM16_CALL_LOCAL,
	/* 0xe0 */ &&unknown_instruction,
	/* 0xe1 */ &&unknown_instruction,
	/* 0xe2 */ &&unknown_instruction,
	/* 0xe3 */ &&unknown_instruction,
	/* 0xe4 */ &&unknown_instruction,
	/* 0xe5 */ &&unknown_instruction,
	/* 0xe6 */ &&unknown_instruction,
	/* 0xe7 */ &&unknown_instruction,
	/* 0xe8 */ &&unknown_instruction,
	/* 0xe9 */ &&unknown_instruction,
	/* 0xea */ &&unknown_instruction,
	/* 0xeb */ &&unknown_instruction,
	/* 0xec */ &&unknown_instruction,
	/* 0xed */ &&unknown_instruction,
	/* 0xee */ &&unknown_instruction,
	/* 0xef */ &&unknown_instruction,
	/* 0xf0 */ &&unknown_instruction,
	/* 0xf1 */ &&unknown_instruction,
	/* 0xf2 */ &&unknown_instruction,
	/* 0xf3 */ &&unknown_instruction,
	/* 0xf4 */ &&unknown_instruction,
	/* 0xf5 */ &&unknown_instruction,
	/* 0xf6 */ &&unknown_instruction,
	/* 0xf7 */ &&unknown_instruction,
	/* 0xf8 */ &&unknown_instruction,
	/* 0xf9 */ &&unknown_instruction,
	/* 0xfa */ &&target_ASM16_STACK,
	/* 0xfb */ &&target_ASM16_STATIC,
	/* 0xfc */ &&target_ASM16_EXTERN,
	/* 0xfd */ &&unknown_instruction,
	/* 0xfe */ &&target_ASM16_GLOBAL,
	/* 0xff */ &&target_ASM16_LOCAL,
};
/*[[[end]]]*/
