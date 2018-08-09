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
#ifndef GUARD_DEEMON_COMPILER_LEXER_COMMA_C
#define GUARD_DEEMON_COMPILER_LEXER_COMMA_C 1

#include <deemon/api.h>
#include <deemon/compiler/tpp.h>
#include <deemon/compiler/ast.h>
#include <deemon/compiler/lexer.h>
#include <deemon/util/string.h>
#include <deemon/seq.h>
#include <deemon/map.h>
#include <deemon/dict.h>
#include <deemon/float.h>
#include <deemon/hashset.h>
#include <deemon/none.h>
#include <deemon/bool.h>
#include <deemon/int.h>
#include <deemon/string.h>
#include <deemon/list.h>
#include <deemon/tuple.h>

DECL_BEGIN

#define IS_SYMBOL_NAME(tok)  \
   (TPP_ISKEYWORD(tok) && (!KWD_ISUNARY(tok) || (tok) == KWD_none))

struct astlist {
 size_t              ast_c; /* Amount of branches in use. */
 size_t              ast_a; /* [>= ast_c] Allocated amount of branches. */
 DREF DeeAstObject **ast_v; /* [1..1][0..ast_c|ALLOC(ast_a)][owned] Vector of branches. */
};
#define ASTLIST_INIT {0,0,NULL}
PRIVATE void DCALL
astlist_fini(struct astlist *__restrict self) {
 size_t i;
 for (i = 0; i < self->ast_c; ++i)
     Dee_Decref(self->ast_v[i]);
 Dee_Free(self->ast_v);
}
PRIVATE int DCALL
astlist_upsize(struct astlist *__restrict self,
               size_t min_add) {
 DREF DeeAstObject **new_vector;
 size_t new_alloc = self->ast_a*2;
 ASSERT(min_add != 0);
 if (!new_alloc) new_alloc = min_add;
 while ((new_alloc-self->ast_c) < min_add) new_alloc *= 2;
do_realloc:
 new_vector = (DREF DeeAstObject **)Dee_TryRealloc(self->ast_v,new_alloc*
                                                   sizeof(DREF DeeAstObject *));
 if unlikely(!new_vector) {
  if (new_alloc != self->ast_c+min_add) {
   new_alloc = self->ast_c+min_add;
   goto do_realloc;
  }
  if (Dee_CollectMemory(new_alloc*sizeof(DREF DeeAstObject *)))
      goto do_realloc;
  return -1;
 }
 self->ast_v = new_vector;
 self->ast_a = new_alloc;
 ASSERT(self->ast_a > self->ast_c);
 ASSERT((self->ast_a-self->ast_c) >= min_add);
 return 0;
}

PRIVATE void DCALL
astlist_trunc(struct astlist *__restrict self) {
 DREF DeeAstObject **new_vector;
 if (self->ast_c == self->ast_a) return;
 new_vector = (DREF DeeAstObject **)Dee_TryRealloc(self->ast_v,self->ast_c*
                                                   sizeof(DREF DeeAstObject *));
 if likely(new_vector) self->ast_v = new_vector;
}
PRIVATE int DCALL
astlist_append(struct astlist *__restrict self,
               DeeAstObject *__restrict ast) {
 if (self->ast_c == self->ast_a &&
     astlist_upsize(self,1)) return -1;
 self->ast_v[self->ast_c++] = ast;
 Dee_Incref(ast);
 return 0;
}
PRIVATE int DCALL
astlist_appendall(struct astlist *__restrict self,
                  struct astlist *__restrict other) {
 size_t avail = self->ast_a-self->ast_c;
 if (avail < other->ast_c) {
  if (!self->ast_c) {
   /* Even more simple (Steal everything) */
   Dee_Free(self->ast_v);
   self->ast_c  = other->ast_c;
   self->ast_a  = other->ast_a;
   self->ast_v  = other->ast_v;
   other->ast_c = 0;
   other->ast_a = 0;
   other->ast_v = NULL;
   return 0;
  }
  if (astlist_upsize(self,other->ast_c-avail))
      return -1;
 }
 assert(self->ast_c+other->ast_c <= self->ast_a);
 MEMCPY_PTR(self->ast_v+self->ast_c,other->ast_v,other->ast_c);
 self->ast_c += other->ast_c;
 other->ast_c = 0; /* Steal all of these references. */
 return 0;
}

