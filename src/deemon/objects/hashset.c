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
#ifndef GUARD_DEEMON_OBJECTS_HASHSET_C
#define GUARD_DEEMON_OBJECTS_HASHSET_C 1

#include <deemon/api.h>
#include <deemon/alloc.h>
#include <deemon/object.h>
#include <deemon/hashset.h>
#include <deemon/bool.h>
#include <deemon/gc.h>
#include <deemon/float.h>
#include <deemon/none.h>
#include <deemon/string.h>
#include <deemon/thread.h>
#include <deemon/format.h>
#include <deemon/arg.h>
#include <deemon/seq.h>
#include <deemon/set.h>
#include <deemon/roset.h>
#include <deemon/int.h>

#include "../runtime/strings.h"
#include "../runtime/runtime_error.h"

DECL_BEGIN

typedef DeeHashSetObject Set;

PRIVATE struct hashset_item empty_set_items[1] = {
    { NULL, 0 }
};

INTDEF DeeObject dict_dummy;
#define dummy  (&dict_dummy)


PUBLIC DREF DeeObject *DCALL
DeeHashSet_NewItemsInherited(size_t num_items, DREF DeeObject **__restrict items) {
 DREF Set *result;
 /* Allocate the set object. */
 result = DeeGCObject_MALLOC(Set);
 if unlikely(!result) goto done;
 if unlikely(!num_items) {
  /* Special case: allocate an empty set. */
  result->s_mask = 0;
  result->s_size = 0;
  result->s_used = 0;
  result->s_elem = empty_set_items;
 } else {
  size_t min_mask = 16-1,mask;
  /* Figure out how large the mask of the set is going to be. */
  while ((num_items&min_mask) != num_items)
          min_mask = (min_mask << 1)|1;
  /* Prefer using a mask of one greater level to improve performance. */
  mask = (min_mask << 1)|1;
  result->s_elem = (struct hashset_item *)Dee_TryCalloc((mask+1)*sizeof(struct hashset_item));
  if unlikely(!result->s_elem) {
   /* Try one level less if that failed. */
   mask = min_mask;
   result->s_elem = (struct hashset_item *)Dee_Calloc((mask+1)*sizeof(struct hashset_item));
   if unlikely(!result->s_elem) goto err_r;
  }
  /* Without any dummy items, these are identical. */
  result->s_used = num_items;
  result->s_size = num_items;
  while (num_items--) {
   DREF DeeObject *key = *items++;
   dhash_t i,perturb,hash = DeeObject_Hash(key);
   perturb = i = hash & mask;
   for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
    struct hashset_item *item = &result->s_elem[i & mask];
    if (item->si_key) continue; /* Already in use */
    item->si_hash = hash;
    item->si_key  = key;   /* Inherit reference. */
   }
  }
 }
#ifndef CONFIG_NO_THREADS
 rwlock_init(&result->s_lock);
#endif /* !CONFIG_NO_THREADS */
 /* Initialize and start tracking the new set. */
 weakref_support_init(result);
 DeeObject_Init(result,&DeeHashSet_Type);
 DeeGC_Track((DeeObject *)result);
done:
 return (DREF DeeObject *)result;
err_r:
 Dee_Free(result->s_elem);
 DeeGCObject_FREE(result);
 return NULL;
}


PRIVATE int DCALL
set_init_iterator(Set *__restrict self,
                  DeeObject *__restrict iterator) {
 DREF DeeObject *elem;
 self->s_mask = 0;
 self->s_size = 0;
 self->s_used = 0;
 self->s_elem = empty_set_items;
#ifndef CONFIG_NO_THREADS
 rwlock_init(&self->s_lock);
#endif /* !CONFIG_NO_THREADS */
 weakref_support_init(self);
 while (ITER_ISOK(elem = DeeObject_IterNext(iterator))) {
  if unlikely(DeeHashSet_Insert((DeeObject *)self,elem) < 0)
     goto err_elem;
  Dee_Decref(elem);
  if (DeeThread_CheckInterrupt())
      goto err;
 }
 if unlikely(!elem) goto err;
 return 0;
err_elem:
 Dee_Decref(elem);
err:
 return -1;
}

PRIVATE int DCALL set_copy(Set *__restrict self, Set *__restrict other);

STATIC_ASSERT(sizeof(struct hashset_item) == sizeof(struct roset_item));
STATIC_ASSERT(COMPILER_OFFSETOF(struct hashset_item,si_key) == COMPILER_OFFSETOF(struct roset_item,si_key));
STATIC_ASSERT(COMPILER_OFFSETOF(struct hashset_item,si_hash) == COMPILER_OFFSETOF(struct roset_item,si_hash));

PRIVATE int DCALL
set_init_sequence(Set *__restrict self,
                  DeeObject *__restrict sequence) {
 DREF DeeObject *iterator; int error;
 DeeTypeObject *tp = Dee_TYPE(sequence);
 if (tp == &DeeHashSet_Type)
     return set_copy(self,(Set *)sequence);
 /* Optimizations for `_roset' */
 if (tp == &DeeRoSet_Type) {
  struct hashset_item *iter,*end;
  DeeRoSetObject *src = (DeeRoSetObject *)sequence;
#ifndef CONFIG_NO_THREADS
  rwlock_init(&self->s_lock);
#endif /* !CONFIG_NO_THREADS */
  self->s_mask = src->rs_mask;
  self->s_used = self->s_size = src->rs_size;
  if unlikely(!self->s_size)
     self->s_elem = (struct hashset_item *)empty_set_items;
  else {
   self->s_elem = (struct hashset_item *)Dee_Malloc((src->rs_mask + 1) *
                                                     sizeof(struct hashset_item));
   if unlikely(!self->s_elem)
      goto err;
   memcpy(self->s_elem,src->rs_elem,
         (self->s_mask + 1) * sizeof(struct hashset_item));
   end = (iter = self->s_elem) + (self->s_mask + 1);
   for (; iter != end; ++iter)
       Dee_XIncref(iter->si_key);
  }
  weakref_support_init(self);
  return 0;
 }
 /* TODO: Optimizations for `_sharedvector' */
 /* TODO: Fast-sequence support */

 iterator = DeeObject_IterSelf(sequence);
 if unlikely(!iterator) goto err;
 error = set_init_iterator(self,iterator);
 Dee_Decref(iterator);
 return error;
err:
 return -1;
}

PUBLIC DREF DeeObject *DCALL
DeeHashSet_FromIterator(DeeObject *__restrict self) {
 DREF Set *result;
 result = DeeGCObject_MALLOC(Set);
 if unlikely(!result) goto done;
 if unlikely(set_init_iterator(result,self)) goto err;
 DeeObject_Init(result,&DeeHashSet_Type);
 DeeGC_Track((DeeObject *)result);
done:
 return (DREF DeeObject *)result;
err:
 DeeGCObject_FREE(result);
 return NULL;
}

PUBLIC DREF DeeObject *DCALL
DeeHashSet_FromSequence(DeeObject *__restrict self) {
 DREF Set *result;
 result = DeeGCObject_MALLOC(Set);
 if unlikely(!result) goto done;
 if unlikely(set_init_sequence(result,self)) goto err;
 DeeObject_Init(result,&DeeHashSet_Type);
 DeeGC_Track((DeeObject *)result);
done:
 return (DREF DeeObject *)result;
err:
 DeeGCObject_FREE(result);
 return NULL;
}


