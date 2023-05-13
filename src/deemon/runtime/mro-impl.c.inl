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
#ifdef __INTELLISENSE__
#include "mro.c"
#define MRO_LEN
#endif /* __INTELLISENSE__ */

DECL_BEGIN

#ifdef MRO_LEN
#define S(without_len, with_len) with_len
#define NLen(x)                  x##Len
#define N_len(x)                 x##_len
#define IFELSE(if_nlen, if_len)  if_len
#define IF_LEN(...)              __VA_ARGS__
#define IF_NLEN(...)             /* nothing */
#define ATTR_ARG                 char const *__restrict attr, size_t attrlen
#define ATTREQ(item)             streq_len((item)->mcs_name, attr, attrlen)
#define NAMEEQ(name)             streq_len(name, attr, attrlen)
#else /* MRO_LEN */
#define S(without_len, with_len) without_len
#define NLen(x)                  x
#define N_len(x)                 x
#define IFELSE(if_nlen, if_len)  if_nlen
#define IF_LEN(...)              /* nothing */
#define IF_NLEN(...)             __VA_ARGS__
#define ATTR_ARG                 char const *__restrict attr
#define ATTREQ(item)             streq((item)->mcs_name, attr)
#define NAMEEQ(name)             streq(name, attr)
#endif /* !MRO_LEN */


/* Helpers to acquire and release cache tables. */
#ifndef Dee_membercache_acquiretable
#define Dee_membercache_acquiretable(self, p_table)                   \
	(Dee_membercache_tabuse_inc(self),                                \
	 *(p_table) = atomic_read(&(self)->mc_table),                     \
	 *(p_table) ? Dee_membercache_table_incref(*(p_table)) : (void)0, \
	 Dee_membercache_tabuse_dec(self),                                \
	 *(p_table) != NULL)
#define Dee_membercache_releasetable(self, table) \
	Dee_membercache_table_decref(table)
#endif /* !Dee_membercache_acquiretable */


/* Lookup an attribute from cache.
 * @return: * :        The attribute value.
 * @return: NULL:      An error occurred.
 * @return: ITER_DONE: The attribute could not be found in the cache. */
INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_GetCachedAttr)(DeeTypeObject *tp_self, DeeObject *self,
                            ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return DeeKwObjMethod_New((dkwobjmethod_t)func, self);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeObjMethod_New(func, self);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return (*getter)(self);
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return type_member_get((struct type_member const *)&buf, self);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_GetAttribute(desc,
			                                DeeInstance_DESC(desc, self),
			                                self, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err:
	return NULL;
}

INTERN WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
NLen(DeeType_GetCachedClassAttr)(DeeTypeObject *__restrict tp_self,
                                 ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return DeeKwObjMethod_New((dkwobjmethod_t)func, (DeeObject *)tp_self);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeObjMethod_New(func, (DeeObject *)tp_self);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return (*getter)((DeeObject *)tp_self);
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return type_member_get((struct type_member const *)&buf, (DeeObject *)tp_self);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_GetAttribute(desc, class_desc_as_instance(desc),
			                                (DeeObject *)tp_self, catt);
		}

		case MEMBERCACHE_INSTANCE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return DeeKwClsMethod_New(decl, (dkwobjmethod_t)func);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClsMethod_New(decl, func);
		}

		case MEMBERCACHE_INSTANCE_GETSET: {
			dgetmethod_t get;
			ddelmethod_t del;
			dsetmethod_t set;
			DeeTypeObject *decl;
			get  = item->mcs_getset.gs_get;
			del  = item->mcs_getset.gs_del;
			set  = item->mcs_getset.gs_set;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClsProperty_New(decl, get, del, set);
		}

		case MEMBERCACHE_INSTANCE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClsMember_New(decl, &member);
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_GetInstanceAttribute(decl, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return ITER_DONE;
err:
	return NULL;
}

INTERN WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
NLen(DeeType_GetCachedInstanceAttr)(DeeTypeObject *__restrict tp_self,
                                    ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return DeeKwClsMethod_New(decl, (dkwobjmethod_t)func);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClsMethod_New(decl, func);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t get;
			ddelmethod_t del;
			dsetmethod_t set;
			DeeTypeObject *decl;
			get  = item->mcs_getset.gs_get;
			del  = item->mcs_getset.gs_del;
			set  = item->mcs_getset.gs_set;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClsProperty_New(decl, get, del, set);
		}

		case MEMBERCACHE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClsMember_New(decl, &member);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_GetInstanceAttribute(decl, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
}


/* @return: 1 : Attribute is bound.
 * @return: 0 : Attribute isn't bound.
 * @return: -1: An error occurred.
 * @return: -2: The attribute doesn't exist. */
INTERN WUNUSED NONNULL((1, 2, 3)) int DCALL
NLen(DeeType_BoundCachedAttr)(DeeTypeObject *tp_self,
                              DeeObject *self,
                              ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD:
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return 1;

		case MEMBERCACHE_GETSET: {
			dboundmethod_t bound;
			dgetmethod_t getter;
			DREF DeeObject *temp;
			bound  = item->mcs_getset.gs_bound;
			getter = item->mcs_getset.gs_get;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if (bound)
				return (*bound)(self);
			if unlikely(!getter)
				return 0;
			temp = (*getter)(self);
			if unlikely(!temp) {
				if (CATCH_ATTRIBUTE_ERROR())
					return -3;
				if (DeeError_Catch(&DeeError_UnboundAttribute))
					return 0;
				goto err;
			}
			Dee_Decref(temp);
			return 1;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return type_member_bound((struct type_member const *)&buf, self);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_BoundAttribute(desc,
			                                  DeeInstance_DESC(desc, self),
			                                  self, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return -2;
err:
	return -1;
}

INTERN WUNUSED NONNULL((1, 2)) int DCALL
NLen(DeeType_BoundCachedClassAttr)(DeeTypeObject *__restrict tp_self,
                                   ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD:
		case MEMBERCACHE_INSTANCE_METHOD:
		case MEMBERCACHE_INSTANCE_GETSET:
		case MEMBERCACHE_INSTANCE_MEMBER:
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return 1;

		case MEMBERCACHE_GETSET: {
			dboundmethod_t bound;
			dgetmethod_t getter;
			DREF DeeObject *temp;
			bound  = item->mcs_getset.gs_bound;
			getter = item->mcs_getset.gs_get;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if (bound)
				return (*bound)((DeeObject *)tp_self);
			if unlikely(!getter)
				return 0;
			temp = (*getter)((DeeObject *)tp_self);
			if unlikely(!temp) {
				if (CATCH_ATTRIBUTE_ERROR())
					return -3;
				if (DeeError_Catch(&DeeError_UnboundAttribute))
					return 0;
				goto err;
			}
			Dee_Decref(temp);
			return 1;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return type_member_bound((struct type_member const *)&buf, (DeeObject *)tp_self);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_BoundAttribute(desc, class_desc_as_instance(desc), (DeeObject *)tp_self, catt);
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_BoundInstanceAttribute(decl, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return -2;
err:
	return -1;
}

INTERN WUNUSED NONNULL((1, 2)) int DCALL
NLen(DeeType_BoundCachedInstanceAttr)(DeeTypeObject *__restrict tp_self,
                                      ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD:
		case MEMBERCACHE_GETSET:
		case MEMBERCACHE_MEMBER:
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return 1;

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_BoundInstanceAttribute(decl, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return -2;
}


/* @return: true : The attribute exists.
 * @return: false: The attribute doesn't exist. */
INTERN WUNUSED NONNULL((1, 2)) bool DCALL
NLen(DeeType_HasCachedAttr)(DeeTypeObject *__restrict tp_self,
                            ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		Dee_membercache_releasetable(&tp_self->tp_cache, table);
		return true;
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return false;
}

INTERN WUNUSED NONNULL((1, 2)) bool DCALL
NLen(DeeType_HasCachedClassAttr)(DeeTypeObject *__restrict tp_self,
                                 ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
		return true;
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return false;
}

/* @return:  1: The attribute could not be found in the cache.
 * @return:  0: Successfully invoked the delete-operator on the attribute.
 * @return: -1: An error occurred. */
INTERN WUNUSED NONNULL((1, 2, 3)) int DCALL
NLen(DeeType_DelCachedAttr)(DeeTypeObject *tp_self,
                            DeeObject *self,
                            ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			DeeTypeObject *decl;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return err_cant_access_attribute(decl, attr, ATTR_ACCESS_DEL);
		}

		case MEMBERCACHE_GETSET: {
			ddelmethod_t del;
			DeeTypeObject *decl;
			del = item->mcs_getset.gs_del;
			if likely(del) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return (*del)(self);
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_DEL);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return type_member_del((struct type_member const *)&buf, self);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_DelAttribute(desc,
			                                DeeInstance_DESC(desc, self),
			                                self, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return 1;
err:
	return -1;
}

INTERN WUNUSED NONNULL((1, 2)) int DCALL
NLen(DeeType_DelCachedClassAttr)(DeeTypeObject *__restrict tp_self,
                                 ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD:
		case MEMBERCACHE_INSTANCE_METHOD:
		case MEMBERCACHE_INSTANCE_GETSET:
		case MEMBERCACHE_INSTANCE_MEMBER: {
			DeeTypeObject *decl;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return err_cant_access_attribute(decl, attr, ATTR_ACCESS_DEL);
		}

		case MEMBERCACHE_GETSET: {
			ddelmethod_t del;
			DeeTypeObject *decl;
			del = item->mcs_getset.gs_del;
			if likely(del) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return (*del)((DeeObject *)tp_self);
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_DEL);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return type_member_del((struct type_member const *)&buf, (DeeObject *)tp_self);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_DelAttribute(desc, class_desc_as_instance(desc), (DeeObject *)tp_self, catt);
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_DelInstanceAttribute(decl, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return 1;
err:
	return -1;
}

INTERN WUNUSED NONNULL((1, 2)) int DCALL
NLen(DeeType_DelCachedInstanceAttr)(DeeTypeObject *__restrict tp_self,
                                    ATTR_ARG, dhash_t hash) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD:
		case MEMBERCACHE_GETSET:
		case MEMBERCACHE_MEMBER: {
			DeeTypeObject *decl;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return err_cant_access_attribute(decl, attr, ATTR_ACCESS_DEL);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_DelInstanceAttribute(decl, catt);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return 1;
}

/* @return:  1: The attribute could not be found in the cache.
 * @return:  0: Successfully invoked the set-operator on the attribute.
 * @return: -1: An error occurred. */
INTERN WUNUSED NONNULL((1, 2, 3, IFELSE(5, 6))) int DCALL
NLen(DeeType_SetCachedAttr)(DeeTypeObject *tp_self,
                            DeeObject *self,
                            ATTR_ARG, dhash_t hash,
                            DeeObject *value) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			DeeTypeObject *decl;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return err_cant_access_attribute(decl, attr, ATTR_ACCESS_SET);
		}

		case MEMBERCACHE_GETSET: {
			dsetmethod_t set;
			DeeTypeObject *decl;
			set = item->mcs_getset.gs_set;
			if likely(set) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return (*set)(self, value);
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_SET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return type_member_set((struct type_member const *)&buf, self, value);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_SetAttribute(desc,
			                                DeeInstance_DESC(desc, self),
			                                self, catt, value);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return 1;
err:
	return -1;
}

INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) int DCALL
NLen(DeeType_SetCachedClassAttr)(DeeTypeObject *tp_self,
                                 ATTR_ARG, dhash_t hash,
                                 DeeObject *value) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD:
		case MEMBERCACHE_INSTANCE_METHOD:
		case MEMBERCACHE_INSTANCE_GETSET:
		case MEMBERCACHE_INSTANCE_MEMBER: {
			DeeTypeObject *decl;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return err_cant_access_attribute(decl, attr, ATTR_ACCESS_SET);
		}

		case MEMBERCACHE_GETSET: {
			dsetmethod_t set;
			DeeTypeObject *decl;
			set = item->mcs_getset.gs_set;
			if likely(set) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return (*set)((DeeObject *)tp_self, value);
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_SET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return type_member_set((struct type_member const *)&buf, (DeeObject *)tp_self, value);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_SetAttribute(desc, class_desc_as_instance(desc), (DeeObject *)tp_self, catt, value);
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_SetInstanceAttribute(decl, catt, value);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return 1;
err:
	return -1;
}

INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) int DCALL
NLen(DeeType_SetCachedInstanceAttr)(DeeTypeObject *tp_self,
                                    ATTR_ARG, dhash_t hash,
                                    DeeObject *value) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD:
		case MEMBERCACHE_GETSET:
		case MEMBERCACHE_MEMBER: {
			DeeTypeObject *decl;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return err_cant_access_attribute(decl, attr, ATTR_ACCESS_SET);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_SetInstanceAttribute(decl, catt, value);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return 1;
}

/* @return:  2: The attribute is non-basic.
/* @return:  1: The attribute could not be found in the cache.
 * @return:  0: Successfully invoked the set-operator on the attribute.
 * @return: -1: An error occurred. */
INTERN WUNUSED NONNULL((1, 2, 3, IFELSE(5, 6))) int DCALL
NLen(DeeType_SetBasicCachedAttr)(DeeTypeObject *tp_self,
                                 DeeObject *self,
                                 ATTR_ARG, dhash_t hash,
                                 DeeObject *value) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return type_member_set((struct type_member const *)&buf, self, value);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_SetBasicAttribute(desc,
			                                     DeeInstance_DESC(desc, self),
			                                     self, catt, value);
		}

		default:
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return 2;
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) int DCALL
NLen(DeeType_SetBasicCachedClassAttr)(DeeTypeObject *tp_self,
                                      ATTR_ARG, dhash_t hash,
                                      DeeObject *value) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return type_member_set((struct type_member const *)&buf, (DeeObject *)tp_self, value);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_SetBasicAttribute(desc, class_desc_as_instance(desc), (DeeObject *)tp_self, catt, value);
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_SetBasicInstanceAttribute(decl, catt, value);
		}

		default:
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return 2;
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return 1;
}