INTERN int DCALL
ast_parse_lookup_mode(unsigned int *__restrict pmode) {
next_modifier:
 switch (tok) {

 case KWD_local:
  if (*pmode & LOOKUP_SYM_VLOCAL &&
      WARN(W_VARIABLE_MODIFIER_DUPLICATED))
      goto err;
  if (*pmode & LOOKUP_SYM_VGLOBAL) {
   if (WARN(W_VARIABLE_MODIFIER_INCOMPATIBLE,"global"))
       goto err;
   *pmode &= ~LOOKUP_SYM_VGLOBAL;
  }
  *pmode |= LOOKUP_SYM_VLOCAL;
continue_modifier:
  if unlikely(yield() < 0) goto err;
  goto next_modifier;

 case TOK_COLLON_COLLON:
  /* Backwards compatibility with deemon 100+ */
  if (WARN(W_DEPRECATED_GLOBAL_PREFIX))
      goto err;
  ATTR_FALLTHROUGH
 case KWD_global:
  if (*pmode & LOOKUP_SYM_VGLOBAL &&
      WARN(W_VARIABLE_MODIFIER_DUPLICATED))
      goto err;
  if (*pmode & LOOKUP_SYM_VLOCAL) {
   if (WARN(W_VARIABLE_MODIFIER_INCOMPATIBLE,"local"))
       goto err;
   *pmode &= ~LOOKUP_SYM_VLOCAL;
  }
  *pmode |= LOOKUP_SYM_VGLOBAL;
  goto continue_modifier;

 case KWD_static:
  if (*pmode & LOOKUP_SYM_STATIC &&
      WARN(W_VARIABLE_MODIFIER_DUPLICATED))
      goto err;
  if (*pmode & LOOKUP_SYM_STACK) {
   if (WARN(W_VARIABLE_MODIFIER_INCOMPATIBLE,"__stack"))
       goto err;
   *pmode &= ~LOOKUP_SYM_STACK;
  }
  *pmode |= LOOKUP_SYM_STATIC;
  goto continue_modifier;

 case KWD___stack:
  if (*pmode & LOOKUP_SYM_STACK &&
      WARN(W_VARIABLE_MODIFIER_DUPLICATED))
      goto err;
  if (*pmode & LOOKUP_SYM_STATIC) {
   if (WARN(W_VARIABLE_MODIFIER_INCOMPATIBLE,"static"))
       goto err;
   *pmode &= ~LOOKUP_SYM_STATIC;
  }
  *pmode |= LOOKUP_SYM_STACK;
  goto continue_modifier;

 default: break;
 }
 return 0;
err:
 return -1;
}


