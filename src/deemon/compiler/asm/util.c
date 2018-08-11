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
#ifndef GUARD_DEEMON_COMPILER_ASM_UTIL_C
#define GUARD_DEEMON_COMPILER_ASM_UTIL_C 1
#define _KOS_SOURCE 1

#include <deemon/api.h>
#include <deemon/bool.h>
#include <deemon/object.h>
#include <deemon/code.h>
#include <deemon/rodict.h>
#include <deemon/roset.h>
#include <deemon/module.h>
#include <deemon/dict.h>
#include <deemon/hashset.h>
#include <deemon/error.h>
#include <deemon/float.h>
#include <deemon/none.h>
#include <deemon/asm.h>
#include <deemon/super.h>
#include <deemon/int.h>
#include <deemon/dict.h>
#include <deemon/list.h>
#include <deemon/tuple.h>
#include <deemon/compiler/ast.h>
#include <deemon/compiler/compiler.h>
#include <deemon/compiler/assembler.h>
#include <deemon/class.h>

DECL_BEGIN

INTERN int DCALL
asm_gpush_stack(uint16_t absolute_stack_addr) {
 uint16_t offset;
 ASSERTF(current_assembler.a_stackcur > absolute_stack_addr,
         "Invalid stack address");
 offset = (current_assembler.a_stackcur-1)-absolute_stack_addr;
 return offset == 0 ? asm_gdup() : asm_gdup_n(offset-1);
}
INTERN int DCALL
asm_gpop_stack(uint16_t absolute_stack_addr) {
 uint16_t offset;
 ASSERTF(current_assembler.a_stackcur > absolute_stack_addr,
         "Invalid stack address");
 offset = (current_assembler.a_stackcur-1)-absolute_stack_addr;
 /* XXX: `offset == 0' doesn't ~really~ make sense as that would
  *       mean to pop a stack value into itself, then discard it.
  *       Though we still allow doing this... */
 return offset == 0 ? asm_gpop() : asm_gpop_n(offset-1);
}
INTERN int DCALL
asm_gadjstack(int16_t offset) {
 switch (offset) {
 case -1: return asm_gpop();
 case  0: return 0;
 case  1: return asm_gpush_none();
 }
 return _asm_gadjstack(offset);
}
INTERN int DCALL
asm_gsetstack(uint16_t absolute_stack_size) {
 return asm_gadjstack((int16_t)absolute_stack_size -
                      (int16_t)current_assembler.a_stackcur);
}

INTERN int DCALL asm_glrot(uint16_t num_slots) {
 if (num_slots <= 1) return 0;
 if (num_slots == 2) return asm_gswap();
 return _asm_glrot(num_slots-3);
}
INTERN int DCALL asm_grrot(uint16_t num_slots) {
 if (num_slots <= 1) return 0;
 if (num_slots == 2) return asm_gswap();
 return _asm_grrot(num_slots-3);
}

INTERN int DCALL
asm_gpush_u32(uint32_t value) {
 DREF DeeObject *obj;
 int32_t cid;
 obj = DeeInt_NewU32(value);
 if unlikely(!obj) goto err;
 cid = asm_newconst(obj);
 Dee_Decref(obj);
 if unlikely(cid < 0) goto err;
 return asm_gpush_const((uint16_t)cid);
err:
 return -1;
}
INTERN int DCALL
asm_gpush_s32(int32_t value) {
 DREF DeeObject *obj;
 int32_t cid;
 obj = DeeInt_NewS32(value);
 if unlikely(!obj) goto err;
 cid = asm_newconst(obj);
 Dee_Decref(obj);
 if unlikely(cid < 0) goto err;
 return asm_gpush_const((uint16_t)cid);
err:
 return -1;
}


INTERN DREF DeeObject *DCALL
tuple_getrange_i(DeeTupleObject *__restrict self,
                 dssize_t begin, dssize_t end);

INTERN DREF DeeObject *DCALL
list_getrange_as_tuple(DeeListObject *__restrict self,
                       dssize_t begin, dssize_t end) {
 DREF DeeTupleObject *result;
 size_t i;
again:
 DeeList_LockRead(self);
 if unlikely(begin < 0) begin += self->l_size;
 if unlikely(end < 0) end += self->l_size;
 if unlikely((size_t)begin >= self->l_size ||
             (size_t)begin >= (size_t)end) {
  /* Empty list. */
  DeeList_LockEndRead(self);
  return DeeList_New();
 }
 if unlikely((size_t)end > self->l_size)
              end = (dssize_t)self->l_size;
 end -= begin;
 ASSERT(end != 0);
 result = (DREF DeeTupleObject *)DeeObject_TryMalloc(offsetof(DeeTupleObject,t_elem)+
                                                    (size_t)end*sizeof(DREF DeeObject *));
 if unlikely(!result) {
  DeeList_LockEndRead(self);
  if (Dee_CollectMemory(offsetof(DeeTupleObject,t_elem)+
                       (size_t)end*sizeof(DREF DeeObject *)))
      goto again;
  return NULL;
 }
 /* Copy vector elements. */
 for (i = 0; i < (size_t)end; ++i) {
  result->t_elem[i] = self->l_elem[(size_t)begin+i];
  Dee_Incref(result->t_elem[i]);
 }
 DeeList_LockEndRead(self);
 DeeObject_Init(result,&DeeTuple_Type);
 result->t_size = (size_t)end;
 return (DREF DeeObject *)result;
}


/* NOTE: _Always_ inherits references to `key' and `value' */
INTERN void DCALL
rodict_insert_nocheck(DeeRoDictObject *__restrict self,
                      dhash_t hash,
                      DREF DeeObject *__restrict key,
                      DREF DeeObject *__restrict value) {
 size_t i,perturb; struct rodict_item *item;
 perturb = i = hash & self->rd_mask;
 for (;; i = RODICT_HASHNX(i,perturb),RODICT_HASHPT(perturb)) {
  item = &self->rd_elem[i & self->rd_mask];
  if (!item->di_key) break;
 }
 /* Fill in the item. */
 item->di_hash  = hash;
 item->di_key   = key;   /* Inherit reference. */
 item->di_value = value; /* Inherit reference. */
}


/* NOTE: _Always_ inherits references to `key' */
INTERN void DCALL
roset_insert_nocheck(DeeRoSetObject *__restrict self,
                     dhash_t hash,
                     DREF DeeObject *__restrict key) {
 size_t i,perturb; struct roset_item *item;
 perturb = i = hash & self->rs_mask;
 for (;; i = ROSET_HASHNX(i,perturb),ROSET_HASHPT(perturb)) {
  item = &self->rs_elem[i & self->rs_mask];
  if (!item->si_key) break;
 }
 /* Fill in the item. */
 item->si_hash = hash;
 item->si_key  = key;   /* Inherit reference. */
}

INTDEF DeeObject dict_dummy;
#define SIZEOF_RODICT(mask)        \
       (offsetof(DeeRoDictObject,rd_elem)+(((mask)+1)*sizeof(struct rodict_item)))
#define RODICT_INITIAL_MASK         0x1f

#define SIZEOF_ROSET(mask)        \
       (offsetof(DeeRoSetObject,rs_elem)+(((mask)+1)*sizeof(struct roset_item)))
#define ROSET_INITIAL_MASK          0x1f



