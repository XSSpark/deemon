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
#ifndef GUARD_DEEMON_COMPILER_LEXER_TAG_C
#define GUARD_DEEMON_COMPILER_LEXER_TAG_C 1

#include <deemon/api.h>
#include <deemon/none.h>
#include <deemon/class.h>
#include <deemon/string.h>
#include <deemon/compiler/tpp.h>
#include <deemon/compiler/ast.h>
#include <deemon/compiler/lexer.h>
#include <deemon/compiler/optimize.h>
#include <deemon/tuple.h>
#include <deemon/error.h>
#include <deemon/util/string.h>

DECL_BEGIN

INTERN struct ast_tags current_tags;

INTERN DREF struct ast *
(DCALL ast_annotations_apply)(struct ast_annotations *__restrict self,
                              /*inherit(always)*/DREF struct ast *__restrict input) {
 DREF struct ast *merge,**expr_v,*args;
 while (self->an_annoc) {
  struct ast *func = self->an_annov[self->an_annoc - 1].aa_func;
  if ((self->an_annov[self->an_annoc - 1].aa_flag & AST_ANNOTATION_FNOFUNC) ||
      (func->a_type != AST_OPERATOR) || (func->a_flag != OPERATOR_CALL) ||
      (func->a_operator.o_op1 == NULL)) {
   /* Invoke the annotation function using the current input:
    * >> input = aa_func(input); */
   expr_v = (struct ast **)Dee_Malloc(1 * sizeof(struct ast *));
   if unlikely(!expr_v) goto err_input;
   expr_v[0] = input; /* Inherit reference. */
   args = ast_setddi(ast_multiple(AST_FMULTIPLE_TUPLE,1,expr_v),&func->a_ddi);
   if unlikely(!args) { Dee_Free(expr_v); goto err_input; }
   merge = ast_operator2(OPERATOR_CALL,
                         AST_OPERATOR_FNORMAL,
                         func,args);
  } else {
   /* Invoke the annotation function using the current input:
    * >> input = aa_func.op0(input,aa_func.op1...); */
   struct ast *base;
   base = func->a_operator.o_op0;
   args = func->a_operator.o_op1;
#ifdef CONFIG_AST_IS_STRUCT
   if (args->a_refcnt == 1)
#else
   if (!DeeObject_IsShared(args))
#endif
   {
    if (args->a_type == AST_MULTIPLE &&
        args->a_flag == AST_FMULTIPLE_TUPLE) {
     expr_v = (DREF struct ast **)Dee_Realloc(args->a_multiple.m_astv,
                                             (args->a_multiple.m_astc + 1) *
                                              sizeof(DREF struct ast *));
     if unlikely(!expr_v) goto err_input;
     MEMMOVE_PTR(expr_v + 1,expr_v,args->a_multiple.m_astc);
     expr_v[0] = input; /* inherit reference. */
     args->a_multiple.m_astv = expr_v;
     ++args->a_multiple.m_astc;
     ast_incref(args);
     goto set_merge_from_inherit_args;
    }
   }
   merge = ast_setddi(ast_expand(args),&func->a_ddi);
   if unlikely(!merge) goto err_input;
   expr_v = (struct ast **)Dee_Malloc(2 * sizeof(struct ast *));
   if unlikely(!expr_v) goto err_input_merge;
   expr_v[0] = input; /* Inherit reference. */
   expr_v[1] = merge; /* Inherit reference. */
   args = ast_setddi(ast_multiple(AST_FMULTIPLE_TUPLE,2,expr_v),&func->a_ddi);
   if unlikely(!args) { Dee_Free(expr_v); goto err_input_merge; }
set_merge_from_inherit_args:
   merge = ast_operator2(OPERATOR_CALL,
                         AST_OPERATOR_FNORMAL,
                         base,args);
  }
  ast_decref_unlikely(args);
  if unlikely(!merge) goto err;
  input = ast_setddi(merge,&func->a_ddi);
  --self->an_annoc;
  ast_decref(self->an_annov[self->an_annoc].aa_func);
 }
 ast_annotations_free(self);
 return input;
err_input_merge:
 ast_decref(merge);
err_input:
 ast_decref(input);
err:
 ast_annotations_free(self);
 return NULL;
}