PRIVATE int DCALL
set_ctor(Set *__restrict self) {
 self->s_mask = 0;
 self->s_size = 0;
 self->s_used = 0;
 self->s_elem = empty_set_items;
#ifndef CONFIG_NO_THREADS
 rwlock_init(&self->s_lock);
#endif /* !CONFIG_NO_THREADS */
 weakref_support_init(self);
 return 0;
}

PRIVATE int DCALL
set_copy(Set *__restrict self,
         Set *__restrict other) {
 struct hashset_item *iter,*end;
#ifndef CONFIG_NO_THREADS
 rwlock_init(&self->s_lock);
#endif /* !CONFIG_NO_THREADS */
again:
 DeeHashSet_LockRead(other);
 self->s_mask = other->s_mask;
 self->s_size = other->s_size;
 self->s_used = other->s_used;
 if ((self->s_elem = other->s_elem) != empty_set_items) {
  self->s_elem = (struct hashset_item *)Dee_TryMalloc((other->s_mask+1)*sizeof(struct hashset_item));
  if unlikely(!self->s_elem) {
   DeeHashSet_LockEndRead(other);
   if (Dee_CollectMemory((other->s_mask+1)*sizeof(struct hashset_item)))
       goto again;
   return -1;
  }
  memcpy(self->s_elem,other->s_elem,
        (self->s_mask+1)*sizeof(struct hashset_item));
  end = (iter = self->s_elem)+(self->s_mask+1);
  for (; iter != end; ++iter) {
   if (!iter->si_key) continue;
   Dee_Incref(iter->si_key);
  }
 }
 DeeHashSet_LockEndRead(other);
 weakref_support_init(self);
 return 0;
}

PRIVATE int DCALL
set_deepload(Set *__restrict self) {
 DREF DeeObject **new_items,**items = NULL;
 size_t i,hash_i,item_count,ols_item_count = 0;
 struct hashset_item *new_map,*ols_map; size_t new_mask;
 for (;;) {
  DeeHashSet_LockRead(self);
  /* Optimization: if the dict is empty, then there's nothing to copy! */
  if (self->s_elem == empty_set_items) {
   DeeHashSet_LockEndRead(self);
   return 0;
  }
  item_count = self->s_used;
  if (item_count <= ols_item_count)
      break;
  DeeHashSet_LockEndRead(self);
  new_items = (DREF DeeObject **)Dee_Realloc(items,item_count*sizeof(DREF DeeObject *));
  if unlikely(!new_items) goto err_items;
  ols_item_count = item_count;
  items = new_items;
 }
 /* Copy all used items. */
 for (i = 0,hash_i = 0; i < item_count; ++hash_i) {
  ASSERT(hash_i <= self->s_mask);
  if (self->s_elem[hash_i].si_key == NULL) continue;
  if (self->s_elem[hash_i].si_key == dummy) continue;
  items[i] = self->s_elem[hash_i].si_key;
  Dee_Incref(items[i]);
  ++i;
 }
 DeeHashSet_LockEndRead(self);
 /* With our own local copy of all items being
  * used, replace all of them with deep copies. */
 for (i = 0; i < item_count; ++i) {
  if (DeeObject_InplaceDeepCopy(&items[i]))
      goto err_items_v;
 }
 new_mask = 1;
 while ((item_count & new_mask) != item_count)
         new_mask = (new_mask << 1)|1;
 new_map = (struct hashset_item *)Dee_Calloc((new_mask+1)*sizeof(struct hashset_item));
 if unlikely(!new_map) goto err_items_v;
 /* Insert all the copied items into the new map. */
 for (i = 0; i < item_count; ++i) {
  dhash_t j,perturb,hash;
  hash = DeeObject_Hash(items[i]);
  perturb = j = hash & new_mask;
  for (;; j = HASHSET_HASHNX(j,perturb),HASHSET_HASHPT(perturb)) {
   struct hashset_item *item = &new_map[j & new_mask];
   if (item->si_key) continue; /* Already in use */
   item->si_hash = hash;
   item->si_key  = items[i]; /* Inherit reference. */
   break;
  }
 }
 DeeHashSet_LockWrite(self);
 i = self->s_mask + 1;
 self->s_mask = new_mask;
 self->s_used = item_count;
 self->s_size = item_count;
 ols_map = self->s_elem;
 self->s_elem = new_map;
 DeeHashSet_LockEndWrite(self);
 if (ols_map != empty_set_items) {
  while (i--)
      Dee_XDecref(ols_map[i].si_key);
  Dee_Free(ols_map);
 }
 Dee_Free(items);
 return 0;
err_items_v:
 i = item_count;
 while (i--)
     Dee_Decref(items[i]);
err_items:
 Dee_Free(items);
 return -1;
}

PRIVATE void DCALL set_fini(Set *__restrict self) {
 weakref_support_fini(self);
 ASSERT((self->s_elem == empty_set_items) == (self->s_mask == 0));
 ASSERT((self->s_elem == empty_set_items) == (self->s_size == 0));
 ASSERT(self->s_used <= self->s_size);
 if (self->s_elem != empty_set_items) {
  struct hashset_item *iter,*end;
  end = (iter = self->s_elem)+(self->s_mask+1);
  for (; iter != end; ++iter) {
   if (!iter->si_key) continue;
   Dee_Decref(iter->si_key);
  }
  Dee_Free(self->s_elem);
 }
}

PRIVATE void DCALL set_clear(Set *__restrict self) {
 struct hashset_item *elem; size_t mask;
 DeeHashSet_LockWrite(self);
 ASSERT((self->s_elem == empty_set_items) == (self->s_mask == 0));
 ASSERT((self->s_elem == empty_set_items) == (self->s_used == 0));
 ASSERT((self->s_elem == empty_set_items) == (self->s_size == 0));
 ASSERT(self->s_used <= self->s_size);
 /* Extract the vector and mask. */
 elem = self->s_elem;
 mask = self->s_mask;
 self->s_elem = empty_set_items;
 self->s_mask = 0;
 self->s_used = 0;
 self->s_size = 0;
 DeeHashSet_LockEndWrite(self);
 /* Destroy the vector. */
 if (elem != empty_set_items) {
  struct hashset_item *iter,*end;
  end = (iter = elem)+(mask+1);
  for (; iter != end; ++iter) {
   if (!iter->si_key) continue;
   Dee_Decref(iter->si_key);
  }
  Dee_Free(elem);
 }
}

PRIVATE void DCALL
set_visit(Set *__restrict self, dvisit_t proc, void *arg) {
 DeeHashSet_LockRead(self);
 ASSERT((self->s_elem == empty_set_items) == (self->s_mask == 0));
 ASSERT((self->s_elem == empty_set_items) == (self->s_used == 0));
 ASSERT((self->s_elem == empty_set_items) == (self->s_size == 0));
 ASSERT(self->s_used <= self->s_size);
 if (self->s_elem != empty_set_items) {
  struct hashset_item *iter,*end;
  end = (iter = self->s_elem)+(self->s_mask+1);
  for (; iter != end; ++iter) {
   if (!iter->si_key) continue;
   /* Visit all keys and associated values. */
   Dee_Visit(iter->si_key);
  }
 }
 DeeHashSet_LockEndRead(self);
}


/* Resize the hash size by a factor of 2 and re-insert all elements.
 * When `sizedir > 0', increase the hash; When `sizedir < 0', decrease it.
 * During this process all dummy items are discarded.
 * @return: true:  Successfully rehashed the set.
 * @return: false: Not enough memory. - The caller should collect some and try again. */