#define STACK_PACK_THRESHOLD 16
INTERN int (DCALL asm_gpush_constexpr)(DeeObject *__restrict value) {
 int32_t cid;
 ASSERT_OBJECT(value);
 if (DeeBool_Check(value)) {
  /* Push a boolean builtin singleton. */
  return (DeeBool_IsTrue(value)
        ? asm_gpush_true()
        : asm_gpush_false());
 }
 if (DeeNone_Check(value))
     return asm_gpush_none();
 if (DeeSuper_Check(value)) {
  /* Construct a super-wrapper. */
  if (asm_gpush_constexpr((DeeObject *)DeeSuper_TYPE(value))) goto err;
  if (asm_gpush_constexpr((DeeObject *)DeeSuper_SELF(value))) goto err;
  return asm_gsuper();
 }
 if (DeeTuple_Check(value)) {
  /* Special case for tuples: If only constant expressions are
   * contained, then we can push the tuple as a constant expression, too. */
  size_t start,end,len = DeeTuple_SIZE(value);
  bool is_first_part;
  /* Special case: empty tuple. */
  if (!len) return asm_gpack_tuple(0);
  /* Check if the tuple can be pushed as a constant expression. */
  for (start = 0; start < len; ++start) {
   if (!asm_allowconst(DeeTuple_GET(value,start)))
        goto push_tuple_parts;
  }
  goto push_constexpr;
push_tuple_parts:
  /* If not, push constant parts as individual segments. */
  end = start,start = 0;
  is_first_part = start == end;
  if (!is_first_part) {
   DREF DeeObject *subrange;
   subrange = tuple_getrange_i((DeeTupleObject *)value,
                               (dssize_t)start,(dssize_t)end);
   if unlikely(!subrange) goto err;
   cid = asm_newconst(subrange);
   Dee_Decref(subrange);
   if unlikely(cid < 0) goto err;
   if (asm_gpush_const((uint16_t)cid)) goto err;
  }
  while (end < len) {
   uint16_t pack_length = 0;
   while (end < len && pack_length < STACK_PACK_THRESHOLD &&
         !asm_allowconst(DeeTuple_GET(value,end))) {
    if (asm_gpush_constexpr(DeeTuple_GET(value,end))) goto err;
    ++pack_length,++end;
   }
   ASSERT(pack_length != 0);
   ASSERT(pack_length <= STACK_PACK_THRESHOLD);
   if (is_first_part) {
    is_first_part = false;
    if (asm_gpack_tuple(pack_length)) goto err;
   } else {
    if (asm_gextend(pack_length)) goto err;
   }
   start = end;
   /* Collect constant parts. */
   while (end < len && asm_allowconst(DeeTuple_GET(value,end))) ++end;
   if (end != start) {
    DREF DeeObject *subrange;
    if (end == start+1) {
     if (asm_gpush_constexpr(DeeTuple_GET(value,start))) goto err;
     if (asm_gextend(1)) goto err;
    } else {
     subrange = tuple_getrange_i((DeeTupleObject *)value,
                                 (dssize_t)start,(dssize_t)end);
     if unlikely(!subrange) goto err;
     cid = asm_newconst(subrange);
     Dee_Decref(subrange);
     if unlikely(cid < 0) goto err;
     if (asm_gpush_const((uint16_t)cid)) goto err;
     if (asm_gconcat()) goto err;
    }
   }
  }
  return 0;
 }
 if (DeeList_Check(value)) {
  size_t start,end; int temp;
  bool is_first_part = true;
  DeeList_LockRead(value);
  /* Special case: empty list. */
  if (!DeeList_SIZE(value)) {
   DeeList_LockEndRead(value);
   return asm_gpack_list(0);
  }
  end = 0;
  while (end < DeeList_SIZE(value)) {
   uint16_t pack_length = 0;
   while (end < DeeList_SIZE(value) &&
          pack_length < STACK_PACK_THRESHOLD) {
    DeeObject *item = DeeList_GET(value,end);
    if (asm_allowconst(item)) break;
    Dee_Incref(item);
    DeeList_LockEndRead(value);
    temp = asm_gpush_constexpr(item);
    Dee_Decref(item);
    if unlikely(temp) goto err;
    DeeList_LockRead(value);
    ++pack_length,++end;
   }
   if (pack_length) {
    ASSERT(pack_length <= STACK_PACK_THRESHOLD);
    DeeList_LockEndRead(value);
    if (is_first_part) {
     if (asm_gpack_list(pack_length)) goto err;
     is_first_part = false;
    } else {
     /* Append this part to the previous portion. */
     if (asm_gextend(pack_length)) goto err;
    }
    DeeList_LockRead(value);
   }
   /* Collect constant parts. */
   start = end;
   while (end < DeeList_SIZE(value) &&
          asm_allowconst(DeeList_GET(value,end))) ++end;
   if (end != start) {
    DREF DeeObject *subrange;
    if (end == start+1) {
     /* Special case: encode as  `pack list, #1' */
     subrange = DeeList_GET(value,start);
     Dee_Incref(subrange);
     DeeList_LockEndRead(value);
     temp = asm_gpush_constexpr(subrange);
     Dee_Decref(subrange);
     if unlikely(temp) goto err;
     if (is_first_part) {
      if (asm_gpack_list(1)) goto err;
      is_first_part = false;
     } else {
      if (asm_gextend(1)) goto err;
     }
    } else {
     DeeList_LockEndRead(value);
     subrange = list_getrange_as_tuple((DeeListObject *)value,
                                       (dssize_t)start,(dssize_t)end);
     if unlikely(!subrange) goto err;
     cid = asm_newconst(subrange);
     Dee_Decref(subrange);
     if unlikely(cid < 0) goto err;
     if (asm_gpush_const((uint16_t)cid)) goto err;
     if (is_first_part) {
      if (asm_gcast_list()) goto err;
      is_first_part = false;
     } else {
      if (asm_gconcat()) goto err;
     }
    }
    DeeList_LockRead(value);
   }
  }
  return 0;
 }
#if 0 /* There's not really an intended way of creating these at runtime anyways,
       * and these types of sequence objects should only really be created when
       * all contained items qualify as constants anways, so we might as well
       * forget about this (for now) */
 if (DeeRoDict_Check(value)) {
  /* TO-DO: Check if all contained key/value-pairs are allocated as constants. */
 }
 if (DeeRoSet_Check(value)) {
  /* TO-DO: Check if all contained keys are allocated as constants. */
 }
#endif
 if (DeeDict_Check(value)) {
  /* Construct dicts in one of 2 ways:
   *   #1: If all dict elements are allocated as constants,
   *       push as a read-only dict, then cast to a regular
   *       dict.
   *   #2: Otherwise, push all dict key/item pairs manually,
   *       before packing everything together as a dict. */
  size_t i,mask,ro_mask,num_items;
  struct dict_item *elem;
  DREF DeeRoDictObject *rodict;
check_dict_again:
  DeeDict_LockRead(value);
  if (!((DeeDictObject *)value)->d_used) {
   /* Simple case: The dict is empty, so we can just pack an empty dict at runtime. */
   DeeDict_LockEndRead(value);
   return asm_gpack_dict(0);
  }
  mask = ((DeeDictObject *)value)->d_mask;
  elem = ((DeeDictObject *)value)->d_elem;
  for (i = 0; i <= mask; ++i) {
   if (!elem[i].di_key) continue;
   if (elem[i].di_key == &dict_dummy) continue;
   if (!asm_allowconst(elem[i].di_key) ||
       !asm_allowconst(elem[i].di_value)) {
    DeeDict_LockEndRead(value);
    goto push_dict_parts;
   }
  }
  num_items = ((DeeDictObject *)value)->d_used;
  ro_mask   = RODICT_INITIAL_MASK;
  while (ro_mask <= num_items) ro_mask = (ro_mask << 1)|1;
  ro_mask = (ro_mask << 1)|1;
  rodict = (DREF DeeRoDictObject *)DeeObject_TryCalloc(SIZEOF_RODICT(ro_mask));
  if unlikely(!rodict) {
   DeeDict_LockEndRead(value);
   if (Dee_CollectMemory(SIZEOF_RODICT(ro_mask)))
       goto check_dict_again;
   goto err;
  }
  rodict->rd_size = num_items;
  rodict->rd_mask = ro_mask;
  /* Pack all key-value pairs into the ro-dict. */
  for (i = 0; i <= mask; ++i) {
   if (!elem[i].di_key) continue;
   if (elem[i].di_key == &dict_dummy) continue;
   Dee_Incref(elem[i].di_key);
   Dee_Incref(elem[i].di_value);
   rodict_insert_nocheck(rodict,
                         elem[i].di_hash,
                         elem[i].di_key,
                         elem[i].di_value);
  }
  DeeDict_LockEndRead(value);
  DeeObject_Init(rodict,&DeeRoDict_Type);
  /* All right! we've got the ro-dict all packed together!
   * -> Register it as a constant. */
  cid = asm_newconst((DeeObject *)rodict);
  Dee_Decref(rodict);
  if unlikely(cid < 0) goto err;
  /* Now push the ro-dict, then cast it to a regular one. */
  if (asm_gpush_const((uint16_t)cid)) goto err;
  if (asm_gcast_dict()) goto err;
  return 0;
push_dict_parts:
  /* Construct a dict by pushing its individual parts. */
  num_items = 0;
  DeeDict_LockRead(value);
  for (i = 0; i <= ((DeeDictObject *)value)->d_mask; ++i) {
   struct dict_item *item; int error;
   DREF DeeObject *item_key,*item_value;
   item = &((DeeDictObject *)value)->d_elem[i];
   item_key = item->di_key;
   if (!item_key || item_key == &dict_dummy) continue;
   item_value = item->di_value;
   Dee_Incref(item_key);
   Dee_Incref(item_value);
   DeeDict_LockEndRead(value);
   /* Push the key & item. */
   error = asm_gpush_constexpr(item_key);
   if likely(!error) error = asm_gpush_constexpr(item_value);
   Dee_Decref(item_value);
   Dee_Decref(item_key);
   if unlikely(error) goto err;
   ++num_items;
   DeeDict_LockRead(value);
  }
  DeeDict_LockEndRead(value);
  /* With everything pushed, pack together the dict. */
  return asm_gpack_dict((uint16_t)num_items);
 }
 if (DeeHashSet_Check(value)) {
  /* Construct hash-sets in one of 2 ways:
   *   #1: If all set elements are allocated as constants,
   *       push as a read-only set, then cast to a regular
   *       set.
   *   #2: Otherwise, push all set keys manually, before
   *       packing everything together as a set. */
  size_t i,mask,ro_mask,num_items;
  struct hashset_item *elem;
  DREF DeeRoSetObject *roset;
check_set_again:
  DeeHashSet_LockRead(value);
  if (!((DeeHashSetObject *)value)->s_used) {
   /* Simple case: The set is empty, so we can just pack an empty hashset at runtime. */
   DeeHashSet_LockRead(value);
   return asm_gpack_hashset(0);
  }
  mask = ((DeeHashSetObject *)value)->s_mask;
  elem = ((DeeHashSetObject *)value)->s_elem;
  for (i = 0; i <= mask; ++i) {
   if (!elem[i].si_key) continue;
   if (elem[i].si_key == &dict_dummy) continue;
   if (!asm_allowconst(elem[i].si_key)) {
    DeeHashSet_LockEndRead(value);
    goto push_set_parts;
   }
  }
  num_items = ((DeeHashSetObject *)value)->s_used;
  ro_mask   = ROSET_INITIAL_MASK;
  while (ro_mask <= num_items) ro_mask = (ro_mask << 1)|1;
  ro_mask = (ro_mask << 1)|1;
  roset = (DREF DeeRoSetObject *)DeeObject_TryCalloc(SIZEOF_ROSET(ro_mask));
  if unlikely(!roset) {
   DeeHashSet_LockEndRead(value);
   if (Dee_CollectMemory(SIZEOF_ROSET(ro_mask)))
       goto check_set_again;
   goto err;
  }
  roset->rs_size = num_items;
  roset->rs_mask = ro_mask;
  /* Pack all key-value pairs into the ro-set. */
  for (i = 0; i <= mask; ++i) {
   if (!elem[i].si_key) continue;
   if (elem[i].si_key == &dict_dummy) continue;
   roset_insert_nocheck(roset,
                        elem[i].si_hash,
                        elem[i].si_key);
  }
  DeeHashSet_LockEndRead(value);
  DeeObject_Init(roset,&DeeRoSet_Type);
  /* All right! we've got the ro-set all packed together!
   * -> Register it as a constant. */
  cid = asm_newconst((DeeObject *)roset);
  Dee_Decref(roset);
  if unlikely(cid < 0) goto err;
  /* Now push the ro-set, then cast it to a regular one. */
  if (asm_gpush_const((uint16_t)cid)) goto err;
  if (asm_gcast_hashset()) goto err;
  return 0;
push_set_parts:
  /* Construct a set by pushing its individual parts. */
  num_items = 0;
  DeeHashSet_LockRead(value);
  for (i = 0; i <= ((DeeHashSetObject *)value)->s_mask; ++i) {
   struct hashset_item *item; int error;
   DREF DeeObject *item_key;
   item = &((DeeHashSetObject *)value)->s_elem[i];
   item_key = item->si_key;
   if (!item_key || item_key == &dict_dummy) continue;
   Dee_Incref(item_key);
   DeeHashSet_LockEndRead(value);
   /* Push the key & item. */
   error = asm_gpush_constexpr(item_key);
   Dee_Decref(item_key);
   if unlikely(error) goto err;
   ++num_items;
   DeeHashSet_LockRead(value);
  }
  DeeHashSet_LockEndRead(value);
  /* With everything pushed, pack together the set. */
  return asm_gpack_hashset((uint16_t)num_items);
 }

push_constexpr:
 /* Fallback: Push a constant variable  */
 cid = asm_newconst(value);
 if unlikely(cid < 0) goto err;
 return asm_gpush_const((uint16_t)cid);
err:
 return -1;
}

