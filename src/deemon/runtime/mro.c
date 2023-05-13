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
#ifndef GUARD_DEEMON_RUNTIME_MRO_C
#define GUARD_DEEMON_RUNTIME_MRO_C 1
#define DEE_SOURCE

#include <deemon/alloc.h>
#include <deemon/api.h>
#include <deemon/arg.h>
#include <deemon/attribute.h>
#include <deemon/class.h>
#include <deemon/error.h>
#include <deemon/file.h>
#include <deemon/instancemethod.h>
#include <deemon/mro.h>
#include <deemon/none.h>
#include <deemon/object.h>
#include <deemon/objmethod.h>
#include <deemon/string.h>
#include <deemon/system-features.h> /* bcmpc(), ... */
#include <deemon/tuple.h>
#include <deemon/util/atomic.h>
#include <deemon/util/lock.h>

#include <hybrid/sequence/list.h>
#include <hybrid/typecore.h>

#include <stddef.h>

#include "runtime_error.h"
#include "strings.h"

DECL_BEGIN

#undef byte_t
#define byte_t __BYTE_TYPE__

#ifndef CONFIG_HAVE_strcmp
#define CONFIG_HAVE_strcmp
#undef strcmp
#define strcmp dee_strcmp
DeeSystem_DEFINE_strcmp(dee_strcmp)
#endif /* !CONFIG_HAVE_strcmp */

#ifndef NDEBUG
#define DBG_memset (void)memset
#else /* !NDEBUG */
#define DBG_memset(dst, byte, n_bytes) (void)0
#endif /* NDEBUG */

#ifndef CONFIG_NO_THREADS
PRIVATE Dee_atomic_lock_t membercache_list_lock = DEE_ATOMIC_LOCK_INIT;
#endif /* !CONFIG_NO_THREADS */

#define membercache_list_lock_available()  Dee_atomic_lock_available(&membercache_list_lock)
#define membercache_list_lock_acquired()   Dee_atomic_lock_acquired(&membercache_list_lock)
#define membercache_list_lock_tryacquire() Dee_atomic_lock_tryacquire(&membercache_list_lock)
#define membercache_list_lock_acquire()    Dee_atomic_lock_acquire(&membercache_list_lock)
#define membercache_list_lock_waitfor()    Dee_atomic_lock_waitfor(&membercache_list_lock)
#define membercache_list_lock_release()    Dee_atomic_lock_release(&membercache_list_lock)

LIST_HEAD(membercache_list_struct, Dee_membercache);

/* [0..1][lock(membercache_list_lock)]
 * Linked list of all existing type-member caches. */
PRIVATE struct membercache_list_struct
membercache_list = LIST_HEAD_INITIALIZER(membercache_list);

#define streq(a, b) (strcmp(a, b) == 0)
LOCAL NONNULL((1, 2)) bool DCALL
streq_len(char const *zero_zterminated,
          char const *compare_to, size_t compare_to_length) {
	/* TODO: return strcmpz(zero_zterminated, compare_to, compare_to_length) == 0; */
	if (strlen(zero_zterminated) != compare_to_length)
		return false;
	return bcmpc(zero_zterminated, compare_to,
	             compare_to_length, sizeof(char)) == 0;
}



/* Finalize a given member-cache. */
INTERN NONNULL((1)) void DCALL
Dee_membercache_fini(struct Dee_membercache *__restrict self) {
	if (LIST_ISBOUND(self, mc_link)) {
		membercache_list_lock_acquire();
		if (LIST_ISBOUND(self, mc_link))
			LIST_REMOVE(self, mc_link);
		membercache_list_lock_release();
	}
	Dee_Free(self->mc_table);
	DBG_memset(self, 0xcc, sizeof(*self));
}

INTERN size_t DCALL
Dee_membercache_clearall(size_t max_clear) {
	size_t result = 0;
	struct Dee_membercache *cache;
again:
	membercache_list_lock_acquire();
	if (LIST_EMPTY(&membercache_list)) {
		membercache_list_lock_release();
	} else {
		DREF struct Dee_membercache_table *table;
		cache = LIST_FIRST(&membercache_list);

		/* Steal the table and wait for everyone to stop using it. */
		table = atomic_xch(&cache->mc_table, NULL);
		Dee_membercache_waitfor(cache);

		/* Pop this entry from the global chain. */
		LIST_UNBIND(cache, mc_link);
		membercache_list_lock_release();

		if (table) {
			/* Track how much member this operation will be freeing up. */
			result += (table->mc_mask + 1) * sizeof(struct Dee_membercache);

			/* Drop the table reference that was held by the type. */
			Dee_membercache_table_decref(table);
		}

		if (result < max_clear)
			goto again;
	}
	return result;
}