PRIVATE bool DCALL
set_rehash(Set *__restrict self, int sizedir) {
 struct hashset_item *new_vector,*iter,*end;
 size_t new_mask = self->s_mask;
 if (sizedir > 0) {
  new_mask = (new_mask << 1)|1;
  if unlikely(new_mask == 1) new_mask = 16-1; /* Start out bigger than 2. */
 } else if (sizedir < 0) {
  new_mask = (new_mask >> 1);
  if unlikely(!new_mask) {
   ASSERT(!self->s_used);
   /* Special case: delete the vector. */
   if (self->s_size) {
    ASSERT(self->s_elem != empty_set_items);
    /* Must discard dummy items. */
    end = (iter = self->s_elem)+(self->s_mask+1);
    for (; iter != end; ++iter) {
     ASSERT(iter->si_key == NULL ||
            iter->si_key == dummy);
     if (iter->si_key == dummy)
         Dee_DecrefNokill(dummy);
    }
   }
   if (self->s_elem != empty_set_items)
       Dee_Free(self->s_elem);
   self->s_elem = empty_set_items;
   self->s_mask = 0;
   self->s_size = 0;
   return true;
  }
 }
 ASSERT(self->s_size < new_mask);
 ASSERT(self->s_used <= self->s_size);
 new_vector = (struct hashset_item *)Dee_TryCalloc((new_mask+1)*sizeof(struct hashset_item));
 if unlikely(!new_vector) return false;
 ASSERT((self->s_elem == empty_set_items) == (self->s_mask == 0));
 ASSERT((self->s_elem == empty_set_items) == (self->s_used == 0));
 ASSERT((self->s_elem == empty_set_items) == (self->s_size == 0));
 if (self->s_elem != empty_set_items) {
  /* Re-insert all existing items into the new set vector. */
  end = (iter = self->s_elem)+(self->s_mask+1);
  for (; iter != end; ++iter) {
   struct hashset_item *item;
   dhash_t i,perturb;
   /* Skip dummy keys. */
   if (iter->si_key == dummy) continue;
   perturb = i = iter->si_hash & new_mask;
   for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
    item = &new_vector[i & new_mask];
    if (!item->si_key) break; /* Empty slot found. */
   }
   /* Transfer this object. */
   item->si_key  = iter->si_key;
   item->si_hash = iter->si_hash;
  }
  Dee_Free(self->s_elem);
  /* With all dummy items gone, the size now equals what is actually used. */
  self->s_size = self->s_used;
 }
 ASSERT(self->s_size == self->s_used);
 self->s_mask = new_mask;
 self->s_elem = new_vector;
 return true;
}






PUBLIC DREF DeeObject *DCALL
DeeHashSet_Unify(DeeObject *__restrict self,
                 DeeObject *__restrict search_item) {
 Set *me = (Set *)self;
 size_t mask; struct hashset_item *vector; int error;
 struct hashset_item *first_dummy;
 dhash_t i,perturb,hash = DeeObject_Hash(search_item);
again_lock:
 DeeHashSet_LockRead(me);
again:
 first_dummy = NULL;
 vector = me->s_elem;
 mask   = me->s_mask;
 perturb = i = hash & mask;
 for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
  DREF DeeObject *item_key;
  struct hashset_item *item = &vector[i & mask];
  if (!item->si_key) { if (!first_dummy) first_dummy = item; break; /* Not found */ }
  if (item->si_key == dummy) { first_dummy = item; continue; }
  if (item->si_hash != hash) continue; /* Non-matching hash */
  item_key   = item->si_key;
  Dee_Incref(item_key);
  DeeHashSet_LockEndRead(me);
  /* Invoke the compare operator outside of any lock. */
  error = DeeObject_CompareEq(search_item,item_key);
  Dee_Decref(item_key);
  if unlikely(error < 0)
     return NULL; /* Error in compare operator. */
  if (error > 0) {
   DeeHashSet_LockWrite(me);
   /* Check if the set was modified. */
   if (me->s_elem != vector ||
       me->s_mask != mask ||
       item->si_key != item_key) {
    DeeHashSet_LockDowngrade(me);
    goto again;
   }
   Dee_Incref(item_key);
   DeeHashSet_LockEndWrite(me);
   return item_key; /* Already exists. */
  }
  DeeHashSet_LockRead(me);
  /* Check if the set was modified. */
  if (me->s_elem != vector ||
      me->s_mask != mask ||
      item->si_key != item_key)
      goto again;
 }
#ifndef CONFIG_NO_THREADS
 if (!DeeHashSet_LockUpgrade(me)) {
  DeeHashSet_LockEndWrite(me);
  SCHED_YIELD();
  goto again_lock;
 }
#endif
 if (first_dummy && me->s_size+1 < me->s_mask) {
  ASSERT(first_dummy != empty_set_items);
  ASSERT(!first_dummy->si_key ||
          first_dummy->si_key == dummy);
  if (first_dummy->si_key)
      Dee_DecrefNokill(first_dummy->si_key);
  /* Fill in the target slot. */
  first_dummy->si_key   = search_item;
  first_dummy->si_hash  = hash;
  Dee_Incref(search_item);
  ++me->s_used;
  ++me->s_size;
  /* Try to keep the set vector big at least twice as big as the element count. */
  if (me->s_size*2 > me->s_mask)
      set_rehash(me,1);
  DeeHashSet_LockEndWrite(me);
  Dee_Incref(search_item);
  return search_item; /* New item. */
 }
 /* Rehash the set and try again. */
 if (set_rehash(me,1)) {
  DeeHashSet_LockDowngrade(me);
  goto again;
 }
 DeeHashSet_LockEndWrite(me);
 if (Dee_CollectMemory(1)) goto again_lock;
 return NULL;
}
PUBLIC int DCALL
DeeHashSet_Insert(DeeObject *__restrict self, DeeObject *__restrict search_item) {
 Set *me = (Set *)self;
 size_t mask; struct hashset_item *vector; int error;
 struct hashset_item *first_dummy;
 dhash_t i,perturb,hash = DeeObject_Hash(search_item);
again_lock:
 DeeHashSet_LockRead(me);
again:
 first_dummy = NULL;
 vector = me->s_elem;
 mask   = me->s_mask;
 perturb = i = hash & mask;
 for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
  DREF DeeObject *item_key;
  struct hashset_item *item = &vector[i & mask];
  if (!item->si_key) { if (!first_dummy) first_dummy = item; break; /* Not found */ }
  if (item->si_key == dummy) { first_dummy = item; continue; }
  if (item->si_hash != hash) continue; /* Non-matching hash */
  item_key   = item->si_key;
  Dee_Incref(item_key);
  DeeHashSet_LockEndRead(me);
  /* Invoke the compare operator outside of any lock. */
  error = DeeObject_CompareEq(search_item,item_key);
  Dee_Decref(item_key);
  if unlikely(error < 0)
     return -1; /* Error in compare operator. */
  DeeHashSet_LockRead(me);
  /* Check if the set was modified. */
  if (me->s_elem != vector ||
      me->s_mask != mask ||
      item->si_key != item_key)
      goto again;
  if (error > 0) {
   DeeHashSet_LockEndRead(me);
   return 0; /* Already exists. */
  }
 }
