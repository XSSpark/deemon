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
#ifndef GUARD_DEEMON_COMPILER_ASM_DIRECTIVES_C
#define GUARD_DEEMON_COMPILER_ASM_DIRECTIVES_C 1
#define _KOS_SOURCE 1

#include <deemon/api.h>
#include <deemon/error.h>
#include <deemon/none.h>
#include <deemon/bool.h>
#include <deemon/tuple.h>
#include <deemon/list.h>
#include <deemon/dict.h>
#include <deemon/hashset.h>
#include <deemon/int.h>
#include <deemon/string.h>
#include <deemon/compiler/ast.h>
#include <deemon/compiler/tpp.h>
#include <deemon/compiler/compiler.h>
#include <deemon/compiler/assembler.h>
#include <deemon/compiler/optimize.h>
#include <deemon/module.h>

#include "../../runtime/strings.h"

#include <string.h>
#include <limits.h>

#ifndef CONFIG_LANGUAGE_NO_ASM
DECL_BEGIN

#ifdef __INTELLISENSE__
INTERN struct user_assembler current_userasm;
INTERN struct asm_symtab     symtab;
#else
#define symtab current_userasm.ua_symtab
#endif

/* TODO: Implement case-folding and re-write the entire case-insensitive
 *       string/bytes API to use it. - E.g. `� -> ss'
 * Use this: https://unicode.org/Public/UNIDATA/CaseFolding.txt
 * Using it, also introduce a function `string.casefold()'
 */


/* Use libc functions for case-insensitive UTF-8 string compare when available. */
#if defined(__USE_KOS) && !defined(CONFIG_NO_CTYPE)
#define MEMCASEEQ(a,b,s) (memcasecmp(a,b,s) == 0)
#define STRCASEEQ(a,b)   (strcasecmp(a,b) == 0)
#elif defined(_MSC_VER) && !defined(CONFIG_NO_CTYPE)
#define MEMCASEEQ(a,b,s) (_memicmp(a,b,s) == 0)
#define STRCASEEQ(a,b)   (_stricmp(a,b) == 0)
#else
#define MEMCASEEQ(a,b,s)  dee_memcaseeq((uint8_t *)(a),(uint8_t *)(b),s)
LOCAL bool dee_memcaseeq(uint8_t const *a, uint8_t const *b, size_t s) {
 while (s--) {
  if (DeeUni_ToLower(*a) != DeeUni_ToLower(*b))
      return false;
  ++a;
  ++b;
 }
 return true;
}
#define STRCASEEQ(a,b)  dee_strcaseeq((char *)(a),(char *)(b))
LOCAL bool dee_strcaseeq(char *a, char *b) {
 while (*a) {
  if (DeeUni_ToLower(*a) != DeeUni_ToLower(*b))
      return false;
  ++a,++b;
 }
 return true;
}
#endif

#define IS_KWD(str) \
 (COMPILER_STRLEN(str) == token.t_kwd->k_size && \
  memcmp(token.t_kwd->k_name,str,sizeof(str)-sizeof(char)) == 0)
#define IS_KWD_NOCASE(str) \
 (COMPILER_STRLEN(str) == token.t_kwd->k_size && \
  MEMCASEEQ(token.t_kwd->k_name,str,sizeof(str)-sizeof(char)))
                                                                         

PRIVATE DREF DeeObject *FCALL do_parse_constant(void) {
 DREF DeeAstObject *const_ast;
 DREF DeeObject *result;
 if unlikely(scope_push()) goto err;
 const_ast = ast_parse_expression(LOOKUP_SYM_NORMAL);
 scope_pop();
 if unlikely(!const_ast) goto err;
 /* Optimize the constant branch to allow for constant propagation. */
 if unlikely(ast_optimize(const_ast,true))
    goto err_const_ast;
 if (const_ast->ast_type == AST_CONSTEXPR &&
     asm_allowconst(const_ast->ast_constexpr)) {
  result = const_ast->ast_constexpr;
 } else {
  if (WARN(W_UASM_EXPECTED_CONSTANT_EXPRESSION_FOR_PSEUDO_INSTRUCTION))
      goto err_const_ast;
  result = Dee_None;
 }
 Dee_Incref(result);
 Dee_Decref(const_ast);
 return result;
err_const_ast:
 Dee_Decref(const_ast);
err:
 return NULL;
}