INTERN DREF DeeAstObject *DCALL
ast_parse_comma(unsigned int mode, uint16_t flags) {
 DREF DeeAstObject *current; bool need_semi; int error;
 unsigned int lookup_mode; struct ast_loc loc;
 /* In: "foo,bar = 10,x,y = getvalue()...,7"
  *              |     |                 |
  *              +-------------------------  expr_batch = { }
  *                    |                 |   expr_comma = { foo, bar }
  *                    |                 |
  *                    +-------------------  expr_batch = { foo, (bar = 10) }
  *                                      |   expr_comma = { x }
  *                                      |
  *                                      +-  expr_batch = { foo, (bar = 10), (x,y = getvalue()...) }
  *                                          expr_comma = { }
  */
 struct astlist expr_batch = ASTLIST_INIT; /* Expressions that have already been written to. */
 struct astlist expr_comma = ASTLIST_INIT; /* Expressions that are pending getting assigned. */
#if AST_COMMA_ALLOWVARDECLS == LOOKUP_SYM_ALLOWDECL
 lookup_mode = mode&AST_COMMA_ALLOWVARDECLS;
#else
 lookup_mode = ((mode&AST_COMMA_ALLOWVARDECLS)
                ? LOOKUP_SYM_ALLOWDECL
                : LOOKUP_SYM_NORMAL);
#endif
 /* Allow explicit visibility modifiers when variable can be declared. */
 if (mode&AST_COMMA_ALLOWVARDECLS &&
     ast_parse_lookup_mode(&lookup_mode))
     goto err;

next_expr:
 need_semi = !!(mode&AST_COMMA_PARSESEMI);
 /* Parse an expression (special handling for functions/classes) */
 if (tok == KWD_final || tok == KWD_class) {
  /* Declare a new class */
  uint16_t class_flags = current_tags.at_class_flags & 0xf; /* From tags. */
  struct TPPKeyword *class_name = NULL;
  unsigned int symbol_mode = lookup_mode;
  if (tok == KWD_final) {
   class_flags |= TP_FFINAL;
   if unlikely(yield() < 0) goto err;
   if (unlikely(tok != KWD_class) &&
       WARN(W_EXPECTED_CLASS_AFTER_FINAL))
       goto err;
  }
  loc_here(&loc);
  if unlikely(yield() < 0) goto err;
  if (tok == KWD_class && !(class_flags&TP_FFINAL)) {
   class_flags |= TP_FFINAL;
   if unlikely(yield() < 0) goto err;
  }
  if (TPP_ISKEYWORD(tok)) {
   class_name = token.t_kwd;
   if unlikely(yield() < 0) goto err;
   if ((symbol_mode & LOOKUP_SYM_VMASK) == LOOKUP_SYM_VDEFAULT) {
    /* Use the default mode appropriate for the current scope. */
    if (current_scope == &current_rootscope->rs_scope.bs_scope)
         symbol_mode |= LOOKUP_SYM_VGLOBAL;
    else symbol_mode |= LOOKUP_SYM_VLOCAL;
   }
  }
  current = ast_setddi(ast_parse_class(class_flags,class_name,
                                       class_name != NULL,
                                       symbol_mode),
                      &loc);
  if unlikely(!current) goto err;
  need_semi = false; /* Classes always have braces and don't need semicolons. */
#if 0 /* TODO: property variable */
 } else if (tok == KWD_property) {
#endif
 } else if (tok == KWD_function) {
  /* Declare a new function */
  struct TPPKeyword *function_name = NULL;
  struct symbol *function_symbol = NULL;
  unsigned int symbol_mode = lookup_mode;
  struct ast_loc function_name_loc;
  loc_here(&loc);
  if unlikely(yield() < 0) goto err;
  if (TPP_ISKEYWORD(tok)) {
   loc_here(&function_name_loc);
   function_name = token.t_kwd;
   if unlikely(yield() < 0) goto err;
   if ((symbol_mode & LOOKUP_SYM_VMASK) == LOOKUP_SYM_VDEFAULT) {
    /* Use the default mode appropriate for the current scope. */
    if (current_scope == &current_rootscope->rs_scope.bs_scope)
         symbol_mode |= LOOKUP_SYM_VGLOBAL;
    else symbol_mode |= LOOKUP_SYM_VLOCAL;
   }
   /* Create the symbol that will be used by the function. */
   function_symbol = lookup_symbol(symbol_mode,function_name,&loc);
   if unlikely(!function_symbol) goto err;
   /* Pack together the documentation string for the function. */
   if (function_symbol->sym_class == SYM_CLASS_VAR &&
      !function_symbol->sym_var.sym_doc) {
    function_symbol->sym_var.sym_doc = (DREF struct string_object *)ascii_printer_pack(&current_tags.at_doc);
    ascii_printer_init(&current_tags.at_doc);
    if (!function_symbol->sym_var.sym_doc) goto err;
   }
  }
  current = ast_setddi(ast_parse_function(function_name,&need_semi,false),&loc);
  if unlikely(!current) goto err;
  clear_current_tags();
  if (function_symbol) {
   DREF DeeAstObject *function_name_ast,*merge;
   /* Store the function in the parsed symbol. */
   function_name_ast = ast_setddi(ast_sym(function_symbol),&function_name_loc);
   if unlikely(!function_name_ast) goto err_current;
   merge = ast_setddi(ast_action2(AST_FACTION_STORE,function_name_ast,current),&loc);
   Dee_Decref(function_name_ast);
   if unlikely(!merge) goto err_current;
   Dee_Decref(current);
   current = merge;
  }
 } else {
  if (mode & AST_COMMA_ALLOWKWDLIST) {
   if (TPP_ISKEYWORD(tok)) {
    /* If the next token is a `:', then we're currently at a keyword list label,
     * in which case we're supposed to stop and let the caller deal with this. */
    char *next = peek_next_token(NULL);
    if unlikely(!next) goto err;
    if (*next == ':') {
     /* Make sure it isn't a `::' token. */
     ++next;
     while (SKIP_WRAPLF(next,token.t_file->f_end));
     if (*next != ':')
          goto done_expression_nocurrent;
    }
   }
   if (tok == TOK_POW) /* foo(**bar) --> Invoke using `bar' for keyword arguments. */
       goto done_expression_nocurrent;
  }
  if (!IS_SYMBOL_NAME(tok) &&
      (lookup_mode&LOOKUP_SYM_VMASK) != LOOKUP_SYM_VDEFAULT) {
   /* Warn if an explicit visibility modifier isn't followed by a symbol name. */
   if (WARN(W_EXPECTED_VARIABLE_AFTER_VISIBILITY))
       goto err;
  }
  current = ast_parse_brace(lookup_mode,NULL);
  /* Check for errors. */
  if unlikely(!current)
     goto err;
  if (lookup_mode&LOOKUP_SYM_ALLOWDECL && TPP_ISKEYWORD(tok)) {
   struct symbol *var_symbol;
   DREF DeeAstObject *args,*merge;
   struct ast_loc symbol_name_loc;
   loc_here(&symbol_name_loc);
   var_symbol = get_local_symbol(token.t_kwd);
   if unlikely(var_symbol) {
    if (WARN(W_VARIABLE_ALREADY_EXISTS,token.t_kwd))
        goto err_current;
   } else {
    /* Create a new symbol for the initialized variable. */
    var_symbol = new_local_symbol(token.t_kwd);
    if unlikely(!var_symbol) goto err_current;
    if (lookup_mode&LOOKUP_SYM_STATIC) {
     var_symbol->sym_flag = SYM_FVAR_STATIC;
     goto set_typed_var;
    } else if (lookup_mode&LOOKUP_SYM_STACK) {
     var_symbol->sym_class = SYM_CLASS_STACK;
    } else if (lookup_mode&LOOKUP_SYM_VGLOBAL) {
     var_symbol->sym_flag = SYM_FVAR_GLOBAL;
     goto set_typed_var;
    } else {
     var_symbol->sym_flag = SYM_FVAR_LOCAL;
set_typed_var:
     var_symbol->sym_class = SYM_CLASS_VAR;
     /* Package together documentation tags for this variable symbol. */
     var_symbol->sym_var.sym_doc = (DREF struct string_object *)ascii_printer_pack(&current_tags.at_doc);
     ascii_printer_init(&current_tags.at_doc);
     if unlikely(!var_symbol->sym_var.sym_doc) goto err_current;
    }
   }
   if unlikely(yield() < 0) goto err_current;
   /* Allow syntax like this:
    * >> list my_list;
    * >> int my_int = 42;
    * >> MyClass my_instance(10,20,30);
    * >> thread my_thread = []{
    * >>     print "Thread execution...";
    * >> };
    * >> my_thread.start();
    * Same as:
    * >> local my_list = list();
    * >> local my_int = int(42);
    * >> local my_instance = MyClass(10,20,30);
    * >> local my_thread = thread([]{
    * >>     print "Thread execution...";
    * >> });
    * >> my_thread.start();
    * Now that C compatibility is gone, there's no ambiguity to this!
    */
   if (tok == '=' || tok == '{') {
    /* Single-operand argument list. */
    DeeTypeObject *preferred_type;
    DREF DeeAstObject **exprv;
    struct ast_loc equal_loc;
    loc_here(&equal_loc);
    preferred_type = NULL;
    if (current->ast_type == AST_CONSTEXPR) {
     preferred_type = (DeeTypeObject *)current->ast_constexpr;
     if (preferred_type != &DeeHashSet_Type &&
         preferred_type != &DeeDict_Type &&
         preferred_type != &DeeList_Type &&
         preferred_type != &DeeTuple_Type &&
         preferred_type != &DeeSeq_Type &&
         preferred_type != &DeeMapping_Type)
         preferred_type = NULL;
    }
    if (tok == '=' && unlikely(yield() < 0)) goto err_current;
    /* Parse a preferred-type brace expression. */
    args = ast_parse_brace(LOOKUP_SYM_NORMAL,preferred_type);
    if unlikely(!args) goto err_current;
    /* Wrap the returned ast in a 1-element tuple (for the argument list) */
    exprv = (DREF DeeAstObject **)Dee_Malloc(1*sizeof(DREF DeeAstObject *));
    if unlikely(!exprv) goto err_args;
    exprv[0] = args; /* Inherit */
    /* Create a multi-branch AST for the assigned expression. */
    args = ast_setddi(ast_multiple(AST_FMULTIPLE_TUPLE,1,exprv),&equal_loc);
    if unlikely(!args) { Dee_Decref(exprv[0]); Dee_Free(exprv); goto err_current; }
   } else if (tok == '(' || tok == KWD_pack) {
    /* Comma-separated argument list. */
    uint32_t old_flags;
    bool has_paren = tok == '(';
    old_flags = TPPLexer_Current->l_flags;
    TPPLexer_Current->l_flags &= ~TPPLEXER_FLAG_WANTLF;
    if unlikely(yield() < 0) {
     TPPLexer_Current->l_flags |= old_flags & TPPLEXER_FLAG_WANTLF;
     goto err_current;
    }
    if (tok == ')' || (!has_paren && !maybe_expression_begin())) {
     /* Empty argument list (Same as none at all). */
     args = ast_sethere(ast_constexpr(Dee_EmptyTuple));
    } else {
     args = ast_parse_comma(AST_COMMA_FORCEMULTIPLE,
                            AST_FMULTIPLE_TUPLE);
    }
    TPPLexer_Current->l_flags |= old_flags & TPPLEXER_FLAG_WANTLF;
    if unlikely(!args) goto err_current;
    if (has_paren) {
     if unlikely(likely(tok == ')') ? (yield() < 0) :
                 WARN(W_EXPECTED_RPAREN_AFTER_CALL)) {
err_args:
      Dee_Decref(args);
      goto err_current;
     }
    }
   } else {
    /* No argument list. */
    args = ast_setddi(ast_constexpr(Dee_EmptyTuple),&symbol_name_loc);
    if unlikely(!args) goto err_current;
   }
   merge = ast_setddi(ast_operator2(OPERATOR_CALL,AST_OPERATOR_FNORMAL,
                                    current,args),
                     &symbol_name_loc);
   Dee_Decref(args);
   Dee_Decref(current);
   if unlikely(!merge) goto err;
   current = merge;
   /* Now generate a branch to store the expression in the branch. */
   args = ast_setddi(ast_sym(var_symbol),&symbol_name_loc);
   if unlikely(!args) goto err_current;
   merge = ast_setddi(ast_action2(AST_FACTION_STORE,
                                  args,current),
                     &symbol_name_loc);
   Dee_Decref(args);
   Dee_Decref(current);
   if unlikely(!merge) goto err;
   current = merge;
  }
 }

 if (tok == ',' && !(mode&AST_COMMA_PARSESINGLE)) {
  if (mode&AST_COMMA_STRICTCOMMA) {
   /* Peek the next token to check if it might be an expression. */
   char *next = peek_next_token(NULL);
   if unlikely(!next) goto err;
   if (!maybe_expression_begin_c(*next))
        goto done_expression;
  }
  /* Append to the current comma-sequence. */
  error = astlist_append(&expr_comma,current);
  if unlikely(error) goto err_current;
  Dee_Decref(current);
  /* Yield the ',' token. */
continue_at_comma:
  if unlikely(yield() < 0) goto err;
  if (!maybe_expression_begin()) {
   /* Special case: `x = (10,)'
    * Same as `x = pack(10)', in that a single-element tuple is created. */
   /* Flush any remaining entries from the comma-list. */
   error = astlist_appendall(&expr_batch,&expr_comma);
   if unlikely(error) goto err;
   /* Pack the branch together to form a multi-branch AST. */
   astlist_trunc(&expr_batch);
   current = ast_multiple(flags,expr_batch.ast_c,expr_batch.ast_v);
   if unlikely(!current) goto err;
   /* Free an remaining buffers. */
   /*Dee_Free(expr_batch.ast_v);*/ /* This one was inherited. */
   Dee_Free(expr_comma.ast_v);
   /* WARNING: At this point, both `expr_batch' and `expr_comma' are
    *          in an undefined state, but don't hold any real data. */
   goto done_expression_nomerge;
  }
  goto next_expr;
 }
 if (tok == '=') {
  DREF DeeAstObject *store_source;
  /* This is where the magic happens and where we
   * assign to expression in the active comma-list. */
  loc_here(&loc);
  if unlikely(yield() < 0) goto err_current;
  store_source = ast_parse_comma(AST_COMMA_PARSESINGLE|
                                (mode&AST_COMMA_STRICTCOMMA),
                                 AST_FMULTIPLE_KEEPLAST);
  if unlikely(!store_source) goto err_current;
  need_semi = !!(mode&AST_COMMA_PARSESEMI);
  /* Now everything depends on whether or not what
   * we've just parsed is an expand-expression:
   * >> a,b,c = get_value()...; // >> (((a,b,c) = get_value())...);
   * >> a,b,c = get_value();    // >> (a,b,(c = get_value())); */
  if (store_source->ast_type == AST_EXPAND) {
   DREF DeeAstObject *store_target,*store_branch;
   /* Append the last expression (in the example above, that is `c') */
   error = astlist_append(&expr_comma,current);
   Dee_Decref(current);
   if unlikely(error) {
err_store_source:
    Dee_Decref(store_source);
    goto err;
   }
   astlist_trunc(&expr_comma);
   store_target = ast_setddi(ast_multiple(AST_FMULTIPLE_TUPLE,
                                          expr_comma.ast_c,
                                          expr_comma.ast_v),
                            &loc);
   if unlikely(!store_target) goto err_store_source;
   /* Steal all of these. */
   expr_comma.ast_a = 0;
   expr_comma.ast_c = 0;
   expr_comma.ast_v = NULL;
   /* Now store the expand's underlying expression within this tuple. */
   store_branch = ast_setddi(ast_action2(AST_FACTION_STORE,
                                         store_target,
                                         store_source->ast_expandexpr),
                            &loc);
   Dee_Decref(store_target);
   Dee_Decref(store_source);
   if unlikely(!store_branch) goto err;
   /* Now wrap the store-branch in another expand expression. */
   current = ast_setddi(ast_expand(store_branch),&loc);
   Dee_Decref(store_branch);
   if unlikely(!current) goto err;
  } else {
   DREF DeeAstObject *store_branch;
   /* Second case: assign `store_source' to `current' after
    *              flushing everything from the comma-list. */
   error = astlist_appendall(&expr_batch,&expr_comma);
   if unlikely(error) { Dee_Decref(store_source); goto err_current; }
   /* With the comma-list now empty, generate the
    * store as described in the previous comment. */
   store_branch = ast_setddi(ast_action2(AST_FACTION_STORE,
                                         current,
                                         store_source),
                            &loc);
   Dee_Decref(store_source);
   Dee_Decref(current);
   if unlikely(!store_branch) goto err;
   current = store_branch;
  }

  /* Check for further comma or store expressions. */
  if (tok == ',' && !(mode&AST_COMMA_PARSESINGLE)) {
   if (!(mode&AST_COMMA_STRICTCOMMA)) {
do_append_gen_to_batch:
    /* Append the generated expression to the batch. */
    error = astlist_append(&expr_batch,current);
    if unlikely(error) goto err_current;
    Dee_Decref(current);
    goto continue_at_comma;
   } else {
    char *next_token;
    next_token = peek_next_token(NULL);
    if unlikely(!next_token) goto err_current;
    if (maybe_expression_begin_c(*next_token))
        goto do_append_gen_to_batch;
   }
  }
 }
done_expression:

 /* Merge the current expression with the batch and comma lists. */
 if (expr_batch.ast_c || expr_comma.ast_c) {
  /* Flush any remaining entries from the comma-list. */
  error = astlist_appendall(&expr_batch,&expr_comma);
  if unlikely(error) goto err_current;
  /* Append the remaining expression to the batch. */
  error = astlist_append(&expr_batch,current);
  if unlikely(error) goto err_current;
  Dee_Decref(current);
  /* Pack the branch together to form a multi-branch AST. */
  astlist_trunc(&expr_batch);
  current = ast_multiple(flags,expr_batch.ast_c,expr_batch.ast_v);
  if unlikely(!current) goto err;

  /* Free an remaining buffers. */
  /*Dee_Free(expr_batch.ast_v);*/ /* This one was inherited. */
  Dee_Free(expr_comma.ast_v);
  /* WARNING: At this point, both `expr_batch' and `expr_comma' are
   *          in an undefined state, but don't hold any real data. */
 } else {
  ASSERT(!expr_batch.ast_v);
  ASSERT(!expr_comma.ast_v);
  if (mode&AST_COMMA_FORCEMULTIPLE) {
   /* If the caller wants to force us to package
    * everything in a multi-branch, grant that wish. */
   DREF DeeAstObject **astv,*result;
   astv = (DREF DeeAstObject **)Dee_Malloc(1*sizeof(DREF DeeAstObject *));
   if unlikely(!astv) goto err_current;
   astv[0] = current;
   result = ast_multiple(flags,1,astv);
   if unlikely(!result) { Dee_Free(astv); goto err_current; }
   current = result;
  }
 }
done_expression_nomerge:
 if (need_semi) {
  /* Consume a `;' token as part of the expression. */
  if likely(tok == ';' || tok == '\n') {
   do {
    if (yieldnbif(mode & AST_COMMA_ALLOWNONBLOCK) < 0)
        goto err_clear_current_only;
   } while (tok == '\n');
  } else {
   if unlikely(WARN(W_EXPECTED_SEMICOLLON_AFTER_EXPRESSION)) {
err_clear_current_only:
    Dee_Clear(current);
   }
  }
 }
 return current;
done_expression_nocurrent:
 /* Merge the current expression with the batch and comma lists. */
 if (expr_batch.ast_c || expr_comma.ast_c) {
  /* Flush any remaining entries from the comma-list. */
  error = astlist_appendall(&expr_batch,&expr_comma);
  if unlikely(error) goto err;
  /* Pack the branch together to form a multi-branch AST. */
  astlist_trunc(&expr_batch);
  current = ast_multiple(flags,expr_batch.ast_c,expr_batch.ast_v);
  if unlikely(!current) goto err;
  /* Free an remaining buffers. */
  /*Dee_Free(expr_batch.ast_v);*/ /* This one was inherited. */
  Dee_Free(expr_comma.ast_v);
  /* WARNING: At this point, both `expr_batch' and `expr_comma' are
   *          in an undefined state, but don't hold any real data. */
 } else {
  ASSERT(!expr_batch.ast_v);
  ASSERT(!expr_comma.ast_v);
  /* If the caller wants to force us to package
   * everything in a multi-branch, grant that wish. */
  current = ast_multiple(flags,0,NULL);
  if unlikely(!current) goto err;
 }
 goto done_expression_nomerge;
err_current:
 Dee_Decref(current);
err:
 astlist_fini(&expr_comma);
 astlist_fini(&expr_batch);
 return NULL;
}


DECL_END

#endif /* !GUARD_DEEMON_COMPILER_LEXER_COMMA_C */