#ifndef CONFIG_NO_THREADS
 if (!DeeHashSet_LockUpgrade(me)) {
  DeeHashSet_LockEndWrite(me);
  SCHED_YIELD();
  goto again_lock;
 }
#endif
 if (first_dummy && me->s_size+1 < me->s_mask) {
  ASSERT(first_dummy != empty_set_items);
  ASSERT(!first_dummy->si_key ||
          first_dummy->si_key == dummy);
  if (first_dummy->si_key)
      Dee_DecrefNokill(first_dummy->si_key);
  /* Fill in the target slot. */
  first_dummy->si_key   = search_item;
  first_dummy->si_hash  = hash;
  Dee_Incref(search_item);
  ++me->s_used;
  ++me->s_size;
  /* Try to keep the set vector big at least twice as big as the element count. */
  if (me->s_size*2 > me->s_mask)
      set_rehash(me,1);
  DeeHashSet_LockEndWrite(me);
  return 1; /* New item. */
 }
 /* Rehash the set and try again. */
 if (set_rehash(me,1)) {
  DeeHashSet_LockDowngrade(me);
  goto again;
 }
 DeeHashSet_LockEndWrite(me);
 if (Dee_CollectMemory(1)) goto again_lock;
 return -1;
}
PUBLIC int DCALL
DeeHashSet_Remove(DeeObject *__restrict self, DeeObject *__restrict search_item) {
 Set *me = (Set *)self;
 size_t mask; struct hashset_item *vector;
 dhash_t i,perturb; int error;
 dhash_t hash = DeeObject_Hash(search_item);
 DeeHashSet_LockRead(me);
restart:
 vector = me->s_elem;
 mask   = me->s_mask;
 perturb = i = hash & mask;
 for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
  DREF DeeObject *item_key;
  struct hashset_item *item = &vector[i & mask];
  if (!item->si_key) break; /* Not found */
  if (item->si_hash != hash) continue; /* Non-matching hash */
  if (item->si_key == dummy) continue; /* Dummy key. */
  item_key   = item->si_key;
  Dee_Incref(item_key);
  DeeHashSet_LockEndWrite(me);
  /* Invoke the compare operator outside of any lock. */
  error = DeeObject_CompareEq(search_item,item_key);
  Dee_Decref(item_key);
  if unlikely(error < 0)
     return -1; /* Error in compare operator. */
  if (error > 0) {
   /* Found it! */
   DeeHashSet_LockWrite(me);
   /* Check if the set was modified. */
   if (me->s_elem != vector ||
       me->s_mask != mask ||
       item->si_key != item_key)
       goto restart;
   item->si_key = dummy;
   Dee_Incref(dummy);
   ASSERT(me->s_used);
   if (--me->s_used < me->s_size/2)
       set_rehash(me,-1);
   DeeHashSet_LockEndWrite(me);
   Dee_Decref(item_key);
   return 0;
  }
  DeeHashSet_LockRead(me);
  /* Check if the set was modified. */
  if (me->s_elem != vector ||
      me->s_mask != mask ||
      item->si_key != item_key)
      goto restart;
 }
 DeeHashSet_LockEndRead(me);
 err_unknown_key((DeeObject *)me,search_item);
 return -1;
}



PUBLIC DREF DeeObject *DCALL
DeeHashSet_UnifyString(DeeObject *__restrict self,
                       char const *__restrict search_item,
                       size_t search_item_length) {
 Set *me = (Set *)self; DREF DeeStringObject *result = NULL;
 size_t mask; struct hashset_item *vector; struct hashset_item *first_dummy;
 dhash_t i,perturb,hash;
 hash = Dee_HashPtr(search_item,search_item_length);
again_lock:
 DeeHashSet_LockRead(me);
again:
 first_dummy = NULL;
 vector = me->s_elem;
 mask   = me->s_mask;
 perturb = i = hash & mask;
 for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
  DREF DeeObject *existing_key;
  struct hashset_item *item = &vector[i & mask];
  if (!item->si_key) { if (!first_dummy) first_dummy = item; break; /* Not found */ }
  if (!DeeString_Check(item->si_key)) {
   if (item->si_key == dummy) first_dummy = item;
   continue;
  }
  if (item->si_hash != hash) continue; /* Non-matching hash */
  if (DeeString_SIZE(item->si_key) != search_item_length) continue; /* Non-matching length */
  if (memcmp(DeeString_STR(item->si_key),search_item,
             search_item_length*sizeof(char)) != 0)
      continue; /* Differing strings. */
  existing_key = item->si_key;
  Dee_Incref(existing_key);
  DeeHashSet_LockEndRead(me);
  Dee_XDecref(result);
  return existing_key; /* Already exists. */
 }
 if likely(!result) {
  /* Create the actual string that's going to get stored. */
  result = (DREF DeeStringObject *)DeeObject_TryMalloc(offsetof(DeeStringObject,s_str)+
                                                      (search_item_length+1)*sizeof(char));
  if unlikely(!result) {
   DeeHashSet_LockEndRead(me);
   if (Dee_CollectMemory(offsetof(DeeStringObject,s_str)+(search_item_length+1)*sizeof(char)))
       goto again_lock;
   return NULL;
  }
  DeeObject_Init(result,&DeeString_Type);
  result->s_data = NULL;
  result->s_hash = hash;
  result->s_len  = search_item_length;
  memcpy(result->s_str,search_item,search_item_length*sizeof(char));
  result->s_str[search_item_length] = '\0';
 }
#ifndef CONFIG_NO_THREADS
 if (!DeeHashSet_LockUpgrade(me)) {
  DeeHashSet_LockEndWrite(me);
  SCHED_YIELD();
  goto again_lock;
 }
#endif
 if (first_dummy && me->s_size+1 < me->s_mask) {
  ASSERT(first_dummy != empty_set_items);
  ASSERT(!first_dummy->si_key ||
          first_dummy->si_key == dummy);
  if (first_dummy->si_key)
      Dee_DecrefNokill(first_dummy->si_key);
  /* Fill in the target slot. */
  first_dummy->si_key  = (DREF DeeObject *)result;
  first_dummy->si_hash = hash;
  Dee_Incref(result);
  ++me->s_used;
  ++me->s_size;
  /* Try to keep the set vector big at least twice as big as the element count. */
  if (me->s_size*2 > me->s_mask)
      set_rehash(me,1);
  DeeHashSet_LockEndWrite(me);
  return (DREF DeeObject *)result; /* New item. */
 }
 /* Rehash the set and try again. */
 if (set_rehash(me,1)) {
  DeeHashSet_LockDowngrade(me);
  goto again;
 }
 DeeHashSet_LockEndWrite(me);
 if (Dee_CollectMemory(1)) goto again_lock;
 Dee_Decref(result);
 return NULL;
}
PUBLIC int DCALL
DeeHashSet_InsertString(DeeObject *__restrict self,
                        char const *__restrict search_item,
                        size_t search_item_length) {
 Set *me = (Set *)self; DREF DeeStringObject *new_item = NULL;
 size_t mask; struct hashset_item *vector; struct hashset_item *first_dummy;
 dhash_t i,perturb,hash;
 hash = Dee_HashPtr(search_item,search_item_length);
again_lock:
 DeeHashSet_LockRead(me);
again:
 first_dummy = NULL;
 vector = me->s_elem;
 mask   = me->s_mask;
 perturb = i = hash & mask;
 for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
  struct hashset_item *item = &vector[i & mask];
  if (!item->si_key) { if (!first_dummy) first_dummy = item; break; /* Not found */ }
  if (!DeeString_Check(item->si_key)) {
   if (item->si_key == dummy) first_dummy = item;
   continue;
  }
  if (item->si_hash != hash) continue; /* Non-matching hash */
  if (DeeString_SIZE(item->si_key) != search_item_length) continue; /* Non-matching length */
  if (memcmp(DeeString_STR(item->si_key),search_item,
             search_item_length*sizeof(char)) != 0)
      continue; /* Differing strings. */
  DeeHashSet_LockEndRead(me);
  Dee_XDecref(new_item);
  return 0; /* Already exists. */
 }
 if likely(!new_item) {
  /* Create the actual string that's going to get stored. */
  new_item = (DREF DeeStringObject *)DeeObject_TryMalloc(offsetof(DeeStringObject,s_str)+
                                                      (search_item_length+1)*sizeof(char));
  if unlikely(!new_item) {
   DeeHashSet_LockEndRead(me);
   if (Dee_CollectMemory(offsetof(DeeStringObject,s_str)+(search_item_length+1)*sizeof(char)))
       goto again_lock;
   return -1;
  }
  DeeObject_Init(new_item,&DeeString_Type);
  new_item->s_data = NULL;
  new_item->s_hash = hash;
  new_item->s_len  = search_item_length;
  memcpy(new_item->s_str,search_item,search_item_length*sizeof(char));
  new_item->s_str[search_item_length] = '\0';
 }