PRIVATE int DCALL check_thiscall(struct symbol *__restrict sym) {
 ASSERT(sym->sym_class == SYM_CLASS_MEMBER);
 /* Throw a compiler-error when one attempts to
  * access an instance member from a class method.
  * We must check this now because at runtime, the fast-mode interpreter
  * assumes that a this-argument is present when an instruction using it
  * is encountered, while the safe-mode interpreter throws an error if not.
  * But since we're trying to generate code for fast-mode, we need to check this now. */
 if (current_basescope->bs_flags&CODE_FTHISCALL)
     return 0;
 return ASM_ERR(W_ASM_INSTANCE_MEMBER_FROM_CLASS_METHOD,
                sym->sym_member.sym_member->cme_name,
                current_basescope->bs_name ?
                current_basescope->bs_name->k_name : "?");
}

/* Pushes the `this' argument as a super-wrapper for `class_type'. */
PRIVATE int (DCALL asm_gpush_thisas)(struct symbol *__restrict class_type,
                                     DeeAstObject *__restrict warn_ast) {
 int32_t symid;
 /* Special instructions exist to push `this' as a
  * specific class type when the type is a certain symbol. */
check_ct_class:
 switch (class_type->sym_class) {
 
 case SYM_CLASS_ALIAS:
  ASSERT(class_type != class_type->sym_alias.sym_alias);
  ASSERT(class_type->sym_scope == class_type->sym_alias.sym_alias->sym_scope);
  class_type = class_type->sym_alias.sym_alias;
  goto check_ct_class;

 case SYM_CLASS_REF:
  symid = asm_rsymid(class_type);
  if unlikely(symid < 0) goto err;
  return asm_gsuper_this_r((uint16_t)symid);

 case SYM_CLASS_VAR:
  if ((class_type->sym_flag&SYM_FVAR_MASK) == SYM_FVAR_GLOBAL) {
   symid = asm_gsymid_for_read(class_type,warn_ast);
   if unlikely(symid < 0) goto err;
   return asm_gsuper_this_g((uint16_t)symid);
  }
  break;

 case SYM_CLASS_EXTERN:
  ASSERT(class_type->sym_extern.sym_modsym);
  if (class_type->sym_extern.sym_modsym->ss_flags & MODSYM_FPROPERTY)
      break;
  symid = asm_esymid(class_type);
  if unlikely(symid < 0) goto err;
  return asm_gsuper_this_e((uint16_t)symid,class_type->sym_extern.sym_modsym->ss_index);

 default: break;
 }

 /* Fallback: push `this' and the class-type individually. */
 if (asm_gpush_this()) goto err;
 if (asm_gpush_symbol(class_type,warn_ast)) goto err;
 if (asm_gsuper()) goto err;
 return 0;
err:
 return -1;
}