#if 0
INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) int DCALL
NLen(DeeType_SetBasicCachedInstanceAttr)(DeeTypeObject *tp_self,
                                         ATTR_ARG, dhash_t hash,
                                         DeeObject *value) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_SetBasicInstanceAttribute(decl, catt, value);
		}

		default:
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return 2;
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return 1;
}
#endif

/* @return: * :        The returned value.
 * @return: NULL:      An error occurred.
 * @return: ITER_DONE: The attribute could not be found in the cache. */
INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_CallCachedAttr)(DeeTypeObject *tp_self,
                             DeeObject *self,
                             ATTR_ARG, dhash_t hash,
                             size_t argc, DeeObject *const *argv) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return (*(dkwobjmethod_t)func)(self, argc, argv, NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return (*func)(self, argc, argv);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				callback = (*getter)(self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_Call(callback, argc, argv);
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			callback = type_member_get((struct type_member const *)&buf, self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_CallAttribute(desc,
			                                 DeeInstance_DESC(desc, self),
			                                 self, catt, argc, argv);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err:
	return NULL;
}


#ifndef ERR_CLSPROPERTY_DEFINED
#define ERR_CLSPROPERTY_DEFINED 1
PRIVATE ATTR_COLD int DCALL err_cant_access_clsproperty_get(void) {
	return err_cant_access_attribute(&DeeClsProperty_Type,
	                                 DeeString_STR(&str_get),
	                                 ATTR_ACCESS_GET);
}
#endif /* !ERR_CLSPROPERTY_DEFINED */

PRIVATE ATTR_COLD NONNULL((1)) int DCALL
N_len(err_classmember_requires_1_argument)(ATTR_ARG) {
	return DeeError_Throwf(&DeeError_TypeError,
	                       "classmember `%" IF_LEN("$") "s' must be called with exactly 1 argument",
	                       IF_LEN(attrlen, ) attr);
}

PRIVATE ATTR_COLD NONNULL((1)) int DCALL
N_len(err_classproperty_requires_1_argument)(ATTR_ARG) {
	return DeeError_Throwf(&DeeError_TypeError,
	                       "classproperty `%" IF_LEN("$") "s' must be called with exactly 1 argument",
	                       IF_LEN(attrlen, ) attr);
}

PRIVATE ATTR_COLD NONNULL((1)) int DCALL
N_len(err_classmethod_requires_at_least_1_argument)(ATTR_ARG) {
	return DeeError_Throwf(&DeeError_TypeError,
	                       "classmethod `%" IF_LEN("$") "s' must be called with at least 1 argument",
	                       IF_LEN(attrlen, ) attr);
}

INTERN WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
NLen(DeeType_CallCachedClassAttr)(DeeTypeObject *tp_self,
                                  ATTR_ARG, dhash_t hash,
                                  size_t argc, DeeObject *const *argv) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return (*(dkwobjmethod_t)func)((DeeObject *)tp_self, argc, argv, NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return (*func)((DeeObject *)tp_self, argc, argv);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				callback = (*getter)((DeeObject *)tp_self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_Call(callback, argc, argv);
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			callback = type_member_get((struct type_member const *)&buf, (DeeObject *)tp_self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_CallAttribute(desc, class_desc_as_instance(desc), (DeeObject *)tp_self, catt, argc, argv);
		}

		case MEMBERCACHE_INSTANCE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				if unlikely(!argc)
					goto err_classmethod_noargs;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
					goto err;
				/* Use the first argument as the this-argument. */
				return (*(dkwobjmethod_t)func)(argv[0], argc - 1, argv + 1, NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!argc)
				goto err_classmethod_noargs;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return (*func)(argv[0], argc - 1, argv + 1);
		}

		case MEMBERCACHE_INSTANCE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			if unlikely(argc != 1)
				goto err_classproperty_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return (*get)(argv[0]);
		}

		case MEMBERCACHE_INSTANCE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(argc != 1)
				goto err_classmember_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return type_member_get(&member, argv[0]);
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_CallInstanceAttribute(decl, catt, argc, argv);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args:
	N_len(err_classmember_requires_1_argument)(attr IF_LEN(, attrlen));
	goto err;
err_classproperty_invalid_args:
	N_len(err_classproperty_requires_1_argument)(attr IF_LEN(, attrlen));
	goto err;
err_classmethod_noargs:
	N_len(err_classmethod_requires_at_least_1_argument)(attr IF_LEN(, attrlen));
err:
	return NULL;
}

#if 0
INTERN WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
NLen(DeeType_CallCachedInstanceAttr)(DeeTypeObject *tp_self,
                                     ATTR_ARG, dhash_t hash,
                                     size_t argc, DeeObject *const *argv) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				if unlikely(!argc)
					goto err_classmethod_noargs;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
					goto err;
				/* Use the first argument as the this-argument. */
				return (*(dkwobjmethod_t)func)(argv[0], argc - 1, argv + 1, NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!argc)
				goto err_classmethod_noargs;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return (*func)(argv[0], argc - 1, argv + 1);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			if unlikely(argc != 1)
				goto err_classproperty_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return (*get)(argv[0]);
		}

		case MEMBERCACHE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(argc != 1)
				goto err_classmember_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return type_member_get(&member, argv[0]);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_CallInstanceAttribute(decl, catt, argc, argv);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args:
	err_classmember_requires_1_argument(attr);
	goto err;
err_classproperty_invalid_args:
	err_classproperty_requires_1_argument(attr);
	goto err;
err_classmethod_noargs:
	err_classmethod_requires_at_least_1_argument(attr);
err:
	return NULL;
}
#endif

/* @return: * :        The returned value.
 * @return: NULL:      An error occurred.
 * @return: ITER_DONE: The attribute could not be found in the cache. */
INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
S(DeeType_CallCachedAttrKw,
  DeeType_CallCachedAttrLenKw)(DeeTypeObject *tp_self,
                               DeeObject *self,
                               ATTR_ARG, dhash_t hash,
                               size_t argc, DeeObject *const *argv,
                               DeeObject *kw) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return (*(dkwobjmethod_t)func)(self, argc, argv, kw);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if (kw) {
				if (DeeKwds_Check(kw)) {
					if (DeeKwds_SIZE(kw) != 0)
						goto err_no_keywords;
				} else {
					size_t temp = DeeObject_Size(kw);
					if unlikely(temp == (size_t)-1)
						goto err;
					if (temp != 0)
						goto err_no_keywords;
				}
			}
			return (*func)(self, argc, argv);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				callback = (*getter)(self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_CallKw(callback, argc, argv, kw);
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			callback = type_member_get((struct type_member const *)&buf, self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_CallAttributeKw(desc,
			                                   DeeInstance_DESC(desc, self),
			                                   self, catt, argc, argv, kw);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err_no_keywords:
	err_keywords_func_not_accepted(attr, kw);
err:
	return NULL;
}

INTDEF struct keyword getter_kwlist[];

INTERN WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
S(DeeType_CallCachedClassAttrKw,
  DeeType_CallCachedClassAttrLenKw)(DeeTypeObject *tp_self,
                                    ATTR_ARG, dhash_t hash,
                                    size_t argc, DeeObject *const *argv,
                                    DeeObject *kw) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return (*(dkwobjmethod_t)func)((DeeObject *)tp_self, argc, argv, kw);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if (kw) {
				if (DeeKwds_Check(kw)) {
					if (DeeKwds_SIZE(kw) != 0)
						goto err_no_keywords;
				} else {
					size_t temp = DeeObject_Size(kw);
					if unlikely(temp == (size_t)-1)
						goto err;
					if (temp != 0)
						goto err_no_keywords;
				}
			}
			return (*func)((DeeObject *)tp_self, argc, argv);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				callback = (*getter)((DeeObject *)tp_self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_CallKw(callback, argc, argv, kw);
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			callback = type_member_get((struct type_member const *)&buf, (DeeObject *)tp_self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_CallAttributeKw(desc, class_desc_as_instance(desc), (DeeObject *)tp_self, catt, argc, argv, kw);
		}

		case MEMBERCACHE_INSTANCE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				if unlikely(!argc)
					goto err_classmethod_noargs;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
					goto err;
				/* Use the first argument as the this-argument. */
				return (*(dkwobjmethod_t)func)(argv[0], argc - 1, argv + 1, kw);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!argc)
				goto err_classmethod_noargs;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			if (kw) {
				if (DeeKwds_Check(kw)) {
					if (DeeKwds_SIZE(kw) != 0)
						goto err_no_keywords;
				} else {
					size_t temp = DeeObject_Size(kw);
					if unlikely(temp == (size_t)-1)
						goto err;
					if (temp != 0)
						goto err_no_keywords;
				}
			}
			/* Use the first argument as the this-argument. */
			return (*func)(argv[0], argc - 1, argv + 1);
		}

		case MEMBERCACHE_INSTANCE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			DeeObject *thisarg;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			if unlikely(argc != 1)
				goto err_classproperty_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			if (DeeArg_UnpackKw(argc, argv, kw, getter_kwlist, "o:get", &thisarg))
				goto err;
			return (*get)(thisarg);
		}

		case MEMBERCACHE_INSTANCE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			DeeObject *thisarg;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(argc != 1)
				goto err_classmember_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			if (DeeArg_UnpackKw(argc, argv, kw, getter_kwlist, "o:get", &thisarg))
				goto err;
			return type_member_get(&member, thisarg);
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_CallInstanceAttributeKw(decl, catt, argc, argv, kw);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args:
	N_len(err_classmember_requires_1_argument)(attr IF_LEN(, attrlen));
	goto err;
err_classproperty_invalid_args:
	N_len(err_classproperty_requires_1_argument)(attr IF_LEN(, attrlen));
	goto err;
err_classmethod_noargs:
	N_len(err_classmethod_requires_at_least_1_argument)(attr IF_LEN(, attrlen));
	goto err;
err_no_keywords:
	err_keywords_func_not_accepted(attr, kw);