INTERN void
(DCALL ast_annotations_get)(struct ast_annotations *__restrict result) {
 memcpy(result,&current_tags.at_anno,sizeof(struct ast_annotations));
 memset(&current_tags.at_anno,0,sizeof(struct ast_annotations));
}
INTERN void
(DCALL ast_annotations_free)(struct ast_annotations *__restrict self) {
 if (!self->an_annov) return;
 while (self->an_annoc) {
  --self->an_annoc;
  ast_decref(self->an_annov[self->an_annoc].aa_func);
 }
 if (!current_tags.at_anno.an_annov) {
  memcpy(&current_tags.at_anno,self,sizeof(struct ast_annotations));
 } else if (!current_tags.at_anno.an_annoc &&
             current_tags.at_anno.an_annoa < self->an_annoa) {
  Dee_Free(current_tags.at_anno.an_annov);
  memcpy(&current_tags.at_anno,self,sizeof(struct ast_annotations));
 } else {
  Dee_Free(self->an_annov);
 }
}
INTERN int
(DCALL ast_annotations_clear)(struct ast_annotations *__restrict self) {
 if (!self->an_annov) goto done;
 while (self->an_annoc) {
  if (WARNAST(self->an_annov[self->an_annoc].aa_func,W_UNUSED_ANNOTATION))
      goto err;
  --self->an_annoc;
  ast_decref(self->an_annov[self->an_annoc].aa_func);
 }
 if (!current_tags.at_anno.an_annov) {
  memcpy(&current_tags.at_anno,self,sizeof(struct ast_annotations));
 } else if (!current_tags.at_anno.an_annoc &&
             current_tags.at_anno.an_annoa < self->an_annoa) {
  Dee_Free(current_tags.at_anno.an_annov);
  memcpy(&current_tags.at_anno,self,sizeof(struct ast_annotations));
 } else {
  Dee_Free(self->an_annov);
 }
done:
 return 0;
err:
 return -1;
}

INTERN int
(DCALL ast_annotations_add)(struct ast *__restrict func,
                            uint16_t flag) {
 ASSERT(current_tags.at_anno.an_annoc <= current_tags.at_anno.an_annoa);
 if (current_tags.at_anno.an_annoc >= current_tags.at_anno.an_annoa) {
  struct ast_annotation *new_anno;
  size_t new_alloc = current_tags.at_anno.an_annoa * 2;
  if (!new_alloc) new_alloc = 2;
  new_anno = (struct ast_annotation *)Dee_TryRealloc(current_tags.at_anno.an_annov,
                                                     new_alloc *
                                                     sizeof(struct ast_annotation));
  if unlikely(!new_anno) {
   new_alloc = current_tags.at_anno.an_annoc + 1;
   new_anno = (struct ast_annotation *)Dee_Realloc(current_tags.at_anno.an_annov,
                                                   new_alloc *
                                                   sizeof(struct ast_annotation));
   if unlikely(!new_anno) goto err;
  }
  current_tags.at_anno.an_annoa = new_alloc;
  current_tags.at_anno.an_annov = new_anno;
 }
 ast_incref(func);
 current_tags.at_anno.an_annov[current_tags.at_anno.an_annoc].aa_flag = flag;
 current_tags.at_anno.an_annov[current_tags.at_anno.an_annoc].aa_func = func;
 ++current_tags.at_anno.an_annoc;
 return 0;
err:
 return -1;
}