/* Generate a call to `function' that pops `num_args' arguments from the stack,
 * then pushes its return value back onto the stack. */
PRIVATE int (DCALL asm_gcall_symbol)(struct symbol *__restrict function,
                                     uint16_t num_args,
                                     DeeAstObject *__restrict warn_ast) {
 int32_t symid;
 /* Attempt a direct call to the symbol. */
check_function_class:
 switch (function->sym_class) {

 case SYM_CLASS_ALIAS:
  ASSERT(function != function->sym_alias.sym_alias);
  ASSERT(function->sym_scope == function->sym_alias.sym_alias->sym_scope);
  function = function->sym_alias.sym_alias;
  goto check_function_class;

 case SYM_CLASS_EXTERN:
  if (function->sym_extern.sym_modsym->ss_flags & MODSYM_FEXTERN) break;
  if unlikely((symid = asm_esymid(function)) < 0) goto err;
  ASSERT(function->sym_extern.sym_modsym);
  if (asm_gcall_extern((uint16_t)symid,function->sym_extern.sym_modsym->ss_index,num_args))
      goto err;
  goto done;

 case SYM_CLASS_VAR:
  if ((function->sym_flag&SYM_FVAR_MASK) == SYM_FVAR_GLOBAL) {
   if unlikely((symid = asm_gsymid_for_read(function,warn_ast)) < 0) goto err;
   if (asm_gcall_global((uint16_t)symid,num_args)) goto err;
   goto done;
  }
  if ((function->sym_flag&SYM_FVAR_MASK) == SYM_FVAR_LOCAL) {
   if unlikely((symid = asm_lsymid_for_read(function,warn_ast)) < 0) goto err;
   if (asm_gcall_local((uint16_t)symid,num_args)) goto err;
   goto done;
  }
  break;

 default: break;
 }
 /* Fallback: Generate an extended call. */
 if unlikely(asm_gpush_symbol(function,warn_ast))
    goto err;
 if (asm_grrot(num_args+1)) goto err;
 if (asm_gcall(num_args)) goto err;
done:
 return 0;
err:
 return -1;
}


INTERN int (DCALL asm_gpush_symbol)(struct symbol *__restrict sym,
                                    DeeAstObject *__restrict warn_ast) {
 int32_t symid;
 ASSERT(sym);
check_sym_class:
 switch (sym->sym_class) {

 case SYM_CLASS_ALIAS:
  ASSERT(sym != sym->sym_alias.sym_alias);
  ASSERT(sym->sym_scope == sym->sym_alias.sym_alias->sym_scope);
  sym = sym->sym_alias.sym_alias;
  goto check_sym_class;

 case SYM_CLASS_EXTERN:
  symid = asm_esymid(sym);
  if unlikely(symid < 0) goto err;
  if (sym->sym_extern.sym_modsym->ss_flags & MODSYM_FPROPERTY) {
   /* Generate an external call to the getter. */
   return asm_gcall_extern((uint16_t)symid,
                            sym->sym_extern.sym_modsym->ss_index + MODULE_PROPERTY_GET,
                            0);
  }
  ASSERT(sym->sym_extern.sym_modsym);
  return asm_gpush_extern((uint16_t)symid,sym->sym_extern.sym_modsym->ss_index);

 case SYM_CLASS_VAR:
  switch (sym->sym_flag&SYM_FVAR_MASK) {

  case SYM_FVAR_GLOBAL:
   symid = asm_gsymid_for_read(sym,warn_ast);
   if unlikely(symid < 0) goto err;
   return asm_gpush_global((uint16_t)symid);

  case SYM_FVAR_LOCAL:
   symid = asm_lsymid_for_read(sym,warn_ast);
   if unlikely(symid < 0) goto err;
   return asm_gpush_local((uint16_t)symid);

  case SYM_FVAR_STATIC:
   symid = asm_ssymid_for_read(sym,warn_ast);
   if unlikely(symid < 0) goto err;
   return asm_gpush_static((uint16_t)symid);

  default: ASSERTF(0,"Invalid variable type");
  }
  break;

 {
  uint16_t offset,absolute_stack_addr;
 case SYM_CLASS_STACK:
  if unlikely(!(sym->sym_flag&SYM_FSTK_ALLOC)) {
   ASM_ERR(W_ASM_STACK_VARIABLE_NOT_INITIALIZED,
           sym->sym_name->k_name);
   goto err;
  }
  absolute_stack_addr = sym->sym_stack.sym_offset;
  if (current_assembler.a_stackcur <= absolute_stack_addr) {
   /* This can happen in code like this:
    * __stack local foo;
    * {
    *     __stack local bar = "ValueOfBar";
    *     foo = "ValueOfFoo";
    * }
    * print foo; // Error here
    */
   if (ASM_WARN(W_ASM_STACK_VARIABLE_WAS_DEALLOCATED,
                sym->sym_name->k_name))
       goto err;
   return asm_gpush_none();
  }
  offset = (current_assembler.a_stackcur-1)-absolute_stack_addr;
  return offset == 0 ? asm_gdup() : asm_gdup_n(offset-1);
 } break;

 case SYM_CLASS_ARG:
  return asm_gpush_varg(sym->sym_arg.sym_index);

 case SYM_CLASS_REF:
  symid = asm_rsymid(sym);
  if unlikely(symid < 0) goto err;
  return asm_gpush_ref((uint16_t)symid);

 {
  struct symbol *class_sym;
  struct member_entry *member;
 case SYM_CLASS_MEMBER:
  class_sym = sym->sym_member.sym_class;
  member = sym->sym_member.sym_member;
  if (sym->sym_flag&SYM_FMEMBER_CLASS) {
   /* Do a regular attribute lookup on the class itself. */
   if (asm_gpush_symbol(class_sym,warn_ast)) goto err;
  } else {
   if unlikely(check_thiscall(sym)) goto err;
   /* Generate special assembly for accessing different kinds of members. */
   if (!(member->cme_flag&(CLASS_MEMBER_FMETHOD|CLASS_MEMBER_FPROPERTY))) {
    /* Regular, old member variable. (this one has its own instruction) */
    while (class_sym->sym_class == SYM_CLASS_ALIAS)
        class_sym = class_sym->sym_alias.sym_alias;
    if (class_sym->sym_class == SYM_CLASS_REF) {
     symid = asm_rsymid(class_sym);
     if unlikely(symid < 0) goto err;
     return asm_ggetmember_r((uint16_t)symid,member->cme_addr);
    }
    if (class_sym->sym_class == SYM_CLASS_VAR &&
       (class_sym->sym_flag&SYM_FVAR_MASK) == SYM_FVAR_GLOBAL &&
        current_scope != (DeeScopeObject *)current_rootscope) {
     symid = asm_grsymid(class_sym);
     if unlikely(symid < 0) goto err;
     return asm_ggetmember_r((uint16_t)symid,member->cme_addr);
    }
    if (asm_gpush_symbol(class_sym,warn_ast)) goto err;
    return asm_ggetmember(member->cme_addr);
   }
   /* Fallback: Access the member at runtime.
    * XXX: When the surrounding class is final, then we could use `ASM_GETATTR_THIS_C'! */
   if (asm_gpush_thisas(class_sym,warn_ast)) goto err;
  }
  symid = asm_newconst((DeeObject *)member->cme_name);
  if unlikely(symid < 0) goto err;
  return asm_ggetattr_const((uint16_t)symid);
 } break;

 case SYM_CLASS_MODULE:
  symid = asm_msymid(sym);
  if unlikely(symid < 0) goto err;
  return asm_gpush_module((uint16_t)symid);

 case SYM_CLASS_PROPERTY:
  if (!sym->sym_property.sym_get) {
   if (ASM_WARN(W_ASM_PROPERTY_VARIABLE_NOT_READABLE,
                sym->sym_name->k_name))
       goto err;
   return asm_gpush_none();
  }
  /* Generate a zero-argument call to the getter symbol. */
  return asm_gcall_symbol(sym->sym_property.sym_get,0,warn_ast);

  /* Misc. symbol classes. */
 case SYM_CLASS_EXCEPT:
  return asm_gpush_except();
 case SYM_CLASS_THIS_MODULE:
  return asm_gpush_this_module();
 case SYM_CLASS_THIS_FUNCTION:
  return asm_gpush_this_function();
 case SYM_CLASS_THIS:
  return asm_gpush_this();

 case SYM_CLASS_AMBIGUOUS:
  if (ASM_WARN(W_ASM_AMBIGUOUS_SYMBOL,
               sym->sym_name->k_name))
      goto err;
  return asm_gpush_none();

 default:
  ASSERTF(0,"Unsupporetd variable type");
 }
 __builtin_unreachable();
err:
 return -1;
}