err:
	return NULL;
}

INTERN WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
S(DeeType_CallCachedInstanceAttrKw,
  DeeType_CallCachedInstanceAttrLenKw)(DeeTypeObject *tp_self,
                                       ATTR_ARG, dhash_t hash,
                                       size_t argc, DeeObject *const *argv,
                                       DeeObject *kw) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				if unlikely(!argc)
					goto err_classmethod_noargs;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
					goto err;
				/* Use the first argument as the this-argument. */
				return (*(dkwobjmethod_t)func)(argv[0], argc - 1, argv + 1, kw);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!argc)
				goto err_classmethod_noargs;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			if (kw) {
				if (DeeKwds_Check(kw)) {
					if (DeeKwds_SIZE(kw) != 0)
						goto err_no_keywords;
				} else {
					size_t temp = DeeObject_Size(kw);
					if unlikely(temp == (size_t)-1)
						goto err;
					if (temp != 0)
						goto err_no_keywords;
				}
			}
			/* Use the first argument as the this-argument. */
			return (*func)(argv[0], argc - 1, argv + 1);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			DeeObject *thisarg;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			if unlikely(argc != 1)
				goto err_classproperty_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			if (DeeArg_UnpackKw(argc, argv, kw, getter_kwlist, "o:get", &thisarg))
				goto err;
			return (*get)(thisarg);
		}

		case MEMBERCACHE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			DeeObject *thisarg;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(argc != 1)
				goto err_classmember_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(argv[0], decl))
				goto err;
			if (DeeArg_UnpackKw(argc, argv, kw, getter_kwlist, "o:get", &thisarg))
				goto err;
			return type_member_get(&member, thisarg);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_CallInstanceAttributeKw(decl, catt, argc, argv, kw);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args:
	N_len(err_classmember_requires_1_argument)(attr IF_LEN(, attrlen));
	goto err;
err_classproperty_invalid_args:
	N_len(err_classproperty_requires_1_argument)(attr IF_LEN(, attrlen));
	goto err;
err_classmethod_noargs:
	N_len(err_classmethod_requires_at_least_1_argument)(attr IF_LEN(, attrlen));
	goto err;
err_no_keywords:
	err_keywords_func_not_accepted(attr, kw);
err:
	return NULL;
}

#ifndef MRO_LEN
#ifdef CONFIG_CALLTUPLE_OPTIMIZATIONS
/* @return: * :        The returned value.
 * @return: NULL:      An error occurred.
 * @return: ITER_DONE: The attribute could not be found in the cache. */
INTERN WUNUSED NONNULL((1, 2, 3, IFELSE(5, 6))) DREF DeeObject *DCALL
S(DeeType_CallCachedAttrTuple,
  DeeType_CallCachedAttrLenTuple)(DeeTypeObject *tp_self,
                                  DeeObject *self,
                                  ATTR_ARG, dhash_t hash,
                                  DeeObject *args) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return (*(dkwobjmethod_t)func)(self, DeeTuple_SIZE(args), DeeTuple_ELEM(args), NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return (*func)(self, DeeTuple_SIZE(args), DeeTuple_ELEM(args));
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				callback = (*getter)(self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_Call(callback, DeeTuple_SIZE(args), DeeTuple_ELEM(args));
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			callback = type_member_get((struct type_member const *)&buf, self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_CallAttributeTuple(desc,
			                                      DeeInstance_DESC(desc, self),
			                                      self, catt, args);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err:
	return NULL;
}

INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) DREF DeeObject *DCALL
S(DeeType_CallCachedClassAttrTuple,
  DeeType_CallCachedClassAttrLenTuple)(DeeTypeObject *tp_self,
                                       ATTR_ARG, dhash_t hash,
                                       DeeObject *args) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return (*(dkwobjmethod_t)func)((DeeObject *)tp_self, DeeTuple_SIZE(args), DeeTuple_ELEM(args), NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return (*func)((DeeObject *)tp_self, DeeTuple_SIZE(args), DeeTuple_ELEM(args));
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				callback = (*getter)((DeeObject *)tp_self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_Call(callback, DeeTuple_SIZE(args), DeeTuple_ELEM(args));
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}
			{
				struct buffer {
					uint8_t dat[offsetof(struct type_member, m_doc)];
				} buf;
			case MEMBERCACHE_MEMBER:
				buf = *(struct buffer *)&item->mcs_member;
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				callback = type_member_get((struct type_member const *)&buf, (DeeObject *)tp_self);
				goto check_and_invoke_callback;
			}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_CallAttributeTuple(desc, class_desc_as_instance(desc),
			                                      (DeeObject *)tp_self, catt, args);
		}

		case MEMBERCACHE_INSTANCE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				if unlikely(!DeeTuple_SIZE(args))
					goto err_classmethod_noargs;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
					goto err;
				/* Use the first argument as the this-argument. */
				return (*(dkwobjmethod_t)func)(DeeTuple_GET(args, 0), DeeTuple_SIZE(args) - 1, DeeTuple_ELEM(args) + 1, NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!DeeTuple_SIZE(args))
				goto err_classmethod_noargs;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return (*func)(DeeTuple_GET(args, 0), DeeTuple_SIZE(args) - 1, DeeTuple_ELEM(args) + 1);
		}

		case MEMBERCACHE_INSTANCE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			if unlikely(DeeTuple_SIZE(args) != 1)
				goto err_classproperty_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return (*get)(DeeTuple_GET(args, 0));
		}

		case MEMBERCACHE_INSTANCE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(DeeTuple_SIZE(args) != 1)
				goto err_classmember_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return type_member_get(&member, DeeTuple_GET(args, 0));
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_CallInstanceAttributeTuple(decl, catt, args);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args:
	err_classmember_requires_1_argument(attr);
	goto err;
err_classproperty_invalid_args:
	err_classproperty_requires_1_argument(attr);
	goto err;
err_classmethod_noargs:
	err_classmethod_requires_at_least_1_argument(attr);
err:
	return NULL;
}

#if 0
INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) DREF DeeObject *DCALL
S(DeeType_CallCachedInstanceAttrTuple,
  DeeType_CallCachedInstanceAttrLenTuple)(DeeTypeObject *tp_self,
                                          ATTR_ARG, dhash_t hash,
                                          DeeObject *args) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				if unlikely(!DeeTuple_SIZE(args))
					goto err_classmethod_noargs;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
					goto err;
				/* Use the first argument as the this-argument. */
				return (*(dkwobjmethod_t)func)(DeeTuple_GET(args, 0), DeeTuple_SIZE(args) - 1, DeeTuple_ELEM(args) + 1, NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!DeeTuple_SIZE(args))
				goto err_classmethod_noargs;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return (*func)(DeeTuple_GET(args, 0), DeeTuple_SIZE(args) - 1, DeeTuple_ELEM(args) + 1);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			if unlikely(DeeTuple_SIZE(args) != 1)
				goto err_classproperty_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return (*get)(DeeTuple_GET(args, 0));
		}

		case MEMBERCACHE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(DeeTuple_SIZE(args) != 1)
				goto err_classmember_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			/* Use the first argument as the this-argument. */
			return type_member_get(&member, DeeTuple_GET(args, 0));
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_CallInstanceAttributeTuple(decl, catt, args);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args:
	err_classmember_requires_1_argument(attr);
	goto err;
err_classproperty_invalid_args:
	err_classproperty_requires_1_argument(attr);
	goto err;
err_classmethod_noargs:
	err_classmethod_requires_at_least_1_argument(attr);
err:
	return NULL;
}
#endif

INTERN WUNUSED NONNULL((1, 2, 3, IFELSE(5, 6))) DREF DeeObject *DCALL
S(DeeType_CallCachedAttrTupleKw,
  DeeType_CallCachedAttrLenTupleKw)(DeeTypeObject *tp_self,
                                    DeeObject *self,
                                    ATTR_ARG, dhash_t hash,
                                    DeeObject *args, DeeObject *kw) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return (*(dkwobjmethod_t)func)(self, DeeTuple_SIZE(args), DeeTuple_ELEM(args), kw);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if (kw) {
				if (DeeKwds_Check(kw)) {
					if (DeeKwds_SIZE(kw) != 0)
						goto err_no_keywords;
				} else {
					size_t temp = DeeObject_Size(kw);
					if unlikely(temp == (size_t)-1)
						goto err;
					if (temp != 0)
						goto err_no_keywords;
				}
			}
			return (*func)(self, DeeTuple_SIZE(args), DeeTuple_ELEM(args));
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				callback = (*getter)(self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_CallTupleKw(callback, args, kw);
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			callback = type_member_get((struct type_member const *)&buf, self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_CallAttributeTupleKw(desc,
			                                        DeeInstance_DESC(desc, self),
			                                        self, catt, args, kw);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err_no_keywords:
	err_keywords_func_not_accepted(attr, kw);
err:
	return NULL;
}

INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) DREF DeeObject *DCALL
S(DeeType_CallCachedClassAttrTupleKw,
  DeeType_CallCachedClassAttrLenTupleKw)(DeeTypeObject *tp_self,
                                         ATTR_ARG, dhash_t hash,
                                         DeeObject *args, DeeObject *kw) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return (*(dkwobjmethod_t)func)((DeeObject *)tp_self, DeeTuple_SIZE(args), DeeTuple_ELEM(args), kw);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if (kw) {
				if (DeeKwds_Check(kw)) {
					if (DeeKwds_SIZE(kw) != 0)
						goto err_no_keywords;
				} else {
					size_t temp = DeeObject_Size(kw);
					if unlikely(temp == (size_t)-1)
						goto err;
					if (temp != 0)
						goto err_no_keywords;
				}
			}
			return (*func)((DeeObject *)tp_self, DeeTuple_SIZE(args), DeeTuple_ELEM(args));
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				callback = (*getter)((DeeObject *)tp_self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_CallTupleKw(callback, args, kw);
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			callback = type_member_get((struct type_member const *)&buf, (DeeObject *)tp_self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_CallAttributeTupleKw(desc, class_desc_as_instance(desc), (DeeObject *)tp_self, catt, args, kw);
		}

		case MEMBERCACHE_INSTANCE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				if unlikely(!DeeTuple_SIZE(args))
					goto err_classmethod_noargs;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
					goto err;
				/* Use the first argument as the this-argument. */
				return (*(dkwobjmethod_t)func)(DeeTuple_GET(args, 0),
				                               DeeTuple_SIZE(args) - 1,
				                               DeeTuple_ELEM(args) + 1,
				                               kw);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!DeeTuple_SIZE(args))
				goto err_classmethod_noargs;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			if (kw) {
				if (DeeKwds_Check(kw)) {
					if (DeeKwds_SIZE(kw) != 0)
						goto err_no_keywords;
				} else {
					size_t temp = DeeObject_Size(kw);
					if unlikely(temp == (size_t)-1)
						goto err;
					if (temp != 0)
						goto err_no_keywords;
				}
			}
			/* Use the first argument as the this-argument. */
			return (*func)(DeeTuple_GET(args, 0),
			               DeeTuple_SIZE(args) - 1,
			               DeeTuple_ELEM(args) + 1);
		}

		case MEMBERCACHE_INSTANCE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			DeeObject *thisarg;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			if unlikely(DeeTuple_SIZE(args) != 1)
				goto err_classproperty_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) &&
			            DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			if (DeeArg_UnpackKw(DeeTuple_SIZE(args), DeeTuple_ELEM(args),
			                    kw, getter_kwlist, "o:get", &thisarg))
				goto err;
			return (*get)(thisarg);
		}

		case MEMBERCACHE_INSTANCE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			DeeObject *thisarg;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(DeeTuple_SIZE(args) != 1)
				goto err_classmember_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) &&
			            DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			if (DeeArg_UnpackKw(DeeTuple_SIZE(args), DeeTuple_ELEM(args),
			                    kw, getter_kwlist, "o:get", &thisarg))
				goto err;
			return type_member_get(&member, thisarg);
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_CallInstanceAttributeTupleKw(decl, catt, args, kw);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args:
	err_classmember_requires_1_argument(attr);
	goto err;