/* Try to allocate a new member-cache table. */
#define Dee_membercache_table_trycalloc(mask)                                                        \
	(struct Dee_membercache_table *)Dee_TryCalloc(offsetof(struct Dee_membercache_table, mc_table) + \
	                                              ((mask) + 1) * sizeof(struct Dee_membercache_slot))
STATIC_ASSERT(MEMBERCACHE_UNUSED == 0);

PRIVATE NONNULL((1, 2)) void DCALL
Dee_membercache_table_do_addslot(struct Dee_membercache_table *__restrict self,
                                 struct Dee_membercache_slot const *__restrict item) {
	dhash_t i, perturb;
	struct Dee_membercache_slot *slot;
	perturb = i = Dee_membercache_table_hashst(self, item->mcs_hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		slot = Dee_membercache_table_hashit(self, i);
		if (slot->mcs_type == MEMBERCACHE_UNUSED)
			break;
		ASSERTF(slot->mcs_type != MEMBERCACHE_UNINITIALIZED,
		        "We're building a new cache-table, so how can "
		        "that new table contain UNINITIALIZED items?");
	}

	/* Found a free slot -> write to it! */
	memcpy(slot, item, sizeof(struct Dee_membercache_slot));
	++self->mc_size;
}

/* Try to construct a new member-cache table */
PRIVATE WUNUSED NONNULL((2)) DREF struct Dee_membercache_table *DCALL
Dee_membercache_table_new(struct Dee_membercache_table const *old_table,
                          struct Dee_membercache_slot const *__restrict item) {
	DREF struct Dee_membercache_table *result;
	size_t new_mask = 0;
	if (old_table != NULL)
		new_mask = old_table->mc_mask;
	new_mask = (new_mask << 1) | 1;
	if unlikely(new_mask == 1)
		new_mask = 16 - 1; /* Start out bigger than 2. */
	result = Dee_membercache_table_trycalloc(new_mask);
	if unlikely(!result)
		return NULL;

	/* Fill in the basic characteristics of the new cache-table. */
	result->mc_mask   = new_mask;
	result->mc_refcnt = 1;
	ASSERTF(result->mc_size == 0, "Should be the case because of calloc()");

	/* Migrate the old cache-table into the new one. */
	if (old_table != NULL) {
		size_t i;
		for (i = 0; i <= old_table->mc_mask; ++i) {
			uint16_t type;
			struct Dee_membercache_slot const *slot;
			slot = &old_table->mc_table[i];
			type = atomic_read(&slot->mcs_type);
			if (type == MEMBERCACHE_UNUSED)
				continue;
			if (type == MEMBERCACHE_UNINITIALIZED)
				continue; /* Don't migrate slots that aren't fully initialized. */

			/* Keep this cache-slot as part of the resulting cache-table. */
			Dee_membercache_table_do_addslot(result, slot);
		}
	}

	/* Add the caller's new item. */
	Dee_membercache_table_do_addslot(result, item);
	return result;
}


/* Try to add a slot to the given member-cache table.
 * @return: true:  Success
 * @return: false: No more free slots (caller must allocate a new member-cache table) */