INTERN bool DCALL
asm_can_prefix_symbol(struct symbol *__restrict sym) {
 ASSERT(sym);
check_sym_class:
 switch (sym->sym_class) {

 case SYM_CLASS_ALIAS:
  ASSERT(sym != sym->sym_alias.sym_alias);
  ASSERT(sym->sym_scope == sym->sym_alias.sym_alias->sym_scope);
  sym = sym->sym_alias.sym_alias;
  goto check_sym_class;

 case SYM_CLASS_VAR:
  switch (sym->sym_flag&SYM_FVAR_MASK) {

  case SYM_FVAR_GLOBAL:
  case SYM_FVAR_LOCAL:
  case SYM_FVAR_STATIC:
   return true;

  default: break;
  }
  break;

 case SYM_CLASS_EXTERN:
  if (sym->sym_extern.sym_modsym->ss_flags & (MODSYM_FPROPERTY|MODSYM_FREADONLY))
      break; /* Cannot write-prefix properties, or read-only symbols. */
  return true;

 case SYM_CLASS_STACK:
  /* Only allocated stack symbols can be used in prefix expressions. */
  return !!(sym->sym_flag & SYM_FSTK_ALLOC);

 default: break;
 }
 return false;
}

INTERN bool DCALL
asm_can_prefix_symbol_for_read(struct symbol *__restrict sym) {
 ASSERT(sym);
check_sym_class:
 switch (sym->sym_class) {

 case SYM_CLASS_ALIAS:
  ASSERT(sym != sym->sym_alias.sym_alias);
  ASSERT(sym->sym_scope == sym->sym_alias.sym_alias->sym_scope);
  sym = sym->sym_alias.sym_alias;
  goto check_sym_class;

 case SYM_CLASS_VAR:
  switch (sym->sym_flag&SYM_FVAR_MASK) {
  case SYM_FVAR_GLOBAL:
  case SYM_FVAR_LOCAL:
  case SYM_FVAR_STATIC:
   return true;
  default: break;
  }
  break;

 case SYM_CLASS_EXTERN:
  if (sym->sym_extern.sym_modsym->ss_flags & MODSYM_FPROPERTY)
      break; /* Cannot prefix properties. */
  return true;

 case SYM_CLASS_STACK:
  /* Only allocated stack symbols can be used in prefix expressions. */
  return !!(sym->sym_flag & SYM_FSTK_ALLOC);

 default: break;
 }
 return false;
}


INTERN int (DCALL asm_gprefix_symbol)(struct symbol *__restrict sym,
                                      DeeAstObject *__restrict warn_ast) {
 int32_t symid;
 ASSERT(sym);
 (void)warn_ast;
check_sym_class:
 switch (sym->sym_class) {

 case SYM_CLASS_ALIAS:
  ASSERT(sym != sym->sym_alias.sym_alias);
  ASSERT(sym->sym_scope == sym->sym_alias.sym_alias->sym_scope);
  sym = sym->sym_alias.sym_alias;
  goto check_sym_class;

 case SYM_CLASS_EXTERN:
  ASSERTF(!(sym->sym_extern.sym_modsym->ss_flags & MODSYM_FPROPERTY),
          "Cannot prefix property symbols");
  symid = asm_esymid(sym);
  if unlikely(symid < 0) goto err;
  ASSERT(sym->sym_extern.sym_modsym);
  return asm_pextern((uint16_t)symid,sym->sym_extern.sym_modsym->ss_index);

 case SYM_CLASS_VAR:
  switch (sym->sym_flag&SYM_FVAR_MASK) {

  case SYM_FVAR_GLOBAL:
   symid = asm_gsymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_pglobal((uint16_t)symid);

  case SYM_FVAR_LOCAL:
   symid = asm_lsymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_plocal((uint16_t)symid);

  case SYM_FVAR_STATIC:
   symid = asm_ssymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_pstatic((uint16_t)symid);

  default: goto err_invalid;
  }
  break;

 case SYM_CLASS_STACK:
  if unlikely(!(sym->sym_flag&SYM_FSTK_ALLOC)) {
   ASM_ERR(W_ASM_STACK_VARIABLE_NOT_INITIALIZED,
           sym->sym_name->k_name);
   goto err;
  }
  if (current_assembler.a_stackcur <= sym->sym_stack.sym_offset) {
   /* This can happen in code like this:
    * __stack local foo;
    * {
    *     __stack local bar = "ValueOfBar";
    *     foo = "ValueOfFoo";
    * }
    * print foo; // Error here
    */
   ASM_ERR(W_ASM_STACK_VARIABLE_WAS_DEALLOCATED,
           sym->sym_name->k_name);
   goto err;
  }
  return asm_pstack(sym->sym_stack.sym_offset);

 default:
err_invalid:
  ASM_ERR(W_ASM_CANNOT_PREFIX_SYMBOL_CLASS,
          sym_realsym(sym)->sym_name->k_name,
          SYMCLASS_NAME(sym->sym_class));
  goto err;
 }
 __builtin_unreachable();
err:
 return -1;
}