#ifndef CONFIG_NO_THREADS
 if (!DeeHashSet_LockUpgrade(me)) {
  DeeHashSet_LockEndWrite(me);
  SCHED_YIELD();
  goto again_lock;
 }
#endif
 if (first_dummy && me->s_size+1 < me->s_mask) {
  ASSERT(first_dummy != empty_set_items);
  ASSERT(!first_dummy->si_key ||
          first_dummy->si_key == dummy);
  if (first_dummy->si_key)
      Dee_DecrefNokill(first_dummy->si_key);
  /* Fill in the target slot. */
  first_dummy->si_key  = (DREF DeeObject *)new_item; /* Inherit reference. */
  first_dummy->si_hash = hash;
  ++me->s_used;
  ++me->s_size;
  /* Try to keep the set vector big at least twice as big as the element count. */
  if (me->s_size*2 > me->s_mask)
      set_rehash(me,1);
  DeeHashSet_LockEndWrite(me);
  return 1; /* New item. */
 }
 /* Rehash the set and try again. */
 if (set_rehash(me,1)) {
  DeeHashSet_LockDowngrade(me);
  goto again;
 }
 DeeHashSet_LockEndWrite(me);
 if (Dee_CollectMemory(1)) goto again_lock;
 Dee_Decref(new_item);
 return -1;
}

PUBLIC int DCALL
DeeHashSet_RemoveString(DeeObject *__restrict self,
                        char const *__restrict search_item,
                        size_t search_item_length) {
 Set *me = (Set *)self; DeeObject *old_item;
 size_t mask; struct hashset_item *vector; dhash_t i,perturb;
 dhash_t hash = Dee_HashPtr(search_item,search_item_length);
#ifndef CONFIG_NO_THREADS
again_lock:
#endif /* !CONFIG_NO_THREADS */
 DeeHashSet_LockRead(me);
 vector = me->s_elem;
 mask   = me->s_mask;
 perturb = i = hash & mask;
 for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
  struct hashset_item *item = &vector[i & mask];
  if ((old_item = item->si_key) == NULL) break; /* Not found */
  if (item->si_hash != hash) continue; /* Non-matching hash */
  if (!DeeString_Check(old_item)) continue; /* Not-a-string. */
  if (DeeString_SIZE(old_item) != search_item_length) continue; /* Differing lengths. */
  if (memcmp(DeeString_STR(old_item),search_item,
             search_item_length*sizeof(char)) != 0)
      continue; /* Different strings. */
  /* Found it! */
#ifndef CONFIG_NO_THREADS
  if (!DeeHashSet_LockUpgrade(me)) {
   DeeHashSet_LockEndWrite(me);
   SCHED_YIELD();
   goto again_lock;
  }
#endif
  item->si_key = dummy;
  Dee_Incref(dummy);
  ASSERT(me->s_used);
  if (--me->s_used < me->s_size/2)
      set_rehash(me,-1);
  DeeHashSet_LockEndRead(me);
  Dee_Decref(old_item);
  return 0;
 }
 DeeHashSet_LockEndRead(me);
 err_unknown_key_str((DeeObject *)me,search_item);
 return -1;
}





DFUNDEF int DCALL
DeeHashSet_Contains(DeeObject *__restrict self,
                    DeeObject *__restrict search_item) {
 Set *me = (Set *)self; size_t mask;
 struct hashset_item *vector;
 dhash_t i,perturb; int error;
 dhash_t hash = DeeObject_Hash(search_item);
 DeeHashSet_LockRead(me);
restart:
 vector = me->s_elem;
 mask   = me->s_mask;
 perturb = i = hash & mask;
 for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
  DREF DeeObject *item_key;
  struct hashset_item *item = &vector[i & mask];
  if (!item->si_key) break; /* Not found */
  if (item->si_hash != hash) continue; /* Non-matching hash */
  if (item->si_key == dummy) continue; /* Dummy key. */
  item_key   = item->si_key;
  Dee_Incref(item_key);
  DeeHashSet_LockEndRead(me);
  /* Invoke the compare operator outside of any lock. */
  error = DeeObject_CompareEq(search_item,item_key);
  Dee_Decref(item_key);
  if (error > 0) return 1; /* Found the item. */
  if unlikely(error < 0)
     return -1; /* Error in compare operator. */
  DeeHashSet_LockRead(me);
  /* Check if the set was modified. */
  if (me->s_elem != vector ||
      me->s_mask != mask ||
      item->si_key != item_key)
      goto restart;
 }
 DeeHashSet_LockEndRead(me);
 return 0;
}
DFUNDEF bool DCALL
DeeHashSet_ContainsString(DeeObject *__restrict self,
                          char const *__restrict search_item,
                          size_t search_item_length) {
 Set *me = (Set *)self; size_t mask;
 struct hashset_item *vector; dhash_t i,perturb;
 dhash_t hash = Dee_HashPtr(search_item,search_item_length);
 DeeHashSet_LockRead(me);
 vector = me->s_elem;
 mask   = me->s_mask;
 perturb = i = hash & mask;
 for (;; i = HASHSET_HASHNX(i,perturb),HASHSET_HASHPT(perturb)) {
  struct hashset_item *item = &vector[i & mask];
  if (!item->si_key) break; /* Not found */
  if (item->si_hash != hash) continue; /* Non-matching hash */
  if (!DeeString_Check(item->si_key)) continue; /* Not-a-string. */
  if (DeeString_SIZE(item->si_key) != search_item_length) continue; /* Not-a-string. */
  if (memcmp(DeeString_STR(item->si_key),search_item,
             search_item_length*sizeof(char)) != 0)
      continue; /* Different strings. */
  DeeHashSet_LockEndRead(me);
  return true;
 }
 DeeHashSet_LockEndRead(me);
 return false;
}




PRIVATE DREF DeeObject *DCALL
set_size(Set *__restrict self) {
#ifdef CONFIG_NO_THREADS
 return DeeInt_NewSize(self->s_size);
#else
 return DeeInt_NewSize(ATOMIC_READ(self->s_size));
#endif
}

