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
#ifndef GUARD_DEEMON_RUNTIME_MISC_C
#define GUARD_DEEMON_RUNTIME_MISC_C 1

#include <deemon/api.h>
#include <deemon/alloc.h>
#include <deemon/object.h>
#include <deemon/gc.h>
#include <deemon/error.h>
#include <deemon/format.h>
#include <deemon/string.h>
#include <deemon/stringutils.h>
#include <deemon/file.h>
#include <deemon/compiler/ast.h>

#ifndef CONFIG_NO_THREADS
#include <deemon/util/rwlock.h>
#endif
#ifndef CONFIG_NO_DEC
#include <deemon/dec.h>
#endif

#ifndef NDEBUG
#ifndef CONFIG_HOST_WINDOWS
#include <deemon/file.h>
#else
#include <hybrid/minmax.h>
#include <hybrid/limits.h>
#include <Windows.h>
#endif
#endif

#include <hybrid/byteswap.h>
#include <hybrid/byteorder.h>
#include <hybrid/unaligned.h>

#include <string.h>
#include <stdlib.h>

#include "runtime_error.h"

#ifndef NDEBUG
#ifdef __KOS_SYSTEM_HEADERS__
#   include <malloc.h>
#elif defined(_MSC_VER)
#   include <crtdbg.h>
#endif
#endif /* !NDEBUG */

DECL_BEGIN

#define packw(x) (((x)&0xff) ^ (((x)&0xff00) >> 8))
#define packl(x) \
  (((x)&0xff) ^ (((x)&0xff00) >> 8) ^ \
   (((x)&0xff0000) >> 16) ^ (((x)&0xff000000) >> 24))


#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ESEL(l,b) l
#else
#define ESEL(l,b) b
#endif



#if __SIZEOF_POINTER__ == 4
// This Hash function is based on code from here:
// https://en.wikipedia.org/wiki/MurmurHash
// It was referenced as pretty good here:
// http://programmers.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed

#define ROT32(x, y) ((x << y) | (x >> (32 - y))) // avoid effort
#define c1 0xcc9e2d51
#define c2 0x1b873593
#define r1 15
#define r2 13
#define m  5
#define n  0xe6546b64
PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashPtr(void const *__restrict ptr, size_t n_bytes) {
 uint8_t const *tail;
 uint32_t k1; size_t i;
 size_t const nblocks = n_bytes / 4;
 uint32_t const *blocks = (uint32_t const *)ptr;
 uint32_t hash = 0;
 uint32_t k;
 for (i = 0; i < nblocks; ++i) {
  k  = LESWAP32(blocks[i]);
  k *= c1;
  k  = ROT32(k,r1);
  k *= c2;
  hash ^= k;
  hash = ROT32(hash,r2)*m+n;
 }
 tail = ((uint8_t const *)ptr)+(nblocks*4);
 k1 = 0;
 switch (n_bytes & 3) {
 case 3:  k1 ^= (uint32_t)tail[2] << 16; ATTR_FALLTHROUGH
 case 2:  k1 ^= (uint32_t)tail[1] << 8; ATTR_FALLTHROUGH
 case 1:  k1 ^= (uint32_t)tail[0];
          k1 *= c1;
          k1 = ROT32(k1, r1);
          k1 *= c2;
          hash ^= k1;
          break;
 default: break;
 }
 hash ^= n_bytes;
 hash ^= (hash >> 16);
 hash *= 0x85ebca6b;
 hash ^= (hash >> 13);
 hash *= 0xc2b2ae35;
 hash ^= (hash >> 16);
 return hash;
}

PUBLIC ATTR_PURE dhash_t DCALL
Dee_Hash2Byte(uint16_t const *__restrict ptr, size_t n_words) {
 uint16_t const *tail;
 uint32_t k1; size_t i;
 size_t const nblocks = n_words / 4;
 uint32_t hash = 0;
 uint32_t k;
 uint16_t ch;
 for (i = 0; i < nblocks; ++i) {
  ch = ptr[i*2+0];
  k  = packw(ch) << ESEL(0,24);
  ch = ptr[i*2+1];
  k ^= packw(ch) << ESEL(8,16);
  ch = ptr[i*2+2];
  k ^= packw(ch) << ESEL(16,8);
  ch = ptr[i*2+3];
  k ^= packw(ch) << ESEL(24,0);
  k *= c1;
  k = ROT32(k,r1);
  k *= c2;
  hash ^= k;
  hash = ROT32(hash,r2)*m+n;
 }
 tail = ((uint16_t const *)ptr)+(nblocks*4);
 k1 = 0;
 switch (n_words & 3) {
 case 3:  k1 ^= packw(tail[2]) << 16; ATTR_FALLTHROUGH
 case 2:  k1 ^= packw(tail[1]) << 8; ATTR_FALLTHROUGH
 case 1:  k1 ^= packw(tail[0]);
          k1 *= c1;
          k1 = ROT32(k1, r1);
          k1 *= c2;
          hash ^= k1;
          break;
 default: break;
 }
 hash ^= n_words;
 hash ^= (hash >> 16);
 hash *= 0x85ebca6b;
 hash ^= (hash >> 13);
 hash *= 0xc2b2ae35;
 hash ^= (hash >> 16);
 return hash;
}
PUBLIC ATTR_PURE dhash_t DCALL
Dee_Hash4Byte(uint32_t const *__restrict ptr, size_t n_dwords) {
 uint32_t const *tail;
 uint32_t k1; size_t i;
 size_t const nblocks = n_dwords / 4;
 uint32_t hash = 0;
 uint32_t k;
 uint32_t ch;
 for (i = 0; i < nblocks; ++i) {
  ch = ptr[i*2+0];
  k  = packl(ch) << ESEL(0,24);
  ch = ptr[i*2+1];
  k ^= packl(ch) << ESEL(8,16);
  ch = ptr[i*2+2];
  k ^= packl(ch) << ESEL(16,8);
  ch = ptr[i*2+3];
  k ^= packl(ch) << ESEL(24,0);
  k *= c1;
  k = ROT32(k,r1);
  k *= c2;
  hash ^= k;
  hash = ROT32(hash,r2)*m+n;
 }
 tail = ((uint32_t const *)ptr)+(nblocks*4);
 k1 = 0;
 switch (n_dwords & 3) {
 case 3:  ch = tail[2];
          k1 ^= packl(ch) << 16;
          ATTR_FALLTHROUGH
 case 2:  ch = tail[1];
          k1 ^= packl(ch) << 8;
          ATTR_FALLTHROUGH
 case 1:  ch = tail[0];
          k1 ^= packl(ch);
          k1 *= c1;
          k1 = ROT32(k1, r1);
          k1 *= c2;
          hash ^= k1;
          ATTR_FALLTHROUGH
 default: break;
 }
 hash ^= n_dwords;
 hash ^= (hash >> 16);
 hash *= 0x85ebca6b;
 hash ^= (hash >> 13);
 hash *= 0xc2b2ae35;
 hash ^= (hash >> 16);
 return hash;
}
PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashCasePtr(void const *__restrict ptr, size_t n_bytes) {
 uint8_t const *tail;
 uint32_t k1; size_t i;
 size_t const nblocks = n_bytes / 4;
 uint32_t const *blocks = (uint32_t const *)ptr;
 uint32_t hash = 0;
 uint32_t k;
 for (i = 0; i < nblocks; ++i) {
  union {
   uint32_t block;
   char     part[4];
  } b;
  b.block   = LESWAP32(blocks[i]);
  b.part[0] = (char)DeeUni_ToLower(b.part[0]);
  b.part[1] = (char)DeeUni_ToLower(b.part[1]);
  b.part[2] = (char)DeeUni_ToLower(b.part[2]);
  b.part[3] = (char)DeeUni_ToLower(b.part[3]);
  k = b.block;
  k *= c1;
  k = ROT32(k,r1);
  k *= c2;
  hash ^= k;
  hash = ROT32(hash,r2)*m+n;
 }
 tail = ((uint8_t const *)ptr)+(nblocks*4);
 k1 = 0;
 switch (n_bytes & 3) {
 case 3:  k1 ^= (uint8_t)DeeUni_ToLower(tail[2]) << 16; ATTR_FALLTHROUGH
 case 2:  k1 ^= (uint8_t)DeeUni_ToLower(tail[1]) << 8; ATTR_FALLTHROUGH
 case 1:  k1 ^= (uint8_t)DeeUni_ToLower(tail[0]);
          k1 *= c1;
          k1 = ROT32(k1, r1);
          k1 *= c2;
          hash ^= k1;
          ATTR_FALLTHROUGH
 default: break;
 }
 hash ^= n_bytes;
 hash ^= (hash >> 16);
 hash *= 0x85ebca6b;
 hash ^= (hash >> 13);
 hash *= 0xc2b2ae35;
 hash ^= (hash >> 16);
 return hash;
}

PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashCase2Byte(uint16_t const *__restrict ptr, size_t n_words) {
 uint16_t const *tail;
 uint32_t k1; size_t i;
 size_t const nblocks = n_words / 4;
 uint32_t hash = 0;
 uint32_t k;
 uint32_t ch;
 for (i = 0; i < nblocks; ++i) {
  ch = DeeUni_ToLower(ptr[i*2+0]);
  k  = packl(ch) << ESEL(0,24);
  ch = DeeUni_ToLower(ptr[i*2+1]);
  k ^= packl(ch) << ESEL(8,16);
  ch = DeeUni_ToLower(ptr[i*2+2]);
  k ^= packl(ch) << ESEL(16,8);
  ch = DeeUni_ToLower(ptr[i*2+3]);
  k ^= packl(ch) << ESEL(24,0);
  k *= c1;
  k = ROT32(k,r1);
  k *= c2;
  hash ^= k;
  hash = ROT32(hash,r2)*m+n;
 }
 tail = ((uint16_t const *)ptr)+(nblocks*4);
 k1 = 0;
 switch (n_words & 3) {
 case 3:  k1 ^= packw(DeeUni_ToLower(tail[2])) << 16; ATTR_FALLTHROUGH
 case 2:  k1 ^= packw(DeeUni_ToLower(tail[1])) << 8; ATTR_FALLTHROUGH
 case 1:  k1 ^= packw(DeeUni_ToLower(tail[0]));
          k1 *= c1;
          k1 = ROT32(k1, r1);
          k1 *= c2;
          hash ^= k1;
          ATTR_FALLTHROUGH
 default: break;
 }
 hash ^= n_words;
 hash ^= (hash >> 16);
 hash *= 0x85ebca6b;
 hash ^= (hash >> 13);
 hash *= 0xc2b2ae35;
 hash ^= (hash >> 16);
 return hash;
}
PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashCase4Byte(uint32_t const *__restrict ptr, size_t n_dwords) {
 uint32_t const *tail;
 uint32_t k1; size_t i;
 size_t const nblocks = n_dwords / 4;
 uint32_t hash = 0;
 uint32_t k;
 uint32_t ch;
 for (i = 0; i < nblocks; ++i) {
  ch = DeeUni_ToLower(ptr[i*2+0]);
  k  = packl(ch) << ESEL(0,24);
  ch = DeeUni_ToLower(ptr[i*2+1]);
  k ^= packl(ch) << ESEL(8,16);
  ch = DeeUni_ToLower(ptr[i*2+2]);
  k ^= packl(ch) << ESEL(16,8);
  ch = DeeUni_ToLower(ptr[i*2+3]);
  k ^= packl(ch) << ESEL(24,0);
  k *= c1;
  k = ROT32(k,r1);
  k *= c2;
  hash ^= k;
  hash = ROT32(hash,r2)*m+n;
 }
 tail = ((uint32_t const *)ptr)+(nblocks*4);
 k1 = 0;
 switch (n_dwords & 3) {
 case 3:  ch = DeeUni_ToLower(tail[2]);
          k1 ^= packl(ch) << 16;
          ATTR_FALLTHROUGH
 case 2:  ch = DeeUni_ToLower(tail[1]);
          k1 ^= packl(ch) << 8;
          ATTR_FALLTHROUGH
 case 1:  ch = DeeUni_ToLower(tail[0]);
          k1 ^= packl(ch);
          k1 *= c1;
          k1 = ROT32(k1, r1);
          k1 *= c2;
          hash ^= k1;
          ATTR_FALLTHROUGH
 default: break;
 }
 hash ^= n_dwords;
 hash ^= (hash >> 16);
 hash *= 0x85ebca6b;
 hash ^= (hash >> 13);
 hash *= 0xc2b2ae35;
 hash ^= (hash >> 16);
 return hash;
}

PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashUtf8(char const *__restrict ptr, size_t n_bytes) {
 char const *end = ptr + n_bytes;
 uint32_t k1 = 0;
 uint32_t block[4];
 uint32_t hash = 0;
 uint32_t k;
 size_t n_chars = 0;
 for (;;) {
  if unlikely(ptr >= end) goto done;
  block[0] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_1;
  block[1] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_2;
  block[2] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_3;
  block[3] = utf8_readchar((char const **)&ptr,end);
  n_chars += 4;
  k  = packl(block[0]) << ESEL(0,24);
  k ^= packl(block[1]) << ESEL(8,16);
  k ^= packl(block[2]) << ESEL(16,8);
  k ^= packl(block[3]) << ESEL(24,0);
  k *= c1;
  k = ROT32(k,r1);
  k *= c2;
  hash ^= k;
  hash = ROT32(hash,r2)*m+n;
 }
 goto done;
do_tail_3:
 ++n_chars;
 k1 ^= packl(block[2]) << 16;
do_tail_2:
 ++n_chars;
 k1 ^= packl(block[1]) << 8;
do_tail_1:
 ++n_chars;
 k1 ^= packl(block[0]);
 k1 *= c1;
 k1  = ROT32(k1, r1);
 k1 *= c2;
 hash ^= k1;
done:
 hash ^= n_chars;
 hash ^= (hash >> 16);
 hash *= 0x85ebca6b;
 hash ^= (hash >> 13);
 hash *= 0xc2b2ae35;
 hash ^= (hash >> 16);
 return hash;
}

PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashCaseUtf8(char const *__restrict ptr, size_t n_bytes) {
 char const *end = ptr + n_bytes;
 uint32_t k1 = 0;
 uint32_t block[4];
 uint32_t hash = 0;
 uint32_t k;
 size_t n_chars = 0;
 for (;;) {
  if unlikely(ptr >= end) goto done;
  block[0] = utf8_readchar((char const **)&ptr,end);
  block[0] = DeeUni_ToLower(block[0]);
  if unlikely(ptr >= end) goto do_tail_1;
  block[1] = utf8_readchar((char const **)&ptr,end);
  block[1] = DeeUni_ToLower(block[1]);
  if unlikely(ptr >= end) goto do_tail_2;
  block[2] = utf8_readchar((char const **)&ptr,end);
  block[2] = DeeUni_ToLower(block[2]);
  if unlikely(ptr >= end) goto do_tail_3;
  block[3] = utf8_readchar((char const **)&ptr,end);
  block[3] = DeeUni_ToLower(block[3]);
  n_chars += 4;
  k  = packl(block[0]) << ESEL(0,24);
  k ^= packl(block[1]) << ESEL(8,16);
  k ^= packl(block[2]) << ESEL(16,8);
  k ^= packl(block[3]) << ESEL(24,0);
  k *= c1;
  k = ROT32(k,r1);
  k *= c2;
  hash ^= k;
  hash = ROT32(hash,r2)*m+n;
 }
 goto done;
do_tail_3:
 ++n_chars;
 k1 ^= packl(block[2]) << 16;
do_tail_2:
 ++n_chars;
 k1 ^= packl(block[1]) << 8;
do_tail_1:
 ++n_chars;
 k1 ^= packl(block[0]);
 k1 *= c1;
 k1  = ROT32(k1, r1);
 k1 *= c2;
 hash ^= k1;
done:
 hash ^= n_chars;
 hash ^= (hash >> 16);
 hash *= 0x85ebca6b;
 hash ^= (hash >> 13);
 hash *= 0xc2b2ae35;
 hash ^= (hash >> 16);
 return hash;
}


