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
#ifndef GUARD_DEEMON_CXX_SUPER_H
#define GUARD_DEEMON_CXX_SUPER_H 1

#include "api.h"
#include "object.h"
#include "../super.h"

DEE_CXX_BEGIN

class super: public object {
public:
    static DeeTypeObject *classtype() DEE_CXX_NOTHROW { return &DeeSuper_Type; }
    static bool check(DeeObject *__restrict ob) DEE_CXX_NOTHROW { return DeeSuper_Check(ob); }
    static bool checkexact(DeeObject *__restrict ob) DEE_CXX_NOTHROW { return DeeSuper_CheckExact(ob); }
public:
    DEFINE_OBJECT_CONSTRUCTORS(super,object)
    super(DeeTypeObject *__restrict tp_self, DeeObject *__restrict self)
        : object(inherit(DeeSuper_New(tp_self,self))) {}
    deemon::type<object> supertype() const;
    object const &superself() const {
        if (DeeSuper_CheckExact(this->ptr()))
            return *(object const *)&DeeSuper_SELF(this->ptr());
        return *this;
    }
};



#ifdef GUARD_DEEMON_CXX_TYPE_H
inline deemon::type<object> super::supertype() const {
    if (DeeSuper_CheckExact(this->ptr()))
        return nonnull(DeeSuper_TYPE(this->ptr()));
    return nonnull(Dee_TYPE(this->ptr()));
}
#endif /* GUARD_DEEMON_CXX_TYPE_H */

inline deemon::super object::super() const {
    return inherit(DeeSuper_Of(*this));
}
inline deemon::super object::super(DeeTypeObject *__restrict super_type) const {
    return inherit(DeeSuper_New(super_type,*this));
}
template<class T> inline deemon::super object::super() const {
    return inherit(DeeSuper_New(T::classtype(),*this));
}


DEE_CXX_END

#endif /* !GUARD_DEEMON_CXX_SUPER_H */
