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
#ifndef GUARD_DEEMON_OBJECTS_INSTANCEMETHOD_C
#define GUARD_DEEMON_OBJECTS_INSTANCEMETHOD_C 1

#include <deemon/api.h>
#include <deemon/class.h>
#include <deemon/object.h>
#include <deemon/arg.h>
#include <deemon/callable.h>
#include <deemon/instancemethod.h>
#include <deemon/super.h>
#include <deemon/bool.h>
#include <deemon/string.h>
#include <deemon/none.h>
#include <deemon/util/cache.h>

#include "../runtime/strings.h"

DECL_BEGIN

typedef DeeInstanceMethodObject InstanceMethod;

/* Since `super' and `instancemethod' objects share the same
 * size, we also let them share a pool of pre-allocated objects. */
STATIC_ASSERT(sizeof(DeeSuperObject) == sizeof(InstanceMethod));
DECLARE_OBJECT_CACHE(super,DeeSuperObject);
#ifndef NDEBUG
#define super_alloc()  super_dbgalloc(__FILE__,__LINE__)
#endif
#define instance_method_alloc     (InstanceMethod *)super_alloc
#define instance_method_free(p)   super_free((DeeSuperObject *)(p))
#define instance_method_tp_alloc  super_tp_alloc
#define instance_method_tp_free   super_tp_free

PUBLIC DREF DeeObject *DCALL
DeeInstanceMethod_New(DeeObject *__restrict func,
                      DeeObject *__restrict this_arg) {
 DREF InstanceMethod *result;
 ASSERT_OBJECT(func);
 ASSERT_OBJECT(this_arg);
 result = instance_method_alloc();
 if unlikely(!result) return NULL;
 DeeObject_Init(result,&DeeInstanceMethod_Type);
 result->im_func = func;
 result->im_self = this_arg;
 Dee_Incref(func);
 Dee_Incref(this_arg);
 return (DREF DeeObject *)result;
}

PRIVATE int DCALL
im_ctor(InstanceMethod *__restrict self) {
 /* Initialize a stub instance-method. */
 self->im_func = Dee_None;
 self->im_self = Dee_None;
#ifdef CONFIG_NO_THREADS
 Dee_None->ob_refcnt += 2;
#else
 ATOMIC_FETCHADD(Dee_None->ob_refcnt,2);
#endif
 return 0;
}

PRIVATE int DCALL
im_copy(InstanceMethod *__restrict self,
        InstanceMethod *__restrict other) {
 self->im_self = other->im_self;
 self->im_func = other->im_func;
 Dee_Incref(self->im_self);
 Dee_Incref(self->im_func);
 return 0;
}

PRIVATE int DCALL
im_deepcopy(InstanceMethod *__restrict self,
            InstanceMethod *__restrict other) {
 self->im_self = DeeObject_DeepCopy(other->im_self);
 if unlikely(!self->im_self) return -1;
 self->im_func = DeeObject_DeepCopy(other->im_func);
 if unlikely(!self->im_func) { Dee_Decref(self->im_self); return -1; }
 return 0;
}

PRIVATE int DCALL
im_init(InstanceMethod *__restrict self,
        size_t argc, DeeObject **__restrict argv,
        DeeObject *kw) {
 DeeObject *thisarg,*func;
 PRIVATE struct keyword kwlist[] = { K(func), K(thisarg), KEND };
 if (DeeArg_UnpackKw(argc,argv,kw,kwlist,"oo:instancemethod",&func,&thisarg))
     return -1;
 self->im_self = thisarg;
 self->im_func = func;
 Dee_Incref(thisarg);
 Dee_Incref(func);
 return 0;
}

PRIVATE DREF DeeObject *DCALL
im_repr(InstanceMethod *__restrict self) {
 return DeeString_Newf("instancemethod(func: %r, thisarg: %r)",self->im_func,self->im_self);
}

PRIVATE DREF DeeObject *DCALL
im_call(InstanceMethod *__restrict self,
        size_t argc, DeeObject **__restrict argv) {
 return DeeObject_ThisCall(self->im_func,self->im_self,argc,argv);
}
PRIVATE DREF DeeObject *DCALL
im_callkw(InstanceMethod *__restrict self, size_t argc,
          DeeObject **__restrict argv, DeeObject *kw) {
 return DeeObject_ThisCallKw(self->im_func,self->im_self,argc,argv,kw);
}
PRIVATE dhash_t DCALL
im_hash(InstanceMethod *__restrict self) {
 return DeeObject_Hash(self->im_func) ^ DeeObject_Hash(self->im_self);
}