#undef c1
#undef c2
#undef r1
#undef r2
#undef m
#undef n

#else

#define m    0xc6a4a7935bd1e995ull
#define r    47
//#define seed 0xe17a1465
PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashPtr(void const *__restrict ptr, size_t n_bytes) {
#ifdef seed
 dhash_t h = seed ^ (n_bytes * m);
#else
 dhash_t h = 0;
#endif
 size_t len8 = n_bytes / 8;
 while (len8--) {
  dhash_t k;
  k = UNALIGNED_GETLE64((*(dhash_t **)&ptr)++);
  k *= m;
  k ^= k >> r;
  k *= m;
  h ^= k;
  h *= m;
 }
 switch (n_bytes%8) {
 case 7:  h ^= (dhash_t)((uint8_t *)ptr)[6] << 48; ATTR_FALLTHROUGH
 case 6:  h ^= (dhash_t)((uint8_t *)ptr)[5] << 40; ATTR_FALLTHROUGH
 case 5:  h ^= (dhash_t)((uint8_t *)ptr)[4] << 32; ATTR_FALLTHROUGH
 case 4:  h ^= (dhash_t)((uint8_t *)ptr)[3] << 24; ATTR_FALLTHROUGH
 case 3:  h ^= (dhash_t)((uint8_t *)ptr)[2] << 16; ATTR_FALLTHROUGH
 case 2:  h ^= (dhash_t)((uint8_t *)ptr)[1] << 8; ATTR_FALLTHROUGH
 case 1:  h ^= (dhash_t)((uint8_t *)ptr)[0];
          h *= m;
          break;
 default: break;
 };
 h ^= h >> r;
 h *= m;
 h ^= h >> r;
 return h;
}
PUBLIC ATTR_PURE dhash_t DCALL
Dee_Hash2Byte(uint16_t const *__restrict ptr, size_t n_words) {
#ifdef seed
 dhash_t h = seed ^ (n_words * m);
#else
 dhash_t h = 0;
#endif
 size_t len8 = n_words / 8;
 while (len8--) {
  dhash_t k;
  k  = (dhash_t)packw(ptr[0]) << ESEL(0,56);
  k |= (dhash_t)packw(ptr[1]) << ESEL(8,48);
  k |= (dhash_t)packw(ptr[2]) << ESEL(16,40);
  k |= (dhash_t)packw(ptr[3]) << ESEL(24,32);
  k |= (dhash_t)packw(ptr[4]) << ESEL(32,24);
  k |= (dhash_t)packw(ptr[5]) << ESEL(40,16);
  k |= (dhash_t)packw(ptr[6]) << ESEL(48,8);
  k |= (dhash_t)packw(ptr[7]) << ESEL(56,0);
  ptr += 8;
  k *= m;
  k ^= k >> r;
  k *= m;
  h ^= k;
  h *= m;
 }
 switch (n_words%8) {
 case 7:  h ^= (dhash_t)packw(ptr[6]) << 48; ATTR_FALLTHROUGH
 case 6:  h ^= (dhash_t)packw(ptr[5]) << 40; ATTR_FALLTHROUGH
 case 5:  h ^= (dhash_t)packw(ptr[4]) << 32; ATTR_FALLTHROUGH
 case 4:  h ^= (dhash_t)packw(ptr[3]) << 24; ATTR_FALLTHROUGH
 case 3:  h ^= (dhash_t)packw(ptr[2]) << 16; ATTR_FALLTHROUGH
 case 2:  h ^= (dhash_t)packw(ptr[1]) << 8; ATTR_FALLTHROUGH
 case 1:  h ^= (dhash_t)packw(ptr[0]);
          h *= m;
          break;
 default: break;
 };
 h ^= h >> r;
 h *= m;
 h ^= h >> r;
 return h;
}
PUBLIC ATTR_PURE dhash_t DCALL
Dee_Hash4Byte(uint32_t const *__restrict ptr, size_t n_dwords) {
#ifdef seed
 dhash_t h = seed ^ (n_dwords * m);
#else
 dhash_t h = 0;
#endif
 size_t len8 = n_dwords / 8;
 while (len8--) {
  dhash_t k;
  k  = (dhash_t)packl(ptr[0]) << ESEL(0,56);
  k |= (dhash_t)packl(ptr[1]) << ESEL(8,48);
  k |= (dhash_t)packl(ptr[2]) << ESEL(16,40);
  k |= (dhash_t)packl(ptr[3]) << ESEL(24,32);
  k |= (dhash_t)packl(ptr[4]) << ESEL(32,24);
  k |= (dhash_t)packl(ptr[5]) << ESEL(40,16);
  k |= (dhash_t)packl(ptr[6]) << ESEL(48,8);
  k |= (dhash_t)packl(ptr[7]) << ESEL(56,0);
  ptr += 8;
  k *= m;
  k ^= k >> r;
  k *= m;
  h ^= k;
  h *= m;
 }
 switch (n_dwords%8) {
 case 7:  h ^= (dhash_t)packl(ptr[6]) << 48; ATTR_FALLTHROUGH
 case 6:  h ^= (dhash_t)packl(ptr[5]) << 40; ATTR_FALLTHROUGH
 case 5:  h ^= (dhash_t)packl(ptr[4]) << 32; ATTR_FALLTHROUGH
 case 4:  h ^= (dhash_t)packl(ptr[3]) << 24; ATTR_FALLTHROUGH
 case 3:  h ^= (dhash_t)packl(ptr[2]) << 16; ATTR_FALLTHROUGH
 case 2:  h ^= (dhash_t)packl(ptr[1]) << 8; ATTR_FALLTHROUGH
 case 1:  h ^= (dhash_t)packl(ptr[0]);
          h *= m;
          break;
 default: break;
 };
 h ^= h >> r;
 h *= m;
 h ^= h >> r;
 return h;
}
PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashCasePtr(void const *__restrict ptr, size_t n_bytes) {
#ifdef seed
 dhash_t h = seed ^ (n_bytes * m);
#else
 dhash_t h = 0;
#endif
 size_t len8 = n_bytes / 8;
 while (len8--) {
  union {
      char    ch[8];
      dhash_t x;
  } k;
  k.x = UNALIGNED_GETLE64((*(dhash_t **)&ptr)++);
  k.ch[0] = (char)DeeUni_ToLower(k.ch[0]);
  k.ch[1] = (char)DeeUni_ToLower(k.ch[1]);
  k.ch[2] = (char)DeeUni_ToLower(k.ch[2]);
  k.ch[3] = (char)DeeUni_ToLower(k.ch[3]);
  k.ch[4] = (char)DeeUni_ToLower(k.ch[4]);
  k.ch[5] = (char)DeeUni_ToLower(k.ch[5]);
  k.ch[6] = (char)DeeUni_ToLower(k.ch[6]);
  k.ch[7] = (char)DeeUni_ToLower(k.ch[7]);
  k.x *= m;
  k.x ^= k.x >> r;
  k.x *= m;
  h ^= k.x;
  h *= m;
 }
 switch (n_bytes%8) {
 case 7:  h ^= (dhash_t)DeeUni_ToLower(((uint8_t *)ptr)[6]) << 48; ATTR_FALLTHROUGH
 case 6:  h ^= (dhash_t)DeeUni_ToLower(((uint8_t *)ptr)[5]) << 40; ATTR_FALLTHROUGH
 case 5:  h ^= (dhash_t)DeeUni_ToLower(((uint8_t *)ptr)[4]) << 32; ATTR_FALLTHROUGH
 case 4:  h ^= (dhash_t)DeeUni_ToLower(((uint8_t *)ptr)[3]) << 24; ATTR_FALLTHROUGH
 case 3:  h ^= (dhash_t)DeeUni_ToLower(((uint8_t *)ptr)[2]) << 16; ATTR_FALLTHROUGH
 case 2:  h ^= (dhash_t)DeeUni_ToLower(((uint8_t *)ptr)[1]) << 8; ATTR_FALLTHROUGH
 case 1:  h ^= (dhash_t)DeeUni_ToLower(((uint8_t *)ptr)[0]);
          h *= m;
          break;
 default: break;
 };
 h ^= h >> r;
 h *= m;
 h ^= h >> r;
 return h;
}
PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashCase2Byte(uint16_t const *__restrict ptr, size_t n_words) {
#ifdef seed
 dhash_t h = seed ^ (n_words * m);
#else
 dhash_t h = 0;
#endif
 size_t len8 = n_words / 8;
 uint32_t ch;
 while (len8--) {
  dhash_t k;
  ch = DeeUni_ToLower(ptr[0]);
  k  = (dhash_t)packl(ch) << ESEL(0,56);
  ch = DeeUni_ToLower(ptr[1]);
  k |= (dhash_t)packl(ch) << ESEL(8,48);
  ch = DeeUni_ToLower(ptr[2]);
  k |= (dhash_t)packl(ch) << ESEL(16,40);
  ch = DeeUni_ToLower(ptr[3]);
  k |= (dhash_t)packl(ch) << ESEL(24,32);
  ch = DeeUni_ToLower(ptr[4]);
  k |= (dhash_t)packl(ch) << ESEL(32,24);
  ch = DeeUni_ToLower(ptr[5]);
  k |= (dhash_t)packl(ch) << ESEL(40,16);
  ch = DeeUni_ToLower(ptr[6]);
  k |= (dhash_t)packl(ch) << ESEL(48,8);
  ch = DeeUni_ToLower(ptr[7]);
  k |= (dhash_t)packl(ch) << ESEL(56,0);
  ptr += 8;
  k *= m;
  k ^= k >> r;
  k *= m;
  h ^= k;
  h *= m;
 }
 switch (n_words%8) {
 case 7:  ch = DeeUni_ToLower(ptr[6]);
          h ^= (dhash_t)packl(ch) << 48; ATTR_FALLTHROUGH
 case 6:  ch = DeeUni_ToLower(ptr[5]);
          h ^= (dhash_t)packl(ch) << 40; ATTR_FALLTHROUGH
 case 5:  ch = DeeUni_ToLower(ptr[4]);
          h ^= (dhash_t)packl(ch) << 32; ATTR_FALLTHROUGH
 case 4:  ch = DeeUni_ToLower(ptr[3]);
          h ^= (dhash_t)packl(ch) << 24; ATTR_FALLTHROUGH
 case 3:  ch = DeeUni_ToLower(ptr[2]);
          h ^= (dhash_t)packl(ch) << 16; ATTR_FALLTHROUGH
 case 2:  ch = DeeUni_ToLower(ptr[1]);
          h ^= (dhash_t)packl(ch) << 8; ATTR_FALLTHROUGH
 case 1:  ch = DeeUni_ToLower(ptr[0]);
          h ^= (dhash_t)packl(ch);
          h *= m;
          break;
 default: break;
 };
 h ^= h >> r;
 h *= m;
 h ^= h >> r;
 return h;
}
PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashCase4Byte(uint32_t const *__restrict ptr, size_t n_dwords) {
#ifdef seed
 dhash_t h = seed ^ (n_dwords * m);
#else
 dhash_t h = 0;
#endif
 size_t len8 = n_dwords / 8;
 uint32_t ch;
 while (len8--) {
  dhash_t k;
  ch = DeeUni_ToLower(ptr[0]);
  k  = (dhash_t)packl(ch) << ESEL(0,56);
  ch = DeeUni_ToLower(ptr[1]);
  k |= (dhash_t)packl(ch) << ESEL(8,48);
  ch = DeeUni_ToLower(ptr[2]);
  k |= (dhash_t)packl(ch) << ESEL(16,40);
  ch = DeeUni_ToLower(ptr[3]);
  k |= (dhash_t)packl(ch) << ESEL(24,32);
  ch = DeeUni_ToLower(ptr[4]);
  k |= (dhash_t)packl(ch) << ESEL(32,24);
  ch = DeeUni_ToLower(ptr[5]);
  k |= (dhash_t)packl(ch) << ESEL(40,16);
  ch = DeeUni_ToLower(ptr[6]);
  k |= (dhash_t)packl(ch) << ESEL(48,8);
  ch = DeeUni_ToLower(ptr[7]);
  k |= (dhash_t)packl(ch) << ESEL(56,0);
  ptr += 8;
  k *= m;
  k ^= k >> r;
  k *= m;
  h ^= k;
  h *= m;
 }
 switch (n_dwords%8) {
 case 7:  ch = DeeUni_ToLower(ptr[6]);
          h ^= (dhash_t)packl(ch) << 48; ATTR_FALLTHROUGH
 case 6:  ch = DeeUni_ToLower(ptr[5]);
          h ^= (dhash_t)packl(ch) << 40; ATTR_FALLTHROUGH
 case 5:  ch = DeeUni_ToLower(ptr[4]);
          h ^= (dhash_t)packl(ch) << 32; ATTR_FALLTHROUGH
 case 4:  ch = DeeUni_ToLower(ptr[3]);
          h ^= (dhash_t)packl(ch) << 24; ATTR_FALLTHROUGH
 case 3:  ch = DeeUni_ToLower(ptr[2]);
          h ^= (dhash_t)packl(ch) << 16; ATTR_FALLTHROUGH
 case 2:  ch = DeeUni_ToLower(ptr[1]);
          h ^= (dhash_t)packl(ch) << 8; ATTR_FALLTHROUGH
 case 1:  ch = DeeUni_ToLower(ptr[0]);
          h ^= (dhash_t)packl(ch);
          h *= m;
          break;
 default: break;
 };
 h ^= h >> r;
 h *= m;
 h ^= h >> r;
 return h;
}

PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashUtf8(char const *__restrict ptr, size_t n_bytes) {
#ifdef seed
 dhash_t h = seed ^ (n_bytes * m); /* XXX: num_characters */
#else
 dhash_t h = 0;
#endif
 char const *end = ptr + n_bytes;
 uint32_t block[8];;
 for (;;) {
  dhash_t k;
  if unlikely(ptr >= end) goto done;
  block[0] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_1;
  block[1] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_2;
  block[2] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_3;
  block[3] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_4;
  block[4] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_5;
  block[5] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_6;
  block[6] = utf8_readchar((char const **)&ptr,end);
  if unlikely(ptr >= end) goto do_tail_7;
  block[7] = utf8_readchar((char const **)&ptr,end);
  k  = (dhash_t)packl(block[0]) << ESEL(0,56);
  k |= (dhash_t)packl(block[1]) << ESEL(8,48);
  k |= (dhash_t)packl(block[2]) << ESEL(16,40);
  k |= (dhash_t)packl(block[3]) << ESEL(24,32);
  k |= (dhash_t)packl(block[4]) << ESEL(32,24);
  k |= (dhash_t)packl(block[5]) << ESEL(40,16);
  k |= (dhash_t)packl(block[6]) << ESEL(48,8);
  k |= (dhash_t)packl(block[7]) << ESEL(56,0);
  k *= m;
  k ^= k >> r;
  k *= m;
  h ^= k;
  h *= m;
 }
 goto done;
do_tail_7:
 h ^= (dhash_t)packl(block[6]) << 48;
do_tail_6:
 h ^= (dhash_t)packl(block[5]) << 40;
do_tail_5:
 h ^= (dhash_t)packl(block[4]) << 32;
do_tail_4:
 h ^= (dhash_t)packl(block[3]) << 24;
do_tail_3:
 h ^= (dhash_t)packl(block[2]) << 16;
do_tail_2:
 h ^= (dhash_t)packl(block[1]) << 8;
do_tail_1:
 h ^= (dhash_t)packl(block[0]);
 h *= m;
done:
 h ^= h >> r;
 h *= m;
 h ^= h >> r;
 return h;
}

PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashCaseUtf8(char const *__restrict ptr, size_t n_bytes) {
#ifdef seed
 dhash_t h = seed ^ (n_bytes * m); /* XXX: num_characters */
#else
 dhash_t h = 0;
#endif
 char const *end = ptr + n_bytes;
 uint32_t block[8];;
 for (;;) {
  dhash_t k;
  if unlikely(ptr >= end) goto done;
  block[0] = utf8_readchar((char const **)&ptr,end);
  block[0] = DeeUni_ToLower(block[0]);
  if unlikely(ptr >= end) goto do_tail_1;
  block[1] = utf8_readchar((char const **)&ptr,end);
  block[1] = DeeUni_ToLower(block[1]);
  if unlikely(ptr >= end) goto do_tail_2;
  block[2] = utf8_readchar((char const **)&ptr,end);
  block[2] = DeeUni_ToLower(block[2]);
  if unlikely(ptr >= end) goto do_tail_3;
  block[3] = utf8_readchar((char const **)&ptr,end);
  block[3] = DeeUni_ToLower(block[3]);
  if unlikely(ptr >= end) goto do_tail_4;
  block[4] = utf8_readchar((char const **)&ptr,end);
  block[4] = DeeUni_ToLower(block[4]);
  if unlikely(ptr >= end) goto do_tail_5;
  block[5] = utf8_readchar((char const **)&ptr,end);
  block[5] = DeeUni_ToLower(block[5]);
  if unlikely(ptr >= end) goto do_tail_6;
  block[6] = utf8_readchar((char const **)&ptr,end);
  block[6] = DeeUni_ToLower(block[6]);
  if unlikely(ptr >= end) goto do_tail_7;
  block[7] = utf8_readchar((char const **)&ptr,end);
  block[7] = DeeUni_ToLower(block[7]);
  k  = (dhash_t)packl(block[0]) << ESEL(0,56);
  k |= (dhash_t)packl(block[1]) << ESEL(8,48);
  k |= (dhash_t)packl(block[2]) << ESEL(16,40);
  k |= (dhash_t)packl(block[3]) << ESEL(24,32);
  k |= (dhash_t)packl(block[4]) << ESEL(32,24);
  k |= (dhash_t)packl(block[5]) << ESEL(40,16);
  k |= (dhash_t)packl(block[6]) << ESEL(48,8);
  k |= (dhash_t)packl(block[7]) << ESEL(56,0);
  k *= m;
  k ^= k >> r;
  k *= m;
  h ^= k;
  h *= m;
 }
 goto done;
do_tail_7:
 h ^= (dhash_t)packl(block[6]) << 48;
do_tail_6:
 h ^= (dhash_t)packl(block[5]) << 40;
do_tail_5:
 h ^= (dhash_t)packl(block[4]) << 32;
do_tail_4:
 h ^= (dhash_t)packl(block[3]) << 24;
do_tail_3:
 h ^= (dhash_t)packl(block[2]) << 16;
do_tail_2:
 h ^= (dhash_t)packl(block[1]) << 8;
do_tail_1:
 h ^= (dhash_t)packl(block[0]);
 h *= m;
done:
 h ^= h >> r;
 h *= m;
 h ^= h >> r;
 return h;
}

#undef seed
#undef r
#undef m

#endif

#if 0
#define HEAP_CHECK()  DEE_CHECKMEMORY()
#else
#define HEAP_CHECK()  (void)0
#endif


#if 0
#define BEGIN_ALLOC()    (DBG_ALIGNMENT_DISABLE(),DeeMem_ClearCaches((size_t)-1))
#else
#define BEGIN_ALLOC()     DBG_ALIGNMENT_DISABLE()
#endif
#define END_ALLOC()       DBG_ALIGNMENT_ENABLE()
#define BEGIN_TRYALLOC()  DBG_ALIGNMENT_DISABLE()
#define END_TRYALLOC()    DBG_ALIGNMENT_ENABLE()

PUBLIC ATTR_PURE dhash_t DCALL
Dee_HashStr(char const *__restrict str) {
 return Dee_HashPtr(str,strlen(str));
}

PUBLIC ATTR_MALLOC void *(DCALL Dee_Malloc)(size_t n_bytes) {
 void *result;
again:
 BEGIN_ALLOC();
 HEAP_CHECK();
 result = (malloc)(n_bytes);
 if unlikely(!result) {
#ifndef __MALLOC_ZERO_IS_NONNULL
  if (!n_bytes) {
   BEGIN_ALLOC();
   result = (calloc)(1,1);
   END_ALLOC();
   if (result)
       return result;
  }
#endif
  if (Dee_CollectMemory(n_bytes)) goto again;
 }
 return result;
}
PUBLIC ATTR_MALLOC void *(DCALL Dee_Calloc)(size_t n_bytes) {
 void *result;
again:
 BEGIN_ALLOC();
 HEAP_CHECK();
 result = (calloc)(1,n_bytes);
 END_ALLOC();
 if unlikely(!result) {
#ifndef __MALLOC_ZERO_IS_NONNULL
  if (!n_bytes) {
   BEGIN_ALLOC();
   result = (calloc)(1,1);
   END_ALLOC();
   if (result)
       return result;
  }
#endif
  if (Dee_CollectMemory(n_bytes)) goto again;
 }
 return result;
}
PUBLIC void *(DCALL Dee_Realloc)(void *ptr, size_t n_bytes) {
 void *result;
#ifndef __REALLOC_ZERO_IS_NONNULL
 if unlikely(!n_bytes)
    n_bytes = 1;
#endif
again:
 BEGIN_ALLOC();
 HEAP_CHECK();
 result = (realloc)(ptr,n_bytes);
 END_ALLOC();
 if unlikely(!result) {
  if (Dee_CollectMemory(n_bytes))
      goto again;
 }
 return result;
}




PUBLIC ATTR_MALLOC void *(DCALL Dee_TryMalloc)(size_t n_bytes) {
 void *result;
 BEGIN_TRYALLOC();
 HEAP_CHECK();
 result = (malloc)(n_bytes);
 END_TRYALLOC();
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!result && !n_bytes) {
  BEGIN_TRYALLOC();
  result = (malloc)(1);
  END_TRYALLOC();
 }
#endif
 return result;
}

PUBLIC ATTR_MALLOC void *(DCALL Dee_TryCalloc)(size_t n_bytes) {
 void *result;
 BEGIN_TRYALLOC();
 HEAP_CHECK();
 result = (calloc)(1,n_bytes);
 END_TRYALLOC();
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!result && !n_bytes) {
  BEGIN_TRYALLOC();
  result = (calloc)(1,1);
  END_TRYALLOC();
 }
#endif
 return result;
}

PUBLIC void *(DCALL Dee_TryRealloc)(void *ptr, size_t n_bytes) {
 void *result;
#ifndef __REALLOC_ZERO_IS_NONNULL
 if unlikely(!n_bytes)
    n_bytes = 1;
#endif
 BEGIN_TRYALLOC();
 HEAP_CHECK();
 result = (realloc)(ptr,n_bytes);
 END_TRYALLOC();
 return result;
}

PUBLIC void (DCALL Dee_Free)(void *ptr) {
 DBG_ALIGNMENT_DISABLE();
 HEAP_CHECK();
 (free)(ptr);
 DBG_ALIGNMENT_ENABLE();
}


