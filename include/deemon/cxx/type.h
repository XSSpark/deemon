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
#ifndef GUARD_DEEMON_CXX_TYPE_H
#define GUARD_DEEMON_CXX_TYPE_H 1

#include "api.h"
#include "object.h"

DEE_CXX_BEGIN

namespace detail {
class type_base: public object {
public:
    static DeeTypeObject *classtype() DEE_CXX_NOTHROW { return &DeeType_Type; }
    static bool check(DeeObject *__restrict ob) DEE_CXX_NOTHROW { return DeeType_Check(ob); }
    static bool checkexact(DeeObject *__restrict ob) DEE_CXX_NOTHROW { return DeeType_CheckExact(ob); }
public:
    type_base(DeeTypeObject *obj): object((DeeObject *)obj) {}
    DEFINE_OBJECT_CONSTRUCTORS(type_base,object)
    WUNUSED operator DeeTypeObject *() const DEE_CXX_NOTHROW { return (DeeTypeObject *)this->m_ptr; }
    WUNUSED bool baseof(DeeTypeObject *__restrict other) const DEE_CXX_NOTHROW { return DeeType_IsInherited(other,*this); }
    WUNUSED bool derivedfrom(DeeTypeObject *__restrict other) const DEE_CXX_NOTHROW { return DeeType_IsInherited(*this,other); }
    WUNUSED bool hasoperator(uint16_t name) const DEE_CXX_NOTHROW { return DeeType_HasOperator(*this,name); }
    WUNUSED bool hasprivateoperator(uint16_t name) const DEE_CXX_NOTHROW { return DeeType_HasPrivateOperator(*this,name); }
};
}

template<class T>
class type: public detail::type_base {
public: /* type_ from deemon */
    type(DeeTypeObject *obj): type_base(obj) {}
    DEFINE_OBJECT_CONSTRUCTORS(type,type_base)
    T new_() { return inherit(DeeObject_NewDefault(*this)); }
    T new_(size_t argc, DeeObject **__restrict argv) { return inherit(DeeObject_New(*this,argc,argv)); }
    T new_(size_t argc, DeeObject *const *__restrict argv) { return inherit(DeeObject_New(*this,argc,(DeeObject **)argv)); }
    T new_(size_t argc, object **__restrict argv) { return inherit(DeeObject_New(*this,argc,(DeeObject **)argv)); }
    T new_(size_t argc, object *const *__restrict argv) { return inherit(DeeObject_New(*this,argc,(DeeObject **)argv)); }
    T new_(std::initializer_list<DeeObject *> const &args) { return inherit(DeeObject_New(*this,args.size(),(DeeObject **)args.begin())); }
    T new_(size_t argc, DeeObject **__restrict argv, DeeObject *kw) { return inherit(DeeObject_NewKw(*this,argc,argv,kw)); }
    T new_(size_t argc, DeeObject *const *__restrict argv, DeeObject *kw) { return inherit(DeeObject_NewKw(*this,argc,(DeeObject **)argv,kw)); }
    T new_(size_t argc, object **__restrict argv, DeeObject *kw) { return inherit(DeeObject_NewKw(*this,argc,(DeeObject **)argv,kw)); }
    T new_(size_t argc, object *const *__restrict argv, DeeObject *kw) { return inherit(DeeObject_NewKw(*this,argc,(DeeObject **)argv,kw)); }
    T new_(std::initializer_list<DeeObject *> const &args, DeeObject *kw) { return inherit(DeeObject_NewKw(*this,args.size(),(DeeObject **)args.begin(),kw)); }
};

inline WUNUSED NONNULL((1)) obj_nonnull DCALL nonnull(DeeTypeObject *__restrict ptr) { return obj_nonnull((DeeObject *)ptr); }
inline WUNUSED NONNULL((1)) obj_maybenull DCALL maybenull(DeeTypeObject *ptr) { return obj_maybenull((DeeObject *)ptr); }
inline WUNUSED NONNULL((1)) obj_inherited DCALL inherit(DeeTypeObject *__restrict ptr) { return obj_inherited((DeeObject *)ptr); }

#ifdef GUARD_DEEMON_CXX_SUPER_H
inline deemon::type<object> super::supertype() const {
    if (DeeSuper_CheckExact(this->ptr()))
        return nonnull(DeeSuper_TYPE(this->ptr()));
    return nonnull(Dee_TYPE(this->ptr()));
}
#endif /* GUARD_DEEMON_CXX_SUPER_H */

inline deemon::type<object> object::type() const { return nonnull(Dee_TYPE((DeeObject *)*this)); }
inline deemon::type<object> object::class_() const { return nonnull(DeeObject_Class(*this)); }

DEE_CXX_END

#endif /* !GUARD_DEEMON_CXX_TYPE_H */
