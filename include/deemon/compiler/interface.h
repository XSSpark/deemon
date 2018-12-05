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
#ifndef GUARD_DEEMON_COMPILER_INTERFACE_H
#define GUARD_DEEMON_COMPILER_INTERFACE_H 1

#include "../api.h"
#include "../object.h"
#include "compiler.h"
#include "ast.h"
#include "tpp.h"

DECL_BEGIN

#ifdef CONFIG_BUILDING_DEEMON
struct ast;
struct scope_object;
struct base_scope_object;
struct root_scope_object;
typedef struct compiler_keyword_object DeeCompilerKeywordObject;
typedef struct compiler_symbol_object DeeCompilerSymbolObject;
typedef struct compiler_ast_object DeeCompilerAstObject;
typedef struct compiler_scope_object DeeCompilerScopeObject;
struct compiler_keyword_object { COMPILER_ITEM_OBJECT_HEAD(struct TPPKeyword) };
struct compiler_symbol_object { COMPILER_ITEM_OBJECT_HEAD(struct symbol) };
struct compiler_ast_object { COMPILER_ITEM_OBJECT_HEAD(DREF struct ast) };
struct compiler_scope_object { COMPILER_ITEM_OBJECT_HEAD(DREF struct scope_object) };

INTDEF DeeTypeObject DeeCompilerKeyword_Type;         /* item */
INTDEF DeeTypeObject DeeCompilerSymbol_Type;          /* item */
INTDEF DeeTypeObject DeeCompilerFile_Type;            /* item */
INTDEF DeeTypeObject DeeCompilerLexer_Type;           /* wrapper */
INTDEF DeeTypeObject DeeCompilerLexerKeywords_Type;   /* wrapper */
INTDEF DeeTypeObject DeeCompilerLexerExtensions_Type; /* wrapper */
INTDEF DeeTypeObject DeeCompilerLexerWarnings_Type;   /* wrapper */
INTDEF DeeTypeObject DeeCompilerLexerSyspaths_Type;   /* wrapper */
INTDEF DeeTypeObject DeeCompilerLexerIfdef_Type;      /* wrapper */
INTDEF DeeTypeObject DeeCompilerLexerToken_Type;      /* wrapper */
INTDEF DeeTypeObject DeeCompilerParser_Type;          /* wrapper */
INTDEF DeeTypeObject DeeCompilerAst_Type;             /* objitem */
INTDEF DeeTypeObject DeeCompilerScope_Type;           /* objitem */
INTDEF DeeTypeObject DeeCompilerBaseScope_Type;       /* objitem (extends `DeeCompilerScope_Type') */
INTDEF DeeTypeObject DeeCompilerRootScope_Type;       /* objitem (extends `DeeCompilerBaseScope_Type') */


INTDEF DREF DeeObject *DCALL DeeCompiler_GetScope(struct scope_object *__restrict scope);
#ifdef __INTELLISENSE__
INTDEF DREF DeeObject *DCALL DeeCompiler_GetKeyword(struct TPPKeyword *__restrict kwd);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetSymbol(struct symbol *__restrict sym);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetFile(struct TPPFile *__restrict file);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetLexer(DeeCompilerObject *__restrict self);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetLexerKeywords(DeeCompilerObject *__restrict self);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetLexerExtensions(DeeCompilerObject *__restrict self);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetLexerWarnings(DeeCompilerObject *__restrict self);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetLexerSyspaths(DeeCompilerObject *__restrict self);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetLexerIfdef(DeeCompilerObject *__restrict self);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetLexerToken(DeeCompilerObject *__restrict self);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetParser(DeeCompilerObject *__restrict self);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetAst(struct ast *__restrict branch);
#else
#define DeeCompiler_GetKeyword(kwd)          DeeCompiler_GetItem(&DeeCompilerKeyword_Type,kwd)
#define DeeCompiler_GetSymbol(sym)           DeeCompiler_GetItem(&DeeCompilerSymbol_Type,sym)
#define DeeCompiler_GetFile(file)            DeeCompiler_GetItem(&DeeCompilerFile_Type,file)
#define DeeCompiler_GetLexer(self)           DeeCompiler_GetWrapper(self,&DeeCompilerLexer_Type)
#define DeeCompiler_GetLexerKeywords(self)   DeeCompiler_GetWrapper(self,&DeeCompilerLexerKeywords_Type)
#define DeeCompiler_GetLexerExtensions(self) DeeCompiler_GetWrapper(self,&DeeCompilerLexerExtensions_Type)
#define DeeCompiler_GetLexerWarnings(self)   DeeCompiler_GetWrapper(self,&DeeCompilerLexerWarnings_Type)
#define DeeCompiler_GetLexerSyspaths(self)   DeeCompiler_GetWrapper(self,&DeeCompilerLexerSyspaths_Type)
#define DeeCompiler_GetLexerIfdef(self)      DeeCompiler_GetWrapper(self,&DeeCompilerLexerIfdef_Type)
#define DeeCompiler_GetLexerToken(self)      DeeCompiler_GetWrapper(self,&DeeCompilerLexerToken_Type)
#define DeeCompiler_GetParser(self)          DeeCompiler_GetWrapper(self,&DeeCompilerParser_Type)
#define DeeCompiler_GetAst(branch)           DeeCompiler_GetObjItem(&DeeCompilerAst_Type,(DeeObject *)(branch))
#endif