PRIVATE DREF DeeObject *DCALL
set_contains(Set *__restrict self, DeeObject *__restrict search_item) {
 int result = DeeHashSet_Contains((DeeObject *)self,search_item);
 if unlikely(result < 0) return NULL;
 return_bool_(result);
}


typedef struct {
    OBJECT_HEAD
    DREF Set            *si_set;  /* [1..1][const] The set that is being iterated. */
    struct hashset_item *si_next; /* [?..1][MAYBE(in(si_set->s_elem))][atomic]
                                   *   The first candidate for the next item.
                                   *   NOTE: Before being dereferenced, this pointer is checked
                                   *         for being located inside the set's element vector.
                                   *         In the event that it is located at its end, `ITER_DONE'
                                   *         is returned, though in the event that it is located
                                   *         outside, an error is thrown (`err_changed_sequence()'). */
} SetIterator;

#ifdef CONFIG_NO_THREADS
#define READ_ITEM(x)            ((x)->si_next)
#else
#define READ_ITEM(x) ATOMIC_READ((x)->si_next)
#endif

PRIVATE DREF DeeObject *DCALL
setiterator_next(SetIterator *__restrict self) {
 DREF DeeObject *result;
 struct hashset_item *item,*end;
 Set *set = self->si_set;
 DeeHashSet_LockRead(set);
 end = set->s_elem+(set->s_mask+1);
#ifndef CONFIG_NO_THREADS
 for (;;)
#endif
 {
#ifdef CONFIG_NO_THREADS
  item = ATOMIC_READ(self->si_next);
#else
  struct hashset_item *old_item;
  old_item = item = ATOMIC_READ(self->si_next);
#endif
  /* Validate that the pointer is still located in-bounds. */
  if (item >= end) {
   if unlikely(item > end)
      goto set_has_changed;
   goto iter_exhausted;
  }
  if unlikely(item < set->s_elem)
     goto set_has_changed;
  /* Search for the next non-empty item. */
  while (item != end && (!item->si_key || item->si_key == dummy)) ++item;
  if (item == end) {
#ifdef CONFIG_NO_THREADS
   self->si_next = item;
#else
   if (!ATOMIC_CMPXCH(self->si_next,old_item,item))
        continue;
#endif
   goto iter_exhausted;
  }
#ifdef CONFIG_NO_THREADS
  self->si_next = item+1;
#else
  if (ATOMIC_CMPXCH(self->si_next,old_item,item+1))
      break;
#endif
 }
 result = item->si_key;
 Dee_Incref(result);
 DeeHashSet_LockEndRead(set);
 return result;
set_has_changed:
 DeeHashSet_LockEndRead(set);
 err_changed_sequence((DeeObject *)set);
 return NULL;
iter_exhausted:
 DeeHashSet_LockEndRead(set);
 return ITER_DONE;
}

INTERN int DCALL
setiterator_ctor(SetIterator *__restrict self) {
 self->si_set = (DeeHashSetObject *)DeeHashSet_New();
 if unlikely(!self->si_set) return -1;
 self->si_next = self->si_set->s_elem;
 return 0;
}

INTERN int DCALL
setiterator_copy(SetIterator *__restrict self,
                 SetIterator *__restrict other) {
 self->si_set = other->si_set;
 Dee_Incref(self->si_set);
 self->si_next = READ_ITEM(other);
 return 0;
}

PRIVATE void DCALL
setiterator_fini(SetIterator *__restrict self) {
 Dee_Decref(self->si_set);
}
PRIVATE void DCALL
setiterator_visit(SetIterator *__restrict self, dvisit_t proc, void *arg) {
 Dee_Visit(self->si_set);
}

INTDEF DeeTypeObject HashSetIterator_Type;

INTERN int DCALL
setiterator_init(SetIterator *__restrict self,
                 size_t argc, DeeObject **__restrict argv) {
 Set *set;
 if (DeeArg_Unpack(argc,argv,"o:_HashSetIterator",&set) ||
     DeeObject_AssertType((DeeObject *)set,&DeeHashSet_Type))
     return -1;
 self->si_set = set;
 Dee_Incref(set);
#ifdef CONFIG_NO_THREADS
 self->si_next = set->s_elem;
#else
 self->si_next = ATOMIC_READ(set->s_elem);
#endif
 return 0;
}

PRIVATE int DCALL
setiterator_bool(SetIterator *__restrict self) {
 struct hashset_item *item = READ_ITEM(self);
 Set *set = self->si_set;
 /* Check if the iterator is in-bounds.
  * NOTE: Since this is nothing but a shallow boolean check anyways, there
  *       is no need to lock the dict since we're not dereferencing anything. */
 return (item >= set->s_elem &&
         item <  set->s_elem+(set->s_mask+1));
}

#define DEFINE_ITERATOR_COMPARE(name,op) \
PRIVATE DREF DeeObject *DCALL \
name(SetIterator *__restrict self, \
     SetIterator *__restrict other) { \
 if (DeeObject_AssertType((DeeObject *)other,&HashSetIterator_Type)) \
     goto err; \
 return_bool(READ_ITEM(self) op READ_ITEM(other)); \
err: \
 return NULL; \
}
DEFINE_ITERATOR_COMPARE(setiterator_eq,==)
DEFINE_ITERATOR_COMPARE(setiterator_ne,!=)
DEFINE_ITERATOR_COMPARE(setiterator_lo,<)
DEFINE_ITERATOR_COMPARE(setiterator_le,<=)
DEFINE_ITERATOR_COMPARE(setiterator_gr,>)
DEFINE_ITERATOR_COMPARE(setiterator_ge,>=)
#undef DEFINE_ITERATOR_COMPARE

PRIVATE struct type_member setiterator_members[] = {
    TYPE_MEMBER_FIELD("seq",STRUCT_OBJECT,offsetof(SetIterator,si_set)),
    TYPE_MEMBER_END
};

PRIVATE struct type_cmp setiterator_cmp = {
    /* .tp_hash = */NULL,
    /* .tp_eq   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&setiterator_eq,
    /* .tp_ne   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&setiterator_ne,
    /* .tp_lo   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&setiterator_lo,
    /* .tp_le   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&setiterator_le,
    /* .tp_gr   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&setiterator_gr,
    /* .tp_ge   = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&setiterator_ge
};

INTERN DeeTypeObject HashSetIterator_Type = {
    OBJECT_HEAD_INIT(&DeeType_Type),
    /* .tp_name     = */"_HashSetIterator",
    /* .tp_doc      = */NULL,
    /* .tp_flags    = */TP_FNORMAL,
    /* .tp_weakrefs = */0,
    /* .tp_features = */TF_NONE,
    /* .tp_base     = */&DeeIterator_Type,
    /* .tp_init = */{
        {
            /* .tp_alloc = */{
                /* .tp_ctor      = */(void *)&setiterator_ctor,
                /* .tp_copy_ctor = */(void *)&setiterator_copy,
                /* .tp_deep_ctor = */(void *)NULL, /* TODO */
                /* .tp_any_ctor  = */(void *)&setiterator_init,
                TYPE_FIXED_ALLOCATOR(SetIterator)
            }
        },
        /* .tp_dtor        = */(void(DCALL *)(DeeObject *__restrict))&setiterator_fini,
        /* .tp_assign      = */NULL,
        /* .tp_move_assign = */NULL
    },
    /* .tp_cast = */{
        /* .tp_str  = */NULL,
        /* .tp_repr = */NULL,
        /* .tp_bool = */(int(DCALL *)(DeeObject *__restrict))&setiterator_bool
    },
    /* .tp_call          = */NULL,
    /* .tp_visit         = */(void(DCALL *)(DeeObject *__restrict,dvisit_t,void *))&setiterator_visit,
    /* .tp_gc            = */NULL,
    /* .tp_math          = */NULL,
    /* .tp_cmp           = */&setiterator_cmp,
    /* .tp_seq           = */NULL,
    /* .tp_iter_next     = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&setiterator_next,
    /* .tp_attr          = */NULL,
    /* .tp_with          = */NULL,
    /* .tp_buffer        = */NULL,
    /* .tp_methods       = */NULL,
    /* .tp_getsets       = */NULL,
    /* .tp_members       = */setiterator_members,
    /* .tp_class_methods = */NULL,
    /* .tp_class_getsets = */NULL,
    /* .tp_class_members = */NULL
};