err_classproperty_invalid_args:
	err_classproperty_requires_1_argument(attr);
	goto err;
err_classmethod_noargs:
	err_classmethod_requires_at_least_1_argument(attr);
	goto err;
err_no_keywords:
	err_keywords_func_not_accepted(attr, kw);
err:
	return NULL;
}

#if 0
INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) DREF DeeObject *DCALL
S(DeeType_CallCachedInstanceAttrTupleKw,
  DeeType_CallCachedInstanceAttrLenTupleKw)(DeeTypeObject *tp_self,
                                            ATTR_ARG, dhash_t hash,
                                            DeeObject *args, DeeObject *kw) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				if unlikely(!DeeTuple_SIZE(args))
					goto err_classmethod_noargs;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
					goto err;
				/* Use the first argument as the this-argument. */
				return (*(dkwobjmethod_t)func)(DeeTuple_GET(args, 0),
				                               DeeTuple_SIZE(args) - 1,
				                               DeeTuple_ELEM(args) + 1,
				                               kw);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!DeeTuple_SIZE(args))
				goto err_classmethod_noargs;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			if (kw) {
				if (DeeKwds_Check(kw)) {
					if (DeeKwds_SIZE(kw) != 0)
						goto err_no_keywords;
				} else {
					size_t temp = DeeObject_Size(kw);
					if unlikely(temp == (size_t)-1)
						goto err;
					if (temp != 0)
						goto err_no_keywords;
				}
			}
			/* Use the first argument as the this-argument. */
			return (*func)(DeeTuple_GET(args, 0),
			               DeeTuple_SIZE(args) - 1,
			               DeeTuple_ELEM(args) + 1);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			DeeObject *thisarg;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			if unlikely(DeeTuple_SIZE(args) != 1)
				goto err_classproperty_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) &&
			            DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			if (DeeArg_UnpackKw(DeeTuple_SIZE(args), DeeTuple_ELEM(args),
			                    kw, getter_kwlist, "o:get", &thisarg))
				goto err;
			return (*get)(thisarg);
		}

		case MEMBERCACHE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			DeeObject *thisarg;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(DeeTuple_SIZE(args) != 1)
				goto err_classmember_invalid_args;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) &&
			            DeeObject_AssertType(DeeTuple_GET(args, 0), decl))
				goto err;
			if (DeeArg_UnpackKw(DeeTuple_SIZE(args), DeeTuple_ELEM(args),
			                    kw, getter_kwlist, "o:get", &thisarg))
				goto err;
			return type_member_get(&member, thisarg);
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_CallInstanceAttributeTupleKw(decl, catt, args, kw);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args:
	err_classmember_requires_1_argument(attr);
	goto err;
err_classproperty_invalid_args:
	err_classproperty_requires_1_argument(attr);
	goto err;
err_classmethod_noargs:
	err_classmethod_requires_at_least_1_argument(attr);
	goto err;
err_no_keywords:
	err_keywords_func_not_accepted(attr, kw);
err:
	return NULL;
}
#endif
#endif /* CONFIG_CALLTUPLE_OPTIMIZATIONS */
#endif /* !MRO_LEN */


#ifndef MRO_LEN
#ifndef DKWOBJMETHOD_VCALLF_DEFINED
#define DKWOBJMETHOD_VCALLF_DEFINED 1
PRIVATE NONNULL((1, 2, 3)) DREF DeeObject *DCALL
dkwobjmethod_vcallf(dkwobjmethod_t self,
                    DeeObject *thisarg,
                    char const *__restrict format,
                    va_list args, DeeObject *kw) {
	DREF DeeObject *result, *args_tuple;
	args_tuple = DeeTuple_VNewf(format, args);
	if unlikely(!args_tuple)
		goto err;
	result = (*self)(thisarg,
	                 DeeTuple_SIZE(args_tuple),
	                 DeeTuple_ELEM(args_tuple),
	                 kw);
	Dee_Decref(args_tuple);
	return result;
err:
	return NULL;
}

PRIVATE NONNULL((1, 2, 3)) DREF DeeObject *DCALL
dobjmethod_vcallf(dobjmethod_t self,
                  DeeObject *thisarg,
                  char const *__restrict format,
                  va_list args) {
	DREF DeeObject *result, *args_tuple;
	args_tuple = DeeTuple_VNewf(format, args);
	if unlikely(!args_tuple)
		goto err;
	result = (*self)(thisarg,
	                 DeeTuple_SIZE(args_tuple),
	                 DeeTuple_ELEM(args_tuple));
	Dee_Decref(args_tuple);
	return result;
err:
	return NULL;
}
#endif /* !DKWOBJMETHOD_VCALLF_DEFINED */

/* @return: * :        The returned value.
 * @return: NULL:      An error occurred.
 * @return: ITER_DONE: The attribute could not be found in the cache. */
INTERN WUNUSED NONNULL((1, 2, 3, IFELSE(5, 6))) DREF DeeObject *DCALL
S(DeeType_VCallCachedAttrf,
  DeeType_VCallCachedAttrLenf)(DeeTypeObject *tp_self, DeeObject *self,
                               ATTR_ARG, dhash_t hash,
                               char const *__restrict format, va_list args) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				return dkwobjmethod_vcallf((dkwobjmethod_t)func, self, format, args, NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return dobjmethod_vcallf(func, self, format, args);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				callback = (*getter)(self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_VCallf(callback, format, args);
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			callback = type_member_get((struct type_member const *)&buf, self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeInstance_VCallAttributef(desc,
			                                   DeeInstance_DESC(desc, self),
			                                   self, catt, format, args);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err:
	return NULL;
}

INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) DREF DeeObject *DCALL
S(DeeType_VCallCachedClassAttrf,
  DeeType_VCallCachedClassAttrLenf)(DeeTypeObject *tp_self,
                                    ATTR_ARG, dhash_t hash,
                                    char const *__restrict format, va_list args) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result, *args_tuple;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			func = item->mcs_method.m_func;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				return dkwobjmethod_vcallf((dkwobjmethod_t)func, (DeeObject *)tp_self, format, args, NULL);
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return dobjmethod_vcallf(func, (DeeObject *)tp_self, format, args);
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t getter;
			DeeTypeObject *decl;
			getter = item->mcs_getset.gs_get;
			if likely(getter) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				callback = (*getter)((DeeObject *)tp_self);
check_and_invoke_callback:
				if unlikely(!callback)
					goto err;
				result = DeeObject_VCallf(callback, format, args);
				Dee_Decref(callback);
				return result;
			}
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			err_cant_access_attribute(decl, attr, ATTR_ACCESS_GET);
			goto err;
		}

		case MEMBERCACHE_MEMBER: {
			struct buffer {
				uint8_t dat[offsetof(struct type_member, m_doc)];
			} buf;
			buf = *(struct buffer *)&item->mcs_member;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			callback = type_member_get((struct type_member const *)&buf, (DeeObject *)tp_self);
			goto check_and_invoke_callback;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct class_desc *desc;
			catt = item->mcs_attrib.a_attr;
			desc = item->mcs_attrib.a_desc;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeInstance_VCallAttributef(desc, class_desc_as_instance(desc), (DeeObject *)tp_self, catt, format, args);
		}

		case MEMBERCACHE_INSTANCE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
				args_tuple = DeeTuple_VNewf(format, args);
				if unlikely(!args_tuple)
					goto err;
				if unlikely(!DeeTuple_SIZE(args_tuple))
					goto err_classmethod_noargs_args_tuple;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args_tuple, 0), decl))
					goto err_args_tuple;
				/* Use the first argument as the this-argument. */
				result = (*(dkwobjmethod_t)func)(DeeTuple_GET(args_tuple, 0),
				                                 DeeTuple_SIZE(args_tuple) - 1,
				                                 DeeTuple_ELEM(args_tuple) + 1,
				                                 NULL);
				Dee_Decref(args_tuple);
				return result;
			}
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			args_tuple = DeeTuple_VNewf(format, args);
			if unlikely(!args_tuple)
				goto err;
			if unlikely(!DeeTuple_SIZE(args_tuple))
				goto err_classmethod_noargs_args_tuple;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args_tuple, 0), decl))
				goto err_args_tuple;
			/* Use the first argument as the this-argument. */
			result = (*func)(DeeTuple_GET(args_tuple, 0),
			                 DeeTuple_SIZE(args_tuple) - 1,
			                 DeeTuple_ELEM(args_tuple) + 1);
			Dee_Decref(args_tuple);
			return result;
		}

		case MEMBERCACHE_INSTANCE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			args_tuple = DeeTuple_VNewf(format, args);
			if unlikely(!args_tuple)
				goto err;
			if unlikely(DeeTuple_SIZE(args_tuple) != 1)
				goto err_classproperty_invalid_args_args_tuple;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args_tuple, 0), decl))
				goto err_args_tuple;
			/* Use the first argument as the this-argument. */
			result = (*get)(DeeTuple_GET(args_tuple, 0));
			Dee_Decref(args_tuple);
			return result;
		}

		case MEMBERCACHE_INSTANCE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			args_tuple = DeeTuple_VNewf(format, args);
			if unlikely(!args_tuple)
				goto err;
			if unlikely(DeeTuple_SIZE(args_tuple) != 1)
				goto err_classmember_invalid_args_args_tuple;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args_tuple, 0), decl))
				goto err_args_tuple;
			/* Use the first argument as the this-argument. */
			result = type_member_get(&member, DeeTuple_GET(args_tuple, 0));
			Dee_Decref(args_tuple);
			return result;
		}

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
			return DeeClass_VCallInstanceAttributef(decl, catt, format, args);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args_args_tuple:
	Dee_Decref(args_tuple);
/*err_classmember_invalid_args:*/
	err_classmember_requires_1_argument(attr);
	goto err;
err_classproperty_invalid_args_args_tuple:
	Dee_Decref(args_tuple);
/*err_classproperty_invalid_args:*/
	err_classproperty_requires_1_argument(attr);
	goto err;
err_classmethod_noargs_args_tuple:
	Dee_Decref(args_tuple);
/*err_classmethod_noargs:*/
	err_classmethod_requires_at_least_1_argument(attr);
	goto err;
err_args_tuple:
	Dee_Decref(args_tuple);
err:
	return NULL;
}