/* Type fields of DeeCompilerItem_Type and DeeCompilerWrapper_Type */
INTDEF struct type_member DeeCompilerItem_Members[];
INTDEF void DCALL DeeCompilerItem_Fini(DeeCompilerItemObject *__restrict self);
INTDEF void DCALL DeeCompilerItem_Visit(DeeCompilerItemObject *__restrict self, dvisit_t proc, void *arg);
INTDEF void DCALL DeeCompilerObjItem_Fini(DeeCompilerItemObject *__restrict self);
INTDEF void DCALL DeeCompilerObjItem_Visit(DeeCompilerItemObject *__restrict self, dvisit_t proc, void *arg);

INTDEF void DCALL DeeCompilerWrapper_Fini(DeeCompilerWrapperObject *__restrict self);
#define DeeCompilerWrapper_Visit    DeeCompilerItem_Visit
#define DeeCompilerWrapper_Members  DeeCompilerItem_Members

struct symbol;
INTDEF ATTR_COLD int DCALL err_invalid_ast_basescope(DeeCompilerAstObject *__restrict obj, struct base_scope_object *__restrict base_scope);
INTDEF ATTR_COLD int DCALL err_invalid_ast_compiler(DeeCompilerAstObject *__restrict obj);
INTDEF ATTR_COLD int DCALL err_invalid_file_compiler(DeeCompilerItemObject *__restrict obj);
INTDEF ATTR_COLD int DCALL err_invalid_scope_compiler(DeeCompilerScopeObject *__restrict obj);
INTDEF ATTR_COLD int DCALL err_invalid_symbol_compiler(DeeCompilerSymbolObject *__restrict obj);
INTDEF ATTR_COLD int DCALL err_different_base_scope(void);
INTDEF ATTR_COLD int DCALL err_different_root_scope(void);
INTDEF ATTR_COLD int DCALL err_compiler_item_deleted(DeeCompilerItemObject *__restrict item);
INTDEF ATTR_COLD int DCALL err_symbol_not_reachable(struct scope_object *__restrict scope, struct symbol *__restrict sym);
INTDEF bool DCALL scope_reaches_symbol(struct scope_object *__restrict scope, struct symbol *__restrict sym);

struct unicode_printer;

/* @return: 0:  OK
 * @return: -1: Error. */
INTDEF int DCALL get_astloc_from_obj(DeeObject *obj, struct ast_loc *__restrict result);
/* Helper functions for setting the DDI location of a given ast `dst'
 * WARNING: Previously set DDI information is overwritten,
 *          and the old DDI file will _NOT_ be decref'ed! */
INTDEF int DCALL set_astloc_from_obj(DeeObject *obj, struct ast *__restrict result);

/* Print the repr-form of the given ast-location to the given unicode printer `(filename,line,col)' */
INTDEF int DCALL print_ast_loc_repr(struct ast_loc *__restrict self, struct unicode_printer *__restrict printer);

/* @return: TOK_ERR: An error occurred (and was thrown)
 * @return: -2:      A keyword wasn't found (and `create_missing' was false) */
INTDEF tok_t DCALL get_token_from_str(char const *__restrict name, bool create_missing);
INTDEF tok_t DCALL get_token_from_obj(DeeObject *__restrict obj, bool create_missing);
/* @return: NULL:      An error occurred (and was thrown)
 * @return: ITER_DONE: The given `id' does not refer to a valid token id. */
INTDEF DREF /*String*/DeeObject *DCALL get_token_name(tok_t id, struct TPPKeyword *kwd);
INTDEF dhash_t DCALL get_token_namehash(tok_t id, struct TPPKeyword *kwd);

/* For AST_MULTIPLE: Return the flags for constructing a sequence for `typing'
 * NOTE: `typing' doesn't necessarily need to be a type object!
 * @return: (uint16_t)-1: Error. */
INTDEF uint16_t DCALL get_ast_multiple_typing(DeeTypeObject *__restrict typing);

struct catch_expr;
struct base_scope_object;

/* Unpack and validate a sequence `{(string,ast,ast)...} handlers' */
INTDEF struct catch_expr *DCALL
unpack_catch_expressions(DeeObject *__restrict handlers,
                         size_t *__restrict pcatch_c,
                         struct base_scope_object *__restrict base_scope);

/* Parse the flags for a loop-ast from a string (:rt:Compiler.makeloop) */
INTDEF int DCALL
parse_loop_flags(char const *__restrict flags,
                 uint16_t *__restrict presult);

/* Parse the flags for a conditional-ast from a string (:rt:Compiler.makeconditional) */
INTDEF int DCALL
parse_conditional_flags(char const *__restrict flags,
                        uint16_t *__restrict presult);

/* Parse the flags for an operator-ast from a string (:rt:Compiler.makeoperator) */
INTDEF int DCALL
parse_operator_flags(char const *__restrict flags,
                     uint16_t *__restrict presult);

/* Parse the operator name and determine its ID. */
INTDEF int DCALL get_operator_id(DeeObject *__restrict opid,
                                 uint16_t *__restrict presult);

INTDEF int32_t DCALL get_action_by_name(char const *__restrict name);
INTDEF char const *DCALL get_action_name(uint16_t action);



INTDEF int DCALL
check_function_code_scope(DeeBaseScopeObject *code_scope,
                          DeeBaseScopeObject *ast_base_scope);

#endif /* CONFIG_BUILDING_DEEMON */

DECL_END

#endif /* !GUARD_DEEMON_COMPILER_INTERFACE_H */