INTERN int (DCALL asm_gpush_bnd_symbol)(struct symbol *__restrict sym,
                                        DeeAstObject *__restrict warn_ast) {
 int32_t symid;
 ASSERT(sym);
check_sym_class:
 switch (sym->sym_class) {

 case SYM_CLASS_ALIAS:
  ASSERT(sym != sym->sym_alias.sym_alias);
  ASSERT(sym->sym_scope == sym->sym_alias.sym_alias->sym_scope);
  sym = sym->sym_alias.sym_alias;
  goto check_sym_class;

 {
  uint16_t opt_index;
 case SYM_CLASS_ARG:
  if (!DeeBaseScope_IsArgOptional(current_basescope,sym->sym_arg.sym_index))
       goto fallback;
  /* Optional arguments can be unbound */
  opt_index  = sym->sym_arg.sym_index;
  opt_index -= current_basescope->bs_argc_max;
  /* The argument is bound when there is a sufficient number of variable arguments present. */
  return asm_gcmp_gr_varargs_sz(opt_index);
 } break;

 case SYM_CLASS_EXTERN:
  if (sym->sym_extern.sym_modsym->ss_flags & MODSYM_FPROPERTY)
      goto fallback;
  symid = asm_esymid(sym);
  if unlikely(symid < 0) goto err;
  ASSERT(sym->sym_extern.sym_modsym);
  return asm_gpush_bnd_extern((uint16_t)symid,sym->sym_extern.sym_modsym->ss_index);

 case SYM_CLASS_VAR:
  switch (sym->sym_flag&SYM_FVAR_MASK) {

  case SYM_FVAR_GLOBAL:
   if (!sym->sym_write)
        return asm_gpush_false();
   symid = asm_gsymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_gpush_bnd_global((uint16_t)symid);

  case SYM_FVAR_LOCAL:
   if (!sym->sym_write)
        return asm_gpush_false();
   symid = asm_lsymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_gpush_bnd_local((uint16_t)symid);

  default: break;
  }
  goto fallback;

  /* XXX: Class members can be unbound... */
 {
  struct symbol *class_sym;
  struct member_entry *member;
 case SYM_CLASS_MEMBER:
  class_sym = sym->sym_member.sym_class;
  member = sym->sym_member.sym_member;
  if (sym->sym_flag&SYM_FMEMBER_CLASS) {
   /* Do a regular attribute lookup on the class itself. */
   if (asm_gpush_symbol(class_sym,warn_ast)) goto err;
  } else {
   if unlikely(check_thiscall(sym)) goto err;
   /* Generate special assembly for accessing different kinds of members. */
   if (!(member->cme_flag&(CLASS_MEMBER_FMETHOD|CLASS_MEMBER_FPROPERTY))) {
    while (class_sym->sym_class == SYM_CLASS_ALIAS)
        class_sym = class_sym->sym_alias.sym_alias;
    if (class_sym->sym_class == SYM_CLASS_REF) {
     symid = asm_rsymid(class_sym);
     if unlikely(symid < 0) goto err;
     return asm_gboundmember_r((uint16_t)symid,member->cme_addr);
    }
    if (class_sym->sym_class == SYM_CLASS_VAR &&
       (class_sym->sym_flag&SYM_FVAR_MASK) == SYM_FVAR_GLOBAL &&
        current_scope != (DeeScopeObject *)current_rootscope) {
     symid = asm_grsymid(class_sym);
     if unlikely(symid < 0) goto err;
     return asm_gboundmember_r((uint16_t)symid,member->cme_addr);
    }
    /* Regular, old member variable. (this one has its own instruction) */
    if (asm_gpush_symbol(class_sym,warn_ast)) goto err;
    return asm_gboundmember(member->cme_addr);
   }
   /* Fallback: Access the member at runtime.
    * XXX: When the surrounding class is final, then we could use `ASM_GETATTR_THIS_C'! */
   if (asm_gpush_thisas(class_sym,warn_ast)) goto err;
  }
  symid = asm_newconst((DeeObject *)member->cme_name);
  if unlikely(symid < 0) goto err;
  if (asm_gpush_const((uint16_t)symid)) goto err;
  return asm_gboundattr();
 } break;

 case SYM_CLASS_AMBIGUOUS:
  if (ASM_WARN(W_ASM_AMBIGUOUS_SYMBOL,
               sym->sym_name->k_name))
      goto err;
  goto fallback;

 default:
fallback:
  return asm_gpush_true();
 }
 __builtin_unreachable();
err:
 return -1;
}