PRIVATE struct asm_sym *FCALL do_parse_symbol_for_op(int wid) {
 struct asm_intexpr expr;
 if unlikely(uasm_parse_intexpr(&expr,UASM_INTEXPR_FHASSP))
    goto err;
 if (!expr.ie_sym) {
  expr.ie_sym = asm_newsym();
  asm_defsym(expr.ie_sym);
warn_symbol:
  if (WARN(wid))
      goto err;
 } else {
  if (expr.ie_rel != ASM_OVERLOAD_FRELABS &&
      expr.ie_rel != (uint16_t)-1)
      goto warn_symbol;
 }
 if (expr.ie_val != 0) {
  expr.ie_val = 0;
  goto warn_symbol;
 }
 return expr.ie_sym;
err:
 return NULL;
}

PRIVATE struct asm_sym *FCALL do_parse_symbol_for_except(void) {
 return do_parse_symbol_for_op(W_UASM_EXCEPT_NEED_ABSOLUTE_SYMBOL);
}
PRIVATE struct asm_sym *FCALL do_parse_symbol_for_reloc(void) {
 return do_parse_symbol_for_op(W_UASM_RELOC_NEED_ABSOLUTE_SYMBOL);
}


struct reloc_type {
    char     rt_name[12];
};

PRIVATE struct reloc_type const reloc_db[] = {
    /* [R_DMN_NONE]     = */ { "NONE" },
    /* [R_DMN_STATIC16] = */ { "STATIC16" },
    /* [R_DMN_ABSS8]    = */ { "ABSS8" },
    /* [R_DMN_ABS16]    = */ { "ABS16" },
    /* [R_DMN_ABS32]    = */ { "ABS32" },
    /* [R_DMN_DISP8]    = */ { "DISP8" },
    /* [R_DMN_DISP16]   = */ { "DISP16" },
    /* [R_DMN_DISP32]   = */ { "DISP32" },
    /* [R_DMN_STCK8]    = */ { "STCK8" },
    /* [R_DMN_STCK16]   = */ { "STCK16" },
    /* [R_DMN_STCKAS8]  = */ { "STCKAS8" },
    /* [R_DMN_STCKA16]  = */ { "STCKA16" },
    /* [R_DMN_DELHAND]  = */ { "DELHAND" },
};
STATIC_ASSERT(COMPILER_LENOF(reloc_db) == R_DMN_COUNT);

PRIVATE uint16_t FCALL
get_reloc_by_name(char const *__restrict name) {
 uint16_t result; size_t namelen;
 if (memcmp(name,"R_DMN_",6*sizeof(char)) == 0)
     name += 6;
 namelen = strlen(name);
 if unlikely(namelen >= COMPILER_LENOF(reloc_db[0].rt_name))
    return R_DMN_COUNT;
 for (result = 0; result < R_DMN_COUNT; ++result) {
  if (memcmp(reloc_db[result].rt_name,name,namelen*sizeof(char)) == 0)
      break;
 }
 return result;
}



