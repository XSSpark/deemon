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
#ifndef GUARD_DEEMON_COMPILER_OPTIMIZE_OPTIMIZE_C
#define GUARD_DEEMON_COMPILER_OPTIMIZE_OPTIMIZE_C 1
#define _KOS_SOURCE 1

#include <deemon/api.h>
#include <deemon/object.h>
#include <deemon/none.h>
#include <deemon/arg.h>
#include <deemon/bytes.h>
#include <deemon/class.h>
#include <deemon/code.h>
#include <deemon/roset.h>
#include <deemon/rodict.h>
#include <deemon/bool.h>
#include <deemon/seq.h>
#include <deemon/error.h>
#include <deemon/file.h>
#include <deemon/float.h>
#include <deemon/int.h>
#include <deemon/string.h>
#include <deemon/cell.h>
#include <deemon/dict.h>
#include <deemon/dec.h>
#include <deemon/super.h>
#include <deemon/hashset.h>
#include <deemon/tuple.h>
#include <deemon/map.h>
#include <deemon/list.h>
#include <deemon/compiler/ast.h>
#include <deemon/compiler/compiler.h>
#include <deemon/compiler/assembler.h>
#include <deemon/compiler/optimize.h>
#include <deemon/util/string.h>
#include <deemon/objmethod.h>
#include <deemon/module.h>
#include <deemon/numeric.h>
#include <deemon/callable.h>

#include <stdarg.h>
#include <string.h>

DECL_BEGIN

INTERN uint16_t optimizer_flags        = OPTIMIZE_FDISABLED;
INTERN uint16_t optimizer_unwind_limit = 0;
INTERN unsigned int optimizer_count    = 0;

#ifdef CONFIG_HAVE_OPTIMIZE_VERBOSE
INTERN void ast_optimize_verbose(DeeAstObject *__restrict self, char const *format, ...) {
 va_list args; va_start(args,format);
 if (self->ast_ddi.l_file) {
  DEE_DPRINTF("%s(%d,%d) : ",
              TPPFile_Filename(self->ast_ddi.l_file,NULL),
              self->ast_ddi.l_line+1,
              self->ast_ddi.l_col+1);
 }
 DEE_VDPRINTF(format,args);
 va_end(args);
}
#endif