INTERN int (DCALL clear_current_tags)(void) {
 while (current_tags.at_anno.an_annoc) {
  struct ast_annotation *anno;
  anno = &current_tags.at_anno.an_annov[current_tags.at_anno.an_annoc - 1];
  if (WARNAST(anno->aa_func,W_UNUSED_ANNOTATION))
      goto err;
  ast_decref(anno->aa_func);
  --current_tags.at_anno.an_annoc;
 }
 unicode_printer_fini(&current_tags.at_doc);
 unicode_printer_init(&current_tags.at_doc);
 current_tags.at_expect      = 0;
 current_tags.at_class_flags = 0;
 return 0;
err:
 return -1;
}

PRIVATE int DCALL append_doc_string(void) {
 ASSERT(tok == TOK_STRING ||
       (tok == TOK_CHAR && !HAS(EXT_CHARACTER_LITERALS)));
 do {
  if unlikely(ast_decode_unicode_string(&current_tags.at_doc)) goto err;
  if unlikely(yield() < 0) goto err;
 } while (tok == TOK_STRING ||
         (tok == TOK_CHAR && !HAS(EXT_CHARACTER_LITERALS)));
 /* Append a line-feed at the end. */
 if unlikely(unicode_printer_putascii(&current_tags.at_doc,'\n'))
    goto err;
 return 0;
err:
 return -1;
}

INTERN int (DCALL parse_tags)(void) {
 if (tok == TOK_STRING ||
    (tok == TOK_CHAR && !HAS(EXT_CHARACTER_LITERALS))) {
  if unlikely(append_doc_string())
     goto err;
 } else if (tok == ':') {
  /* Implementation-specific / compile-time tags */
  char const *tag_name_str;
  size_t tag_name_len;
#define IS_TAG(x) \
       (tag_name_len == COMPILER_STRLEN(x) && !memcmp(tag_name_str,x,COMPILER_STRLEN(x)))
  if unlikely(yield() < 0) goto err;
  if (!TPP_ISKEYWORD(tok)) {
   if (WARN(W_COMPILER_TAG_EXPECTED_KEYWORD))
       goto err;
  } else {
   tag_name_str = token.t_kwd->k_name;
   tag_name_len = token.t_kwd->k_size;
   /* Trim leading/trailing underscores from tag names (prevent ambiguity when macros are used). */
   while (tag_name_len && *tag_name_str == '_') ++tag_name_str,--tag_name_len;
   while (tag_name_len && tag_name_str[tag_name_len-1] == '_') --tag_name_len;
   /**/ if (IS_TAG("truncate"))  current_tags.at_class_flags |= TP_FTRUNCATE;
   else if (IS_TAG("interrupt")) current_tags.at_class_flags |= TP_FINTERRUPT;
   else if (IS_TAG("likely"))    current_tags.at_expect      |= AST_FCOND_LIKELY;
   else if (IS_TAG("unlikely"))  current_tags.at_expect      |= AST_FCOND_UNLIKELY;
   else if (WARN(W_COMPILER_TAG_UNKNOWN,tag_name_len,tag_name_str)) goto err;
   if unlikely(yield() < 0) goto err;
  }
#undef IS_TAG
 } else {
  uint16_t flags; int error;
  DREF struct ast *annotation;
  flags = AST_ANNOTATION_FNORMAL;
  if (tok == '(') flags |= AST_ANNOTATION_FNOFUNC;
  annotation = ast_parse_expr(LOOKUP_SYM_NORMAL);
  if unlikely(!annotation) goto err;
  error = ast_annotations_add(annotation,flags);
  ast_decref_unlikely(annotation);
  if unlikely(error) goto err;
  goto done;
 }
done:
 return 0;
err:
 return -1;
}

INTERN int DCALL parse_tags_block(void) {
 while (tok == '@') {
  if unlikely(yield() < 0) goto err;
  if unlikely(parse_tags()) goto err;
 }
 return 0;
err:
 return -1;
}



DECL_END

#endif /* !GUARD_DEEMON_COMPILER_LEXER_TAG_C */