#if 0
INTERN WUNUSED NONNULL((1, 2, IFELSE(4, 5))) DREF DeeObject *DCALL
S(DeeType_VCallCachedInstanceAttrf,
  DeeType_VCallCachedInstanceAttrLenf)(DeeTypeObject *tp_self,
                                       ATTR_ARG, dhash_t hash,
                                       char const *__restrict format, va_list args) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	DREF DeeObject *callback, *result, *args_tuple;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!ATTREQ(item))
			continue;
		switch (type) {

		case MEMBERCACHE_METHOD: {
			dobjmethod_t func;
			DeeTypeObject *decl;
			func = item->mcs_method.m_func;
			decl = item->mcs_decl;
			if (item->mcs_method.m_flag & TYPE_METHOD_FKWDS) {
				Dee_membercache_releasetable(&tp_self->tp_cache, table);
				args_tuple = DeeTuple_VNewf(format, args);
				if unlikely(!args_tuple)
					goto err;
				if unlikely(!DeeTuple_SIZE(args_tuple))
					goto err_classmethod_noargs_args_tuple;
				/* Allow non-instance objects for generic types. */
				if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args_tuple, 0), decl))
					goto err_args_tuple;
				/* Use the first argument as the this-argument. */
				result = (*(dkwobjmethod_t)func)(DeeTuple_GET(args_tuple, 0),
				                                 DeeTuple_SIZE(args_tuple) - 1,
				                                 DeeTuple_ELEM(args_tuple) + 1,
				                                 NULL);
				Dee_Decref(args_tuple);
				return result;
			}
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			args_tuple = DeeTuple_VNewf(format, args);
			if unlikely(!args_tuple)
				goto err;
			if unlikely(!DeeTuple_SIZE(args_tuple))
				goto err_classmethod_noargs_args_tuple;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args_tuple, 0), decl))
				goto err_args_tuple;
			/* Use the first argument as the this-argument. */
			result = (*func)(DeeTuple_GET(args_tuple, 0),
			                 DeeTuple_SIZE(args_tuple) - 1,
			                 DeeTuple_ELEM(args_tuple) + 1);
			Dee_Decref(args_tuple);
			return result;
		}

		case MEMBERCACHE_GETSET: {
			dgetmethod_t get;
			DeeTypeObject *decl;
			get  = item->mcs_getset.gs_get;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			if unlikely(!get)
				goto err_no_getter;
			args_tuple = DeeTuple_VNewf(format, args);
			if unlikely(!args_tuple)
				goto err;
			if unlikely(DeeTuple_SIZE(args_tuple) != 1)
				goto err_classproperty_invalid_args_args_tuple;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args_tuple, 0), decl))
				goto err_args_tuple;
			/* Use the first argument as the this-argument. */
			result = (*get)(DeeTuple_GET(args_tuple, 0));
			Dee_Decref(args_tuple);
			return result;
		}

		case MEMBERCACHE_MEMBER: {
			struct type_member member;
			DeeTypeObject *decl;
			member = item->mcs_member;
			decl   = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			args_tuple = DeeTuple_VNewf(format, args);
			if unlikely(!args_tuple)
				goto err;
			if unlikely(DeeTuple_SIZE(args_tuple) != 1)
				goto err_classmember_invalid_args_args_tuple;
			/* Allow non-instance objects for generic types. */
			if unlikely(!(decl->tp_flags & TP_FABSTRACT) && DeeObject_AssertType(DeeTuple_GET(args_tuple, 0), decl))
				goto err_args_tuple;
			/* Use the first argument as the this-argument. */
			result = type_member_get(&member, DeeTuple_GET(args_tuple, 0));
			Dee_Decref(args_tuple);
			return result;
		}

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			DeeTypeObject *decl;
			catt = item->mcs_attrib.a_attr;
			decl = item->mcs_decl;
			Dee_membercache_releasetable(&tp_self->tp_cache, table);
			return DeeClass_VCallInstanceAttributef(decl, catt, format, args);
		}

		default: __builtin_unreachable();
		}
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
	return ITER_DONE;
err_no_getter:
	err_cant_access_clsproperty_get();
	goto err;
err_classmember_invalid_args_args_tuple:
	Dee_Decref(args_tuple);
/*err_classmember_invalid_args:*/
	err_classmember_requires_1_argument(attr);
	goto err;
err_classproperty_invalid_args_args_tuple:
	Dee_Decref(args_tuple);
/*err_classproperty_invalid_args:*/
	err_classproperty_requires_1_argument(attr);
	goto err;
err_classmethod_noargs_args_tuple:
	Dee_Decref(args_tuple);
/*err_classmethod_noargs:*/
	err_classmethod_requires_at_least_1_argument(attr);
	goto err;
err_args_tuple:
	Dee_Decref(args_tuple);
err:
	return NULL;
}
#endif
#endif /* !MRO_LEN */


#ifndef TYPE_MEMBER_TYPEFOR_DEFINED
#define TYPE_MEMBER_TYPEFOR_DEFINED 1
INTDEF WUNUSED NONNULL((1)) DeeTypeObject *DCALL
type_member_typefor(struct type_member const *__restrict self);
#endif

#ifndef MRO_LEN
INTERN WUNUSED NONNULL((1, 3, 4)) int DCALL
DeeType_FindCachedAttr(DeeTypeObject *tp_self, DeeObject *instance,
                       struct attribute_info *__restrict result,
                       struct attribute_lookup_rules const *__restrict rules) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, rules->alr_hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		char const *doc;
		uint16_t perm;
		DREF DeeTypeObject *member_decl, *member_type;
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != rules->alr_hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!streq(item->mcs_name, rules->alr_name))
			continue;
		if (rules->alr_decl && (DeeObject *)item->mcs_decl != result->a_decl)
			break; /* Attribute isn't declared by the requested declarator. */
		switch (type) {

		case MEMBERCACHE_METHOD:
			perm        = ATTR_IMEMBER | ATTR_PERMGET | ATTR_PERMCALL;
			doc         = item->mcs_method.m_doc;
			member_type = (item->mcs_method.m_flag & TYPE_METHOD_FKWDS)
			              ? &DeeKwObjMethod_Type
			              : &DeeObjMethod_Type;
			Dee_Incref(member_type);
			break;

		case MEMBERCACHE_GETSET:
			perm        = ATTR_IMEMBER | ATTR_PROPERTY;
			doc         = item->mcs_getset.gs_doc;
			member_type = NULL;
			if (item->mcs_getset.gs_get)
				perm |= ATTR_PERMGET;
			if (item->mcs_getset.gs_del)
				perm |= ATTR_PERMDEL;
			if (item->mcs_getset.gs_set)
				perm |= ATTR_PERMSET;
			break;

		case MEMBERCACHE_MEMBER:
			perm = ATTR_IMEMBER | ATTR_PERMGET;
			doc  = item->mcs_member.m_doc;
			if (TYPE_MEMBER_ISCONST(&item->mcs_member)) {
				member_type = Dee_TYPE(item->mcs_member.m_const);
				Dee_Incref(member_type);
			} else {
				/* TODO: Use `type_member_get(&item->mcs_member, instance)' to determine the proper attribute type! */
				member_type = type_member_typefor(&item->mcs_member);
				Dee_XIncref(member_type);
				if (!(item->mcs_member.m_field.m_type & STRUCT_CONST))
					perm |= ATTR_PERMDEL | ATTR_PERMSET;
			}
			break;

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct instance_desc *inst;
			doc  = NULL;
			catt = item->mcs_attrib.a_attr;
			perm = ATTR_IMEMBER | ATTR_PERMGET | ATTR_PERMDEL | ATTR_PERMSET;
			if (catt->ca_doc) {
				doc = DeeString_STR(catt->ca_doc);
				perm |= ATTR_DOCOBJ;
				Dee_Incref(catt->ca_doc);
			}
			if (catt->ca_flag & CLASS_ATTRIBUTE_FPRIVATE)
				perm |= ATTR_PRIVATE;
			if (catt->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
				perm |= ATTR_PROPERTY;
				member_type = NULL;
			} else if (catt->ca_flag & CLASS_ATTRIBUTE_FMETHOD) {
				perm |= ATTR_PERMCALL;
				member_type = &DeeInstanceMethod_Type;
				Dee_Incref(member_type);
			} else {
				member_type = NULL;
			}
			inst = NULL;
			if (catt->ca_flag & CLASS_ATTRIBUTE_FCLASSMEM) {
				inst = class_desc_as_instance(item->mcs_attrib.a_desc);
			} else if (instance) {
				inst = DeeInstance_DESC(item->mcs_attrib.a_desc, instance);
			}
			if (inst) {
				Dee_instance_desc_lock_read(inst);
				if (catt->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
					if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_GET])
						perm &= ~ATTR_PERMGET;
					if (!(catt->ca_flag & CLASS_ATTRIBUTE_FREADONLY)) {
						if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_DEL])
							perm &= ~ATTR_PERMDEL;
						if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_SET])
							perm &= ~ATTR_PERMSET;
					}
				} else if (!(catt->ca_flag & CLASS_ATTRIBUTE_FMETHOD)) {
					ASSERT(!member_type);
					member_type = (DREF DeeTypeObject *)inst->id_vtab[catt->ca_addr + CLASS_GETSET_GET];
					if (member_type) {
						member_type = Dee_TYPE(member_type);
						Dee_Incref(member_type);
					}
				}
				Dee_instance_desc_lock_endread(inst);
			}
			if (catt->ca_flag & CLASS_ATTRIBUTE_FREADONLY)
				perm &= ~(ATTR_PERMDEL | ATTR_PERMSET);
		}	break;

		default: __builtin_unreachable();
		}
		member_decl = item->mcs_decl;
		Dee_Incref(member_decl);
		Dee_membercache_releasetable(&tp_self->tp_cache, table);
		if ((perm & rules->alr_perm_mask) != rules->alr_perm_value) {
			if (perm & ATTR_DOCOBJ)
				Dee_Decref(COMPILER_CONTAINER_OF(doc, DeeStringObject, s_str));
			Dee_XDecref(member_type);
			Dee_Decref(member_decl);
			goto not_found;
		}
		result->a_decl     = (DREF DeeObject *)member_decl;
		result->a_doc      = doc; /* Inherit reference. */
		result->a_perm     = perm;
		result->a_attrtype = member_type; /* Inherit reference. */
		return 0;
	}
	Dee_membercache_releasetable(&tp_self->tp_cache, table);