PRIVATE NONNULL((1, 2)) bool DCALL
Dee_membercache_table_addslot(struct Dee_membercache_table *__restrict self,
                              struct Dee_membercache_slot const *__restrict item,
                              bool allow_bad_hash_ratios) {
	dhash_t i, perturb;
	struct Dee_membercache_slot *slot;

	/* Try to allocate a slot within the table. */
	for (;;) {
		size_t size = atomic_read(&self->mc_size);
		if (allow_bad_hash_ratios ? (size * 1) >= self->mc_mask
		                          : (size * 2) >= self->mc_mask)
			return false; /* You should (or need to) allocate a new table. */
		if (atomic_cmpxch_or_write(&self->mc_size, size, size + 1))
			break;
	}

	/* Re-check that the named attribute isn't already in-cache. */
again_search_slots:
	perturb = i = Dee_membercache_table_hashst(self, item->mcs_hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		uint16_t type;
		slot = Dee_membercache_table_hashit(self, i);
		type = atomic_read(&slot->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
#ifndef CONFIG_NO_THREADS
		if (type == MEMBERCACHE_UNINITIALIZED) {
			/* Encountered an uninitialized slot along the cache-chain.
			 *
			 * This means that some other thread is currently writing an
			 * item into the cache-table that has a similar hash to the
			 * item that we're writing right now.
			 *
			 * This means that there is a chance that the other thread
			 * is caching the same item as we are, and we have to make
			 * such that no member is cached twice within the same table,
			 * so to prevent any chance of that happening, we have to
			 * assume the worst, and believe that the other thread _is_
			 * trying to cache the same item as we are.
			 *
			 * As such, the only course of action we have left is to tell
			 * our caller that their new element is already cached, even
			 * if that *may* not actually be the case. */
			atomic_dec(&self->mc_size);
			return true;
		}
#endif /* !CONFIG_NO_THREADS */
		if (slot->mcs_hash != item->mcs_hash)
			continue;
		if (slot->mcs_method.m_name != item->mcs_name &&
		    !streq(slot->mcs_name, item->mcs_name))
			continue;

		/* Already in cache!
		 * -> Free the slot we allocated above and indicate success to our caller. */
		atomic_dec(&self->mc_size);
		return true;
	}

	/* Not found. - Try to allocate this empty slot.
	 * If it's no longer empty, */
	if (!atomic_cmpxch_or_write(&slot->mcs_type,
	                            MEMBERCACHE_UNUSED,
	                            MEMBERCACHE_UNINITIALIZED))
		goto again_search_slots;

	/* At this point, we've successfully allocated a slot within the cache-table.
	 * With that in mind, we must now fill in that slot, but we have to be careful
	 * about the order here, in that we _MUST_ fill in the type-field LAST!
	 *
	 * This is because by setting the type-field to something other than
	 * `MEMBERCACHE_UNINITIALIZED', we essentially commit the new slot
	 * into the cache. */
	memcpy((byte_t *)slot + COMPILER_OFFSETAFTER(struct Dee_membercache_slot, mcs_type),
	       (byte_t *)item + COMPILER_OFFSETAFTER(struct Dee_membercache_slot, mcs_type),
	       sizeof(struct Dee_membercache_slot) -
	       COMPILER_OFFSETAFTER(struct Dee_membercache_slot, mcs_type));
	COMPILER_WRITE_BARRIER(); /* The type-field must be written last! */
	atomic_write(&slot->mcs_type, item->mcs_type);
	return true;
}


PRIVATE NONNULL((1, 2)) bool DCALL
Dee_membercache_addslot(struct Dee_membercache *__restrict self,
                        struct Dee_membercache_slot const *__restrict slot) {
	DREF struct Dee_membercache_table *old_table;
	DREF struct Dee_membercache_table *new_table;

	/* Get a reference to the current table. */
	Dee_membercache_tabuse_inc(self);
	old_table = atomic_read(&self->mc_table);
	if (old_table != NULL) {
		Dee_membercache_table_incref(old_table);
		Dee_membercache_tabuse_dec(self);

		/* Try to add the slot to an existing cache-table. */
do_operate_with_old_table:
		if (Dee_membercache_table_addslot(old_table, slot, false)) {
			Dee_membercache_table_decref(old_table);
			return true;
		}
	} else {
		Dee_membercache_tabuse_dec(self);
	}

	/* Either there isn't a table, or we're unable to add more items
	 * to the table. In either case, try to construct a new table! */
	new_table = Dee_membercache_table_new(old_table, slot);
	if (!new_table) {
		/* Failed to create a new table -> try again to add to the
		 * existing table, but ignore bad hash characteristics this
		 * time around. */
		bool result = false;
		if (old_table != NULL) {
			/* It doesn't matter if this addslot() call succeeds or not... */
			result = Dee_membercache_table_addslot(old_table, slot, true);
			Dee_membercache_table_decref(old_table);
		}
		return result;
	}

	/* Drop our original reference to the old table. */
	if (old_table != NULL)
		Dee_membercache_table_decref(old_table);

	/* Got a new table -> now to store it within the type.
	 * But: do one more check if the currently set table
	 *      might actually be better than our `new_table' */
	if (atomic_read(&self->mc_table) != old_table) {
		Dee_membercache_tabuse_inc(self);
		old_table = atomic_read(&self->mc_table);
		if (old_table == NULL) {
			Dee_membercache_tabuse_dec(self);
		} else {
			Dee_membercache_table_incref(old_table);
			Dee_membercache_tabuse_dec(self);

			/* Try not to override a larger table with a smaller one! */
			if (old_table->mc_mask > new_table->mc_mask) {
				Dee_membercache_table_destroy(new_table);
				goto do_operate_with_old_table;
			}
			Dee_membercache_table_decref(old_table);
		}
	}

	/* Store our new table within the member-cache. */
	old_table = atomic_xch(&self->mc_table, new_table); /* Inherit reference */
	Dee_membercache_waitfor(self);

	/* Destroy the old table reference. */
	if (old_table)
		Dee_membercache_table_decref(old_table);

	/* Make sure that the member-cache controller is linked into the global list. */
	if (!LIST_ISBOUND(self, mc_link)) {
		membercache_list_lock_acquire();
		if likely(!LIST_ISBOUND(self, mc_link))
			LIST_INSERT_HEAD(&membercache_list, self, mc_link);
		membercache_list_lock_release();
	}

	return true;
}



#define PRIVATE_IS_KNOWN_TYPETYPE(x) \
	((x) == &DeeType_Type || (x) == &DeeFileType_Type)
#define MEMBERCACHE_GETTYPENAME(x)                                                                 \
	(PRIVATE_IS_KNOWN_TYPETYPE(COMPILER_CONTAINER_OF(x, DeeTypeObject, tp_cache)->ob_type)         \
	 ? COMPILER_CONTAINER_OF(x, DeeTypeObject, tp_cache)->tp_name                                  \
	 : PRIVATE_IS_KNOWN_TYPETYPE(COMPILER_CONTAINER_OF(x, DeeTypeObject, tp_class_cache)->ob_type) \
	   ? COMPILER_CONTAINER_OF(x, DeeTypeObject, tp_class_cache)->tp_name                          \
	   : "?")


INTERN NONNULL((1, 2, 4)) bool DCALL
Dee_membercache_addmethod(struct Dee_membercache *self,
                          DeeTypeObject *decl, dhash_t hash,
                          struct type_method const *method) {
	struct Dee_membercache_slot slot;
	Dee_DPRINTF("[RT] Caching method `%s.%s' in `%s'\n",
	            decl->tp_name, method->m_name,
	            MEMBERCACHE_GETTYPENAME(self));
	slot.mcs_type = MEMBERCACHE_METHOD;
	slot.mcs_hash = hash;
	slot.mcs_decl = decl;
	memcpy(&slot.mcs_method, method, sizeof(struct type_method));
	return Dee_membercache_addslot(self, &slot);
}

INTERN NONNULL((1, 2, 4)) bool DCALL
Dee_membercache_addinstancemethod(struct Dee_membercache *self,
                                  DeeTypeObject *decl, dhash_t hash,
                                  struct type_method const *method) {
	struct Dee_membercache_slot slot;
	ASSERT(self != &decl->tp_cache);
	Dee_DPRINTF("[RT] Caching instance_method `%s.%s' in `%s'\n",
	            decl->tp_name, method->m_name,
	            MEMBERCACHE_GETTYPENAME(self));
	slot.mcs_type = MEMBERCACHE_INSTANCE_METHOD;
	slot.mcs_hash = hash;
	slot.mcs_decl = decl;
	memcpy(&slot.mcs_method, method, sizeof(struct type_method));
	return Dee_membercache_addslot(self, &slot);
}

INTERN NONNULL((1, 2, 4)) bool DCALL
Dee_membercache_addgetset(struct Dee_membercache *self,
                          DeeTypeObject *decl, dhash_t hash,
                          struct type_getset const *getset) {
	struct Dee_membercache_slot slot;
	Dee_DPRINTF("[RT] Caching getset `%s.%s' in `%s'\n",
	            decl->tp_name, getset->gs_name,
	            MEMBERCACHE_GETTYPENAME(self));
	slot.mcs_type = MEMBERCACHE_GETSET;
	slot.mcs_hash = hash;
	slot.mcs_decl = decl;
	memcpy(&slot.mcs_getset, getset, sizeof(struct type_getset));
	return Dee_membercache_addslot(self, &slot);
}

INTERN NONNULL((1, 2, 4)) bool DCALL
Dee_membercache_addinstancegetset(struct Dee_membercache *self,
                                  DeeTypeObject *decl, dhash_t hash,
                                  struct type_getset const *getset) {
	struct Dee_membercache_slot slot;
	Dee_DPRINTF("[RT] Caching instance_getset `%s.%s' in `%s'\n",
	            decl->tp_name, getset->gs_name,
	            MEMBERCACHE_GETTYPENAME(self));
	ASSERT(self != &decl->tp_cache);
	slot.mcs_type = MEMBERCACHE_INSTANCE_GETSET;
	slot.mcs_hash = hash;
	slot.mcs_decl = decl;
	memcpy(&slot.mcs_getset, getset, sizeof(struct type_getset));
	return Dee_membercache_addslot(self, &slot);
}

INTERN NONNULL((1, 2, 4)) bool DCALL
Dee_membercache_addmember(struct Dee_membercache *self,
                          DeeTypeObject *decl, dhash_t hash,
                          struct type_member const *member) {
	struct Dee_membercache_slot slot;
	Dee_DPRINTF("[RT] Caching member `%s.%s' in `%s'\n",
	            decl->tp_name, member->m_name,
	            MEMBERCACHE_GETTYPENAME(self));
	slot.mcs_type = MEMBERCACHE_MEMBER;
	slot.mcs_hash = hash;
	slot.mcs_decl = decl;
	memcpy(&slot.mcs_member, member, sizeof(struct type_member));
	return Dee_membercache_addslot(self, &slot);
}

INTERN NONNULL((1, 2, 4)) bool DCALL
Dee_membercache_addinstancemember(struct Dee_membercache *self,
                                  DeeTypeObject *decl, dhash_t hash,
                                  struct type_member const *member) {
	struct Dee_membercache_slot slot;
	Dee_DPRINTF("[RT] Caching instance_member `%s.%s' in `%s'\n",
	            decl->tp_name, member->m_name,
	            MEMBERCACHE_GETTYPENAME(self));
	ASSERT(self != &decl->tp_cache);
	slot.mcs_type = MEMBERCACHE_INSTANCE_MEMBER;
	slot.mcs_hash = hash;
	slot.mcs_decl = decl;
	memcpy(&slot.mcs_member, member, sizeof(struct type_member));
	return Dee_membercache_addslot(self, &slot);
}

INTERN NONNULL((1, 2, 4)) bool DCALL
Dee_membercache_addattrib(struct Dee_membercache *self,
                          DeeTypeObject *decl, dhash_t hash,
                          struct class_attribute *attrib) {
	struct Dee_membercache_slot slot;
	char const *name = DeeString_STR(attrib->ca_name);
	Dee_DPRINTF("[RT] Caching attribute `%s.%s' in `%s'\n",
	            decl->tp_name, name,
	            MEMBERCACHE_GETTYPENAME(self));
	slot.mcs_type          = MEMBERCACHE_ATTRIB;
	slot.mcs_hash          = hash;
	slot.mcs_decl          = decl;
	slot.mcs_attrib.a_name = name;
	slot.mcs_attrib.a_attr = attrib;
	slot.mcs_attrib.a_desc = DeeClass_DESC(decl);
	return Dee_membercache_addslot(self, &slot);
}

INTERN NONNULL((1, 2, 4)) bool DCALL
Dee_membercache_addinstanceattrib(struct Dee_membercache *self,
                                  DeeTypeObject *decl, dhash_t hash,
                                  struct class_attribute *attrib) {
	struct Dee_membercache_slot slot;
	char const *name = DeeString_STR(attrib->ca_name);
	Dee_DPRINTF("[RT] Caching instance_attribute `%s.%s' in `%s'\n",
	            decl->tp_name, name,
	            MEMBERCACHE_GETTYPENAME(self));
	ASSERT(self != &decl->tp_cache);
	slot.mcs_type          = MEMBERCACHE_INSTANCE_ATTRIB;
	slot.mcs_hash          = hash;
	slot.mcs_decl          = decl;
	slot.mcs_attrib.a_name = name;
	slot.mcs_attrib.a_attr = attrib;
	slot.mcs_attrib.a_desc = DeeClass_DESC(decl);
	return Dee_membercache_addslot(self, &slot);
}

#ifndef __INTELLISENSE__
#define MRO_LEN
#include "mro-impl.c.inl"
/**/
#include "mro-impl.c.inl"
#endif /* !__INTELLISENSE__ */

DECL_END

#endif /* !GUARD_DEEMON_RUNTIME_MRO_C */
