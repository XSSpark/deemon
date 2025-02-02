/* Copyright (c) 2018-2023 Griefer@Work                                       *
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
 *    in a product, an acknowledgement (see the following) in the product     *
 *    documentation is required:                                              *
 *    Portions Copyright (c) 2018-2023 Griefer@Work                           *
 * 2. Altered source versions must be plainly marked as such, and must not be *
 *    misrepresented as being the original software.                          *
 * 3. This notice may not be removed or altered from any source distribution. *
 */
#ifndef GUARD_DEX_COLLECTIONS_LIBCOLLECTIONS_C
#define GUARD_DEX_COLLECTIONS_LIBCOLLECTIONS_C 1
#define DEE_SOURCE
#define _KOS_SOURCE 1

#include "libcollections.h"
/**/

#include <deemon/api.h>
#include <deemon/dex.h>
#include <deemon/error.h>
#include <deemon/format.h>

DECL_BEGIN

INTERN ATTR_COLD NONNULL((1)) int DCALL
err_changed_sequence(DeeObject *__restrict seq) {
	ASSERT_OBJECT(seq);
	return DeeError_Throwf(&DeeError_RuntimeError,
	                       "A sequence `%k' has changed while being iterated: `%k'",
	                       Dee_TYPE(seq), seq);
}

INTERN ATTR_COLD NONNULL((1)) int DCALL
err_empty_sequence(DeeObject *__restrict seq) {
	ASSERT_OBJECT(seq);
	return DeeError_Throwf(&DeeError_ValueError,
	                       "Empty sequence of type `%k' encountered",
	                       Dee_TYPE(seq));
}

INTERN ATTR_COLD NONNULL((1)) int DCALL
err_index_out_of_bounds(DeeObject *__restrict self,
                        size_t index, size_t size) {
	ASSERT_OBJECT(self);
	ASSERT(index >= size);
	return DeeError_Throwf(&DeeError_IndexError,
	                       "Index `%" PRFuSIZ "' lies outside the valid bounds "
	                       "`0...%" PRFuSIZ "' of sequence of type `%k'",
	                       index, size, Dee_TYPE(self));
}

INTERN ATTR_COLD NONNULL((1)) int DCALL
err_unbound_index(DeeObject *__restrict self, size_t index) {
	ASSERT_OBJECT(self);
	return DeeError_Throwf(&DeeError_UnboundItem,
	                       "Index `%" PRFuSIZ "' of instance of `%k': %k has not been bound",
	                       index, Dee_TYPE(self), self);
}

INTERN ATTR_COLD NONNULL((1, 2)) int DCALL
err_unknown_key(DeeObject *__restrict map, DeeObject *__restrict key) {
	ASSERT_OBJECT(map);
	ASSERT_OBJECT(key);
	return DeeError_Throwf(&DeeError_KeyError,
	                       "Could not find key `%k' in %k `%k'",
	                       key, Dee_TYPE(map), map);
}


PRIVATE struct dex_symbol symbols[] = {
	{ "Deque", (DeeObject *)&Deque_Type },
	{ "FixedList", (DeeObject *)&FixedList_Type },
	{ "UniqueDict", (DeeObject *)&UDict_Type },
	{ "UniqueSet", (DeeObject *)&USet_Type },
	{ NULL }
};

PUBLIC struct dex DEX = {
	/* .d_symbols = */ symbols
};

DECL_END


#endif /* !GUARD_DEX_COLLECTIONS_LIBCOLLECTIONS_C */
