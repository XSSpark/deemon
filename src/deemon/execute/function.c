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
#ifndef GUARD_DEEMON_EXECUTE_FUNCTION_C
#define GUARD_DEEMON_EXECUTE_FUNCTION_C 1
#define _KOS_SOURCE 1

#include <deemon/api.h>
#include <deemon/object.h>
#include <deemon/bool.h>
#include <deemon/class.h>
#include <deemon/int.h>
#include <deemon/arg.h>
#include <deemon/module.h>
#include <deemon/tuple.h>
#include <deemon/error.h>
#include <deemon/traceback.h>
#include <deemon/none.h>
#include <deemon/seq.h>
#include <deemon/format.h>
#include <deemon/string.h>
#include <deemon/callable.h>
#include <deemon/code.h>
#include <deemon/gc.h>
#include <deemon/asm.h>
#include <deemon/thread.h>
#include <deemon/util/string.h>

#include <string.h>

#include "../runtime/strings.h"
#include "../runtime/runtime_error.h"

DECL_BEGIN

typedef DeeFunctionObject              Function;
typedef DeeYieldFunctionIteratorObject YFIterator;
typedef DeeYieldFunctionObject         YFunction;


PRIVATE int DCALL
lookup_code_info_in_class(DeeTypeObject *__restrict type,
                          DeeCodeObject *__restrict self,
                          DeeFunctionObject *function,
                          struct function_info *__restrict info) {
 struct class_desc *my_class;
 DeeClassDescriptorObject *desc;
 uint16_t addr; size_t i;
 my_class = DeeClass_DESC(type);
 desc     = my_class->cd_desc;
 rwlock_read(&my_class->cd_lock);
 for (addr = 0; addr < desc->cd_cmemb_size; ++addr) {
  if (my_class->cd_members[addr] == (DeeObject *)function ||
     (my_class->cd_members[addr] &&
      DeeFunction_Check(my_class->cd_members[addr]) &&
    ((DeeFunctionObject *)my_class->cd_members[addr])->fo_code == self)) {
   rwlock_endread(&my_class->cd_lock);
   for (i = 0; i <= desc->cd_iattr_mask; ++i) {
    struct class_attribute *attr;
    attr = &desc->cd_iattr_list[i];
    if (!attr->ca_name) continue;
    if (addr < attr->ca_addr) continue;
    if ((attr->ca_flag & (CLASS_ATTRIBUTE_FCLASSMEM|CLASS_ATTRIBUTE_FMETHOD)) !=
                         (CLASS_ATTRIBUTE_FCLASSMEM|CLASS_ATTRIBUTE_FMETHOD))
        continue;
    if (attr->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
     if (addr >= attr->ca_addr + 3) continue;
     info->fi_getset = (uint16_t)(addr - attr->ca_addr);
    } else {
     if (addr > attr->ca_addr) continue;
    }
    /* Found it! (it's an instance method) */
    info->fi_type = type;
    info->fi_name = attr->ca_name;
    info->fi_doc  = attr->ca_doc;
    Dee_Incref(type);
    Dee_Incref(attr->ca_name);
    Dee_XIncref(attr->ca_doc);
    return 0;
   }
   for (i = 0; i <= desc->cd_cattr_mask; ++i) {
    struct class_attribute *attr;
    attr = &desc->cd_cattr_list[i];
    if (!attr->ca_name) continue;
    if (addr < attr->ca_addr) continue;
    if (attr->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
     if (addr >= attr->ca_addr + 3) continue;
     info->fi_getset = (uint16_t)(addr - attr->ca_addr);
    } else {
     if (addr > attr->ca_addr) continue;
    }
    /* Found it! (it's a class method) */
    info->fi_type = type;
    info->fi_name = attr->ca_name;
    info->fi_doc  = attr->ca_doc;
    Dee_Incref(type);
    Dee_Incref(attr->ca_name);
    Dee_XIncref(attr->ca_doc);
    return 0;
   }
   /* Check if we can find the address as an operator binding. */
   for (i = 0; i <= desc->cd_clsop_mask; ++i) {
    struct class_operator *op;
    op = &desc->cd_clsop_list[i];
    if (op->co_name == (uint16_t)-1) continue;
    if (op->co_addr != addr) continue;
    /* Found it! */
    info->fi_type   = type;
    info->fi_opname = op->co_name;
    Dee_Incref(type);
    return 0;
   }
   rwlock_read(&my_class->cd_lock);
  }
 }
 rwlock_endread(&my_class->cd_lock);
 return 1;
}

PRIVATE int DCALL
lookup_code_info(DeeCodeObject *__restrict self,
                 DeeFunctionObject *function,                      
                 struct function_info *__restrict info) {
 DeeModuleObject *module;
 uint16_t addr; int result;
 info->fi_type   = NULL;
 info->fi_name   = NULL;
 info->fi_doc    = NULL;
 info->fi_opname = (uint16_t)-1;
 info->fi_getset = (uint16_t)-1;
 /* Step #1: Search the code object's module for the given `function' */
 module = self->co_module;
 if unlikely(!module) goto without_module;
 if unlikely(DeeInteractiveModule_Check(module)) goto without_module;
 rwlock_read(&module->mo_lock);
 for (addr = 0; addr < module->mo_globalc; ++addr) {
  if (!module->mo_globalv[addr]) continue;
  if (module->mo_globalv[addr] == (DeeObject *)function ||
     (DeeFunction_Check(module->mo_globalv[addr]) &&
    ((DeeFunctionObject *)module->mo_globalv[addr])->fo_code == self)) {
   struct module_symbol *function_symbol;
   rwlock_endread(&module->mo_lock);
   function_symbol = DeeModule_GetSymbolID(module,addr);
   if (function_symbol) {
    /* Found it! (it's a global) */
    info->fi_name = module_symbol_getnameobj(function_symbol);
    if unlikely(!info->fi_name) goto err;
    if (function_symbol->ss_doc) {
     info->fi_doc = module_symbol_getdocobj(function_symbol);
     if unlikely(!info->fi_doc) goto err_name;
    }
    return 0;
   }
   rwlock_read(&module->mo_lock);
  }
 }
 /* Do another pass, this time looking for class objects
  * which the function may be defined to be apart of. */
 for (addr = 0; addr < module->mo_globalc; ++addr) {
  DeeObject *glob = module->mo_globalv[addr];
  if (!glob) continue;
  if (!DeeType_Check(glob)) continue;
  if (!DeeType_IsClass(glob)) continue;
  Dee_Incref(glob);
  rwlock_endread(&module->mo_lock);
  result = lookup_code_info_in_class((DeeTypeObject *)glob,self,function,info);
  Dee_Decref(glob);
  if (result <= 0)
      return result;
  rwlock_read(&module->mo_lock);
 }
 rwlock_endread(&module->mo_lock);
without_module:
 if (function) {
  /* Search though the function's references for class types, and
   * check if the given code object may be referring to one of them. */
  uint16_t i,count = self->co_refc;
  for (i = 0; i < count; ++i) {
   DeeObject *ref = function->fo_refv[i];
   if (!ref) continue;
   if (!DeeType_Check(ref)) continue;
   if (!DeeType_IsClass(ref)) continue;
   result = lookup_code_info_in_class((DeeTypeObject *)ref,self,function,info);
   if (result <= 0)
       return result;
  }
 }
 /* If we still haven't found anything about the function it's
  * probably been locally created as part of a caller's stack-frame.
  * That, or it's been bound externally, before being deleted from its
  * declaration module.
  * In either case: we probably won't be able to find it, so the best
  * we can do is to check what kind of DDI information is provided by
  * the function's code. */
 {
  char *name = DeeCode_NAME(self);
  if (name) {
   /* Well... At least we got the name. - That's something. */
   info->fi_name = (DREF DeeStringObject *)DeeString_New(name);
   if unlikely(!info->fi_name) goto err;
   return 0;
  }
 }
 return 1;
err_name:
 Dee_Decref(info->fi_name);
err:
 return -1;
}


PUBLIC int DCALL
DeeCode_GetInfo(DeeObject *__restrict self,
                struct function_info *__restrict info) {
 ASSERT_OBJECT_TYPE_EXACT(self,&DeeCode_Type);
 return lookup_code_info((DeeCodeObject *)self,NULL,info);
}
PUBLIC int DCALL
DeeFunction_GetInfo(DeeObject *__restrict self,
                    struct function_info *__restrict info) {
 ASSERT_OBJECT_TYPE_EXACT(self,&DeeFunction_Type);
 return lookup_code_info(((DeeFunctionObject *)self)->fo_code,
                          (DeeFunctionObject *)self,
                           info);
}