INTERN int (DCALL asm_gdel_symbol)(struct symbol *__restrict sym,
                                   DeeAstObject *__restrict warn_ast) {
 int32_t symid;
 ASSERT(sym);
check_sym_class:
 switch (sym->sym_class) {

 case SYM_CLASS_ALIAS:
  ASSERT(sym != sym->sym_alias.sym_alias);
  ASSERT(sym->sym_scope == sym->sym_alias.sym_alias->sym_scope);
  sym = sym->sym_alias.sym_alias;
  goto check_sym_class;

 case SYM_CLASS_VAR:
  switch (sym->sym_flag&SYM_FVAR_MASK) {

  case SYM_FVAR_GLOBAL:
   if (!(sym->sym_flag & SYM_FVAR_ALLOC) &&
       !sym->sym_write) return 0;
   symid = asm_gsymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_gdel_global((uint16_t)symid);

  case SYM_FVAR_LOCAL:
   if (!(sym->sym_flag & SYM_FVAR_ALLOC) &&
       !sym->sym_write) return 0;
   symid = asm_lsymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_gdel_local((uint16_t)symid);

  default: goto err_invalid;
  }
  break;

 {
  struct module_symbol *modsym;
  int32_t mid;
 case SYM_CLASS_EXTERN:
  modsym = sym->sym_extern.sym_modsym;
  mid = asm_esymid(sym);
  if unlikely(mid < 0) goto err;
  if (modsym->ss_flags & MODSYM_FPROPERTY) {
   /* Call an external property:
    * >> call    extern <mid>:<gid + MODULE_PROPERTY_DEL>, #0
    * >> pop     top */
   if (asm_gcall_extern((uint16_t)mid,
                         modsym->ss_index + MODULE_PROPERTY_DEL,
                         0))
       goto err;
   return asm_gpop();
  } else {
   /* Delete an external global variable:
    * >> push    module <mid>
    * >> delattr pop, const DeeModule_GlobalName(<module>,<gid>) */
   int32_t cid;
   cid = asm_newconst((DeeObject *)modsym->ss_name);
   if unlikely(cid < 0) goto err;
   if (asm_gpush_module((uint16_t)mid)) goto err;
   return asm_gdelattr_const((uint16_t)cid);
  }
 } break;

 case SYM_CLASS_STACK:
  /* If `bound()' is used on the symbol, warn about the fact that
   * doing this will not actually unbind the symbol, but only delete
   * the value that was being stored. */
  if (sym->sym_stack.sym_bound != 0 &&
    !(sym->sym_flag & SYM_FSTK_NOUNBIND_OK) &&
      WARN(W_ASM_DELETED_STACK_VARIABLE_ISNT_UNBOUND,sym->sym_name->k_name))
      goto err;
  if (!(sym->sym_flag & SYM_FSTK_ALLOC)) {
   /* If the stack variable hasn't been allocated, but is being written
    * to at some point, then we can't actually unbind it by overwriting
    * it, meaning that we can't generate the code that the user would
    * expect from us. */
   if (sym->sym_write != 0 &&
       WARN(W_ASM_CANNOT_UNBIND_UNDESIGNATED_STACK_VARIABLE,sym->sym_name->k_name))
       goto err;
   return 0;
  }
  /* Overwrite the stack-variable with `none':
   * >> mov stack #..., none */
  if (asm_pstack(sym->sym_stack.sym_offset)) goto err;
  return asm_gpush_none_p();


 {
  struct symbol *class_sym;
  struct member_entry *member;
 case SYM_CLASS_MEMBER:
  class_sym = sym->sym_member.sym_class;
  member = sym->sym_member.sym_member;
  if (sym->sym_flag&SYM_FMEMBER_CLASS) {
   /* Do a regular attribute lookup on the class itself. */
   if (asm_gpush_symbol(class_sym,warn_ast)) goto err;
  } else {
   if unlikely(check_thiscall(sym)) goto err;
   /* Generate special assembly for accessing different kinds of members. */
   if (!(member->cme_flag&(CLASS_MEMBER_FMETHOD|CLASS_MEMBER_FPROPERTY))) {
    /* Regular, old member variable. (this one has its own instruction) */
    while (class_sym->sym_class == SYM_CLASS_ALIAS)
        class_sym = class_sym->sym_alias.sym_alias;
    if (class_sym->sym_class == SYM_CLASS_REF) {
     symid = asm_rsymid(class_sym);
     if unlikely(symid < 0) goto err;
     return asm_gdelmember_r((uint16_t)symid,member->cme_addr);
    }
    if (class_sym->sym_class == SYM_CLASS_VAR &&
       (class_sym->sym_flag&SYM_FVAR_MASK) == SYM_FVAR_GLOBAL &&
        current_scope != (DeeScopeObject *)current_rootscope) {
     symid = asm_grsymid(class_sym);
     if unlikely(symid < 0) goto err;
     return asm_gdelmember_r((uint16_t)symid,member->cme_addr);
    }
    if (asm_gpush_symbol(class_sym,warn_ast)) goto err;
    return asm_gdelmember(member->cme_addr);
   }
   /* Fallback: Access the member at runtime.
    * XXX: When the surrounding class is final, then we could use `ASM_GETATTR_THIS_C'! */
   if (asm_gpush_thisas(class_sym,warn_ast)) goto err;
  }
  symid = asm_newconst((DeeObject *)member->cme_name);
  if unlikely(symid < 0) goto err;
  return asm_gdelattr_const((uint16_t)symid);
 } break;

 case SYM_CLASS_PROPERTY:
  if (!sym->sym_property.sym_del)
       return 0; /* TODO: Warning */
  /* Generate a zero-argument call to the delete symbol. */
  if (asm_gcall_symbol(sym->sym_property.sym_del,0,warn_ast))
      goto err;
  return asm_gpop();

 case SYM_CLASS_AMBIGUOUS:
  if (ASM_WARN(W_ASM_AMBIGUOUS_SYMBOL,
               sym->sym_name->k_name))
      goto err;
  return 0;

 default:
err_invalid:
  return ASM_WARN(W_ASM_CANNOT_UNBIND_SYMBOL,
                  sym_realsym(sym)->sym_name->k_name,
                  SYMCLASS_NAME(sym->sym_class));
 }
 __builtin_unreachable();
err:
 return -1;
}

INTERN int (DCALL asm_gpop_symbol)(struct symbol *__restrict sym,
                                   DeeAstObject *__restrict warn_ast) {
 int32_t symid;
 ASSERT(sym);
check_sym_class:
 switch (sym->sym_class) {

 case SYM_CLASS_ALIAS:
  ASSERT(sym != sym->sym_alias.sym_alias);
  ASSERT(sym->sym_scope == sym->sym_alias.sym_alias->sym_scope);
  sym = sym->sym_alias.sym_alias;
  goto check_sym_class;

 case SYM_CLASS_EXTERN:
  ASSERT(sym->sym_extern.sym_modsym);
  if (sym->sym_extern.sym_modsym->ss_flags&MODSYM_FREADONLY) {
   /* ERROR: Can't modify read-only external symbol. */
   if (ASM_WARN(W_ASM_EXTERNAL_SYMBOL_IS_READONLY,
                sym->sym_extern.sym_modsym->ss_name,
                sym->sym_extern.sym_module->mo_name))
       goto err;
   return asm_gpop(); /* Fallback: Simply discard the value. */
  }
  symid = asm_esymid(sym);
  if unlikely(symid < 0) goto err;
  if (sym->sym_extern.sym_modsym->ss_flags & MODSYM_FPROPERTY) {
   /* Invoke the external setter callback. */
   return asm_gcall_extern((uint16_t)symid,
                            sym->sym_extern.sym_modsym->ss_index + MODULE_PROPERTY_SET,
                            1);
  }
  return asm_gpop_extern((uint16_t)symid,sym->sym_extern.sym_modsym->ss_index);

 case SYM_CLASS_VAR:
  switch (sym->sym_flag&SYM_FVAR_MASK) {

  case SYM_FVAR_GLOBAL:
   symid = asm_gsymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_gpop_global((uint16_t)symid);

  case SYM_FVAR_LOCAL:
   symid = asm_lsymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_gpop_local((uint16_t)symid);

  case SYM_FVAR_STATIC:
   symid = asm_ssymid(sym);
   if unlikely(symid < 0) goto err;
   return asm_gpop_static((uint16_t)symid);

  default: ASSERTF(0,"Invalid variable type");
  }
  break;

 case SYM_CLASS_STACK:
  ASSERT(current_assembler.a_stackcur);
  if unlikely(!(sym->sym_flag&SYM_FSTK_ALLOC)) {
   /* This is where the magic of lazy stack initialization happens! */
   if (current_assembler.a_flag&ASM_FSTACKDISP) {
    if (current_assembler.a_scope != sym->sym_scope) {
     /* Warn about undefined behavior when the variable isn't from the current scope:
      * >> __stack local foo;
      * >> {
      * >>     __stack local bar = "ValueOfBar";
      * >>     foo = "ValueOfFoo"; // Warn here.
      * >> }
      * >> {
      * >>     __stack local x = "ValueOfX";
      * >>     __stack local y = "ValueOfY";
      * >>     print foo; // What is printed here is undefined and at
      * >>                // the time of this being written is `ValueOfY'
      * >>                // Future (unused variable?) optimizations may change this...
      * >>                // In either case: what's printed probably isn't what you want to see here.
      * >> }
      * NOTE: This problem does not arise when stack displacement is disabled...
      */
     if (ASM_WARN(W_ASM_STACK_VARIABLE_DIFFRENT_SCOPE,sym->sym_name->k_name))
         goto err;
    }
    sym->sym_flag            |= SYM_FSTK_ALLOC;
    sym->sym_stack.sym_offset = current_assembler.a_stackcur-1;
    if (asm_putddi_sbind(sym->sym_stack.sym_offset,sym->sym_name))
        goto err;
    return 0; /* Leave without popping anything! */
   }
   ASM_ERR(W_ASM_STACK_VARIABLE_NOT_INITIALIZED,
           sym->sym_name->k_name);
   goto err;
  }
  return asm_gpop_stack(sym->sym_stack.sym_offset);

 {
  struct symbol *class_sym;
  struct member_entry *member;
 case SYM_CLASS_MEMBER:
  class_sym = sym->sym_member.sym_class;
  member = sym->sym_member.sym_member;
  if (sym->sym_flag&SYM_FMEMBER_CLASS) {
   /* Do a regular attribute lookup on the class itself. */
   if (asm_gpush_symbol(class_sym,warn_ast)) goto err;
  } else {
   if unlikely(check_thiscall(sym)) goto err;
   /* Generate special assembly for accessing different kinds of members. */
   if (!(member->cme_flag&(CLASS_MEMBER_FMETHOD|CLASS_MEMBER_FPROPERTY))) {
    /* Regular, old member variable. (this one has its own instruction) */
    while (class_sym->sym_class == SYM_CLASS_ALIAS)
        class_sym = class_sym->sym_alias.sym_alias;
    if (class_sym->sym_class == SYM_CLASS_REF) {
     symid = asm_rsymid(class_sym);
     if unlikely(symid < 0) goto err;
     return asm_gsetmember_r((uint16_t)symid,member->cme_addr);
    }
    if (class_sym->sym_class == SYM_CLASS_VAR &&
       (class_sym->sym_flag&SYM_FVAR_MASK) == SYM_FVAR_GLOBAL &&
        current_scope != (DeeScopeObject *)current_rootscope) {
     /* Bind global variables to reference symbols. */
     symid = asm_grsymid(class_sym);
     if unlikely(symid < 0) goto err;
     return asm_gsetmember_r((uint16_t)symid,member->cme_addr);
    }
    if (asm_gpush_symbol(class_sym,warn_ast)) goto err;
    if (asm_gswap()) goto err; /* Swap value and self-operand to fix the ordering. */
    return asm_gsetmember(member->cme_addr);
   }
   /* Fallback: Access the member at runtime.
    * XXX: When the surrounding class is final, then we could use `ASM_GETATTR_THIS_C'! */
   if (asm_gpush_thisas(class_sym,warn_ast)) goto err;
  }
  symid = asm_newconst((DeeObject *)member->cme_name);
  if unlikely(symid < 0) goto err;
  if (asm_gswap()) goto err; /* Swap value and self-operand to fix the ordering. */
  return asm_gsetattr_const((uint16_t)symid);
 } break;

 case SYM_CLASS_PROPERTY:
  if (!sym->sym_property.sym_set) {
   if (ASM_WARN(W_ASM_PROPERTY_VARIABLE_NOT_WRITABLE,
                sym->sym_name->k_name))
       goto err;
  } else {
   /* Generate a one-argument call to the setter symbol. */
   if (asm_gcall_symbol(sym->sym_property.sym_set,1,warn_ast))
       goto err;
  }
  /* Pop the return value. */
  return asm_gpop();

 case SYM_CLASS_AMBIGUOUS:
  if (ASM_WARN(W_ASM_AMBIGUOUS_SYMBOL,
               sym->sym_name->k_name))
      goto err;
  return asm_gpop();

 default:
  /* Warn about the fact that the symbol cannot be written. */
  if (ASM_WARN(W_ASM_CANNOT_WRITE_SYMBOL,
               sym->sym_name->k_name,
               SYMCLASS_NAME(sym->sym_class)))
      goto err;
  return asm_gpop(); /* Fallback: Simply discard the value. */
 }
 __builtin_unreachable();
err:
 return -1;
}