PRIVATE DREF DeeObject *DCALL
im_eq(InstanceMethod *__restrict self,
      InstanceMethod *__restrict other) {
 int temp;
 if (DeeObject_AssertType((DeeObject *)other,&DeeInstanceMethod_Type))
     return NULL;
 temp = DeeObject_CompareEq(self->im_func,other->im_func);
 if (temp <= 0) return unlikely(temp < 0) ? NULL : (Dee_Incref(Dee_False),Dee_False);
 temp = DeeObject_CompareEq(self->im_self,other->im_self);
 if unlikely(temp < 0) return NULL;
 return_bool_(temp);
}
PRIVATE DREF DeeObject *DCALL
im_ne(InstanceMethod *__restrict self,
      InstanceMethod *__restrict other) {
 int temp;
 if (DeeObject_AssertType((DeeObject *)other,&DeeInstanceMethod_Type))
     return NULL;
 temp = DeeObject_CompareNe(self->im_func,other->im_func);
 if (temp != 0) return unlikely(temp < 0) ? NULL : (Dee_Incref(Dee_True),Dee_True);
 temp = DeeObject_CompareNe(self->im_self,other->im_self);
 if unlikely(temp < 0) return NULL;
 return_bool_(temp);
}

PRIVATE struct type_cmp im_cmp = {
    /* .tp_hash = */(dhash_t(DCALL *)(DeeObject *__restrict))&im_hash,
    /* .tp_eq   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&im_eq,
    /* .tp_ne   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&im_ne,
};


/* Since `super' and `instancemethod' share an identical
 * layout, we can re-use some operators here... */
INTDEF void DCALL super_fini(DeeSuperObject *__restrict self);
INTDEF void DCALL super_visit(DeeSuperObject *__restrict self, dvisit_t proc, void *arg);
#define im_fini   super_fini
#define im_visit  super_visit

PRIVATE struct class_attribute *DCALL
instancemethod_getattr(InstanceMethod *__restrict self,
                       uint16_t *pgetset_index,
                       DeeTypeObject **pdecl_type) {
 struct class_attribute *result;
 DeeTypeObject *tp_iter;
 tp_iter = Dee_TYPE(self->im_self);
 do {
  DeeClassDescriptorObject *desc;
  struct class_desc *my_class;
  uint16_t addr; size_t i;
  if (!DeeType_IsClass(tp_iter)) break;
  my_class = DeeClass_DESC(tp_iter);
  desc = my_class->cd_desc;
  rwlock_read(&my_class->cd_lock);
  for (addr = 0; addr < desc->cd_cmemb_size; ++addr) {
   if (my_class->cd_members[addr] != self->im_func)
       continue;
   rwlock_endread(&my_class->cd_lock);
   /* Found the address at which the function is located. */
   for (i = 0; i <= desc->cd_iattr_mask; ++i) {
    result = &desc->cd_iattr_list[i];
    if (!result->ca_name) continue;
    if (addr < result->ca_addr) continue;
    if ((result->ca_flag & (CLASS_ATTRIBUTE_FCLASSMEM|CLASS_ATTRIBUTE_FMETHOD)) !=
                           (CLASS_ATTRIBUTE_FCLASSMEM|CLASS_ATTRIBUTE_FMETHOD))
        continue;
    if (result->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
     if (addr >= result->ca_addr + 3) continue;
     if (pgetset_index) *pgetset_index = (uint16_t)(addr - result->ca_addr);
    } else {
     if (addr > result->ca_addr) continue;
     if (pgetset_index) *pgetset_index = 0;
    }
    if (pdecl_type) *pdecl_type = tp_iter;
    return result;
   }
  }
  rwlock_endread(&my_class->cd_lock);
 } while ((tp_iter = DeeType_Base(tp_iter)) != NULL);
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
instancemethod_get_name(InstanceMethod *__restrict self) {
 struct class_attribute *attr;
 attr = instancemethod_getattr(self,NULL,NULL);
 if (!attr) return_none;
 return_reference_((DeeObject *)attr->ca_name);
}

PRIVATE DREF DeeObject *DCALL
instancemethod_get_doc(InstanceMethod *__restrict self) {
 struct class_attribute *attr;
 attr = instancemethod_getattr(self,NULL,NULL);
 if (!attr || !attr->ca_doc) return_none;
 return_reference_((DeeObject *)attr->ca_doc);
}

PRIVATE DREF DeeTypeObject *DCALL
instancemethod_get_type(InstanceMethod *__restrict self) {
 DeeTypeObject *result;
 if (!instancemethod_getattr(self,NULL,&result))
      result = (DeeTypeObject *)Dee_None;
 return_reference_(result);
}

PRIVATE struct type_getset im_getsets[] = {
    { "__name__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&instancemethod_get_name, NULL, NULL,
      DOC("->string\n"
          "->none\n"
          "The name of the function being bound, or :none if unknown") },
    { "__doc__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&instancemethod_get_doc, NULL, NULL,
      DOC("->string\n"
          "->none\n"
          "The documentation string of the function being bound, or :none if unknown") },
    { "__type__", (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&instancemethod_get_type, NULL, NULL,
      DOC("->type\n"
          "->none\n"
          "The type implementing the function that is being bound, or :none if unknown") },
    { NULL }
};

PRIVATE struct type_member im_members[] = {
    TYPE_MEMBER_FIELD_DOC("__self__",STRUCT_OBJECT,offsetof(InstanceMethod,im_self),
                          "->object\nThe object to which @this :instancemethod is bound"),
    TYPE_MEMBER_FIELD_DOC("__func__",STRUCT_OBJECT,offsetof(InstanceMethod,im_func),
                          "->callable\n"
                          "The unbound class-function that is being bound by this :instancemethod"),
    TYPE_MEMBER_END
};


PUBLIC DeeTypeObject DeeInstanceMethod_Type = {
    OBJECT_HEAD_INIT(&DeeType_Type),
    /* .tp_name     = */DeeString_STR(&str_instancemethod),
    /* .tp_doc      = */DOC("(callable func,object thisarg)\n"
                            "Construct an object-bound instance method that can be used to invoke @func\n"
                            "\n"
                            "call(args...)\n"
                            "Invoke the $func used to construct @this "
                            "instancemethod as ${func(thisarg,args...)}"),
    /* .tp_flags    = */TP_FNORMAL|TP_FNAMEOBJECT,
    /* .tp_weakrefs = */0,
    /* .tp_features = */TF_NONE,
    /* .tp_base     = */&DeeCallable_Type,
    /* .tp_init = */{
        {
            /* .tp_alloc = */{
                /* .tp_ctor        = */&im_ctor,
                /* .tp_copy_ctor   = */&im_copy,
                /* .tp_deep_ctor   = */&im_deepcopy,
                /* .tp_any_ctor    = */NULL,
                TYPE_ALLOCATOR(&instance_method_tp_alloc,
                               &instance_method_tp_free),
                /* .tp_any_ctor_kw = */&im_init,
            }
        },
        /* .tp_dtor        = */(void(DCALL *)(DeeObject *__restrict))&im_fini,
        /* .tp_assign      = */NULL,
        /* .tp_move_assign = */NULL
    },
    /* .tp_cast = */{
        /* .tp_str  = */NULL,
        /* .tp_repr = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&im_repr,
        /* .tp_bool = */NULL
    },
    /* .tp_call          = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&im_call,
    /* .tp_visit         = */(void(DCALL *)(DeeObject *__restrict,dvisit_t,void*))&im_visit,
    /* .tp_gc            = */NULL,
    /* .tp_math          = */NULL,
    /* .tp_cmp           = */&im_cmp,
    /* .tp_seq           = */NULL,
    /* .tp_iter_next     = */NULL,
    /* .tp_attr          = */NULL,
    /* .tp_with          = */NULL,
    /* .tp_buffer        = */NULL,
    /* .tp_methods       = */NULL,
    /* .tp_getsets       = */NULL,
    /* .tp_members       = */im_members,
    /* .tp_class_methods = */NULL,
    /* .tp_class_getsets = */NULL,
    /* .tp_class_members = */NULL,
    /* .tp_call_kw       = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict,DeeObject*))&im_callkw
};

DECL_END

#endif /* !GUARD_DEEMON_OBJECTS_INSTANCEMETHOD_C */