PRIVATE DREF SetIterator *DCALL
set_iter(Set *__restrict self) {
 DREF SetIterator *result;
 result = DeeObject_MALLOC(SetIterator);
 if unlikely(!result) goto done;
 DeeObject_Init(result,&HashSetIterator_Type);
 result->si_set = self;
 Dee_Incref(self);
#ifdef CONFIG_NO_THREADS
 result->si_next = self->s_elem;
#else
 result->si_next = ATOMIC_READ(self->s_elem);
#endif
done:
 return result;
}


PRIVATE struct type_seq set_seq = {
    /* .tp_iter_self = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&set_iter,
    /* .tp_size      = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&set_size,
    /* .tp_contains  = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict,DeeObject *__restrict))&set_contains,
    /* .tp_get       = */NULL,
    /* .tp_del       = */NULL,
    /* .tp_set       = */NULL,
    /* .tp_range_get = */NULL,
    /* .tp_range_del = */NULL,
    /* .tp_range_set = */NULL
};

PRIVATE DREF DeeObject *DCALL
set_repr(Set *__restrict self) {
 struct unicode_printer p; dssize_t error;
 struct hashset_item *iter,*end; bool is_first;
 struct hashset_item *vector; size_t mask;
again:
 unicode_printer_init(&p);
 if (UNICODE_PRINTER_PRINT(&p,"{ ") < 0) goto err;
 DeeHashSet_LockRead(self);
 is_first = true;
 vector = self->s_elem;
 mask   = self->s_mask;
 end = (iter = vector)+(mask+1);
 for (; iter != end; ++iter) {
  DREF DeeObject *key;
  if (iter->si_key == NULL ||
      iter->si_key == dummy)
      continue;
  key = iter->si_key;
  Dee_Incref(key);
  DeeHashSet_LockEndRead(self);
  /* Print this item. */
  error = unicode_printer_printf(&p,"%s%r",is_first ? "" : ", ",key);
  Dee_Decref(key);
  if unlikely(error < 0) goto err;
  is_first = false;
  DeeHashSet_LockRead(self);
  if (self->s_elem != vector ||
      self->s_mask != mask)
      goto restart;
 }
 DeeHashSet_LockEndRead(self);
 if ((is_first ? unicode_printer_putascii(&p,'}')
               : UNICODE_PRINTER_PRINT(&p," }")) < 0)
     goto err;
 return unicode_printer_pack(&p);
restart:
 DeeHashSet_LockEndRead(self);
 unicode_printer_fini(&p);
 goto again;
err:
 unicode_printer_fini(&p);
 return NULL;
}


PRIVATE int DCALL
set_bool(Set *__restrict self) {
#ifdef CONFIG_NO_THREADS
 return self->s_used != 0;
#else
 return ATOMIC_READ(self->s_used) != 0;
#endif
}

PRIVATE int DCALL
set_init(Set *__restrict self,
         size_t argc, DeeObject **__restrict argv) {
 DeeObject *seq; int error;
 if unlikely(DeeArg_Unpack(argc,argv,"o:hashset",&seq))
    goto err;
 /* TODO: Support for initialization from `_roset' */
 /* TODO: Optimization for fast-sequence types. */
 if unlikely((seq = DeeObject_IterSelf(seq)) == NULL)
    goto err;
 error = set_init_iterator(self,seq);
 Dee_Decref(seq);
 return error;
err:
 return -1;
}