#ifndef NDEBUG
#ifdef __KOS_SYSTEM_HEADERS__
#define HAVE_DEEDBG_MALLOC 1
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_TryMalloc)(size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
 HEAP_CHECK();
 BEGIN_TRYALLOC();
#ifdef __KERNEL__
 result = _malloc_d(n_bytes,file,line,NULL,NULL);
#else
 result = _malloc_d(n_bytes,file,line,NULL);
#endif
 END_TRYALLOC();
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!result && !n_bytes) {
  BEGIN_TRYALLOC();
#ifdef __KERNEL__
  result = _malloc_d(1,file,line,NULL,NULL);
#else
  result = _malloc_d(1,file,line,NULL);
#endif
  END_TRYALLOC();
 }
#endif
 return result;
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_TryCalloc)(size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
 HEAP_CHECK();
 BEGIN_TRYALLOC();
#ifdef __KERNEL__
 result = _calloc_d(1,n_bytes,file,line,NULL,NULL);
#else
 result = _calloc_d(1,n_bytes,file,line,NULL);
#endif
 END_TRYALLOC();
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!result && !n_bytes) {
  BEGIN_TRYALLOC();
#ifdef __KERNEL__
  result = _calloc_d(1,1,file,line,NULL,NULL);
#else
  result = _calloc_d(1,1,file,line,NULL);
#endif
  END_TRYALLOC();
 }
#endif
 return result;
}
PUBLIC void *
(DCALL DeeDbg_TryRealloc)(void *ptr, size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!n_bytes)
    n_bytes = 1;
#endif
 HEAP_CHECK();
 BEGIN_TRYALLOC();
#ifdef __KERNEL__
 result = _realloc_d(ptr,n_bytes,file,line,NULL,NULL);
#else
 result = _realloc_d(ptr,n_bytes,file,line,NULL);
#endif
 END_TRYALLOC();
 return result;
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_Malloc)(size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
 HEAP_CHECK();
again:
 BEGIN_ALLOC();
#ifdef __KERNEL__
 result = _malloc_d(n_bytes,file,line,NULL,NULL);
#else
 result = _malloc_d(n_bytes,file,line,NULL);
#endif
 END_ALLOC();
 if unlikely(!result) {
#ifndef __MALLOC_ZERO_IS_NONNULL
  if (!n_bytes) {
   BEGIN_ALLOC();
#ifdef __KERNEL__
   result = _malloc_d(1,file,line,NULL,NULL);
#else
   result = _malloc_d(1,file,line,NULL);
#endif
   END_ALLOC();
   if (result)
       return result;
  }
#endif
  if (Dee_CollectMemory(n_bytes)) goto again;
 }
 return result;
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_Calloc)(size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
 HEAP_CHECK();
again:
 BEGIN_ALLOC();
#ifdef __KERNEL__
 result = _calloc_d(1,n_bytes,file,line,NULL,NULL);
#else
 result = _calloc_d(1,n_bytes,file,line,NULL);
#endif
 END_ALLOC();
 if unlikely(!result) {
#ifndef __MALLOC_ZERO_IS_NONNULL
  if (n_bytes) {
   BEGIN_ALLOC();
#ifdef __KERNEL__
   result = _calloc_d(1,1,file,line,NULL,NULL);
#else
   result = _calloc_d(1,1,file,line,NULL);
#endif
   END_ALLOC();
   if (result)
       return result;
  }
#endif
  if (Dee_CollectMemory(n_bytes)) goto again;
 }
 return result;
}
PUBLIC void *
(DCALL DeeDbg_Realloc)(void *ptr, size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
 HEAP_CHECK();
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!n_bytes)
    n_bytes = 1;
#endif
again:
 BEGIN_ALLOC();
#ifdef __KERNEL__
 result = _realloc_d(ptr,n_bytes,file,line,NULL,NULL);
#else
 result = _realloc_d(ptr,n_bytes,file,line,NULL);
#endif
 END_ALLOC();
 if unlikely(!result) {
  if (Dee_CollectMemory(n_bytes))
      goto again;
 }
 return result;
}
PUBLIC void
(DCALL DeeDbg_Free)(void *ptr, char const *file, int line) {
 (void)file;
 (void)line;
 DBG_ALIGNMENT_DISABLE();
 HEAP_CHECK();
#ifdef __KERNEL__
 _free_d(ptr,file,line,NULL,NULL);
#else
 _free_d(ptr,file,line,NULL);
#endif
 DBG_ALIGNMENT_ENABLE();
}
#elif defined(_MSC_VER)
#define HAVE_DEEDBG_MALLOC 1
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_TryMalloc)(size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
 BEGIN_TRYALLOC();
 HEAP_CHECK();
 result = _malloc_dbg(n_bytes,_NORMAL_BLOCK,file,line);
 END_TRYALLOC();
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!result && !n_bytes) {
  BEGIN_TRYALLOC();
  result = _malloc_dbg(1,_NORMAL_BLOCK,file,line);
  END_TRYALLOC();
 }
#endif
 return result;
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_TryCalloc)(size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
 BEGIN_TRYALLOC();
 HEAP_CHECK();
 result = _calloc_dbg(1,n_bytes,_NORMAL_BLOCK,file,line);
 END_TRYALLOC();
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!result && !n_bytes) {
  BEGIN_TRYALLOC();
  result = _calloc_dbg(1,1,_NORMAL_BLOCK,file,line);
  END_TRYALLOC();
 }
#endif
 return result;
}
PUBLIC void *
(DCALL DeeDbg_TryRealloc)(void *ptr, size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!n_bytes)
    n_bytes = 1;
#endif
 BEGIN_TRYALLOC();
 HEAP_CHECK();
 result = _realloc_dbg(ptr,n_bytes,_NORMAL_BLOCK,file,line);
 END_TRYALLOC();
 return result;
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_Malloc)(size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
again:
 BEGIN_ALLOC();
 HEAP_CHECK();
 result = _malloc_dbg(n_bytes,_NORMAL_BLOCK,file,line);
 END_ALLOC();
 if unlikely(!result) {
#ifndef __MALLOC_ZERO_IS_NONNULL
  if (!n_bytes) {
   BEGIN_ALLOC();
   result = _malloc_dbg(1,_NORMAL_BLOCK,file,line);
   END_ALLOC();
   if (result)
       return result;
  }
#endif
  if (Dee_CollectMemory(n_bytes)) goto again;
 }
 return result;
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_Calloc)(size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
again:
 BEGIN_ALLOC();
 HEAP_CHECK();
 result = _calloc_dbg(1,n_bytes,_NORMAL_BLOCK,file,line);
 END_ALLOC();
 if unlikely(!result) {
#ifndef __MALLOC_ZERO_IS_NONNULL
  if (!n_bytes) {
   BEGIN_ALLOC();
   result = _calloc_dbg(1,1,_NORMAL_BLOCK,file,line);
   END_ALLOC();
   if (result)
       return result;
  }
#endif
  if (Dee_CollectMemory(n_bytes)) goto again;
 }
 return result;
}
PUBLIC void *
(DCALL DeeDbg_Realloc)(void *ptr, size_t n_bytes, char const *file, int line) {
 void *result;
 (void)file;
 (void)line;
#ifndef __MALLOC_ZERO_IS_NONNULL
 if unlikely(!n_bytes)
    n_bytes = 1;
#endif
again:
 BEGIN_ALLOC();
 HEAP_CHECK();
 result = _realloc_dbg(ptr,n_bytes,_NORMAL_BLOCK,file,line);
 END_ALLOC();
 if unlikely(!result) {
  if (Dee_CollectMemory(n_bytes)) goto again;
 }
 return result;
}
PUBLIC void
(DCALL DeeDbg_Free)(void *ptr, char const *UNUSED(file), int UNUSED(line)) {
 DBG_ALIGNMENT_DISABLE();
 HEAP_CHECK();
 _free_dbg(ptr,_NORMAL_BLOCK);
 DBG_ALIGNMENT_ENABLE();
}