INTERN int (DCALL ast_optimize)(struct ast_optimize_stack *__restrict parent,
                                DeeAstObject *__restrict self, bool result_used) {
 struct ast_optimize_stack stack;
 stack.os_prev   = parent;
 stack.os_ast    = self;
 stack.os_used   = result_used;
#ifdef OPTIMIZE_FASSUME
 stack.os_assume = parent->os_assume;
#endif /* OPTIMIZE_FASSUME */
 return ast_dooptimize(&stack,self,result_used);
}
INTERN int (DCALL ast_startoptimize)(DeeAstObject *__restrict self, bool result_used) {
 struct ast_optimize_stack stack;
 int result;
#ifdef OPTIMIZE_FASSUME
 struct ast_assumes assumes;
#endif /* OPTIMIZE_FASSUME */
 stack.os_prev   = NULL;
 stack.os_ast    = self;
 stack.os_used   = result_used;
#ifdef OPTIMIZE_FASSUME
 stack.os_assume = &assumes;
 ast_assumes_init(&assumes);
#endif /* OPTIMIZE_FASSUME */
 result = ast_dooptimize(&stack,self,result_used);
#ifdef OPTIMIZE_FASSUME
 ast_assumes_fini(&assumes);
#endif /* OPTIMIZE_FASSUME */
 return result;
}
INTERN int (DCALL ast_dooptimize)(struct ast_optimize_stack *__restrict stack,
                                  DeeAstObject *__restrict self, bool result_used) {
 ASSERT_OBJECT_TYPE(self,&DeeAst_Type);
again:
 /* Check for interrupts to allow the user to stop optimization */
 if (DeeThread_CheckInterrupt()) goto err;
 while (self->ast_scope->s_prev &&                    /* Has parent scope */
        self->ast_scope->s_prev->ob_type ==           /* Parent scope has the same type */
        self->ast_scope->ob_type &&                   /* ... */
        self->ast_scope->ob_type == &DeeScope_Type && /* It's a regular scope */
       !self->ast_scope->s_mapc &&                    /* Current scope has no symbols */
       !self->ast_scope->s_del) {                     /* Current scope has no deleted symbol */
  /* Use the parent's scope. */
  DeeScopeObject *new_scope;
  new_scope = self->ast_scope->s_prev;
  Dee_Incref(new_scope);
  Dee_Decref(self->ast_scope);
  self->ast_scope = new_scope;
  ++optimizer_count;
#if 0 /* This happens so often, logging it would just be spam... */
  OPTIMIZE_VERBOSEAT(self,"Inherit parent scope above empty child scope\n");
#endif
 }

 /* TODO: Move variables declared in outer scopes, but only used in inner ones
  *       into those inner scopes, thus improving local-variable-reuse optimizations
  *       later done the line. */
 switch (self->ast_type) {

 case AST_SYM:
  return ast_optimize_symbol(stack,self,result_used);

 case AST_MULTIPLE:
  return ast_optimize_multiple(stack,self,result_used);

#ifdef OPTIMIZE_FASSUME
 case AST_UNBIND:
  /* Delete assumptions made about the symbol. */
  if (optimizer_flags & OPTIMIZE_FASSUME)
      return ast_assumes_setsymval(stack->os_assume,self->ast_unbind,NULL);
  break;
#endif

 /* TODO: Using assumptions, we could also track if a symbol if a symbol is bound? */

 case AST_RETURN:
 case AST_YIELD:
 case AST_THROW:
  if (self->ast_returnexpr &&
      ast_optimize(stack,self->ast_returnexpr,true))
      goto err;
  break;

 case AST_TRY:
  return ast_optimize_try(stack,self,result_used);

 case AST_LOOP:
  return ast_optimize_loop(stack,self,result_used);

 case AST_CONDITIONAL:
  return ast_optimize_conditional(stack,self,result_used);

 {
  int ast_value;
  DREF DeeAstObject **elemv;
 case AST_BOOL:
  /* TODO: Optimize the inner expression in regards to
   *       it only being used as a boolean expression */
  if (ast_optimize(stack,self->ast_boolexpr,result_used)) goto err;
  /* If the result doesn't matter, don't perform a negation. */
  if (!result_used) self->ast_flag &= ~AST_FBOOL_NEGATE;

  /* Figure out if the branch's value is known at compile-time. */
  ast_value = ast_get_boolean(self->ast_boolexpr);
  if (ast_value >= 0) {
   if (self->ast_flag&AST_FBOOL_NEGATE)
       ast_value = !ast_value;
   if (ast_has_sideeffects(self->ast_boolexpr)) {
    /* Replace this branch with `{ ...; true/false; }' */
    elemv = (DREF DeeAstObject **)Dee_Malloc(2*sizeof(DREF DeeAstObject *));
    if unlikely(!elemv) goto err;
    elemv[0] = self->ast_boolexpr;
    elemv[1] = ast_setscope_and_ddi(ast_constexpr(DeeBool_For(ast_value)),self);
    if unlikely(!elemv[1]) { Dee_Free(elemv); goto err; }
    self->ast_type               = AST_MULTIPLE;
    self->ast_flag               = AST_FMULTIPLE_KEEPLAST;
    self->ast_multiple.ast_exprc = 2;
    self->ast_multiple.ast_exprv = elemv;
   } else {
    /* Without side-effects, there is no need to keep the expression around. */
    Dee_Decref_likely(self->ast_boolexpr);
    self->ast_constexpr = DeeBool_For(ast_value);
    Dee_Incref(self->ast_constexpr);
    self->ast_type = AST_CONSTEXPR;
    self->ast_flag = AST_FNORMAL;
   }
   OPTIMIZE_VERBOSE("Propagating constant boolean expression\n");
   goto did_optimize;
  }
  if (ast_predict_type(self->ast_boolexpr) == &DeeBool_Type) {
   if (self->ast_flag & AST_FBOOL_NEGATE) {
    /* TODO: Search for what is making the inner expression
     *       a boolean and try to apply our negation to it:
     * >> x = !({ !foo(); }); // Apply our negation to the inner expression.
     */
   } else {
    /* Optimize away the unnecessary bool-cast */
    if (ast_graft_onto(self,self->ast_boolexpr))
        goto err;
    OPTIMIZE_VERBOSE("Remove unused bool cast\n");
    goto did_optimize;
   }
  }
 } break;

 case AST_EXPAND:
  if (ast_optimize(stack,self->ast_expandexpr,result_used))
      goto err;
  break;
 case AST_FUNCTION:
  if (ast_optimize(stack,self->ast_function.ast_code,false))
      goto err;
  if (!DeeCompiler_Current->cp_options ||
     !(DeeCompiler_Current->cp_options->co_assembler & ASM_FNODEC)) {
   /* TODO: Replace initializers of function default-arguments that
    *       are never actually used by the function with `none'
    *    -> That way, we can reduce the size of a produced DEC file.
    * NOTE: Only enable this optimization when a DEC file is being
    *       generated, as it doesn't affect runtime performance in
    *       direct source->assembly execution mode, and only slows
    *       down compilation then. */
  }
  break;

 case AST_LABEL:
  /* Special label-branch. */
  ASSERT(self->ast_label.ast_label);
  if (!self->ast_label.ast_label->tl_goto) {
   /* The label was never used. - Ergo: it should not exist.
    * To signify this, we simply convert this branch to `none'. */
   Dee_Decref((DeeObject *)self->ast_label.ast_base);
   self->ast_type      = AST_CONSTEXPR;
   self->ast_flag      = AST_FNORMAL;
   self->ast_constexpr = Dee_None;
   Dee_Incref(Dee_None);
   OPTIMIZE_VERBOSE("Removing unused label\n");
   goto did_optimize;
  }
#ifdef OPTIMIZE_FASSUME
  if (optimizer_flags & OPTIMIZE_FASSUME &&
      ast_assumes_undefined(stack->os_assume))
      goto err;
#endif
  break;

 case AST_OPERATOR:
  return ast_optimize_operator(stack,self,result_used);

 case AST_ACTION:
  return ast_optimize_action(stack,self,result_used);

 case AST_SWITCH:
  return ast_optimize_switch(stack,self,result_used);

 {
  struct asm_operand *iter,*end;
 case AST_ASSEMBLY:
  end = (iter = self->ast_assembly.ast_opv)+
               (self->ast_assembly.ast_num_i+
                self->ast_assembly.ast_num_o);
  for (; iter != end; ++iter) {
   if (ast_optimize(stack,iter->ao_expr,true))
       goto err;
  }
 } break;

 default: break;
 }
 return 0;
did_optimize:
 ++optimizer_count;
 goto again;
err:
 return -1;
}

INTERN int (DCALL ast_optimize_all)(DeeAstObject *__restrict self, bool result_used) {
 int result;
 unsigned int old_value;
 do {
  old_value = optimizer_count;
  result = ast_startoptimize(self,result_used);
  if unlikely(result) break;
  /* Stop after the first pass if the `OPTIMIZE_FONEPASS' flag is set. */
  if (optimizer_flags&OPTIMIZE_FONEPASS) break;
  /* Keep optimizing the branch while stuff happens. */
 } while (old_value != optimizer_count);
 return result;
}


DECL_END

#endif /* !GUARD_DEEMON_COMPILER_OPTIMIZE_OPTIMIZE_C */