/* Push the virtual argument known as `argid' */
INTERN int DCALL
asm_gpush_varg(uint16_t argid) {
 if (DeeBaseScope_IsArgReqOrDefl(current_basescope,argid))
     return asm_gpush_arg(argid);
 if (DeeBaseScope_IsArgOptional(current_basescope,argid)) {
  uint16_t varindex;
  varindex = argid-current_basescope->bs_argc_max;
  /* Optional argument. */
  if (varindex <= UINT8_MAX)
      return asm_gvarargs_getitem_i(varindex);
  /* Opt-index is too large (must go the long route) */
  if (asm_gpush_varargs()) goto err;
  return asm_ggetitem_index(varindex);
 }
 ASSERTF(DeeBaseScope_IsArgVarArgs(current_basescope,argid),
         "Invalid argument ID %I16u",argid);
 /* Push the varargs tuple. */
 if (asm_gpush_varargs()) goto err;
 if (DeeBaseScope_HasOptional(current_basescope)) {
  uint16_t va_start_index;
  DREF DeeObject *temp; int32_t cid;
  /* Must access a sub-range of the varargs tuple. */
  va_start_index = current_basescope->bs_argc_opt;
  if (va_start_index < INT16_MAX)
      return asm_ggetrange_in((uint16_t)va_start_index);
  /* varargs start index is too large. - Must encode as a constant. */
  temp = DeeInt_NewU16(va_start_index);
  if unlikely(!temp) goto err;
  cid = asm_newconst(temp);
  Dee_Decref(temp);
  if unlikely(cid < 0) goto err;
  if (asm_gpush_const((uint16_t)cid)) goto err;
  return asm_ggetrange_pn();
 }
 return 0;
err:
 return -1;
}


INTERN int DCALL
asm_gcmp_eq_varargs_sz(uint16_t sz) {
 DREF DeeObject *temp; int32_t cid;
 if (sz <= UINT8_MAX)
     return _asm_gcmp_eq_varargs_sz((uint8_t)sz);
 temp = DeeInt_NewU16(sz);
 if unlikely(!temp) goto err;
 cid = asm_newconst(temp);
 Dee_Decref(temp);
 if unlikely(cid < 0) goto err;
 if (asm_ggetsize_varargs()) goto err;
 if (asm_gpush_const((uint16_t)cid)) goto err;
 return asm_gcmp_eq();
err:
 return -1;
}
INTERN int DCALL
asm_gcmp_gr_varargs_sz(uint16_t sz) {
 DREF DeeObject *temp; int32_t cid;
 if (sz <= UINT8_MAX)
     return _asm_gcmp_gr_varargs_sz((uint8_t)sz);
 temp = DeeInt_NewU16(sz);
 if unlikely(!temp) goto err;
 cid = asm_newconst(temp);
 Dee_Decref(temp);
 if unlikely(cid < 0) goto err;
 if (asm_ggetsize_varargs()) goto err;
 if (asm_gpush_const((uint16_t)cid)) goto err;
 return asm_gcmp_gr();
err:
 return -1;
}



/* Store the value of the virtual argument `argid' in `dst' */
INTERN int DCALL
asm_gmov_varg(struct symbol *__restrict dst, uint16_t argid,
              DeeAstObject *__restrict warn_ast,
              bool ignore_unbound) {
 if (ignore_unbound &&
     DeeBaseScope_IsArgOptional(current_basescope,argid)) {
  /* Check if the argument is bound, and don't do the mov if it isn't. */
  struct asm_sym *skip_mov;
  uint16_t va_index = argid - current_basescope->bs_argc_max;
  if (asm_gcmp_gr_varargs_sz(va_index)) goto err;
  skip_mov = asm_newsym();
  if (asm_gjmp(ASM_JF,skip_mov)) goto err;
  asm_decsp();
  /* Only perform the store if the argument was given. */
  if (asm_gpush_varg(argid)) goto err;
  if (asm_gpop_symbol(dst,warn_ast)) goto err;
  asm_defsym(skip_mov);
  return 0;
 }
 /* Fallback: push the argument, then pop it into the symbol. */
 if (asm_gpush_varg(argid)) goto err;
 return asm_gpop_symbol(dst,warn_ast);
err:
 return -1;
}


DECL_END

#endif /* !GUARD_DEEMON_COMPILER_ASM_UTIL_C */