PRIVATE DREF DeeObject *DCALL
set_pop(Set *__restrict self, size_t argc, DeeObject **__restrict argv) {
 size_t i; DREF DeeObject *result;
 if (DeeArg_Unpack(argc,argv,":pop"))
     goto err;
 DeeHashSet_LockWrite(self);
 for (i = 0; i <= self->s_mask; ++i) {
  struct hashset_item *item = &self->s_elem[i];
  if ((result = item->si_key) == NULL) continue; /* Unused slot. */
  if (result == dummy) continue; /* Deleted slot. */
  item->si_key = dummy;
  Dee_Incref(dummy);
  ASSERT(self->s_used);
  if (--self->s_used < self->s_size/2)
      set_rehash(self,-1);
  DeeHashSet_LockEndWrite(self);
  return result;
 }
 DeeHashSet_LockEndWrite(self);
 /* Set is already empty. */
 err_empty_sequence((DeeObject *)self);
err:
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
set_doclear(Set *__restrict self, size_t argc, DeeObject **__restrict argv) {
 if (DeeArg_Unpack(argc,argv,":clear"))
     goto err;
 set_clear(self);
 return_none;
err:
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
set_insert(Set *__restrict self, size_t argc, DeeObject **__restrict argv) {
 DeeObject *item; int result;
 if (DeeArg_Unpack(argc,argv,"o:insert",&item))
     goto err;
 result = DeeHashSet_Insert((DeeObject *)self,item);
 if unlikely(result < 0)
    goto err;
 return_bool_(result);
err:
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
set_unify(Set *__restrict self, size_t argc, DeeObject **__restrict argv) {
 DeeObject *item;
 if (DeeArg_Unpack(argc,argv,"o:unify",&item))
     goto err;
 return DeeHashSet_Unify((DeeObject *)self,item);
err:
 return NULL;
}

#if __SIZEOF_INT__ >= __SIZEOF_POINTER__ || \
   (defined(__i386__) || defined(__x86_64__))
#define insert_callback   DeeHashSet_Insert
#else
PRIVATE dssize_t DCALL
insert_callback(Set *__restrict self, DeeObject *item) {
 return DeeHashSet_Insert((DeeObject *)self,item);
}
#endif

PRIVATE DREF DeeObject *DCALL
set_update(Set *__restrict self, size_t argc, DeeObject **__restrict argv) {
 DeeObject *items; dssize_t result;
 if (DeeArg_Unpack(argc,argv,"o:update",&items))
     goto err;
 result = DeeObject_Foreach(items,(dforeach_t)&insert_callback,self);
 if unlikely(result < 0)
    goto err;
 return DeeInt_NewSize((size_t)result);
err:
 return NULL;
}

PRIVATE DREF DeeObject *DCALL
set_remove(Set *__restrict self, size_t argc, DeeObject **__restrict argv) {
 DeeObject *item; int result;
 if (DeeArg_Unpack(argc,argv,"o:remove",&item))
     goto err;
 result = DeeHashSet_Remove((DeeObject *)self,item);
 if unlikely(result < 0)
    goto err;
 return_bool_(result);
err:
 return NULL;
}


PRIVATE DREF DeeObject *DCALL
hashset_sizeof(Set *__restrict self,
               size_t argc, DeeObject **__restrict argv) {
 if (DeeArg_Unpack(argc,argv,":__sizeof__"))
     goto err;
 return DeeInt_NewSize(sizeof(Set) +
                     ((self->s_mask + 1) *
                       sizeof(struct hashset_item)));
err:
 return NULL;
}



PRIVATE struct type_method set_methods[] = {
    { DeeString_STR(&str_pop),
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_pop,
      DOC("->\n"
          "@throw ValueError The set is empty\n"
          "Pop a random item from the set and return it") },
    { DeeString_STR(&str_clear),
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_doclear,
      DOC("()\n"
          "Clear all items from the set") },
    { "popitem",
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_pop,
      DOC("->\n"
          "@throw ValueError The set is empty\n"
          "Pop a random item from the set and return it (alias for #pop)") },
    { "unify",
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_unify,
      DOC("(ob)->\n"
          "Insert @ob into the set if it wasn't inserted before, "
          "and re-return it, or the pre-existing instance") },
    { DeeString_STR(&str_insert),
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_insert,
      DOC("(ob)->?Dbool\n"
          "Returns :true if the object wasn't apart of the set before") },
    { "update",
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_update,
      DOC("(items:?S?O)->?Dint\n"
          "Insert all items from @items into @this set, and return the number of inserted items") },
    { DeeString_STR(&str_remove),
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_remove,
      DOC("(ob)->?Dbool\n"
          "Returns :true if the object was removed from the set") },
    /* Alternative function names. */
    { "add",
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_insert,
      DOC("(ob)->?Dbool\n"
          "Deprecated alias for #insert") },
    { "discard",
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_remove,
      DOC("(ob)->?Dbool\n"
          "Deprecated alias for #remove") },
    { "__sizeof__",
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&hashset_sizeof,
      DOC("->?Dint") },
#ifndef CONFIG_NO_DEEMON_100_COMPAT
    /* Old function names. */
    { "insert_all",
     (DREF DeeObject *(DCALL *)(DeeObject *__restrict,size_t,DeeObject **__restrict))&set_update,
      DOC("(ob)->?Dbool\n"
          "Deprecated alias for #update") },
#endif /* !CONFIG_NO_DEEMON_100_COMPAT */
    { NULL }
};


#ifndef CONFIG_NO_DEEMON_100_COMPAT
INTERN DREF DeeObject *DCALL
set_get_maxloadfactor(DeeObject *__restrict UNUSED(self)) {
 return DeeFloat_New(1.0);
}
INTERN int DCALL
set_del_maxloadfactor(DeeObject *__restrict UNUSED(self)) {
 return 0;
}
INTERN int DCALL
set_set_maxloadfactor(DeeObject *__restrict UNUSED(self),
                      DeeObject *__restrict UNUSED(value)) {
 return 0;
}
#endif /* !CONFIG_NO_DEEMON_100_COMPAT */

INTERN struct type_getset hashset_getsets[] = {
    { "frozen",
     &DeeRoSet_FromSequence,
      NULL,
      NULL,
      DOC("->?Ert:RoSet\n"
          "Returns a read-only (frozen) copy of @this hashset") },
#ifndef CONFIG_NO_DEEMON_100_COMPAT
    { "max_load_factor",
     &set_get_maxloadfactor,
     &set_del_maxloadfactor,
     &set_set_maxloadfactor,
      DOC("->?Dfloat\n"
          "Deprecated. Always returns ${1.0}, with del/set being ignored") },
#endif /* !CONFIG_NO_DEEMON_100_COMPAT */
    { NULL }
};

PRIVATE struct type_member set_class_members[] = {
    TYPE_MEMBER_CONST("iterator",&HashSetIterator_Type),
    TYPE_MEMBER_CONST("frozen",&DeeRoSet_Type),
    TYPE_MEMBER_END
};

PRIVATE struct type_gc set_gc = {
    /* .tp_clear = */(void(DCALL *)(DeeObject *__restrict))&set_clear
};

PUBLIC DeeTypeObject DeeHashSet_Type = {
    OBJECT_HEAD_INIT(&DeeType_Type),
    /* .tp_name     = */DeeString_STR(&str_hashset),
    /* .tp_doc      = */DOC("A mutable set-like container that using hashing to detect duplicates\n"
                            "\n"
                            "()\n"
                            "Create an empty hashset\n"
                            "\n"
                            "(items:?S?O)\n"
                            "Create a new hashset populated with elements from @items\n"
                            "\n"
                            "copy->\n"
                            "Returns a shallow copy of @this hashset\n"
                            "\n"
                            "deepcopy->\n"
                            "Returns a deep copy of @this hashset\n"
                            "\n"
                            "bool->\n"
                            "Returns :true if @this hashset is non-empty\n"
                            "\n"
                            "contains->\n"
                            "Returns :true if @item is apart of @this hashset\n"
                            "\n"
                            "#->\n"
                            "Returns the number of items apart of @this hashset\n"
                            "\n"
                            "iter->\n"
                            "Returns an iterator for enumerating all items "
                            "in @this hashset, following a random order\n"
                            ),
    /* .tp_flags    = */TP_FNORMAL|TP_FGC|TP_FNAMEOBJECT,
    /* .tp_weakrefs = */WEAKREF_SUPPORT_ADDR(Set),
    /* .tp_features = */TF_NONE,
    /* .tp_base     = */&DeeSet_Type,
    /* .tp_init = */{
        {
            /* .tp_alloc = */{
                /* .tp_ctor      = */&set_ctor,
                /* .tp_copy_ctor = */&set_copy,
                /* .tp_deep_ctor = */&set_copy,
                /* .tp_any_ctor  = */&set_init,
                TYPE_FIXED_ALLOCATOR_GC(Set)
            }
        },
        /* .tp_dtor        = */(void(DCALL *)(DeeObject *__restrict))&set_fini,
        /* .tp_assign      = */NULL,
        /* .tp_move_assign = */NULL,
        /* .tp_deepload    = */(int(DCALL *)(DeeObject *__restrict))&set_deepload
    },
    /* .tp_cast = */{
        /* .tp_str  = */NULL,
        /* .tp_repr = */(DREF DeeObject *(DCALL *)(DeeObject *__restrict))&set_repr,
        /* .tp_bool = */(int(DCALL *)(DeeObject *__restrict))&set_bool
    },
    /* .tp_call          = */NULL,
    /* .tp_visit         = */(void(DCALL *)(DeeObject *__restrict,dvisit_t,void *))&set_visit,
    /* .tp_gc            = */&set_gc,
    /* .tp_math          = */NULL,
    /* .tp_cmp           = */NULL,
    /* .tp_seq           = */&set_seq,
    /* .tp_iter_next     = */NULL,
    /* .tp_attr          = */NULL,
    /* .tp_with          = */NULL,
    /* .tp_buffer        = */NULL,
    /* .tp_methods       = */set_methods,
    /* .tp_getsets       = */hashset_getsets,
    /* .tp_members       = */NULL,
    /* .tp_class_methods = */NULL,
    /* .tp_class_getsets = */NULL,
    /* .tp_class_members = */set_class_members
};

DECL_END

#endif /* !GUARD_DEEMON_OBJECTS_HASHSET_C */
