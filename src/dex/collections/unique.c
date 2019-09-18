/* Copyright (c) 2019 Griefer@Work                                            *
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
#ifndef GUARD_DEX_COLLECTIONS_UNIQUE_C
#define GUARD_DEX_COLLECTIONS_UNIQUE_C 1
#define DEE_SOURCE 1

#include "libcollections.h"

DECL_BEGIN

INTERN struct udict_item empty_dict_items[1] = { { NULL, NULL } };
#ifdef __INTELLISENSE__
INTERN struct uset_item empty_set_items[1] = { { NULL } };
#else
#define empty_set_items ((struct uset_item *)empty_dict_items)
#endif

DECL_END

#endif /* !GUARD_DEX_COLLECTIONS_UNIQUE_C */