#if defined(_MSC_VER) && _MSC_VER < 1900
extern ATTR_DLLIMPORT void __cdecl _lock(_In_ int _File);
extern ATTR_DLLIMPORT void __cdecl _unlock(_Inout_ int _File);
#define _HEAP_LOCK 4
#define nNoMansLandSize 4

typedef struct _CrtMemBlockHeader {
    struct _CrtMemBlockHeader *pBlockHeaderNext;
    struct _CrtMemBlockHeader *pBlockHeaderPrev;
    char                      *szFileName;
    int                        nLine;
#if __SIZEOF_POINTER__ >= 8
    int                        nBlockUse;
    size_t                     nDataSize;
#else
    size_t                     nDataSize;
    int                        nBlockUse;
#endif
    long                       lRequest;
    unsigned char              gap[nNoMansLandSize];
} _CrtMemBlockHeader;

#define pbData(pblock) ((unsigned char *)((_CrtMemBlockHeader *)pblock + 1))
#define pHdr(pbData) (((_CrtMemBlockHeader *)pbData)-1)

#define IGNORE_REQ  0L              /* Request number for ignore block */
#define IGNORE_LINE 0xFEDCBABC      /* Line number for ignore block */
#ifndef _IGNORE_BLOCK
#define _IGNORE_BLOCK 3
#endif

PRIVATE bool DCALL
validate_header(_CrtMemBlockHeader *__restrict hdr) {
 unsigned int i;
 __try {
  for (i = 0; i < nNoMansLandSize; ++i)
      if (hdr->gap[i] != 0xFD) goto nope;
  if (hdr->pBlockHeaderNext->pBlockHeaderPrev != hdr)
      goto nope;
  if (hdr->pBlockHeaderPrev &&
      hdr->pBlockHeaderPrev->pBlockHeaderNext != hdr)
      goto nope;
  /* Badly named. - Should be `_COUNT_BLOCKS' or `_NUM_BLOCKS'!
   * `_MAX_BLOCKS' would be the max-valid-block, but this is
   * number of block types! */
  if (hdr->nBlockUse >= _MAX_BLOCKS)
      goto nope;
 } __except (EXCEPTION_EXECUTE_HANDLER) {
  /* If we failed to access the header for some reason, assume
   * that something went wrong because the header was malformed. */
  goto nope;
 }
 return true;
nope:
 return false;
}

PRIVATE void DCALL
do_unhook(_CrtMemBlockHeader *__restrict hdr) {
 hdr->pBlockHeaderPrev->pBlockHeaderNext = hdr->pBlockHeaderNext;
 hdr->pBlockHeaderNext->pBlockHeaderPrev = hdr->pBlockHeaderPrev;
 hdr->pBlockHeaderNext = NULL;
 hdr->pBlockHeaderPrev = NULL;
 hdr->szFileName       = NULL;
 hdr->nLine            = IGNORE_LINE;
 hdr->nBlockUse        = _IGNORE_BLOCK;
 hdr->lRequest         = IGNORE_REQ;
}

PUBLIC void *
(DCALL DeeDbg_UntrackAlloc)(void *ptr, char const *file, int line) {
 (void)file;
 (void)line;
 if (ptr) {
  _CrtMemBlockHeader *hdr;
  DBG_ALIGNMENT_DISABLE();
  HEAP_CHECK();
  _lock(_HEAP_LOCK);
  __try {
   hdr = pHdr(ptr);
   /* We can't untrack the first allocation ever made... */
   if likely(hdr->pBlockHeaderNext) {
    /* Validate the header in case something changes in MSVC,
     * and our untracking code would SEGFAULT. */
    if (validate_header(hdr)) {
     if (!hdr->pBlockHeaderPrev) {
      void *temp;
      /* Allocate another block, which should be pre-pended before the one we're trying to delete.
       * ... This work-around is required because we can't actually access the debug-allocation
       *     list head, which is a PRIVATE symbol `_pFirstBlock'
       * (This wouldn't have been a problem if MSVC used a self-pointer, instead of a prev-pointer...) */
      temp = _malloc_dbg(1,_NORMAL_BLOCK,file,line);
      if (hdr->pBlockHeaderPrev)
          do_unhook(hdr);
      _free_dbg(temp,_NORMAL_BLOCK);
     } else {
      do_unhook(hdr);
     }
    }
   }
  } __finally {
   _unlock(_HEAP_LOCK);
  }
  DBG_ALIGNMENT_ENABLE();
 }
 return ptr;
}
#else /* _MSC_VER */
PUBLIC void *
(DCALL DeeDbg_UntrackAlloc)(void *ptr, char const *UNUSED(file), int UNUSED(line)) {
 return ptr;
}
#endif /* !_MSC_VER */

#endif
#endif /* !NDEBUG */

#ifndef HAVE_DEEDBG_MALLOC
/* Fallback: The host does not provide a debug-allocation API. */
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_Malloc)(size_t n_bytes, char const *UNUSED(file), int UNUSED(line)) {
 return (Dee_Malloc)(n_bytes);
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_Calloc)(size_t n_bytes, char const *UNUSED(file), int UNUSED(line)) {
 return (Dee_Calloc)(n_bytes);
}
PUBLIC void *
(DCALL DeeDbg_Realloc)(void *ptr, size_t n_bytes, char const *UNUSED(file), int UNUSED(line)) {
 return (Dee_Realloc)(ptr,n_bytes);
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_TryMalloc)(size_t n_bytes, char const *UNUSED(file), int UNUSED(line)) {
 return (Dee_TryMalloc)(n_bytes);
}
PUBLIC ATTR_MALLOC void *
(DCALL DeeDbg_TryCalloc)(size_t n_bytes, char const *UNUSED(file), int UNUSED(line)) {
 return (Dee_TryCalloc)(n_bytes);
}
PUBLIC void *
(DCALL DeeDbg_TryRealloc)(void *ptr, size_t n_bytes,
                          char const *UNUSED(file), int UNUSED(line)) {
 return (Dee_TryRealloc)(ptr,n_bytes);
}
PUBLIC void
(DCALL DeeDbg_Free)(void *ptr, char const *UNUSED(file), int UNUSED(line)) {
 return (Dee_Free)(ptr);
}
PUBLIC void *
(DCALL DeeDbg_UntrackAlloc)(void *ptr, char const *UNUSED(file), int UNUSED(line)) {
 return ptr;
}
#endif /* !HAVE_DEEDBG_MALLOC */





#define Cs(x) INTDEF size_t DCALL x##_clear(size_t max_clear);
#define Co(x) INTDEF size_t DCALL x##_clear(size_t max_clear);
#include "caches.def"
#undef Co
#undef Cs


/* TODO: CONFIG_NO_CACHES */
typedef size_t (DCALL *pcacheclr)(size_t max_clear);
INTDEF size_t DCALL intcache_clear(size_t max_clear);
INTDEF size_t DCALL tuplecache_clear(size_t max_clear);
INTDEF size_t DCALL latincache_clear(size_t max_clear);
INTDEF size_t DCALL membercache_clear(size_t max_clear);

PRIVATE pcacheclr caches[] = {
#define Cs(x) &x##_clear,
#define Co(x) &x##_clear,
#include "caches.def"
#undef Co
#undef Cs
    /* Custom object/data cache clear functions. */
    &intcache_clear,
    &tuplecache_clear,
    &latincache_clear,
    &membercache_clear,
#ifndef CONFIG_NO_DEC
    &DecTime_ClearCache,
#endif
    NULL
};


PRIVATE size_t DCALL
DeeMem_ClearCaches_onepass(size_t max_collect) {
 pcacheclr *iter;
 size_t result = 0;
 /* Go through all the caches and collect memory. */
 for (iter = caches; *iter; ++iter) {
  result += (*iter)(max_collect - result);
  if (result >= max_collect) break;
 }
 return result;
}