cache_miss:
not_found:
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, 3)) int DCALL
DeeType_FindCachedClassAttr(DeeTypeObject *tp_self,
                            struct attribute_info *__restrict result,
                            struct attribute_lookup_rules const *__restrict rules) {
	DREF struct Dee_membercache_table *table;
	dhash_t i, perturb;
	if unlikely(!Dee_membercache_acquiretable(&tp_self->tp_class_cache, &table))
		goto cache_miss;
	perturb = i = Dee_membercache_table_hashst(table, rules->alr_hash);
	for (;; Dee_membercache_table_hashnx(i, perturb)) {
		char const *doc;
		uint16_t perm;
		DREF DeeTypeObject *member_decl, *member_type;
		struct Dee_membercache_slot *item;
		uint16_t type;
		item = Dee_membercache_table_hashit(table, i);
		type = atomic_read(&item->mcs_type);
		if (type == MEMBERCACHE_UNUSED)
			break;
		if (item->mcs_hash != rules->alr_hash)
			continue;
		if unlikely(type == MEMBERCACHE_UNINITIALIZED)
			continue; /* Don't dereference uninitialized items! */
		if (!streq(item->mcs_name, rules->alr_name))
			continue;
		if (rules->alr_decl && (DeeObject *)item->mcs_decl != result->a_decl)
			break; /* Attribute isn't declared by the requested declarator. */
		switch (type) {

		case MEMBERCACHE_METHOD:
			perm        = ATTR_CMEMBER | ATTR_PERMGET | ATTR_PERMCALL;
			doc         = item->mcs_method.m_doc;
			member_type = (item->mcs_method.m_flag & TYPE_METHOD_FKWDS)
			              ? &DeeKwObjMethod_Type
			              : &DeeObjMethod_Type;
			Dee_Incref(member_type);
			break;

		case MEMBERCACHE_GETSET:
			perm        = ATTR_CMEMBER | ATTR_PROPERTY;
			doc         = item->mcs_getset.gs_doc;
			member_type = NULL;
			if (item->mcs_getset.gs_get)
				perm |= ATTR_PERMGET;
			if (item->mcs_getset.gs_del)
				perm |= ATTR_PERMDEL;
			if (item->mcs_getset.gs_set)
				perm |= ATTR_PERMSET;
			break;

		case MEMBERCACHE_MEMBER:
			perm = ATTR_CMEMBER | ATTR_PERMGET;
			doc  = item->mcs_member.m_doc;
			if (TYPE_MEMBER_ISCONST(&item->mcs_member)) {
				member_type = Dee_TYPE(item->mcs_member.m_const);
				Dee_Incref(member_type);
			} else {
				/* TODO: Use `type_member_get(&item->mcs_member, (DeeObject *)tp_self)' to determine the proper attribute type! */
				member_type = type_member_typefor(&item->mcs_member);
				Dee_XIncref(member_type);
				if (!(item->mcs_member.m_field.m_type & STRUCT_CONST))
					perm |= ATTR_PERMDEL | ATTR_PERMSET;
			}
			break;

		case MEMBERCACHE_ATTRIB: {
			struct class_attribute *catt;
			struct instance_desc *inst;
			doc  = NULL;
			catt = item->mcs_attrib.a_attr;
			perm = ATTR_CMEMBER | ATTR_PERMGET | ATTR_PERMDEL | ATTR_PERMSET;
			if (catt->ca_doc) {
				doc = DeeString_STR(catt->ca_doc);
				perm |= ATTR_DOCOBJ;
				Dee_Incref(catt->ca_doc);
			}
			if (catt->ca_flag & CLASS_ATTRIBUTE_FPRIVATE)
				perm |= ATTR_PRIVATE;
			if (catt->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
				perm |= ATTR_PROPERTY;
				member_type = NULL;
			} else if (catt->ca_flag & CLASS_ATTRIBUTE_FMETHOD) {
				perm |= ATTR_PERMCALL;
				member_type = &DeeInstanceMethod_Type;
				Dee_Incref(member_type);
			} else {
				member_type = NULL;
			}
			inst = class_desc_as_instance(item->mcs_attrib.a_desc);
			Dee_instance_desc_lock_read(inst);
			if (catt->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
				if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_GET])
					perm &= ~ATTR_PERMGET;
				if (!(catt->ca_flag & CLASS_ATTRIBUTE_FREADONLY)) {
					if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_DEL])
						perm &= ~ATTR_PERMDEL;
					if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_SET])
						perm &= ~ATTR_PERMSET;
				}
			} else if (!(catt->ca_flag & CLASS_ATTRIBUTE_FMETHOD)) {
				ASSERT(!member_type);
				member_type = (DREF DeeTypeObject *)inst->id_vtab[catt->ca_addr + CLASS_GETSET_GET];
				if (member_type) {
					member_type = Dee_TYPE(member_type);
					Dee_Incref(member_type);
				}
			}
			Dee_instance_desc_lock_endread(inst);
			if (catt->ca_flag & CLASS_ATTRIBUTE_FREADONLY)
				perm &= ~(ATTR_PERMDEL | ATTR_PERMSET);
		}	break;

		case MEMBERCACHE_INSTANCE_METHOD:
			perm        = ATTR_CMEMBER | ATTR_IMEMBER | ATTR_WRAPPER | ATTR_PERMGET | ATTR_PERMCALL;
			doc         = item->mcs_method.m_doc;
			member_type = (item->mcs_method.m_flag & TYPE_METHOD_FKWDS)
			              ? &DeeKwClsMethod_Type
			              : &DeeClsMethod_Type;
			Dee_Incref(member_type);
			break;

		case MEMBERCACHE_INSTANCE_GETSET:
			perm        = ATTR_CMEMBER | ATTR_IMEMBER | ATTR_WRAPPER | ATTR_PROPERTY;
			doc         = item->mcs_getset.gs_doc;
			member_type = NULL /*&DeeClsProperty_Type*/;
			if (item->mcs_getset.gs_get)
				perm |= ATTR_PERMGET;
			if (item->mcs_getset.gs_del)
				perm |= ATTR_PERMDEL;
			if (item->mcs_getset.gs_set)
				perm |= ATTR_PERMSET;
			break;

		case MEMBERCACHE_INSTANCE_MEMBER:
			perm = ATTR_CMEMBER | ATTR_IMEMBER | ATTR_WRAPPER;
			doc  = item->mcs_member.m_doc;
			/*member_type = &DeeClsMember_Type*/;
			if (TYPE_MEMBER_ISCONST(&item->mcs_member)) {
				member_type = Dee_TYPE(item->mcs_member.m_const);
				Dee_Incref(member_type);
			} else {
				member_type = type_member_typefor(&item->mcs_member);
				Dee_XIncref(member_type);
				if (!(item->mcs_member.m_field.m_type & STRUCT_CONST))
					perm |= ATTR_PERMDEL | ATTR_PERMSET;
			}
			break;

		case MEMBERCACHE_INSTANCE_ATTRIB: {
			struct class_attribute *catt;
			doc  = NULL;
			catt = item->mcs_attrib.a_attr;
			perm = ATTR_CMEMBER | ATTR_IMEMBER | ATTR_WRAPPER | ATTR_PERMGET | ATTR_PERMDEL | ATTR_PERMSET;
			if (catt->ca_doc) {
				doc = DeeString_STR(catt->ca_doc);
				perm |= ATTR_DOCOBJ;
				Dee_Incref(catt->ca_doc);
			}
			if (catt->ca_flag & CLASS_ATTRIBUTE_FPRIVATE)
				perm |= ATTR_PRIVATE;
			if (catt->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
				perm |= ATTR_PROPERTY;
				member_type = NULL;
			} else if (catt->ca_flag & CLASS_ATTRIBUTE_FMETHOD) {
				perm |= ATTR_PERMCALL;
				member_type = &DeeInstanceMethod_Type;
				Dee_Incref(member_type);
			} else {
				member_type = NULL;
			}
			if (catt->ca_flag & CLASS_ATTRIBUTE_FCLASSMEM) {
				struct instance_desc *inst;
				inst = class_desc_as_instance(item->mcs_attrib.a_desc);
				Dee_instance_desc_lock_read(inst);
				if (catt->ca_flag & CLASS_ATTRIBUTE_FGETSET) {
					if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_GET])
						perm &= ~ATTR_PERMGET;
					if (!(catt->ca_flag & CLASS_ATTRIBUTE_FREADONLY)) {
						if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_DEL])
							perm &= ~ATTR_PERMDEL;
						if (!inst->id_vtab[catt->ca_addr + CLASS_GETSET_SET])
							perm &= ~ATTR_PERMSET;
					}
				} else if (!(catt->ca_flag & CLASS_ATTRIBUTE_FMETHOD)) {
					ASSERT(!member_type);
					member_type = (DREF DeeTypeObject *)inst->id_vtab[catt->ca_addr + CLASS_GETSET_GET];
					if (member_type) {
						member_type = Dee_TYPE(member_type);
						Dee_Incref(member_type);
					}
				}
				Dee_instance_desc_lock_endread(inst);
			}
			if (catt->ca_flag & CLASS_ATTRIBUTE_FREADONLY)
				perm &= ~(ATTR_PERMDEL | ATTR_PERMSET);
		}	break;

		default: __builtin_unreachable();
		}
		member_decl = item->mcs_decl;
		Dee_Incref(member_decl);
		Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
		if ((perm & rules->alr_perm_mask) != rules->alr_perm_value) {
			if (perm & ATTR_DOCOBJ)
				Dee_Decref(COMPILER_CONTAINER_OF(doc, DeeStringObject, s_str));
			Dee_XDecref(member_type);
			Dee_Decref(member_decl);
			goto not_found;
		}
		result->a_decl     = (DREF DeeObject *)member_decl;
		result->a_doc      = doc; /* Inherit reference. */
		result->a_perm     = perm;
		result->a_attrtype = member_type; /* Inherit reference. */
		return 0;
	}
	Dee_membercache_releasetable(&tp_self->tp_class_cache, table);
cache_miss:
not_found:
	return 1;
}
#endif /* !MRO_LEN */



#ifndef MRO_LEN
INTERN WUNUSED NONNULL((1, 2, 3)) struct class_attribute *
(DCALL DeeType_QueryAttributeWithHash)(DeeTypeObject *tp_invoker,
                                       DeeTypeObject *tp_self,
                                       /*String*/ DeeObject *attr,
                                       dhash_t hash) {
	struct class_attribute *result;
	result = DeeClass_QueryInstanceAttributeWithHash(tp_self, attr, hash);
	if (result)
		Dee_membercache_addattrib(&tp_invoker->tp_cache, tp_self, hash, result);
	return result;
}
#endif /* !MRO_LEN */

INTERN WUNUSED NONNULL((1, 2, 3)) struct class_attribute *
(DCALL S(DeeType_QueryAttributeStringWithHash,
         DeeType_QueryAttributeStringLenWithHash))(DeeTypeObject *tp_invoker,
                                                   DeeTypeObject *tp_self,
                                                   ATTR_ARG, dhash_t hash) {
	struct class_attribute *result;
	result = S(DeeClass_QueryInstanceAttributeStringWithHash(tp_self, attr, hash),
	           DeeClass_QueryInstanceAttributeStringLenWithHash(tp_self, attr, attrlen, hash));
	if (result)
		Dee_membercache_addattrib(&tp_invoker->tp_cache, tp_self, hash, result);
	return result;
}

#ifndef MRO_LEN
INTERN WUNUSED NONNULL((1, 2, 3)) struct class_attribute *
(DCALL DeeType_QueryClassAttributeWithHash)(DeeTypeObject *tp_invoker,
                                            DeeTypeObject *tp_self,
                                            /*String*/ DeeObject *attr,
                                            dhash_t hash) {
	struct class_attribute *result;
	result = DeeClass_QueryClassAttributeWithHash(tp_self, attr, hash);
	if (result)
		Dee_membercache_addattrib(&tp_invoker->tp_class_cache, tp_self, hash, result);
	return result;
}
#endif /* !MRO_LEN */

INTERN WUNUSED NONNULL((1, 2, 3)) struct class_attribute *
(DCALL S(DeeType_QueryClassAttributeStringWithHash,
         DeeType_QueryClassAttributeStringLenWithHash))(DeeTypeObject *tp_invoker,
                                                        DeeTypeObject *tp_self,
                                                        ATTR_ARG, dhash_t hash) {
	struct class_attribute *result;
	result = S(DeeClass_QueryClassAttributeStringWithHash(tp_self, attr, hash),
	           DeeClass_QueryClassAttributeStringLenWithHash(tp_self, attr, attrlen, hash));
	if (result)
		Dee_membercache_addattrib(&tp_invoker->tp_class_cache, tp_self, hash, result);
	return result;
}

#ifndef MRO_LEN
INTERN WUNUSED NONNULL((1, 2, 3)) struct class_attribute *
(DCALL DeeType_QueryInstanceAttributeWithHash)(DeeTypeObject *tp_invoker,
                                               DeeTypeObject *tp_self,
                                               /*String*/ DeeObject *attr,
                                               dhash_t hash) {
	struct class_attribute *result;
	result = DeeClass_QueryInstanceAttributeWithHash(tp_self, attr, hash);
	if (result)
		Dee_membercache_addinstanceattrib(&tp_invoker->tp_class_cache, tp_self, hash, result);
	return result;
}
#endif /* !MRO_LEN */

INTERN WUNUSED NONNULL((1, 2, 3)) struct class_attribute *
(DCALL S(DeeType_QueryInstanceAttributeStringWithHash,
         DeeType_QueryInstanceAttributeStringLenWithHash))(DeeTypeObject *tp_invoker,
                                                           DeeTypeObject *tp_self,
                                                           ATTR_ARG, dhash_t hash) {
	struct class_attribute *result;
	result = S(DeeClass_QueryInstanceAttributeStringWithHash(tp_self, attr, hash),
	           DeeClass_QueryInstanceAttributeStringLenWithHash(tp_self, attr, attrlen, hash));
	if (result)
		Dee_membercache_addinstanceattrib(&tp_invoker->tp_class_cache, tp_self, hash, result);
	return result;
}



INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) DREF DeeObject *DCALL
N_len(type_method_getattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_method const *chain, DeeObject *self,
                           ATTR_ARG, dhash_t hash) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmethod(cache, decl, hash, chain);
		return type_method_get(chain, self);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *
(DCALL NLen(DeeType_GetInstanceMethodAttr))(DeeTypeObject *tp_invoker,
                                            DeeTypeObject *tp_self,
                                            ATTR_ARG, dhash_t hash) {
	struct type_method const *chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemethod(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obmeth_get(tp_self, chain);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *
(DCALL NLen(DeeType_GetIInstanceMethodAttr))(DeeTypeObject *tp_invoker,
                                             DeeTypeObject *tp_self,
                                             ATTR_ARG, dhash_t hash) {
	struct type_method const *chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmethod(&tp_invoker->tp_cache, tp_self, hash, chain);
		return type_obmeth_get(tp_self, chain);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) DREF DeeObject *DCALL
N_len(type_method_callattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                            struct type_method const *chain, DeeObject *self,
                            ATTR_ARG, dhash_t hash,
                            size_t argc, DeeObject *const *argv) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmethod(cache, decl, hash, chain);
		return type_method_call(chain, self, argc, argv);
	}
	return ITER_DONE;
}