PUBLIC DREF DeeObject *DCALL
DeeFunction_New(DeeObject *__restrict code, size_t refc,
                DeeObject *const *__restrict refv) {
 DREF Function *result; size_t i;
 ASSERT_OBJECT_TYPE_EXACT(code,&DeeCode_Type);
 ASSERT(((DeeCodeObject *)code)->co_refc == refc);
 result = (DREF Function *)DeeObject_Malloc(offsetof(Function,fo_refv)+
                                                    (refc*sizeof(DREF DeeObject *)));
 if unlikely(!result) goto done;
 result->fo_code = (DREF DeeCodeObject *)code;
 for (i = 0; i < refc; ++i) {
  DREF DeeObject *obj = refv[i];
  ASSERT_OBJECT(obj);
  result->fo_refv[i] = obj;
  Dee_Incref(obj);
 }
 Dee_Incref(code);
 DeeObject_Init(result,&DeeFunction_Type);
done:
 return (DREF DeeObject *)result;
}


INTERN DREF DeeObject *DCALL
DeeFunction_NewInherited(DeeObject *__restrict code, size_t refc,
                         DREF DeeObject *const *__restrict refv) {
 DREF Function *result;
 ASSERT_OBJECT_TYPE_EXACT(code,&DeeCode_Type);
 ASSERTF(((DeeCodeObject *)code)->co_refc == refc,
         "((DeeCodeObject *)code)->co_refc = %I16u\n"
         "refc                             = %I16u\n"
         "name                             = %s\n",
         ((DeeCodeObject *)code)->co_refc,refc,
         DeeCode_NAME(code));
 result = (DREF Function *)DeeObject_Malloc(offsetof(Function,fo_refv)+
                                                    (refc*sizeof(DREF DeeObject *)));
 if unlikely(!result) goto done;
 result->fo_code = (DREF DeeCodeObject *)code;
 MEMCPY_PTR(result->fo_refv,refv,refc);
 Dee_Incref(code);
 DeeObject_Init(result,&DeeFunction_Type);
done:
 return (DREF DeeObject *)result;
}
INTERN DREF DeeObject *DCALL
DeeFunction_NewNoRefs(DeeObject *__restrict code) {
 DREF Function *result;
 ASSERT_OBJECT_TYPE_EXACT(code,&DeeCode_Type);
 ASSERT(((DeeCodeObject *)code)->co_refc == 0);
 result = (DREF Function *)DeeObject_Malloc(offsetof(Function,fo_refv));
 if unlikely(!result) goto done;
 result->fo_code = (DREF DeeCodeObject *)code;
 Dee_Incref(code);
 DeeObject_Init(result,&DeeFunction_Type);
done:
 return (DREF DeeObject *)result;
}


INTDEF DREF DeeObject *DCALL
function_call(DeeObject *__restrict self,
              size_t argc, DeeObject **__restrict argv);


PRIVATE DREF Function *DCALL
function_init(size_t argc, DeeObject **__restrict argv) {
 DREF Function *result;
 DeeCodeObject *code = &empty_code;
 DeeObject *refs = Dee_EmptyTuple;
 if (DeeArg_Unpack(argc,argv,"|oo:function",&code,&refs) ||
     DeeObject_AssertTypeExact((DeeObject *)code,&DeeCode_Type))
     goto err;
 result = (DREF Function *)DeeObject_Malloc(offsetof(Function,fo_refv)+
                                                    (code->co_refc*sizeof(DREF DeeObject *)));
 if unlikely(!result)
     goto err;
 if (DeeObject_Unpack(refs,code->co_refc,result->fo_refv))
     goto err_r;
 result->fo_code = code;
 Dee_Incref(code);
 DeeObject_Init(result,&DeeFunction_Type);
 return result;
err_r:
 DeeObject_Free(result);
err:
 return NULL;
}


PRIVATE struct type_member function_class_members[] = {
    TYPE_MEMBER_CONST("yieldfunction",&DeeYieldFunction_Type),
    TYPE_MEMBER_END
};

PRIVATE DREF DeeObject *DCALL
function_get_refs(Function *__restrict self) {
 return DeeRefVector_NewReadonly((DeeObject *)self,
                                  self->fo_code->co_refc,
                                  self->fo_refv);
}