PUBLIC size_t DCALL
DeeMem_ClearCaches(size_t max_collect) {
 size_t part,result = 0;
 do {
  part    = DeeMem_ClearCaches_onepass(max_collect-result);
  result += part;
 } while (result < max_collect && part);
 return result;
}

PUBLIC bool DCALL Dee_TryCollectMemory(size_t req_bytes) {
 /* Clear caches and collect memory from various places. */
 size_t collect_bytes = DeeMem_ClearCaches(req_bytes);
 if (collect_bytes >= req_bytes) return true;
 req_bytes -= collect_bytes;
 /* Collect GC objects.
  * NOTE: When optimizing, only try to collect a single object
  *       in order to prevent lag-time during memory shortages.
  *       However for debug-mode, always collect everything in
  *       order to try and harden the algorithm in times of need. */
#if defined(NDEBUG) || defined(__OPTIMIZE__)
 if (DeeGC_Collect(1)) return true;
#else
 if (DeeGC_Collect((size_t)-1)) return true;
#endif
 return false;
}
PUBLIC bool DCALL Dee_CollectMemory(size_t req_bytes) {
 bool result = Dee_TryCollectMemory(req_bytes);
 if unlikely(!result) Dee_BadAlloc(req_bytes);
 return result;
}



#ifndef NDEBUG

#ifndef CONFIG_OUTPUTDEBUGSTRINGA_DEFINED
#define CONFIG_OUTPUTDEBUGSTRINGA_DEFINED 1
extern ATTR_DLLIMPORT void ATTR_STDCALL OutputDebugStringA(char const *lpOutputString);
extern ATTR_DLLIMPORT int ATTR_STDCALL IsDebuggerPresent(void);
#endif /* !CONFIG_OUTPUTDEBUGSTRINGA_DEFINED */

PRIVATE dssize_t DCALL
debug_printer(void *UNUSED(closure),
              char const *__restrict buffer, size_t bufsize) {
#ifdef CONFIG_HOST_WINDOWS
 size_t result = bufsize;
#ifdef PAGESIZE
 /* (ab-)use the fact that the kernel can't keep us from reading
  *  beyond the end of a buffer so long as that memory location
  *  is located within the same page as the last byte of said
  *  buffer (Trust me... I've written by own OS) */
 if ((bufsize <= 1000) && /* There seems to be some kind of limitation by `OutputDebugStringA()' here... */
     (((uintptr_t)buffer + bufsize)     & ~(uintptr_t)(PAGESIZE-1)) ==
     (((uintptr_t)buffer + bufsize - 1) & ~(uintptr_t)(PAGESIZE-1)) &&
     (*(char *)((uintptr_t)buffer + bufsize)) == '\0') {
  DBG_ALIGNMENT_DISABLE();
  OutputDebugStringA((char *)buffer);
  DBG_ALIGNMENT_ENABLE();
 } else
#endif
 {
  char temp[512];
  while (bufsize) {
   size_t part = MIN(bufsize,sizeof(temp)-sizeof(char));
   memcpy(temp,buffer,part);
   temp[part] = '\0';
   DBG_ALIGNMENT_DISABLE();
   OutputDebugStringA(temp);
   DBG_ALIGNMENT_ENABLE();
   *(uintptr_t *)&buffer += part;
   bufsize -= part;
  }
 }
 return result;
#else
 return DeeFile_Write(DeeFile_DefaultStddbg,buffer,bufsize);
#endif
}
#endif /* !NDEBUG */


#ifdef NDEBUG
PUBLIC int _Dee_dprint_enabled = 0;
#else
PUBLIC int _Dee_dprint_enabled = 2;
PRIVATE void DCALL determine_is_dprint_enabled(void) {
 char *env;
 DBG_ALIGNMENT_DISABLE();
#ifdef CONFIG_HOST_WINDOWS
 if (!IsDebuggerPresent()) {
  DBG_ALIGNMENT_ENABLE();
  _Dee_dprint_enabled = 0;
  return;
 }
#endif /* CONFIG_HOST_WINDOWS */
 env = getenv("DEEMON_SILENT");
 DBG_ALIGNMENT_ENABLE();
#ifdef CONFIG_HOST_WINDOWS
 if (env && *env)
  _Dee_dprint_enabled = 0;
 else {
  _Dee_dprint_enabled = 1;
 }
#else
 if (env && *env)
  _Dee_dprint_enabled = *env == '0';
 else {
  _Dee_dprint_enabled = 0;
 }
#endif
}
#endif



PUBLIC void (DCALL _Dee_vdprintf)(char const *__restrict format, va_list args) {
#ifdef NDEBUG
 (void)format;
 (void)args;
#else
 if (_Dee_dprint_enabled == 2)
     determine_is_dprint_enabled();
 if (!_Dee_dprint_enabled) return;
 if (DeeFormat_VPrintf(&debug_printer,NULL,format,args) < 0)
     DeeError_Handled(ERROR_HANDLED_RESTORE);
#endif
}

PUBLIC void (DCALL _Dee_dprint)(char const *__restrict message) {
#ifdef NDEBUG
 (void)message;
#else
 if (_Dee_dprint_enabled == 2)
     determine_is_dprint_enabled();
 if (!_Dee_dprint_enabled) return;
#ifdef CONFIG_HOST_WINDOWS
 DBG_ALIGNMENT_DISABLE();
 OutputDebugStringA(message);
 DBG_ALIGNMENT_ENABLE();
#else /* CONFIG_HOST_WINDOWS */
 if (debug_printer(NULL,message,strlen(message)) < 0)
     DeeError_Handled(ERROR_HANDLED_RESTORE);
#endif /* !CONFIG_HOST_WINDOWS */
#endif
}

PUBLIC void (_Dee_dprintf)(char const *__restrict format, ...) {
#ifdef NDEBUG
 (void)format;
#else
 va_list args;
 if (_Dee_dprint_enabled == 2)
     determine_is_dprint_enabled();
 if (!_Dee_dprint_enabled) return;
 va_start(args,format);
 if (DeeFormat_VPrintf(&debug_printer,NULL,format,args) < 0)
     DeeError_Handled(ERROR_HANDLED_RESTORE);
 va_end(args);
#endif
}


#ifdef NDEBUG
PUBLIC void (_DeeAssert_Failf)(char const *UNUSED(expr), char const *UNUSED(file), int UNUSED(line), char const *UNUSED(format), ...) {}
PUBLIC void (DCALL _DeeAssert_Fail)(char const *UNUSED(expr), char const *UNUSED(file), int UNUSED(line)) {}
#else
PRIVATE void assert_vprintf(char const *format, va_list args) {
 dssize_t error;
 error = DeeFile_VPrintf(DeeFile_DefaultStddbg,format,args);
 if unlikely(error < 0)
    DeeError_Handled(ERROR_HANDLED_RESTORE);
}
PRIVATE void assert_printf(char const *format, ...) {
 va_list args;
 va_start(args,format);
 assert_vprintf(format,args);
 va_end(args);
}

PUBLIC void
(_DeeAssert_Failf)(char const *expr, char const *file,
                  int line, char const *format, ...) {
 assert_printf("\n\n\n"
               "%s(%d) : Assertion failed : %s\n",
               file,line,expr);
 if (format) {
  va_list args;
  va_start(args,format);
  assert_vprintf(format,args);
  va_end(args);
  assert_printf("\n");
 }
}
PUBLIC void
(DCALL _DeeAssert_Fail)(char const *expr, char const *file, int line) {
 _DeeAssert_Failf(expr,file,line,NULL);
}
#endif

DECL_END

#ifndef __INTELLISENSE__
#include "slab.c.inl"
#endif

#endif /* !GUARD_DEEMON_RUNTIME_MISC_C */