/*  ================================  */
/*  === User-assembly directives ===  */
/*  ================================  */
INTERN int FCALL
uasm_parse_directive(void) {
 struct TPPKeyword *name;
 name = uasm_parse_symnam();
 if unlikely(!name) goto err;
 if (tok == ':') {
  struct TPPKeyword *label_name;
  char backup; struct asm_sym *label;
  /* Actually a label definition. */
  if unlikely(yield() < 0) goto err;
  /* Cheat a bit... */
  backup = name->k_name[-1];
  name->k_name[-1] = '.';
  label_name = TPPLexer_LookupKeyword(name->k_name-1,name->k_size+1,1);
  name->k_name[-1] = backup;
  if unlikely(!label_name) goto err;
  label = uasm_symbol(label_name);
  if unlikely(!label) goto err;
  /* Make sure that the symbol hasn't already been defined. */
  if unlikely(ASM_SYM_DEFINED(label)) {
   if (WARN(W_UASM_SYMBOL_ALREADY_DEFINED,label_name->k_name))
       goto err;
  } else {
   uasm_defsym(label);
  }
  goto done_continue;
 }

 /* Handle assembly directives. */
#define NAMEISKWD(x) \
  (name->k_size == COMPILER_STRLEN(x) && \
   MEMCASEEQ(name->k_name,x,sizeof(x)-sizeof(char)))
 /* Stack alignment opcodes. */
 if (NAMEISKWD("adjstack")) goto do_handle_adjstack;

 /* Code execution mode control opcodes. */
 if (NAMEISKWD("lenient")) goto do_handle_lenient;
 if (NAMEISKWD("copyable")) goto do_handle_copyable;
 if (NAMEISKWD("assembly")) goto do_handle_assembly;
 if (NAMEISKWD("heapframe")) goto do_handle_heapframe;

 /* Debug information control opcodes. */
 if (NAMEISKWD("ddi")) goto do_handle_ddi;

 /* Exception handler control opcodes. */
 if (NAMEISKWD("except")) goto do_handle_except;

 /* Manual relocation opcodes. */
 if (NAMEISKWD("reloc")) goto do_handle_reloc;

 /* Raw text emission opcodes. */
 if (NAMEISKWD("byte")) goto do_handle_byte;
 if (NAMEISKWD("1byte")) goto do_handle_byte;
 if (NAMEISKWD("word")) goto do_handle_word;
 if (NAMEISKWD("short")) goto do_handle_word;
 if (NAMEISKWD("2byte")) goto do_handle_word;
 if (NAMEISKWD("int")) goto do_handle_dword;
 if (NAMEISKWD("long")) goto do_handle_dword;
 if (NAMEISKWD("4byte")) goto do_handle_dword;
 if (NAMEISKWD("dword")) goto do_handle_dword;
 if (NAMEISKWD("qword")) goto do_handle_qword;
 if (NAMEISKWD("quad")) goto do_handle_qword;
 if (NAMEISKWD("8byte")) goto do_handle_qword;

 /* Unknown directive... (Discard the remainder of the line) */
 if (WARN(W_UASM_UNKNOWN_DIRECTIVE,name))
     goto err;
 while (tok > 0 && tok != ';' && tok != '\n')
    if (yield() < 0) goto err;
#undef NAMEISKWD
done:
 return 0;
done_continue:
 return 1;
err:
 return -1;


do_handle_lenient:
  /* `.lenient' -- Simply set the FLENIENT bit of the resulting code object. */
  current_basescope->bs_flags |= CODE_FLENIENT;
  goto done;

do_handle_copyable:
  /* `.copyable' -- Simply set the FCOPYABLE bit of the resulting code object. */
  current_basescope->bs_flags |= CODE_FCOPYABLE;
  goto done;

do_handle_assembly:
  /* `.assembly' -- Simply set the FASSEMBLY bit of the resulting code object. */
  current_basescope->bs_flags |= CODE_FASSEMBLY;
  goto done;

  /* `.heapframe' -- Simply set the FHEAPFRAME bit of the resulting code object. */
do_handle_heapframe:
  current_basescope->bs_flags |= CODE_FHEAPFRAME;
  goto done;

 {
  struct TPPKeyword *reloc_name;
  struct asm_sym *reloc_sym;
  uint16_t reloc_type;
  uint16_t reloc_value;
do_handle_reloc:
  /* `.reloc ., <name> [, <symbol> [, <value>]]'  */
  if likely(tok == '.') {
   if unlikely(yield() < 0) goto err;
  } else {
   struct asm_intexpr expr;
   if (WARN(W_UASM_RELOC_NEED_DOT))
       goto err;
   if (uasm_parse_intexpr(&expr,UASM_INTEXPR_FNORMAL))
       goto err;
  }
  if unlikely(likely(tok == ',') ? (yield() < 0) :
              WARN(W_EXPECTED_COMMA))
     goto err;
  reloc_name  = uasm_parse_symnam();
  reloc_sym   = NULL;
  reloc_value = 0;
  reloc_type = get_reloc_by_name(reloc_name->k_name);
  /* Check if the relocation name could be determined. */
  if unlikely(reloc_type == R_DMN_COUNT) {
   if (WARN(W_UASM_RELOC_UNKNOWN_NAME,reloc_name->k_name))
       goto err;
   reloc_type = R_DMN_NONE;
  }
  if (tok == ',') {
   if unlikely(yield() < 0) goto err;
   /* Parse the relocation symbol. */
   reloc_sym = do_parse_symbol_for_reloc();
   if unlikely(!reloc_sym) goto err;
   if (tok == ',') {
    struct asm_intexpr rval;
    if unlikely(yield() < 0) goto err;
    /* Parse the relocation value. */
    if (uasm_parse_intexpr(&rval,UASM_INTEXPR_FNORMAL))
        goto err;
    if (rval.ie_sym && WARN(W_UASM_RELOC_VALUE_NOT_A_SYMBOL))
        goto err;
    reloc_value = (uint16_t)rval.ie_val;
   }
  }

  /* Make sure that a relocation making use of a symbol actually has one. */
  if (REL_HASSYM(reloc_type) && !reloc_sym) {
   if (WARN(W_UASM_RELOC_NAME_NEEDS_SYMBOL))
       goto err;
   reloc_sym = asm_newsym();
   asm_defsym(reloc_sym);
  }
  /* Emit the relocation. */
  if (asm_putrel(reloc_type|R_DMN_FUSER,reloc_sym,reloc_value))
      goto err;
  goto done;
 }


 {
  struct asm_sym *except_start;
  struct asm_sym *except_end;
  struct asm_sym *except_entry;
  DREF DeeTypeObject *except_mask;
  uint16_t except_flags;
  struct asm_exc *except;
do_handle_except:
  /* `.except <start>, <end>, <entry>, [',' ~~ <tags>...]'
   * tags:
   *   - `[@]finally'     -- Set the `EXCEPTION_HANDLER_FFINALLY' bit.
   *   - `[@]interrupt'   -- Set the `EXCEPTION_HANDLER_FINTERPT' bit.
   *   - `[@]handled'     -- Set the `EXCEPTION_HANDLER_FHANDLED' bit.
   *   - `[@]mask(const)' -- Use `const' as exception handler mask.
   */
  except_start = do_parse_symbol_for_except();
  if unlikely(likely(tok == ',') ? (yield() < 0) : WARN(W_EXPECTED_COMMA)) goto err;
  except_end = do_parse_symbol_for_except();
  if unlikely(likely(tok == ',') ? (yield() < 0) : WARN(W_EXPECTED_COMMA)) goto err;
  except_entry = do_parse_symbol_for_except();
  except_flags = EXCEPTION_HANDLER_FNORMAL;
  except_mask = NULL;
  while (tok == ',') {
   char const *tag_start,*tag_end;
   if unlikely(yield() < 0) goto except_err;
   if (tok == '@' && unlikely(yield() < 0)) goto except_err;
   if (!TPP_ISKEYWORD(tok)) {
except_unknown_tag:
    if (WARN(W_UASM_EXCEPT_UNKNOWN_TAG))
        goto except_err;
    break;
   }
   tag_start = token.t_kwd->k_name;
   tag_end   = tag_start + token.t_kwd->k_size;
   while (tag_start < tag_end && tag_start[0] == '_') ++tag_start;
   while (tag_end > tag_start && tag_end[-1] == '_') --tag_end;
#define IS_TAG(x) \
       (COMPILER_STRLEN(x) == (tag_end-tag_start) && \
        MEMCASEEQ(tag_start,x,sizeof(x)-sizeof(char)))
   /**/ if (IS_TAG("finally"))   except_flags |= EXCEPTION_HANDLER_FFINALLY;
   else if (IS_TAG("interrupt")) except_flags |= EXCEPTION_HANDLER_FINTERPT;
   else if (IS_TAG("handled"))   except_flags |= EXCEPTION_HANDLER_FHANDLED;
   else if (IS_TAG("mask")) {
    DREF DeeObject *mask;
    if unlikely(yield() < 0) goto except_err;
    if unlikely(likely(tok == '(') ? (yield() < 0) : WARN(W_EXPECTED_LPAREN)) goto except_err;
    mask = do_parse_constant();
    if (DeeNone_Check(mask)) {
     /* Special case: `mask(none)' is the same as `mask(type none)' */
     Dee_Decref(mask);
     mask = (DREF DeeObject *)&DeeNone_Type;
     Dee_Incref(mask);
    }
    if unlikely(DeeObject_AssertType(mask,&DeeType_Type) ||
               (likely(tok == ')') ? (yield() < 0) : WARN(W_EXPECTED_RPAREN)))
    { Dee_Decref(mask); goto except_err; }
    Dee_XDecref(except_mask);
    except_mask = (DREF DeeTypeObject *)mask;
    continue;
   } else {
    goto except_unknown_tag;
   }
   if unlikely(yield() < 0) goto except_err;
#undef IS_TAG
  }
  except = asm_newexc();
  if unlikely(!except) goto except_err;
  except->ex_mask  = except_mask; /* Inherit reference. */
  except->ex_start = except_start;
  except->ex_end   = except_end;
  except->ex_addr  = except_entry;
  except->ex_flags = except_flags;
  except_entry->as_stck;
  ++except_start->as_used;
  ++except_end->as_used;
  ++except_entry->as_used;


  goto done;
except_err:
  Dee_XDecref(except_mask);
  goto err;
 }


 {
  unsigned int width;
  struct asm_intexpr value;
do_handle_byte:  width = 0x01; goto do_emit_memory;
do_handle_word:  width = 0x02; goto do_emit_memory;
do_handle_dword: width = 0x04; goto do_emit_memory;
do_handle_qword: width = 0x08; /*goto do_emit_memory;*/
do_emit_memory:
  /* Disable PEEPHOLE and set the ASSEMBLY code bit. */
  current_assembler.a_flag &= ~(ASM_FPEEPHOLE);
  current_basescope->bs_flags |= CODE_FASSEMBLY;
  for (;;) {
   if unlikely(uasm_parse_intexpr(&value,UASM_INTEXPR_FHASSP))
      goto err;
   if (value.ie_sym) {
    /* Emit a relocation. */
    uint16_t relo_type;
    switch (width | (value.ie_rel << 8)) {
    case (ASM_OVERLOAD_FRELABS << 8) | 8:
    case (ASM_OVERLOAD_FRELABS << 8) | 4:
     relo_type = R_DMN_ABS32|R_DMN_FUSER;
     break;
    case (ASM_OVERLOAD_FRELABS << 8) | 2:
     relo_type = R_DMN_ABS16|R_DMN_FUSER;
     break;
    case (ASM_OVERLOAD_FRELABS << 8) | 1:
     relo_type = R_DMN_ABSS8|R_DMN_FUSER;
     break;
    case (ASM_OVERLOAD_FRELDSP << 8) | 8:
    case (ASM_OVERLOAD_FRELDSP << 8) | 4:
     relo_type = R_DMN_DISP32|R_DMN_FUSER;
     break;
    case (ASM_OVERLOAD_FRELDSP << 8) | 2:
     relo_type = R_DMN_DISP16|R_DMN_FUSER;
     break;
    case (ASM_OVERLOAD_FRELDSP << 8) | 1:
     relo_type = R_DMN_DISP8|R_DMN_FUSER;
     break;
    case (ASM_OVERLOAD_FSTKABS << 8) | 8:
    case (ASM_OVERLOAD_FSTKABS << 8) | 4:
    case (ASM_OVERLOAD_FSTKABS << 8) | 2:
     relo_type = R_DMN_STCKA16|R_DMN_FUSER;
     break;
    case (ASM_OVERLOAD_FSTKABS << 8) | 1:
     relo_type = R_DMN_STCKAS8|R_DMN_FUSER;
     break;
    case (ASM_OVERLOAD_FSTKDSP << 8) | 8:
    case (ASM_OVERLOAD_FSTKDSP << 8) | 4:
    case (ASM_OVERLOAD_FSTKDSP << 8) | 2:
     relo_type = R_DMN_STCK16|R_DMN_FUSER;
     goto check_invalid_stack_and_adjust;
    case (ASM_OVERLOAD_FSTKDSP << 8) | 1:
     relo_type = R_DMN_STCK8|R_DMN_FUSER;
check_invalid_stack_and_adjust:
     if (current_assembler.a_userasm.ua_flags & USER_ASM_FSTKINV) {
      DeeError_Throwf(&DeeError_CompilerError,
                      "Cannot use relative SP addressing while the stack is in an undefined state");
      goto err;
     }
     /* Adjust the immediate value to force it to
      * become relative to the current SP location. */
     value.ie_val += current_assembler.a_stackcur;
     break;
    default: __builtin_unreachable();
    }
    if (asm_putrel(relo_type,value.ie_sym,0))
        goto err;
   }
   /* Emit the raw data word. */
   switch (width) {
   case 1:
    if ((value.ie_val < INT8_MIN || value.ie_val > INT8_MAX) &&
         WARN(W_UASM_TRUNCATED_TO_FIT)) goto err;
    if (asm_put_data8((uint8_t)(uint64_t)value.ie_val))
        goto err;
    break;
   case 2:
    if ((value.ie_val < INT16_MIN || value.ie_val > INT16_MAX) &&
         WARN(W_UASM_TRUNCATED_TO_FIT)) goto err;
    if (asm_put_data16((uint16_t)(uint64_t)value.ie_val))
        goto err;
    break;
   case 4:
    if ((value.ie_val < INT32_MIN || value.ie_val > INT32_MAX) &&
         WARN(W_UASM_TRUNCATED_TO_FIT)) goto err;
    if (asm_put_data32((uint32_t)(uint64_t)value.ie_val))
        goto err;
    break;
   case 8:
    if (asm_put_data64((uint64_t)value.ie_val))
        goto err;
    break;
   default: __builtin_unreachable();
   }
   if (tok != ',') break;
   if unlikely(yield() < 0) goto err;
  }
  goto done;
 }

 {
  DREF DeeObject *filename;
  DREF DeeObject *line;
  DREF DeeObject *col;
do_handle_ddi:
  /* `.ddi <line:imm>' */
  /* `.ddi <line:imm>, <col:imm>' */
  /* `.ddi <filename:string>, <line:imm>' */
  /* `.ddi <filename:string>, <line:imm>, <col:imm>' */
  filename = do_parse_constant();
  if unlikely(!filename) goto err;
  if (tok != ',') {
   /* `.ddi <line:imm>' */
   line     = filename;
   filename = NULL;
   col      = NULL;
  } else {
   if unlikely(yield() < 0)
      goto err_ddi_filename;
   line = do_parse_constant();
   if unlikely(!line) goto err_ddi_filename;
   if (tok != ',') {
    if (DeeString_Check(filename)) {
     /* `.ddi <filename:string>, <line:imm>' */
     col = NULL;
    } else {
     /* `.ddi <line:imm>, <col:imm>' */
     col      = line;
     line     = filename;
     filename = NULL;
    }
   } else {
    /* `.ddi <filename:string>, <line:imm>, <col:imm>' */
    if unlikely(yield() < 0) goto err_ddi_line;
    col = do_parse_constant();
    if unlikely(!col) goto err_ddi_line;
   }
  }
  /* Make sure that the filename is a string. */
  if (filename && DeeObject_AssertTypeExact(filename,&DeeString_Type))
      goto err_ddi_col;
  ASSERT(line != NULL);
  if (!(current_assembler.a_flag&ASM_FNODDI)) {
   DREF struct TPPFile *file;
   struct ddi_checkpoint *ddi;
   struct asm_sym *sym;
   if (filename) {
    file = ddi_newfile(DeeString_STR(filename),
                       DeeString_SIZE(filename));
   } else if (current_assembler.a_ddi.da_checkc) {
    file = current_assembler.a_ddi.da_checkv[
           current_assembler.a_ddi.da_checkc-1].
           dc_loc.l_file;
   } else {
#if 1
    file = ddi_newfile("",0);
#else
    file = token.t_file;
#endif
   }
   if (current_assembler.a_ddi.da_checkc) {
    ddi = &current_assembler.a_ddi.da_checkv[
           current_assembler.a_ddi.da_checkc-1];
    if (ASM_SYM_DEFINED(ddi->dc_sym) &&
        ddi->dc_sym->as_sect == asm_getcur() &&
        ddi->dc_sym->as_addr == asm_ip()) {
     /* Update the previous checkpoint. */
     sym = ddi->dc_sym;
     ASSERT(sym->as_used >= 1);
     if (sym->as_used > 1) {
      if unlikely((sym = asm_newsym()) == NULL)
         return -1;
      --ddi->dc_sym->as_used;
      ddi->dc_sym = sym;
      asm_defsym(sym);
     }
     sym->as_stck = current_assembler.a_stackcur;
     goto ddi_update;
    }
   }
   if unlikely((sym = asm_newsym()) == NULL ||
               (ddi = asm_newddi()) == NULL)
      return -1;
   /* Simply define the symbol at the current text position.
    * NOTE: Its actual address may change during later assembly phases. */
   asm_defsym(sym);
   ddi->dc_sym = sym;
   ++sym->as_used; /* Track use of this symbol by DDI information. */
ddi_update:

   /* Fill in the checkpoint. */
   ddi->dc_loc.l_file = file;
   ddi->dc_sp = current_assembler.a_stackcur;
   if (DeeObject_AsInt(line,&ddi->dc_loc.l_line))
       goto err_ddi_col;
   --ddi->dc_loc.l_line;
   if (!col) ddi->dc_loc.l_col = 0;
   else {
    if (DeeObject_AsInt(col,&ddi->dc_loc.l_col))
        goto err_ddi_col;
    if (ddi->dc_loc.l_col) --ddi->dc_loc.l_col;
   }
#ifndef NDEBUG
   sym->as_file = file->f_name;
   sym->as_line = ddi->dc_loc.l_line+1;
#endif

   /* Save the current text position to discard early uses of the same checkpoint. */
   current_assembler.a_ddi.da_last  = sym->as_addr;
   current_assembler.a_ddi.da_slast = current_assembler.a_curr;
  }
  Dee_XDecref(col);
  Dee_Decref(line);
  Dee_XDecref(filename);
  goto done;
err_ddi_col:
  Dee_XDecref(col);
err_ddi_line:
  Dee_XDecref(line);
err_ddi_filename:
  Dee_XDecref(filename);
  goto err;
 }


 {
  struct asm_intexpr new_depth;
do_handle_adjstack:
  /* `.adjstack <imm32>'
   * Adjust/set the virtual stack-depth as seen by the assembler.
   * Doing this usually only makes sense if the previous instruction
   * doesn't return, as well as no symbols having been defined since:
   * >>     push $1f.IP
   * >>     push $1f.SP
   * >>     jmp  pop, #pop
   * >> .setstack 42  // `jmp' doesn't return and no label had been defined
   * >>               // between this point and the end of it's instruction.
   * >> 1:  print @"The current stack depth is 42", nl
   */
  if unlikely(uasm_parse_intexpr(&new_depth,UASM_INTEXPR_FHASSP))
     goto err;
  if unlikely(new_depth.ie_sym &&
              WARN(W_UASM_STACK_DEPTH_DEPENDS_ON_SYMBOL_EXPRESSION))
     goto err;
  if unlikely((new_depth.ie_val < 0 || new_depth.ie_val > UINT16_MAX) &&
               WARN(W_UASM_ILLEGAL_STACK_DEPTH,(long)new_depth.ie_val))
     goto err;
  /* Special case: If nothing changed, don't even sweat it. */
  if (current_assembler.a_stackcur == (uint16_t)new_depth.ie_val &&
    !(current_userasm.ua_mode & USER_ASM_FSTKINV))
      goto done;
  /* Warn if the previous instruction does actually return. */
  if unlikely(!asm_isnoreturn(current_userasm.ua_lasti,current_basescope->bs_flags)) {
   if (WARN(W_UASM_POTENTIALLY_INCONSISTENT_STACK_DEPTH_ADJUSTMENT))
       goto err;
#if 1
   /* Disable peephole, so-as not to confuse it. */
   current_assembler.a_flag &= ~(ASM_FPEEPHOLE);
#endif
  }

#if 0 /* XXX: This shouldn't break peephole... */
  /* Must always disable PEEPHOLE because we can't
   * prevent code like this (which would break peephole):
   * >>    adjstack #0
   * >>    jmp   1f
   * >>.adjstack 42
   * >>1:  // The stack isn't actually aligned to #42, but we're supposed
   * >>    // to belive that it is. However if it isn't, peephole would
   * >>    // crash and take deemon down with it.
   */
  current_assembler.a_flag &= ~(ASM_FPEEPHOLE);
#endif
  /* Make sure to set the assembly flag if the stack-depth was tinkered with. */
  current_basescope->bs_flags |= CODE_FASSEMBLY;
  /* Set the new instruction depth. */
  current_assembler.a_stackcur = (uint16_t)new_depth.ie_val;
  if (current_assembler.a_stackmax < current_assembler.a_stackcur)
      current_assembler.a_stackmax = current_assembler.a_stackcur;
  /* The stack is no longer undefined. */
  current_userasm.ua_mode &= ~(USER_ASM_FSTKINV);
  goto done;
 }
}

DECL_END
#endif /* !CONFIG_LANGUAGE_NO_ASM */

#endif /* !GUARD_DEEMON_COMPILER_ASM_DIRECTIVES_C */