PRIVATE DREF DeeObject *DCALL
function_get_name(Function *__restrict self) {
 struct function_info info;
 if (DeeFunction_GetInfo((DeeObject *)self,&info) < 0)
     goto err;
 Dee_XDecref(info.fi_type);
 Dee_XDecref(info.fi_doc);
 if (!info.fi_name) return_none;
 return (DREF DeeObject *)info.fi_name;
err:
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
function_get_doc(Function *__restrict self) {
 struct function_info info;
 if (DeeFunction_GetInfo((DeeObject *)self,&info) < 0)
     goto err;
 Dee_XDecref(info.fi_type);
 Dee_XDecref(info.fi_name);
 if (!info.fi_doc) return_none;
 return (DREF DeeObject *)info.fi_doc;
err:
 return NULL;
}

PRIVATE DREF DeeTypeObject *DCALL
function_get_type(Function *__restrict self) {
 struct function_info info;
 if (DeeFunction_GetInfo((DeeObject *)self,&info) < 0)
     goto err;
 Dee_XDecref(info.fi_name);
 Dee_XDecref(info.fi_doc);
 if (!info.fi_type) {
  info.fi_type = (DREF DeeTypeObject *)Dee_None;
  Dee_Incref(Dee_None);
 }
 return info.fi_type;
err:
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
function_get_module(Function *__restrict self) {
 if unlikely(!self->fo_code->co_module)
    goto err_unbound; /* Shouldn't happen... */
 return_reference_((DREF DeeObject *)self->fo_code->co_module);
err_unbound:
 err_unbound_attribute(&DeeFunction_Type,"__module__");
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
function_get_operator(Function *__restrict self) {
 struct function_info info;
 if (DeeFunction_GetInfo((DeeObject *)self,&info) < 0)
     goto err;
 Dee_XDecref(info.fi_type);
 Dee_XDecref(info.fi_name);
 Dee_XDecref(info.fi_doc);
 if (info.fi_opname == (uint16_t)-1) return_none;
 return DeeInt_NewU16(info.fi_opname);
err:
 return NULL;
}
PRIVATE DREF DeeObject *DCALL
function_get_operatorname(Function *__restrict self) {
 struct function_info info;
 struct opinfo *op;
 if (DeeFunction_GetInfo((DeeObject *)self,&info) < 0)
     goto err;
 Dee_XDecref(info.fi_name);
 Dee_XDecref(info.fi_doc);
 if (info.fi_opname == (uint16_t)-1) {
  Dee_XDecref(info.fi_type);
  return_none;
 }
 op = Dee_OperatorInfo(info.fi_type,info.fi_opname);
 Dee_XDecref(info.fi_type);
 if (!op) return DeeInt_NewU16(info.fi_opname);
 return DeeString_New(op->oi_sname);
err:
 return NULL;
}
PRIVATE DREF DeeObject *DCALL
function_get_property(Function *__restrict self) {
 struct function_info info;
 if (DeeFunction_GetInfo((DeeObject *)self,&info) < 0)
     goto err;
 Dee_XDecref(info.fi_name);
 Dee_XDecref(info.fi_doc);
 Dee_XDecref(info.fi_type);
 if (info.fi_getset == (uint16_t)-1) return_none;
 return DeeInt_NewU16(info.fi_getset);
err:
 return NULL;
}

PRIVATE struct type_getset function_getsets[] = {
    { "__name__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_get_name, NULL, NULL,
      DOC("->?Dstring\n"
          "->?N\n"
          "Returns the name of @this function, or :none if unknown") },
    { "__doc__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_get_doc, NULL, NULL,
      DOC("->?Dstring\n"
          "->?N\n"
          "Returns the documentation string of @this function, or :none if unknown") },
    { "__type__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_get_type, NULL, NULL,
      DOC("->?Dtype\n"
          "->?N\n"
          "Try to determine if @this function is defined as part of a user-defined class, "
          "and if it is, return that class type, or :none if that class couldn't be found, "
          "of if @this function is defined as stand-alone") },
    { "__module__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_get_module, NULL, NULL,
      DOC("->?Dmodule\n"
          "Return the module as part of which @this function's code was originally written") },
    { "__operator__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_get_operator, NULL, NULL,
      DOC("->?Dint\n"
          "->?N\n"
          "Try to determine if @this function is defined as part of a user-defined class, "
          "and if so, if it is used to define an operator callback. If that is the case, "
          "return the internal ID of the operator that @this function provides, or :none "
          "if that class couldn't be found, @this function is defined as stand-alone, or "
          "defined as a class- or instance-method") },
    { "__operatorname__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_get_operatorname, NULL, NULL,
      DOC("->?Dstring\n"
          "->?Dint\n"
          "->?N\n"
          "Same as #__operator__, but instead try to return the unambiguous name of the "
          "operator, though still return its ID if the operator isn't recognized as being "
          "part of the standard") },
    { "__property__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_get_property, NULL, NULL,
      DOC("->?Dint\n"
          "->?N\n"
          "Returns an integer describing the kind if @this function is part of a property or getset, "
          "or returns :none if the function's property could not be found, or if the function isn't "
          "declared as a property callback\n"
          "%{table Id|Callback|Compatible prototype\n"
          "$" PP_STR(CLASS_GETSET_GET) "|Getter callback|${function get() -> object}\n"
          "$" PP_STR(CLASS_GETSET_DEL) "|Delete callback|${function delete() -> none}\n"
          "$" PP_STR(CLASS_GETSET_SET) "|Setter callback|${function set(object value) -> none}}") },
    { "__refs__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_get_refs, NULL, NULL,
      DOC("->?S?O\n"
          "Returns a sequence of all of the references used by @this function") },
    { NULL }
};
PRIVATE struct type_member function_members[] = {
    TYPE_MEMBER_FIELD("__code__",STRUCT_OBJECT,offsetof(Function,fo_code)),
    TYPE_MEMBER_END
};


PRIVATE void DCALL
function_fini(Function *__restrict self) {
 size_t i;
 for (i = 0; i < self->fo_code->co_refc; ++i)
     Dee_Decref(self->fo_refv[i]);
 Dee_Decref(self->fo_code);
}

PRIVATE void DCALL
function_visit(Function *__restrict self,
               dvisit_t proc, void *arg) {
 size_t i;
 for (i = 0; i < self->fo_code->co_refc; ++i)
     Dee_Visit(self->fo_refv[i]);
 Dee_Visit(self->fo_code);
}

PRIVATE DREF DeeObject *DCALL
function_str(Function *__restrict self) {
 char *name = DeeCode_NAME(self->fo_code);
 if (name)
     return DeeString_New(name);
 return_reference_(&str_function);
}
PRIVATE DREF DeeObject *DCALL
function_repr(Function *__restrict self) {
 uint16_t i;
 struct unicode_printer printer = UNICODE_PRINTER_INIT;
#if 0
 if (UNICODE_PRINTER_PRINT(&printer,"function(code") < 0) goto err;
#else
 if (UNICODE_PRINTER_PRINT(&printer,"function(") < 0) goto err;
 if (unicode_printer_printobjectrepr(&printer,(DeeObject *)self->fo_code) < 0) goto err;
#endif
 if (self->fo_code->co_refc) {
  if (UNICODE_PRINTER_PRINT(&printer,",{ ") < 0) goto err;
  for (i = 0; i < self->fo_code->co_refc; ++i) {
   if (i != 0 && UNICODE_PRINTER_PRINT(&printer,", ") < 0) goto err;
   if (unicode_printer_printobjectrepr(&printer,self->fo_refv[i]) < 0)
       goto err;
  }
  if (UNICODE_PRINTER_PRINT(&printer," }") < 0) goto err;
 }
 if (unicode_printer_putascii(&printer,')')) goto err;
 return unicode_printer_pack(&printer);
err:
 unicode_printer_fini(&printer);
 return NULL;
}

PRIVATE dhash_t DCALL
function_hash(Function *__restrict self) {
 uint16_t i; dhash_t result;
 result = DeeObject_Hash((DeeObject *)self->fo_code);
 for (i = 0; i < self->fo_code->co_refc; ++i)
     result ^= DeeObject_Hash(self->fo_refv[i]);
 return result;
}

PRIVATE DREF DeeObject *DCALL
function_eq(Function *__restrict self,
            Function *__restrict other) {
 uint16_t i; int result;
 if (DeeObject_AssertTypeExact(other,&DeeFunction_Type))
     goto err;
 result = DeeObject_CompareEq((DeeObject *)self->fo_code,
                              (DeeObject *)other->fo_code);
 if unlikely(result <= 0) goto err_or_false;
 ASSERT(self->fo_code->co_refc == other->fo_code->co_refc);
 for (i = 0; i < self->fo_code->co_refc; ++i) {
  result = DeeObject_CompareEq(self->fo_refv[i],
                               other->fo_refv[i]);
  if (result <= 0) goto err_or_false;
 }
 return_true;
err_or_false:
 if (!result)
     return_false;
err:
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
function_ne(Function *__restrict self,
            Function *__restrict other) {
 uint16_t i; int result;
 if (DeeObject_AssertTypeExact(other,&DeeFunction_Type))
     goto err;
 result = DeeObject_CompareNe((DeeObject *)self->fo_code,
                              (DeeObject *)other->fo_code);
 if unlikely(result != 0) goto err_or_true;
 ASSERT(self->fo_code->co_refc == other->fo_code->co_refc);
 for (i = 0; i < self->fo_code->co_refc; ++i) {
  result = DeeObject_CompareNe(self->fo_refv[i],
                               other->fo_refv[i]);
  if (result != 0) goto err_or_true;
 }
 return_false;
err_or_true:
 if (result > 0)
     return_true;
err:
 return NULL;
}

PRIVATE struct type_cmp function_cmp = {
    /* .tp_hash = */(dhash_t(DCALL *)(DeeObject *__restrict))&function_hash,
    /* .tp_eq   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&function_eq,
    /* .tp_ne   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&function_ne
};


PUBLIC DeeTypeObject DeeFunction_Type = {
    OBJECT_HEAD_INIT(&DeeType_Type),
    /* .tp_name     = */DeeString_STR(&str_function),
    /* .tp_doc      = */NULL,
    /* .tp_flags    = */TP_FNORMAL|TP_FFINAL|TP_FNAMEOBJECT|TP_FVARIABLE,
    /* .tp_weakrefs = */0,
    /* .tp_features = */TF_NONE,
    /* .tp_base     = */&DeeCallable_Type,
    /* .tp_init = */{
        {
            /* .tp_var = */{
                /* .tp_ctor      = */NULL,
                /* .tp_copy_ctor = */NULL, /* TODO */
                /* .tp_deep_ctor = */NULL, /* TODO */
                /* .tp_any_ctor  = */&function_init,
                /* .tp_free      = */NULL, /* XXX: Use the tuple-allocator? */
            }
        },
        /* .tp_dtor        = */(void(DCALL *)(DeeObject *__restrict))&function_fini,
        /* .tp_assign      = */NULL,
        /* .tp_move_assign = */NULL
    },
    /* .tp_cast = */{
        /* .tp_str  = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_str,
        /* .tp_repr = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&function_repr,
        /* .tp_bool = */NULL
    },
    /* .tp_call          = */&function_call,
    /* .tp_visit         = */(void(DCALL *)(DeeObject *__restrict,dvisit_t,void*))&function_visit,
    /* .tp_gc            = */NULL,
    /* .tp_math          = */NULL,
    /* .tp_cmp           = */&function_cmp,
    /* .tp_seq           = */NULL,
    /* .tp_iter_next     = */NULL,
    /* .tp_attr          = */NULL,
    /* .tp_with          = */NULL,
    /* .tp_buffer        = */NULL,
    /* .tp_methods       = */NULL,
    /* .tp_getsets       = */function_getsets,
    /* .tp_members       = */function_members,
    /* .tp_class_methods = */NULL,
    /* .tp_class_getsets = */NULL,
    /* .tp_class_members = */function_class_members
};

PRIVATE void DCALL
yf_fini(YFunction *__restrict self) {
 Dee_Decref(self->yf_func);
 Dee_Decref(self->yf_args);
 Dee_XDecref(self->yf_this);
}
PRIVATE void DCALL
yf_visit(YFunction *__restrict self, dvisit_t proc, void *arg) {
 Dee_Visit(self->yf_func);
 Dee_Visit(self->yf_args);
 Dee_XVisit(self->yf_this);
}

PRIVATE int DCALL
yf_ctor(YFunction *__restrict self) {
 self->yf_func = function_init(0,NULL);
 if unlikely(!self->yf_func) goto err;
 self->yf_args = (DREF DeeTupleObject *)Dee_EmptyTuple;
 Dee_Incref(Dee_EmptyTuple);
 self->yf_this = NULL;
 return 0;
err:
 return -1;
}


PRIVATE int DCALL
yf_copy(YFunction *__restrict self,
        YFunction *__restrict other) {
 self->yf_func = other->yf_func;
 self->yf_args = other->yf_args;
 self->yf_this = other->yf_this;
 Dee_Incref(self->yf_func);
 Dee_Incref(self->yf_args);
 Dee_XIncref(self->yf_this);
 return 0;
}

PRIVATE int DCALL
yf_deepcopy(YFunction *__restrict self,
            YFunction *__restrict other) {
 self->yf_args = (DREF DeeTupleObject *)DeeObject_DeepCopy((DeeObject *)other->yf_args);
 if unlikely(!self->yf_args) goto err;
 self->yf_this = NULL;
 if (other->yf_this) {
  self->yf_this = DeeObject_DeepCopy(other->yf_this);
  if unlikely(!self->yf_this) goto err_args;
 }
 self->yf_func = other->yf_func;
 Dee_Incref(self->yf_func);
 return 0;
err_args:
 Dee_Decref(self->yf_args);
err:
 return -1;
}

PRIVATE int DCALL
yf_new(YFunction *__restrict self,
       size_t argc, DeeObject **__restrict argv) {
 DeeObject *func = NULL,*args = Dee_EmptyTuple,*this_ = NULL;
 if (DeeArg_Unpack(argc,argv,"|ooo:yieldfunction",&func,&args,&this_))
     goto err;
 /* The actually available overloads are:
  *   this();
  *   this(function func);
  *   this(function func, tuple args);
  *   this(function func, object this, tuple args); */
 if (this_) { DeeObject *temp = args; args = this_; this_ = temp; }
 if (DeeObject_AssertTypeExact(args,&DeeTuple_Type))
     goto err;
 if (func) {
  if (DeeObject_AssertTypeExact(func,&DeeFunction_Type))
      goto err;
  Dee_Incref(func);
 } else {
  func = (DeeObject *)function_init(0,NULL);
  if unlikely(!func) goto err;
 }
 if ((this_ != NULL) != (((Function *)func)->fo_code->co_flags&CODE_FTHISCALL)) {
  Dee_Decref(func);
  DeeError_Throwf(&DeeError_TypeError,
                  "Invalid presence of this-argument");
  goto err;
 }
 self->yf_func = (DREF Function *)func; /* Inherit reference. */
 self->yf_args = (DREF DeeTupleObject *)args;
 self->yf_this = this_;
 Dee_Incref(self->yf_args);
 Dee_XIncref(self->yf_this);
 return 0;
err:
 return -1;
}

PRIVATE int DCALL
yfi_init(YFIterator *__restrict self,
         YFunction *__restrict yield_function) {
 DeeCodeObject *code;
#ifndef NDEBUG
 memset(&self->yi_frame,0xcc,sizeof(struct code_frame));
#endif
 /* Setup the frame for the iterator. */
 self->yi_func          = yield_function;
 self->yi_frame.cf_func = yield_function->yf_func;
 Dee_Incref(yield_function); /* Reference stored in `self->yi_func' */
 Dee_Incref(self->yi_frame.cf_func); /* Reference stored in here. */
 code = self->yi_frame.cf_func->fo_code;
 /* Allocate memory for frame data. */
 self->yi_frame.cf_prev  = CODE_FRAME_NOT_EXECUTING;
 self->yi_frame.cf_frame = (DREF DeeObject **)Dee_Calloc(code->co_framesize);
 if unlikely(!self->yi_frame.cf_frame) goto err;
 self->yi_frame.cf_stack = self->yi_frame.cf_frame+code->co_localc;
 self->yi_frame.cf_sp    = self->yi_frame.cf_stack;
 self->yi_frame.cf_ip    = code->co_code;
 self->yi_frame.cf_flags = code->co_flags;
 self->yi_frame.cf_vargs = NULL;
 self->yi_frame.cf_argc  = DeeTuple_SIZE(yield_function->yf_args);
 self->yi_frame.cf_argv  = DeeTuple_ELEM(yield_function->yf_args);
 self->yi_frame.cf_this  = yield_function->yf_this;
 Dee_XIncref(self->yi_frame.cf_this);
 self->yi_frame.cf_stacksz = 0;
#ifndef CONFIG_NO_THREADS
 recursive_rwlock_init(&self->yi_lock);
#endif
 return 0;
err:
 return -1;
}

PRIVATE DREF YFIterator *DCALL
yf_iter_self(YFunction *__restrict self) {
 DREF YFIterator *result;
 result = DeeGCObject_MALLOC(YFIterator);
 if unlikely(!result) goto err;
 if unlikely(yfi_init(result,self)) goto err_r;
 DeeObject_Init(result,&DeeYieldFunctionIterator_Type);
 DeeGC_Track((DeeObject *)result);
 return result;
err_r:
 DeeGCObject_Free(result);
err:
 return NULL;
}

PRIVATE struct type_seq yf_seq = {
    /* .tp_iter_self = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_iter_self
};

PRIVATE struct type_member yf_class_members[] = {
    TYPE_MEMBER_CONST("iterator",&DeeYieldFunctionIterator_Type),
    TYPE_MEMBER_END
};

/* Since yieldfunction objects are bound to a specific function, comparing
 * them won't compare the bound function, but rather that function's pointer! */
PRIVATE dhash_t DCALL
yf_hash(DeeYieldFunctionObject *__restrict self) {
 dhash_t result;
 result  = DeeObject_HashGeneric(self->yf_func);
 result ^= DeeObject_Hash((DeeObject *)self->yf_args);
 if (self->yf_this)
     result ^= DeeObject_Hash((DeeObject *)self->yf_this);
 return result;
}

PRIVATE int DCALL
yf_eq_impl(DeeYieldFunctionObject *__restrict self,
           DeeYieldFunctionObject *__restrict other) {
 int error;
 if (DeeObject_AssertTypeExact(other,&DeeYieldFunction_Type))
     goto err;
 if (self == other) goto yes;
 if (self->yf_func != other->yf_func) goto nope;
 error = DeeObject_CompareEq((DeeObject *)self->yf_args,
                             (DeeObject *)other->yf_args);
 if unlikely(error <= 0) goto do_return_error;
 ASSERTF((self->yf_this != NULL) == (other->yf_this != NULL),
         "If the functions are identical, they must also have "
         "identical requirements for the presence of a this-argument!");
 if (self->yf_this)
     return DeeObject_CompareEq(self->yf_this,other->yf_this);
yes:
 return 1;
do_return_error:
 return error;
nope:
 return 0;
err:
 return -1;
}

PRIVATE DREF DeeObject *DCALL
yf_eq(DeeYieldFunctionObject *__restrict self,
      DeeYieldFunctionObject *__restrict other) {
 int result = yf_eq_impl(self,other);
 if unlikely(result < 0) return NULL;
 return_bool_(result);
}
PRIVATE DREF DeeObject *DCALL
yf_ne(DeeYieldFunctionObject *__restrict self,
      DeeYieldFunctionObject *__restrict other) {
 int result = yf_eq_impl(self,other);
 if unlikely(result < 0) return NULL;
 return_bool_(!result);
}

PRIVATE struct type_cmp yf_cmp = {
    /* .tp_hash = */(dhash_t(DCALL *)(DeeObject *__restrict))&yf_hash,
    /* .tp_eq   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&yf_eq,
    /* .tp_ne   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&yf_ne
};


PRIVATE struct type_member yf_members[] = {
    TYPE_MEMBER_FIELD_DOC("__func__",STRUCT_OBJECT,offsetof(YFunction,yf_func),"->?Dfunction"),
    TYPE_MEMBER_FIELD_DOC("__args__",STRUCT_OBJECT,offsetof(YFunction,yf_args),"->?S?O"),
    TYPE_MEMBER_FIELD("__this__",STRUCT_OBJECT,offsetof(YFunction,yf_this)),
    TYPE_MEMBER_END
};

PRIVATE DREF DeeCodeObject *DCALL
yf_get_code(YFunction *__restrict self) {
 return_reference_(self->yf_func->fo_code);
}
PRIVATE DREF DeeObject *DCALL
yf_get_name(YFunction *__restrict self) {
 return function_get_name(self->yf_func);
}
PRIVATE DREF DeeObject *DCALL
yf_get_doc(YFunction *__restrict self) {
 return function_get_doc(self->yf_func);
}
PRIVATE DREF DeeTypeObject *DCALL
yf_get_type(YFunction *__restrict self) {
 return function_get_type(self->yf_func);
}
PRIVATE DREF DeeObject *DCALL
yf_get_module(YFunction *__restrict self) {
 return function_get_module(self->yf_func);
}
PRIVATE DREF DeeObject *DCALL
yf_get_operator(YFunction *__restrict self) {
 return function_get_operator(self->yf_func);
}
PRIVATE DREF DeeObject *DCALL
yf_get_operatorname(YFunction *__restrict self) {
 return function_get_operatorname(self->yf_func);
}
PRIVATE DREF DeeObject *DCALL
yf_get_property(YFunction *__restrict self) {
 return function_get_property(self->yf_func);
}
PRIVATE DREF DeeObject *DCALL
yf_get_refs(YFunction *__restrict self) {
 return function_get_refs(self->yf_func);
}

PRIVATE struct type_getset yf_getsets[] = {
    { "__code__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_code, NULL, NULL,
      DOC("->?Dcode\n"
          "Alias for :function.__code__ though #__func__") },
    { "__name__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_name, NULL, NULL,
      DOC("->?Dstring\n"
          "->?N\n"
          "Alias for :function.__name__ though #__func__") },
    { "__doc__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_doc, NULL, NULL,
      DOC("->?Dstring\n"
          "->?N\n"
          "Alias for :function.__doc__ though #__func__") },
    { "__type__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_type, NULL, NULL,
      DOC("->?Dtype\n"
          "->?N\n"
          "Alias for :function.__type__ though #__func__") },
    { "__module__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_module, NULL, NULL,
      DOC("->?Dmodule\n"
          "Alias for :function.__module__ though #__func__") },
    { "__operator__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_operator, NULL, NULL,
      DOC("->?Dint\n"
          "->?N\n"
          "Alias for :function.__operator__ though #__func__") },
    { "__operatorname__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_operatorname, NULL, NULL,
      DOC("->?Dstring\n"
          "->?Dint\n"
          "->?N\n"
          "Alias for :function.__operatorname__ though #__func__") },
    { "__property__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_property, NULL, NULL,
      DOC("->?Dint\n"
          "->?N\n"
          "Alias for :function.__property__ though #__func__") },
    { "__refs__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yf_get_refs, NULL, NULL,
      DOC("->?S?O\n"
          "Alias for :function.__refs__ though #__func__") },
    { NULL }
};


PUBLIC DeeTypeObject DeeYieldFunction_Type = {
    OBJECT_HEAD_INIT(&DeeType_Type),
    /* .tp_name     = */"yieldfunction",
    /* .tp_doc      = */NULL,
    /* .tp_flags    = */TP_FNORMAL | TP_FFINAL,
    /* .tp_weakrefs = */0,
    /* .tp_features = */TF_NONE,
    /* .tp_base     = */&DeeSeq_Type,
    /* .tp_init = */{
        {
            /* .tp_alloc = */{
                /* .tp_ctor      = */&yf_ctor,
                /* .tp_copy_ctor = */&yf_copy,
                /* .tp_deep_ctor = */&yf_deepcopy,
                /* .tp_any_ctor  = */&yf_new,
                /* .tp_free      = */NULL,
                {
                    /* .tp_instance_size = */sizeof(YFunction)
                }
            }
        },
        /* .tp_dtor        = */(void(DCALL *)(DeeObject *__restrict))&yf_fini,
        /* .tp_assign      = */NULL,
        /* .tp_move_assign = */NULL,
        /* .tp_deepload    = */NULL
    },
    /* .tp_cast = */{
        /* .tp_str  = */NULL,
        /* .tp_repr = */NULL,
        /* .tp_bool = */NULL
    },
    /* .tp_call          = */NULL,
    /* .tp_visit         = */(void(DCALL *)(DeeObject *__restrict,dvisit_t,void*))&yf_visit,
    /* .tp_gc            = */NULL,
    /* .tp_math          = */NULL,
    /* .tp_cmp           = */&yf_cmp,
    /* .tp_seq           = */&yf_seq,
    /* .tp_iter_next     = */NULL,
    /* .tp_attr          = */NULL,
    /* .tp_with          = */NULL,
    /* .tp_buffer        = */NULL,
    /* .tp_methods       = */NULL,
    /* .tp_getsets       = */yf_getsets,
    /* .tp_members       = */yf_members,
    /* .tp_class_methods = */NULL,
    /* .tp_class_getsets = */NULL,
    /* .tp_class_members = */yf_class_members
};

PRIVATE void DCALL
yfi_run_finally(YFIterator *__restrict self) {
 DeeCodeObject *code; code_addr_t ipaddr;
 struct except_handler *iter,*begin;
 if unlikely(!self->yi_func) return;
 /* Recursively execute all finally-handlers that
  * protect the current PC until none are left. */
 code = self->yi_frame.cf_func->fo_code;
 if unlikely(!code) return;
 ASSERT_OBJECT_TYPE(code,&DeeCode_Type);
 /* Simple case: without any finally handlers, we've got nothing to do. */
 if (!(code->co_flags&CODE_FFINALLY)) return;
exec_finally:
 iter = (begin = code->co_exceptv)+code->co_exceptc;
 /* NOTE: The frame-PC is allowed to equal the end of the
  *       associated code object, because it contains the
  *       address of the next instruction to-be executed.
  *       Similarly, range checks of handlers are adjusted, too.
  */
 ASSERT(self->yi_frame.cf_ip >= code->co_code &&
        self->yi_frame.cf_ip <= code->co_code+code->co_codebytes);
 ipaddr = (code_addr_t)(self->yi_frame.cf_ip - code->co_code);
 while (iter-- != begin) {
  DREF DeeObject *result;
  if (!(iter->eh_flags&EXCEPTION_HANDLER_FFINALLY)) continue;
  if (!(ipaddr > iter->eh_start && ipaddr <= iter->eh_end)) continue;
  /* Execute this finally-handler. */
  self->yi_frame.cf_ip = code->co_code+iter->eh_addr;
  /* We must somehow indicate to code-exec to stop when an
   * `ASM_ENDFINALLY' instruction is hit.
   * Normally, this is done when the return value has been
   * assigned, so we simply fake that by pre-assigning `none'. */
  self->yi_frame.cf_result = Dee_None;
  Dee_Incref(Dee_None);
  if unlikely(self->yi_frame.cf_flags&CODE_FASSEMBLY) {
   /* Special case: Execute the code using the safe runtime, rather than the fast. */
   result = DeeCode_ExecFrameSafe(&self->yi_frame);
  } else {
   /* Default case: Execute from a fast yield-function-iterator frame. */
   result = DeeCode_ExecFrameFast(&self->yi_frame);
  }
  if likely(result)
   Dee_Decref(result); /* Most likely, this is `none' */
  else {
   DeeError_Print("Unhandled exception in yieldfunction.iterator destructor\n",
                  ERROR_PRINT_DOHANDLE);
  }
  goto exec_finally;
 }

}

PRIVATE void DCALL
yfi_dtor(YFIterator *__restrict self) {
 size_t stacksize; size_t numlocals = 0;
 /* Execute established finally handlers. */
 yfi_run_finally(self);
 ASSERT(self->yi_frame.cf_prev == CODE_FRAME_NOT_EXECUTING);
 ASSERT_OBJECT_TYPE_OPT(self->yi_func,&DeeYieldFunction_Type);
 ASSERT_OBJECT_TYPE_OPT(self->yi_frame.cf_func,&DeeFunction_Type);
 ASSERT_OBJECT_TYPE_OPT(self->yi_frame.cf_vargs,&DeeTuple_Type);
 ASSERT_OBJECT_OPT(self->yi_frame.cf_this);
 if (self->yi_frame.cf_func)
     numlocals = self->yi_frame.cf_func->fo_code->co_localc;
 stacksize = self->yi_frame.cf_sp-self->yi_frame.cf_stack;
 Dee_XDecref(self->yi_func);
 Dee_XDecref(self->yi_frame.cf_func);
 Dee_XDecref(self->yi_frame.cf_this);
 Dee_XDecref(self->yi_frame.cf_vargs);
 /* Clear local objects. */
 while (numlocals--) Dee_XDecref(self->yi_frame.cf_frame[numlocals]);
 /* Clear objects from the stack. */
 while (stacksize--) Dee_Decref(self->yi_frame.cf_stack[stacksize]);
 if (self->yi_frame.cf_stacksz)
     Dee_Free(self->yi_frame.cf_stack);
 Dee_Free(self->yi_frame.cf_frame);
}


PRIVATE void DCALL
yfi_visit(YFIterator *__restrict self,
          dvisit_t proc, void *arg) {
 if (self->yi_frame.cf_prev != CODE_FRAME_NOT_EXECUTING)
     return; /* Can't visit a frame that is current executing. */
#ifndef CONFIG_NO_THREADS
 recursive_rwlock_write(&self->yi_lock);
 COMPILER_READ_BARRIER();
 if (self->yi_frame.cf_prev != CODE_FRAME_NOT_EXECUTING) {
  recursive_rwlock_endwrite(&self->yi_lock);
  return; /* See above... */
 }
#endif
 Dee_XVisit(self->yi_func);
 Dee_XVisit(self->yi_frame.cf_this);
 Dee_XVisit(self->yi_frame.cf_vargs);
 /* Visit local variables. */
 if (self->yi_frame.cf_func) {
  DeeObject **locals = self->yi_frame.cf_frame;
  size_t numlocals = self->yi_frame.cf_func->fo_code->co_localc;
  Dee_Visit(self->yi_frame.cf_func);
  while (numlocals--) Dee_XVisit(locals[numlocals]);
 }
 /* Visit stack objects. */
 { size_t stacksize = (size_t)(self->yi_frame.cf_sp-
                               self->yi_frame.cf_stack);
   DeeObject **stack = self->yi_frame.cf_stack;
   while (stacksize--) Dee_Visit(stack[stacksize]);
 }

#ifndef CONFIG_NO_THREADS
 recursive_rwlock_endwrite(&self->yi_lock);
#endif
}

PRIVATE void DCALL
yfi_clear(YFIterator *__restrict self) {
 DeeObject *obj[4],**stack; size_t stacksize;
 DeeObject **locals; size_t numlocals = 0;
 bool heap_stack = false;
#ifndef CONFIG_NO_THREADS
 recursive_rwlock_write(&self->yi_lock);
#endif
 /* Execute established finally handlers. */
 yfi_run_finally(self);
 if unlikely(self->yi_frame.cf_prev != CODE_FRAME_NOT_EXECUTING) {
  /* Can't clear a frame currently being executed. */
#ifndef CONFIG_NO_THREADS
  recursive_rwlock_endwrite(&self->yi_lock);
#endif
  return;
 }
 obj[0] = (DeeObject *)self->yi_func;
 obj[1] = (DeeObject *)self->yi_frame.cf_func;
 obj[2] = (DeeObject *)self->yi_frame.cf_this;
 obj[3] = (DeeObject *)self->yi_frame.cf_vargs;
 if (self->yi_frame.cf_func)
     numlocals = self->yi_frame.cf_func->fo_code->co_localc;
 stack     = self->yi_frame.cf_stack;
 stacksize = self->yi_frame.cf_sp-stack;
 if (self->yi_frame.cf_stacksz) {
  self->yi_frame.cf_stacksz = 0;
  heap_stack = true;
 }
 locals = self->yi_frame.cf_frame;
 self->yi_func           = NULL;
 self->yi_frame.cf_func  = NULL;
 self->yi_frame.cf_argc  = 0;
 self->yi_frame.cf_argv  = NULL;
 self->yi_frame.cf_this  = NULL;
 self->yi_frame.cf_vargs = NULL;
 self->yi_frame.cf_frame = NULL;
 self->yi_frame.cf_stack = NULL;
 self->yi_frame.cf_sp    = NULL;
 self->yi_frame.cf_ip    = NULL;
#ifndef CONFIG_NO_THREADS
 recursive_rwlock_endwrite(&self->yi_lock);
#endif
 Dee_XDecref(obj[0]);
 Dee_XDecref(obj[1]);
 Dee_XDecref(obj[2]);
 Dee_XDecref(obj[3]);
 /* Clear local objects. */
 while (numlocals--) Dee_XDecref(locals[numlocals]);
 /* Clear objects from the stack. */
 while (stacksize--) Dee_Decref(stack[stacksize]);
 /* Free a heap-allocated stack, and local variable memory. */
 if (heap_stack) Dee_Free(stack);
 Dee_Free(locals);
}

PRIVATE DREF DeeObject *DCALL
yfi_iter_next(YFIterator *__restrict self) {
 DREF DeeObject *result;
#ifndef CONFIG_NO_THREADS
 recursive_rwlock_write(&self->yi_lock);
#endif
 if unlikely(!self->yi_func) {
  /* Special case: Always be indicative of an exhausted iterator
   * when default-constructed, or after being cleared. */
  result = ITER_DONE;
 } else {
  ASSERT_OBJECT_TYPE(self->yi_func,&DeeYieldFunction_Type);
  ASSERT_OBJECT_TYPE(self->yi_frame.cf_func,&DeeFunction_Type);
  ASSERT_OBJECT_TYPE(self->yi_frame.cf_func->fo_code,&DeeCode_Type);
  ASSERTF(self->yi_frame.cf_func->fo_code->co_flags&CODE_FYIELDING,
          "Code is not assembled as a yield-function");
  ASSERTF(self->yi_frame.cf_ip >= self->yi_frame.cf_func->fo_code->co_code &&
          self->yi_frame.cf_ip <= self->yi_frame.cf_func->fo_code->co_code+
                                  self->yi_frame.cf_func->fo_code->co_codebytes,
          "Illegal PC: %p is not in %p...%p",
          self->yi_frame.cf_ip,
          self->yi_frame.cf_func->fo_code->co_code,
          self->yi_frame.cf_func->fo_code->co_code+
          self->yi_frame.cf_func->fo_code->co_codebytes);
  if unlikely(self->yi_frame.cf_prev != CODE_FRAME_NOT_EXECUTING) {
   DeeError_Throwf(&DeeError_SegFault,"Stack frame is already being executed");
   result = NULL;
   goto done;
  }
  self->yi_frame.cf_result = NULL;
  if unlikely(self->yi_frame.cf_flags&CODE_FASSEMBLY) {
   /* Special case: Execute the code using the safe runtime, rather than the fast. */
   result = DeeCode_ExecFrameSafe(&self->yi_frame);
  } else {
   /* Default case: Execute from a fast yield-function-iterator frame. */
   result = DeeCode_ExecFrameFast(&self->yi_frame);
  }
 }
done:
#ifndef CONFIG_NO_THREADS
 recursive_rwlock_endwrite(&self->yi_lock);
#endif
 return result;
}

PRIVATE int DCALL
yfi_new(YFIterator *__restrict self,
        size_t argc, DeeObject **__restrict argv) {
 YFunction *func;
 if (DeeArg_Unpack(argc,argv,"o:yieldfunction.iterator",&func))
     goto err;
 if (DeeObject_AssertType((DeeObject *)func,&DeeYieldFunction_Type))
     goto err;
 return yfi_init(self,func);
err:
 return -1;
}

LOCAL int DCALL
inplace_deepcopy_noarg(DREF DeeObject **__restrict pob,
                       size_t argc1, DREF DeeObject **__restrict argv1,
                       size_t args2_c, DeeObject **__restrict args2_v) {
 DREF DeeObject *ob = *pob,**iter,**end;
 /* Check if `*pob' is apart of the argument
  * tuple, and don't copy it if it is.
  * We take special care not to copy objects that were loaded
  * from arguments/references, as those are intended to be shared.
  * WARNING: This system isn't, and can never really be perfect.
  *          For example: should we copy an object loaded from
  *          the item of a list accessed through a reference/argument?
  *          The current implementation does, but the user
  *          may expect it not to do so.
  *          As far as logic goes, the sane thing would be to
  *          only copy objects when they are ever written to.
  *          But the again: how do we know when something will be written?
  *          Since everything can be dynamically altered, we have no
  *          way of predicting, or determining when _anything_ is going
  *          to change in the most dramatic way imaginable.
  *      ... Anyways. This is the best we can make out of a bad
  *          situation. - And luckily enough, the old deemon already
  *          knew that this could lead to troubles and established
  *          copyable stackframe as a whitelist-based system that
  *          a user must opt-in if they wish to use it.
  *          So in that sense: we've always got the user to blame if
  *                            they manage to break something or get
  *                            undefined behavior when using [[copyable]]!
  * NOTE: Don't get me wrong, through. I very much believe that copyable
  *       stack frames open up the door for _a_ _lot_ of awesome programming
  *       possibilities, while leaving any undefined behavior as pure-weak,
  *       in the sense that unless for some bug, the design is able to
  *       never crash no matter what the user might do.
  */
 end = (iter = argv1)+argc1;
 for (; iter != end; ++iter) if (*iter == ob) return 0;
 end = (iter = args2_v)+args2_c;
 for (; iter != end; ++iter) if (*iter == ob) return 0;
 /* Create an inplace deep-copy of this object. */
 return DeeObject_InplaceDeepCopy(pob);
}

PRIVATE int DCALL
yfi_copy(YFIterator *__restrict self,
         YFIterator *__restrict other) {
 DeeCodeObject *code; size_t stack_size;
 DREF DeeObject **iter,**end,**src;
again:
 recursive_rwlock_write(&other->yi_lock);
 /* Make sure that the function is actually copyable. */
 code = NULL;
 if (other->yi_frame.cf_func &&
  !((code = other->yi_frame.cf_func->fo_code)->co_flags&CODE_FCOPYABLE)) {
  char *function_name; DeeCodeObject *code;
  code = other->yi_frame.cf_func->fo_code;
  Dee_Incref(code);
  function_name = DeeCode_NAME(code);
  recursive_rwlock_endwrite(&other->yi_lock);
  if (!function_name) function_name = "?";
  DeeError_Throwf(&DeeError_ValueError,"Function `%s' is not copyable",function_name);
  Dee_XDecref(code);
  return -1;
 }
 self->yi_func = other->yi_func;
 /* Copy over frame data. */
 memcpy(&self->yi_frame,&other->yi_frame,sizeof(struct code_frame));
 /* In case the other frame is currently executing, mark ours as not. */
 self->yi_frame.cf_prev = CODE_FRAME_NOT_EXECUTING;
 if (code) {
  *(uintptr_t *)&self->yi_frame.cf_sp -= (uintptr_t)self->yi_frame.cf_stack;
  if (self->yi_frame.cf_stacksz) {
   /* Copy a heap-allocated, extended stack. */
   self->yi_frame.cf_stack = (DREF DeeObject **)Dee_TryMalloc(self->yi_frame.cf_stacksz*
                                                              sizeof(DREF DeeObject *));
   if unlikely(!self->yi_frame.cf_stack) goto nomem;
   src = other->yi_frame.cf_stack;
   end = (iter = self->yi_frame.cf_stack)+self->yi_frame.cf_stacksz;
   for (; iter != end; ++iter,++src) {
    DeeObject *ob = *src;
    Dee_Incref(ob);
    *iter = ob;
   }
  }
  self->yi_frame.cf_frame = (DREF DeeObject **)Dee_TryMalloc(code->co_framesize);
  if unlikely(!self->yi_frame.cf_frame) goto nomem_stack;
  /* Copy local variables. */
  src = other->yi_frame.cf_frame;
  end = (iter = self->yi_frame.cf_frame)+code->co_localc;
  for (; iter != end; ++iter,++src) {
   DeeObject *ob = *src;
   *iter = ob;
   Dee_XIncref(ob);
  }
  if (!self->yi_frame.cf_stacksz) {
   /* Relocate + copy a frame-shared stack. */
   self->yi_frame.cf_stack = self->yi_frame.cf_frame+code->co_localc;
   *(uintptr_t *)&self->yi_frame.cf_sp += (uintptr_t)self->yi_frame.cf_stack;
   stack_size = (self->yi_frame.cf_sp-self->yi_frame.cf_stack);
   ASSERTF(stack_size*sizeof(DeeObject *) <=
          (code->co_framesize-code->co_localc*sizeof(DeeObject *)),
           "The stack is too large");
   /* Copy the stack. */
   src = other->yi_frame.cf_stack;
   end = (iter = self->yi_frame.cf_stack)+stack_size;
   for (; iter != end; ++iter,++src) {
    DeeObject *ob = *src;
    Dee_Incref(ob);
    *iter = ob;
   }
  } else {
   *(uintptr_t *)&self->yi_frame.cf_sp += (uintptr_t)self->yi_frame.cf_stack;
   stack_size = self->yi_frame.cf_stacksz;
  }
 } else {
  self->yi_frame.cf_frame   = NULL;
  self->yi_frame.cf_stack   = NULL;
  self->yi_frame.cf_sp      = NULL;
  self->yi_frame.cf_stacksz = 0;
  stack_size                = 0;
 }

 /* Create references. */
 Dee_XIncref(self->yi_func);
 Dee_XIncref(self->yi_frame.cf_func);
 Dee_XIncref(self->yi_frame.cf_this);
 Dee_XIncref(self->yi_frame.cf_vargs);

 recursive_rwlock_endwrite(&other->yi_lock);
 recursive_rwlock_init(&self->yi_lock);
 if (code) {
  DeeObject *this_arg = self->yi_frame.cf_this;
  DeeObject *varargs = (DeeObject *)self->yi_frame.cf_vargs;
  size_t      argc = self->yi_frame.cf_argc;
  DeeObject **argv = self->yi_frame.cf_argv;
  size_t      refc = self->yi_func->yf_func->fo_code->co_refc;
  DeeObject **refv = self->yi_func->yf_func->fo_refv;
  /* With all objects now referenced, we still have to replace
   * all locals and the stack with deep copies of themself. */
  end = (iter = self->yi_frame.cf_stack)+stack_size;
  for (; iter != end; ++iter) {
   if (*iter != this_arg && *iter != varargs &&
       inplace_deepcopy_noarg(iter,argc,argv,refc,refv)) goto err;
  }
  end = (iter = self->yi_frame.cf_frame)+code->co_localc;
  for (; iter != end; ++iter) {
   if (*iter != this_arg && *iter != varargs &&
       inplace_deepcopy_noarg(iter,argc,argv,refc,refv)) goto err;
  }
  /* WARNING: There are some thing that we don't copy, such as the this-argument.
   *          Similarly, we also don't copy function input arguments! */
 } else {
  ASSERT(!stack_size);
 }
 return 0;
err:
 yfi_dtor(self);
 return -1;
nomem_stack:
 if (self->yi_frame.cf_stacksz) {
  uint16_t n = self->yi_frame.cf_stacksz;
  while (n--) Dee_Decref(self->yi_frame.cf_stack[n]);
  Dee_Free(self->yi_frame.cf_stack);
 }
nomem:
 recursive_rwlock_endwrite(&other->yi_lock);
 if (Dee_CollectMemory(1))
     goto again;
 return -1;
}

#ifndef CONFIG_NO_THREADS
PRIVATE DREF YFunction *DCALL
yfi_get_yfunc(YFIterator *__restrict self) {
 DREF YFunction *result;
 recursive_rwlock_read(&self->yi_lock);
 result = self->yi_func;
 Dee_XIncref(result);
 recursive_rwlock_endread(&self->yi_lock);
 if unlikely(!result)
    err_unbound_attribute(&DeeYieldFunctionIterator_Type,"seq");
 return result;
}
#endif /* !CONFIG_NO_THREADS */

PRIVATE DREF DeeObject *DCALL
yfi_get_this(YFIterator *__restrict self) {
 DREF DeeObject *result;
 recursive_rwlock_read(&self->yi_lock);
 result = self->yi_frame.cf_this;
 if (!(self->yi_frame.cf_flags&CODE_FTHISCALL))
       result = NULL;
 Dee_XIncref(result);
 recursive_rwlock_endread(&self->yi_lock);
 if unlikely(!result)
    err_unbound_attribute(&DeeYieldFunctionIterator_Type,"__this__");
 return result;
}

PRIVATE DREF DeeObject *DCALL
yfi_get_frame(YFIterator *__restrict self) {
 return DeeFrame_NewReferenceWithLock((DeeObject *)self,
                                      &self->yi_frame,
                                       DEEFRAME_FREADONLY|
                                       DEEFRAME_FUNDEFSP|
                                       DEEFRAME_FRECLOCK,
                                      &self->yi_lock);
}

PRIVATE DREF Function *DCALL
yfi_get_func(YFIterator *__restrict self) {
 DREF Function *result;
 recursive_rwlock_write(&self->yi_lock);
 if unlikely(!self->yi_func) {
  recursive_rwlock_endwrite(&self->yi_lock);
  err_unbound_attribute(&DeeYieldFunctionIterator_Type,"__func__");
  return NULL;
 }
 result = self->yi_func->yf_func;
 Dee_Incref(result);
 recursive_rwlock_endwrite(&self->yi_lock);
 return result;
}

PRIVATE DREF DeeCodeObject *DCALL
yfi_get_code(YFIterator *__restrict self) {
 DREF DeeCodeObject *result;
 recursive_rwlock_write(&self->yi_lock);
 if unlikely(!self->yi_func) {
  recursive_rwlock_endwrite(&self->yi_lock);
  err_unbound_attribute(&DeeYieldFunctionIterator_Type,"__code__");
  return NULL;
 }
 result = self->yi_func->yf_func->fo_code;
 Dee_Incref(result);
 recursive_rwlock_endwrite(&self->yi_lock);
 return result;
}

PRIVATE DREF DeeObject *DCALL
yfi_get_refs(YFIterator *__restrict self) {
 DREF DeeObject *result;
 DREF Function *func;
 recursive_rwlock_write(&self->yi_lock);
 if unlikely(!self->yi_func) {
  recursive_rwlock_endwrite(&self->yi_lock);
  err_unbound_attribute(&DeeYieldFunctionIterator_Type,"__refs__");
  return NULL;
 }
 func = self->yi_func->yf_func;
 Dee_Incref(func);
 recursive_rwlock_endwrite(&self->yi_lock);
 result = function_get_refs(func);
 Dee_Decref(func);
 return result;
}

PRIVATE DREF DeeObject *DCALL
yfi_get_args(YFIterator *__restrict self) {
 DREF DeeObject *result;
 recursive_rwlock_write(&self->yi_lock);
 if unlikely(!self->yi_func) {
  recursive_rwlock_endwrite(&self->yi_lock);
  err_unbound_attribute(&DeeYieldFunctionIterator_Type,"__args__");
  return NULL;
 }
 result = (DeeObject *)self->yi_func->yf_args;
 Dee_Incref(result);
 recursive_rwlock_endwrite(&self->yi_lock);
 return result;
}

PRIVATE DREF YFunction *DCALL
yfi_getfunc(YFIterator *__restrict self,
            char const *__restrict attr_name) {
 DREF YFunction *result;
 recursive_rwlock_write(&self->yi_lock);
 if unlikely(!self->yi_func) {
  recursive_rwlock_endwrite(&self->yi_lock);
  err_unbound_attribute(&DeeYieldFunctionIterator_Type,attr_name);
  return NULL;
 }
 result = self->yi_func;
 Dee_Incref(result);
 recursive_rwlock_endwrite(&self->yi_lock);
 return result;
}

PRIVATE DREF DeeObject *DCALL
yfi_get_name(YFIterator *__restrict self) {
 DREF DeeObject *result;
 DREF YFunction *func = yfi_getfunc(self,"__name__");
 if unlikely(!func) goto err;
 result = yf_get_name(func);
 Dee_Decref(func);
 return result;
err:
 return NULL;
}
PRIVATE DREF DeeObject *DCALL
yfi_get_doc(YFIterator *__restrict self) {
 DREF DeeObject *result;
 DREF YFunction *func = yfi_getfunc(self,"__doc__");
 if unlikely(!func) goto err;
 result = yf_get_doc(func);
 Dee_Decref(func);
 return result;
err:
 return NULL;
}
PRIVATE DREF DeeTypeObject *DCALL
yfi_get_type(YFIterator *__restrict self) {
 DREF DeeTypeObject *result;
 DREF YFunction *func = yfi_getfunc(self,"__type__");
 if unlikely(!func) goto err;
 result = yf_get_type(func);
 Dee_Decref(func);
 return result;
err:
 return NULL;
}
PRIVATE DREF DeeObject *DCALL
yfi_get_module(YFIterator *__restrict self) {
 DREF DeeObject *result;
 DREF YFunction *func = yfi_getfunc(self,"__module__");
 if unlikely(!func) goto err;
 result = yf_get_module(func);
 Dee_Decref(func);
 return result;
err:
 return NULL;
}
PRIVATE DREF DeeObject *DCALL
yfi_get_operator(YFIterator *__restrict self) {
 DREF DeeObject *result;
 DREF YFunction *func = yfi_getfunc(self,"__operator__");
 if unlikely(!func) goto err;
 result = yf_get_operator(func);
 Dee_Decref(func);
 return result;
err:
 return NULL;
}
PRIVATE DREF DeeObject *DCALL
yfi_get_operatorname(YFIterator *__restrict self) {
 DREF DeeObject *result;
 DREF YFunction *func = yfi_getfunc(self,"__operatorname__");
 if unlikely(!func) goto err;
 result = yf_get_operatorname(func);
 Dee_Decref(func);
 return result;
err:
 return NULL;
}
PRIVATE DREF DeeObject *DCALL
yfi_get_property(YFIterator *__restrict self) {
 DREF DeeObject *result;
 DREF YFunction *func = yfi_getfunc(self,"__property__");
 if unlikely(!func) goto err;
 result = yf_get_property(func);
 Dee_Decref(func);
 return result;
err:
 return NULL;
}


PRIVATE struct type_getset yfi_getsets[] = {
#ifndef CONFIG_NO_THREADS
    { DeeString_STR(&str_seq), (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_yfunc, NULL, NULL, DOC("->?S?O\nAlias for #__yfunc__") },
#endif /* !CONFIG_NO_THREADS */
    { "__frame__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_frame, NULL, NULL, DOC("->?Dframe\nThe execution stack-frame representing the current state of the iterator") },
    { "__this__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_this, NULL, NULL, DOC("@throw UnboundAttribute No $this-argument available\nThe $this-argument used during execution") },
    { "__yfunc__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_yfunc, NULL, NULL, DOC("->?Ert:yieldfunction\nThe underlying yield-function, describing the :function and arguments that are being executed") },
    { "__func__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_func, NULL, NULL, DOC("->?Dfunction\nThe function that is being executed") },
    { "__code__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_code, NULL, NULL, DOC("->?Dcode\nThe code object that is being executed") },
    { "__refs__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_refs, NULL, NULL, DOC("->?S?O\nReturns a sequence of all of the references used by the function") },
    { "__args__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_args, NULL, NULL, DOC("->?S?O\nReturns a sequence representing the arguments passed to the function") },
    { "__name__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_name, NULL, NULL, DOC("->?Dstring\n->?N\nAlias for :function.__name__ though #__func__") },
    { "__doc__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_doc, NULL, NULL, DOC("->?Dstring\n->?N\nAlias for :function.__doc__ though #__func__") },
    { "__type__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_type, NULL, NULL, DOC("->?Dtype\n->?N\nAlias for :function.__type__ though #__func__") },
    { "__module__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_module, NULL, NULL, DOC("->?Dmodule\nAlias for :function.__module__ though #__func__") },
    { "__operator__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_operator, NULL, NULL, DOC("->?Dint\n->?N\nAlias for :function.__operator__ though #__func__") },
    { "__operatorname__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_operatorname, NULL, NULL, DOC("->?Dstring\n->?Dint\n->?N\nAlias for :function.__operatorname__ though #__func__") },
    { "__property__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_property, NULL, NULL, DOC("->?Dint\n->?N\nAlias for :function.__property__ though #__func__") },
    { "__refs__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_get_refs, NULL, NULL, DOC("->?S?O\nAlias for :function.__refs__ though #__func__") },
    { NULL }
};
#ifdef CONFIG_NO_THREADS
PRIVATE struct type_member yfi_members[] = {
    TYPE_MEMBER_FIELD("seq",STRUCT_OBJECT,offsetof(YFIterator,yi_func)),
    TYPE_MEMBER_END
};
#endif /* CONFIG_NO_THREADS */

PRIVATE struct type_gc yfi_gc = {
    /* .tp_gc = */(void(DCALL *)(DeeObject *__restrict))&yfi_clear
};


PUBLIC DeeTypeObject DeeYieldFunctionIterator_Type = {
    OBJECT_HEAD_INIT(&DeeType_Type),
    /* .tp_name     = */"yieldfunction.iterator",
    /* .tp_doc      = */NULL,
    /* .tp_flags    = */TP_FNORMAL|TP_FFINAL|TP_FGC,
    /* .tp_weakrefs = */0,
    /* .tp_features = */TF_NONE,
    /* .tp_base     = */&DeeIterator_Type,
    /* .tp_init = */{
        {
            /* .tp_alloc = */{
                /* .tp_ctor      = */NULL,
                /* .tp_copy_ctor = */&yfi_copy,
                /* .tp_deep_ctor = */NULL,
                /* .tp_any_ctor  = */&yfi_new,
                /* .tp_free      = */NULL,
                {
                    /* .tp_instance_size = */sizeof(YFIterator)
                }
            }
        },
        /* .tp_dtor        = */(void(DCALL *)(DeeObject *__restrict))&yfi_dtor,
        /* .tp_assign      = */NULL,
        /* .tp_move_assign = */NULL,
        /* .tp_deepload    = */NULL
    },
    /* .tp_cast = */{
        /* .tp_str  = */NULL,
        /* .tp_repr = */NULL,
        /* .tp_bool = */NULL
    },
    /* .tp_call          = */NULL,
    /* .tp_visit         = */(void(DCALL *)(DeeObject *__restrict,dvisit_t,void*))&yfi_visit,
    /* .tp_gc            = */&yfi_gc,
    /* .tp_math          = */NULL,
    /* .tp_cmp           = */NULL,
    /* .tp_seq           = */NULL,
    /* .tp_iter_next     = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&yfi_iter_next,
    /* .tp_attr          = */NULL,
    /* .tp_with          = */NULL,
    /* .tp_buffer        = */NULL,
    /* .tp_methods       = */NULL,
    /* .tp_getsets       = */yfi_getsets,
#ifdef CONFIG_NO_THREADS
    /* .tp_members       = */yfi_members,
#else
    /* .tp_members       = */NULL,
#endif
    /* .tp_class_methods = */NULL,
    /* .tp_class_getsets = */NULL,
    /* .tp_class_members = */NULL
};

DECL_END

#endif /* !GUARD_DEEMON_EXECUTE_FUNCTION_C */