INTDEF WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *
(DCALL NLen(DeeType_CallInstanceMethodAttr))(DeeTypeObject *tp_invoker,
                                             DeeTypeObject *tp_self,
                                             ATTR_ARG, dhash_t hash,
                                             size_t argc, DeeObject *const *argv) {
	struct type_method const *chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemethod(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obmeth_call(tp_self, chain, argc, argv);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) DREF DeeObject *DCALL
S(type_method_callattr_kw,
  type_method_callattr_len_kw)(struct Dee_membercache *cache, DeeTypeObject *decl,
                               struct type_method const *chain, DeeObject *self,
                               ATTR_ARG, dhash_t hash,
                               size_t argc, DeeObject *const *argv,
                               DeeObject *kw) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmethod(cache, decl, hash, chain);
		return type_method_call_kw(chain, self, argc, argv, kw);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
S(DeeType_CallInstanceMethodAttrKw,
  DeeType_CallInstanceMethodAttrLenKw)(DeeTypeObject *tp_invoker,
                                       DeeTypeObject *tp_self,
                                       ATTR_ARG, dhash_t hash,
                                       size_t argc, DeeObject *const *argv,
                                       DeeObject *kw) {
	struct type_method const *chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemethod(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obmeth_call_kw(tp_self, chain, argc, argv, kw);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
S(DeeType_CallIInstanceMethodAttrKw,
  DeeType_CallIInstanceMethodAttrLenKw)(DeeTypeObject *tp_invoker,
                                        DeeTypeObject *tp_self,
                                        ATTR_ARG, dhash_t hash,
                                        size_t argc, DeeObject *const *argv,
                                        DeeObject *kw) {
	struct type_method const *chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmethod(&tp_invoker->tp_cache, tp_self, hash, chain);
		return type_obmeth_call_kw(tp_self, chain, argc, argv, kw);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4)) DREF DeeObject *DCALL
S(type_instance_method_callattr_kw,
  type_instance_method_callattr_len_kw)(struct Dee_membercache *cache, DeeTypeObject *decl,
                                        struct type_method const *chain, ATTR_ARG, dhash_t hash,
                                        size_t argc, DeeObject *const *argv,
                                        DeeObject *kw) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemethod(cache, decl, hash, chain);
		return type_obmeth_call_kw(decl, chain, argc, argv, kw);
	}
	return ITER_DONE;
}

#ifndef MRO_LEN
INTERN WUNUSED NONNULL((1, 2, 3, 4, 5, IFELSE(7, 8))) DREF DeeObject *DCALL
type_method_vcallattrf(struct Dee_membercache *cache, DeeTypeObject *decl,
                       struct type_method const *chain, DeeObject *self,
                       ATTR_ARG, dhash_t hash,
                       char const *__restrict format, va_list args) {
	DREF DeeObject *result, *args_tuple;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmethod(cache, decl, hash, chain);
		args_tuple = DeeTuple_VNewf(format, args);
		if unlikely(!args_tuple)
			goto err;
		result = type_method_call(chain, self,
		                          DeeTuple_SIZE(args_tuple),
		                          DeeTuple_ELEM(args_tuple));
		Dee_Decref(args_tuple);
		return result;
	}
	return ITER_DONE;
err:
	return NULL;
}

#if 0
INTERN WUNUSED NONNULL((1, 2, 3, IFELSE(5, 6))) DREF DeeObject *
(DCALL DeeType_VCallInstanceMethodAttrf)(DeeTypeObject *tp_invoker,
                                         DeeTypeObject *tp_self,
                                         ATTR_ARG, dhash_t hash,
                                         char const *__restrict format, va_list args) {
	DREF DeeObject *result, *args_tuple;
	struct type_method const *chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemethod(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		args_tuple = DeeTuple_VNewf(format, args);
		if unlikely(!args_tuple)
			goto err;
		result = type_obmeth_call(tp_self, chain,
		                          DeeTuple_SIZE(args_tuple),
		                          DeeTuple_ELEM(args_tuple));
		Dee_Decref(args_tuple);
		return result;
	}
	return ITER_DONE;
err:
	return NULL;
}

INTERN WUNUSED NONNULL((1, 2, 3, IFELSE(5, 6))) DREF DeeObject *
(DCALL DeeType_VCallIInstanceMethodAttrf)(DeeTypeObject *tp_invoker,
                                          DeeTypeObject *tp_self,
                                          ATTR_ARG, dhash_t hash,
                                          char const *__restrict format, va_list args) {
	DREF DeeObject *result, *args_tuple;
	struct type_method const *chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmethod(&tp_invoker->tp_cache, tp_self, hash, chain);
		args_tuple = DeeTuple_VNewf(format, args);
		if unlikely(!args_tuple)
			goto err;
		result = type_obmeth_call(tp_self, chain,
		                          DeeTuple_SIZE(args_tuple),
		                          DeeTuple_ELEM(args_tuple));
		Dee_Decref(args_tuple);
		return result;
	}
	return ITER_DONE;
err:
	return NULL;
}
#endif
#endif /* !MRO_LEN */


INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) DREF DeeObject *DCALL /* GET_GETSET */
N_len(type_getset_getattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_getset const *chain, DeeObject *self,
                           ATTR_ARG, dhash_t hash) {
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addgetset(cache, decl, hash, chain);
		return type_getset_get(chain, self);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_GetInstanceGetSetAttr)(DeeTypeObject *tp_invoker,
                                    DeeTypeObject *tp_self,
                                    ATTR_ARG, dhash_t hash) {
	struct type_getset const *chain = tp_self->tp_getsets;
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addinstancegetset(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obprop_get(tp_self, chain);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_GetIInstanceGetSetAttr)(DeeTypeObject *tp_invoker,
                                     DeeTypeObject *tp_self,
                                     ATTR_ARG, dhash_t hash) {
	struct type_getset const *chain = tp_self->tp_getsets;
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addgetset(&tp_invoker->tp_cache, tp_self, hash, chain);
		return type_obprop_get(tp_self, chain);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_CallInstanceGetSetAttr)(DeeTypeObject *tp_invoker,
                                     DeeTypeObject *tp_self,
                                     ATTR_ARG, dhash_t hash,
                                     size_t argc, DeeObject *const *argv) {
	struct type_getset const *chain = tp_self->tp_getsets;
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addinstancegetset(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obprop_call(tp_self, chain, argc, argv);
	}
	return ITER_DONE;
}

#if 0
INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_CallIInstanceGetSetAttr)(DeeTypeObject *tp_invoker,
                                      DeeTypeObject *tp_self,
                                      ATTR_ARG, dhash_t hash,
                                      size_t argc, DeeObject *const *argv) {
	struct type_getset const *chain = tp_self->tp_getsets;
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addgetset(&tp_invoker->tp_cache, tp_self, hash, chain);
		return type_obprop_call(tp_self, chain, argc, argv);
	}
	return ITER_DONE;
}
#endif

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
S(DeeType_CallInstanceGetSetAttrKw,
  DeeType_CallInstanceGetSetAttrLenKw)(DeeTypeObject *tp_invoker,
                                       DeeTypeObject *tp_self,
                                       ATTR_ARG, dhash_t hash,
                                       size_t argc, DeeObject *const *argv,
                                       DeeObject *kw) {
	struct type_getset const *chain = tp_self->tp_getsets;
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addinstancegetset(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obprop_call_kw(tp_self, chain, argc, argv, kw);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
S(DeeType_CallIInstanceGetSetAttrKw,
  DeeType_CallIInstanceGetSetAttrLenKw)(DeeTypeObject *tp_invoker,
                                        DeeTypeObject *tp_self,
                                        ATTR_ARG, dhash_t hash,
                                        size_t argc, DeeObject *const *argv,
                                        DeeObject *kw) {
	struct type_getset const *chain = tp_self->tp_getsets;
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addgetset(&tp_invoker->tp_cache, tp_self, hash, chain);
		return type_obprop_call_kw(tp_self, chain, argc, argv, kw);
	}
	return ITER_DONE;
}



INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) int DCALL /* BOUND_GETSET */
N_len(type_getset_boundattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                             struct type_getset const *chain, DeeObject *self,
                             ATTR_ARG, dhash_t hash) {
	DREF DeeObject *temp;
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addgetset(cache, decl, hash, chain);
		if (chain->gs_bound)
			return (*chain->gs_bound)(self);
		if unlikely(!chain->gs_get)
			return 0;
		temp = (*chain->gs_get)(self);
		if likely(temp) {
			Dee_Decref(temp);
			return 1;
		}
		if (CATCH_ATTRIBUTE_ERROR())
			return -3;
		if (DeeError_Catch(&DeeError_UnboundAttribute))
			return 0;
		return -1;
	}
	return -2;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) int DCALL /* DEL_GETSET */
N_len(type_getset_delattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_getset const *chain, DeeObject *self,
                           ATTR_ARG, dhash_t hash) {
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addgetset(cache, decl, hash, chain);
		return type_getset_del(chain, self);
	}
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4, 5, IFELSE(7, 8))) int DCALL /* SET_GETSET */
N_len(type_getset_setattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_getset const *chain, DeeObject *self,
                           ATTR_ARG, dhash_t hash, DeeObject *value) {
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addgetset(cache, decl, hash, chain);
		return type_getset_set(chain, self, value);
	}
	return 1;
}


INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) DREF DeeObject *DCALL /* GET_MEMBER */
N_len(type_member_getattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_member const *chain, DeeObject *self,
                           ATTR_ARG, dhash_t hash) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmember(cache, decl, hash, chain);
		return type_member_get(chain, self);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_GetInstanceMemberAttr)(DeeTypeObject *tp_invoker,
                                    DeeTypeObject *tp_self,
                                    ATTR_ARG, dhash_t hash) {
	struct type_member const *chain = tp_self->tp_members;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemember(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obmemb_get(tp_self, chain);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_GetIInstanceMemberAttr)(DeeTypeObject *tp_invoker,
                                     DeeTypeObject *tp_self,
                                     ATTR_ARG, dhash_t hash) {
	struct type_member const *chain = tp_self->tp_members;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmember(&tp_invoker->tp_cache, tp_self, hash, chain);
		return type_obmemb_get(tp_self, chain);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_CallInstanceMemberAttr)(DeeTypeObject *tp_invoker,
                                     DeeTypeObject *tp_self,
                                     ATTR_ARG, dhash_t hash,
                                     size_t argc, DeeObject *const *argv) {
	struct type_member const *chain = tp_self->tp_members;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemember(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obmemb_call(tp_self, chain, argc, argv);
	}
	return ITER_DONE;
}

#if 0
INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
NLen(DeeType_CallIInstanceMemberAttr)(DeeTypeObject *tp_invoker,
                                      DeeTypeObject *tp_self,
                                      ATTR_ARG, dhash_t hash,
                                      size_t argc, DeeObject *const *argv) {
	struct type_member const *chain = tp_self->tp_members;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmember(&tp_invoker->tp_cache, tp_self, hash, chain);
		return type_obmemb_call(tp_self, chain, argc, argv);
	}
	return ITER_DONE;
}
#endif

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
S(DeeType_CallInstanceMemberAttrKw,
  DeeType_CallInstanceMemberAttrLenKw)(DeeTypeObject *tp_invoker,
                                       DeeTypeObject *tp_self,
                                       ATTR_ARG, dhash_t hash,
                                       size_t argc, DeeObject *const *argv,
                                       DeeObject *kw) {
	struct type_member const *chain = tp_self->tp_members;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemember(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return type_obmemb_call_kw(tp_self, chain, argc, argv, kw);
	}
	return ITER_DONE;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
S(DeeType_CallIInstanceMemberAttrKw,
  DeeType_CallIInstanceMemberAttrLenKw)(DeeTypeObject *tp_invoker,
                                        DeeTypeObject *tp_self,
                                        ATTR_ARG, dhash_t hash,
                                        size_t argc, DeeObject *const *argv,
                                        DeeObject *kw) {
	struct type_member const *chain = tp_self->tp_members;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmember(&tp_invoker->tp_cache, tp_self, hash, chain);
		return type_obmemb_call_kw(tp_self, chain, argc, argv, kw);
	}
	return ITER_DONE;
}




INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) int DCALL /* BOUND_MEMBER */
N_len(type_member_boundattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                             struct type_member const *chain, DeeObject *self,
                             ATTR_ARG, dhash_t hash) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmember(cache, decl, hash, chain);
		return type_member_bound(chain, self);
	}
	return -2;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) int DCALL /* DEL_MEMBER */
N_len(type_member_delattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_member const *chain, DeeObject *self,
                           ATTR_ARG, dhash_t hash) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmember(cache, decl, hash, chain);
		return type_member_del(chain, self);
	}
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4, 5)) int DCALL /* SET_MEMBER */
N_len(type_member_setattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_member const *chain, DeeObject *self,
                           ATTR_ARG, dhash_t hash, DeeObject *value) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmember(cache, decl, hash, chain);
		return type_member_set(chain, self, value);
	}
	return 1;
}


INTERN WUNUSED NONNULL((1, 2, 3, 4)) bool DCALL /* METHOD */
N_len(type_method_hasattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_method const *chain, ATTR_ARG, dhash_t hash) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmethod(cache, decl, hash, chain);
		return true;
	}
	return false;
}

INTERN WUNUSED NONNULL((1, 2, 3)) bool DCALL
NLen(DeeType_HasInstanceMethodAttr)(DeeTypeObject *tp_invoker,
                                    DeeTypeObject *tp_self,
                                    ATTR_ARG, dhash_t hash) {
	struct type_method const *chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemethod(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return true;
	}
	return false;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4)) bool DCALL /* GETSET */
N_len(type_getset_hasattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_getset const *chain, ATTR_ARG, dhash_t hash) {
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addgetset(cache, decl, hash, chain);
		return true;
	}
	return false;
}

INTERN WUNUSED NONNULL((1, 2, 3)) bool DCALL
NLen(DeeType_HasInstanceGetSetAttr)(DeeTypeObject *tp_invoker,
                                    DeeTypeObject *tp_self,
                                    ATTR_ARG, dhash_t hash) {
	struct type_getset const *chain = tp_self->tp_getsets;
	for (; chain->gs_name; ++chain) {
		if (!NAMEEQ(chain->gs_name))
			continue;
		Dee_membercache_addinstancegetset(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return true;
	}
	return false;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4)) bool DCALL /* MEMBER */
N_len(type_member_hasattr)(struct Dee_membercache *cache, DeeTypeObject *decl,
                           struct type_member const *chain, ATTR_ARG, dhash_t hash) {
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addmember(cache, decl, hash, chain);
		return true;
	}
	return false;
}

INTERN WUNUSED NONNULL((1, 2, 3)) bool DCALL
NLen(DeeType_HasInstanceMemberAttr)(DeeTypeObject *tp_invoker,
                                    DeeTypeObject *tp_self,
                                    ATTR_ARG, dhash_t hash) {
	struct type_member const *chain = tp_self->tp_members;
	for (; chain->m_name; ++chain) {
		if (!NAMEEQ(chain->m_name))
			continue;
		Dee_membercache_addinstancemember(&tp_invoker->tp_class_cache, tp_self, hash, chain);
		return true;
	}
	return false;
}


#ifndef MRO_LEN
INTERN WUNUSED NONNULL((1, 2, 3, 5, 6)) int DCALL /* METHOD */
type_method_findattr(struct Dee_membercache *cache, DeeTypeObject *decl,
                     struct type_method const *chain, uint16_t perm,
                     struct attribute_info *__restrict result,
                     struct attribute_lookup_rules const *__restrict rules) {
	ASSERT(perm & (ATTR_IMEMBER | ATTR_CMEMBER));
	perm |= ATTR_PERMGET | ATTR_PERMCALL;
	if ((perm & rules->alr_perm_mask) != rules->alr_perm_value)
		goto nope;
	for (; chain->m_name; ++chain) {
		if (!streq(chain->m_name, rules->alr_name))
			continue;
		Dee_membercache_addmethod(cache, decl, rules->alr_hash, chain);
		ASSERT(!(perm & ATTR_DOCOBJ));
		result->a_doc      = chain->m_doc;
		result->a_decl     = (DREF DeeObject *)decl;
		result->a_perm     = perm;
		result->a_attrtype = (chain->m_flag & TYPE_METHOD_FKWDS)
		                     ? &DeeKwObjMethod_Type
		                     : &DeeObjMethod_Type;
		Dee_Incref(result->a_attrtype);
		Dee_Incref(decl);
		return 0;
	}
nope:
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4)) int DCALL
DeeType_FindInstanceMethodAttr(DeeTypeObject *tp_invoker,
                               DeeTypeObject *tp_self,
                               struct attribute_info *__restrict result,
                               struct attribute_lookup_rules const *__restrict rules) {
	uint16_t perm;
	struct type_method const *chain;
	perm = ATTR_IMEMBER | ATTR_CMEMBER | ATTR_PERMGET | ATTR_PERMCALL | ATTR_WRAPPER;
	if ((perm & rules->alr_perm_mask) != rules->alr_perm_value)
		goto nope;
	chain = tp_self->tp_methods;
	for (; chain->m_name; ++chain) {
		if (!streq(chain->m_name, rules->alr_name))
			continue;
		Dee_membercache_addinstancemethod(&tp_invoker->tp_class_cache, tp_self, rules->alr_hash, chain);
		ASSERT(!(perm & ATTR_DOCOBJ));
		result->a_doc      = chain->m_doc;
		result->a_decl     = (DREF DeeObject *)tp_self;
		result->a_perm     = perm;
		result->a_attrtype = (chain->m_flag & TYPE_METHOD_FKWDS)
		                     ? &DeeKwObjMethod_Type
		                     : &DeeObjMethod_Type;
		Dee_Incref(result->a_attrtype);
		Dee_Incref(tp_self);
		return 0;
	}
nope:
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, 3, 5, 6)) int DCALL /* GETSET */
type_getset_findattr(struct Dee_membercache *cache, DeeTypeObject *decl,
                     struct type_getset const *chain, uint16_t perm,
                     struct attribute_info *__restrict result,
                     struct attribute_lookup_rules const *__restrict rules) {
	ASSERT(perm & (ATTR_IMEMBER | ATTR_CMEMBER));
	perm |= ATTR_PROPERTY;
	for (; chain->gs_name; ++chain) {
		uint16_t flags = perm;
		if (chain->gs_get)
			flags |= ATTR_PERMGET;
		if (chain->gs_del)
			flags |= ATTR_PERMDEL;
		if (chain->gs_set)
			flags |= ATTR_PERMSET;
		if ((flags & rules->alr_perm_mask) != rules->alr_perm_value)
			continue;
		if (!streq(chain->gs_name, rules->alr_name))
			continue;
		Dee_membercache_addgetset(cache, decl, rules->alr_hash, chain);
		ASSERT(!(perm & ATTR_DOCOBJ));
		result->a_doc      = chain->gs_doc;
		result->a_perm     = flags;
		result->a_decl     = (DREF DeeObject *)decl;
		result->a_attrtype = NULL;
		Dee_Incref(decl);
		return 0;
	}
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4)) int DCALL
DeeType_FindInstanceGetSetAttr(DeeTypeObject *tp_invoker,
                               DeeTypeObject *tp_self,
                               struct attribute_info *__restrict result,
                               struct attribute_lookup_rules const *__restrict rules) {
	uint16_t perm;
	struct type_getset const *chain;
	perm  = ATTR_PROPERTY | ATTR_WRAPPER | ATTR_IMEMBER | ATTR_CMEMBER;
	chain = tp_self->tp_getsets;
	for (; chain->gs_name; ++chain) {
		uint16_t flags = perm;
		if (chain->gs_get)
			flags |= ATTR_PERMGET;
		if (chain->gs_del)
			flags |= ATTR_PERMDEL;
		if (chain->gs_set)
			flags |= ATTR_PERMSET;
		if ((flags & rules->alr_perm_mask) != rules->alr_perm_value)
			continue;
		if (!streq(chain->gs_name, rules->alr_name))
			continue;
		Dee_membercache_addinstancegetset(&tp_invoker->tp_class_cache, tp_self, rules->alr_hash, chain);
		ASSERT(!(perm & ATTR_DOCOBJ));
		result->a_doc      = chain->gs_doc;
		result->a_perm     = flags;
		result->a_decl     = (DREF DeeObject *)tp_self;
		result->a_attrtype = NULL;
		Dee_Incref(tp_self);
		return 0;
	}
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, 3, 5, 6)) int DCALL /* MEMBER */
type_member_findattr(struct Dee_membercache *cache, DeeTypeObject *decl,
                     struct type_member const *chain, uint16_t perm,
                     struct attribute_info *__restrict result,
                     struct attribute_lookup_rules const *__restrict rules) {
	ASSERT(perm & (ATTR_IMEMBER | ATTR_CMEMBER));
	perm |= ATTR_PERMGET;
	for (; chain->m_name; ++chain) {
		uint16_t flags = perm;
		if (!TYPE_MEMBER_ISCONST(chain) &&
		    !(chain->m_field.m_type & STRUCT_CONST))
			flags |= (ATTR_PERMDEL | ATTR_PERMSET);
		if ((flags & rules->alr_perm_mask) != rules->alr_perm_value)
			continue;
		if (!streq(chain->m_name, rules->alr_name))
			continue;
		Dee_membercache_addmember(cache, decl, rules->alr_hash, chain);
		ASSERT(!(perm & ATTR_DOCOBJ));
		result->a_doc      = chain->m_doc;
		result->a_perm     = flags;
		result->a_decl     = (DREF DeeObject *)decl;
		result->a_attrtype = type_member_typefor(chain);
		Dee_Incref(decl);
		Dee_XIncref(result->a_attrtype);
		return 0;
	}
	return 1;
}

INTERN WUNUSED NONNULL((1, 2, 3, 4)) int DCALL
DeeType_FindInstanceMemberAttr(DeeTypeObject *tp_invoker,
                               DeeTypeObject *tp_self,
                               struct attribute_info *__restrict result,
                               struct attribute_lookup_rules const *__restrict rules) {
	uint16_t perm;
	struct type_member const *chain;
	perm  = ATTR_WRAPPER | ATTR_IMEMBER | ATTR_CMEMBER | ATTR_PERMGET;
	chain = tp_self->tp_members;
	for (; chain->m_name; ++chain) {
		uint16_t flags = perm;
		if (!TYPE_MEMBER_ISCONST(chain) &&
		    !(chain->m_field.m_type & STRUCT_CONST))
			flags |= (ATTR_PERMDEL | ATTR_PERMSET);
		if ((flags & rules->alr_perm_mask) != rules->alr_perm_value)
			continue;
		if (!streq(chain->m_name, rules->alr_name))
			continue;
		Dee_membercache_addinstancemember(&tp_invoker->tp_class_cache, tp_self, rules->alr_hash, chain);
		ASSERT(!(perm & ATTR_DOCOBJ));
		result->a_doc      = chain->m_doc;
		result->a_perm     = flags;
		result->a_decl     = (DREF DeeObject *)tp_self;
		result->a_attrtype = type_member_typefor(chain);
		Dee_Incref(tp_self);
		Dee_XIncref(result->a_attrtype);
		return 0;
	}
	return 1;
}
#endif /* !MRO_LEN */

DECL_END

#undef S
#undef N_len
#undef NLen
#undef IFELSE
#undef IF_LEN
#undef IF_NLEN
#undef ATTR_ARG
#undef ATTREQ
#undef NAMEEQ
#undef MRO_LEN
