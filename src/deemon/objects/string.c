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
#ifndef GUARD_DEEMON_OBJECTS_STRING_C
#define GUARD_DEEMON_OBJECTS_STRING_C 1

#include <deemon/alloc.h>
#include <deemon/api.h>
#include <deemon/arg.h>
#include <deemon/bool.h>
#include <deemon/bytes.h>
#include <deemon/error.h>
#include <deemon/format.h>
#include <deemon/int.h>
#include <deemon/none.h>
#include <deemon/object.h>
#include <deemon/seq.h>
#include <deemon/string.h>
#include <deemon/stringutils.h>
#include <deemon/thread.h>
#include <deemon/system-features.h> /* memmem() */
#include <deemon/util/atomic.h>

#include <hybrid/minmax.h>

#include <stddef.h>

#include "../runtime/runtime_error.h"
#include "../runtime/strings.h"

#undef SSIZE_MAX
#include <hybrid/limitcore.h>
#define SSIZE_MAX __SSIZE_MAX__


DECL_BEGIN

#ifndef NDEBUG
#define DBG_memset (void)memset
#else /* !NDEBUG */
#define DBG_memset(dst, byte, n_bytes) (void)0
#endif /* NDEBUG */

typedef DeeStringObject String;

#ifndef CONFIG_HAVE_memmem
#define memmem dee_memmem
DeeSystem_DEFINE_memmem(dee_memmem)
#endif /* !CONFIG_HAVE_memmem */

PUBLIC NONNULL((1)) void
(DCALL Dee_ascii_printer_release)(struct ascii_printer *__restrict self, size_t datalen) {
	ASSERT(self->ap_length >= datalen);
	/* This's actually all that needs to be
	 * done with the current implementation. */
	self->ap_length -= datalen;
}

PUBLIC WUNUSED NONNULL((1)) char *
(DCALL Dee_ascii_printer_alloc)(struct ascii_printer *__restrict self, size_t datalen) {
	String *string;
	size_t alloc_size;
	char *result;
	if ((string = self->ap_string) == NULL) {
		/* Make sure not to allocate a string when the used length remains ZERO.
		 * >> Must be done to assure the expectation of `if(ap_length == 0) ap_string == NULL' */
		if unlikely(!datalen)
			return 0;
		/* Allocate the initial string. */
		alloc_size = 8;
		while (alloc_size < datalen)
			alloc_size *= 2;
alloc_again:
		string = (String *)DeeObject_TryMalloc(offsetof(String, s_str) +
		                                       (alloc_size + 1) * sizeof(char));
		if unlikely(!string) {
			if (alloc_size != datalen) {
				alloc_size = datalen;
				goto alloc_again;
			}
			if (Dee_CollectMemory(offsetof(String, s_str) +
			                      (alloc_size + 1) * sizeof(char)))
				goto alloc_again;
			return NULL;
		}
		self->ap_string = string;
		string->s_len   = alloc_size;
		self->ap_length = datalen;
		return string->s_str;
	}
	alloc_size = string->s_len;
	ASSERT(alloc_size >= self->ap_length);
	alloc_size -= self->ap_length;
	if unlikely(alloc_size < datalen) {
		size_t min_alloc = self->ap_length + datalen;
		alloc_size       = (min_alloc + 63) & ~63;
realloc_again:
		string = (String *)DeeObject_TryRealloc(string,
		                                        offsetof(String, s_str) +
		                                        (alloc_size + 1) * sizeof(char));
		if unlikely(!string) {
			string = self->ap_string;
			if (alloc_size != min_alloc) {
				alloc_size = min_alloc;
				goto realloc_again;
			}
			if (Dee_CollectMemory(offsetof(String, s_str) +
			                      (alloc_size + 1) * sizeof(char)))
				goto realloc_again;
			return NULL;
		}
		self->ap_string = string;
		string->s_len   = alloc_size;
	}
	/* Append text at the end. */
	result = string->s_str + self->ap_length;
	self->ap_length += datalen;
	return result;
}

PUBLIC WUNUSED NONNULL((1)) int
(DCALL Dee_ascii_printer_putc)(struct ascii_printer *__restrict self, char ch) {
	/* Quick check: Can we print to an existing buffer. */
	if (self->ap_string &&
	    self->ap_length < self->ap_string->s_len) {
		self->ap_string->s_str[self->ap_length++] = ch;
		goto done;
	}
	/* Fallback: go the long route. */
	if (ascii_printer_print(self, &ch, 1) < 0)
		goto err;
done:
	return 0;
err:
	return -1;
}

PUBLIC WUNUSED NONNULL((1)) dssize_t
(DPRINTER_CC Dee_ascii_printer_print)(void *__restrict self,
                                      char const *__restrict data,
                                      size_t datalen) {
	struct ascii_printer *me;
	String *string;
	size_t alloc_size;
	me = (struct ascii_printer *)self;
	ASSERT(data || !datalen);
	if ((string = me->ap_string) == NULL) {
		/* Make sure not to allocate a string when the used length remains ZERO.
		 * >> Must be done to assure the expectation of `if(ap_length == 0) ap_string == NULL' */
		if unlikely(!datalen)
			return 0;
		/* Allocate the initial string. */
		alloc_size = 8;
		while (alloc_size < datalen)
			alloc_size *= 2;
alloc_again:
		string = (String *)DeeObject_TryMalloc(offsetof(String, s_str) +
		                                       (alloc_size + 1) * sizeof(char));
		if unlikely(!string) {
			if (alloc_size != datalen) {
				alloc_size = datalen;
				goto alloc_again;
			}
			if (Dee_CollectMemory(offsetof(String, s_str) +
			                      (alloc_size + 1) * sizeof(char)))
				goto alloc_again;
			return -1;
		}
		me->ap_string = string;
		string->s_len = alloc_size;
		memcpyc(string->s_str, data,
		        datalen, sizeof(char));
		me->ap_length = datalen;
		goto done;
	}
	alloc_size = string->s_len;
	ASSERT(alloc_size >= me->ap_length);
	alloc_size -= me->ap_length;
	if unlikely(alloc_size < datalen) {
		size_t min_alloc = me->ap_length + datalen;
		alloc_size       = (min_alloc + 63) & ~63;
realloc_again:
		string = (String *)DeeObject_TryRealloc(string,
		                                        offsetof(String, s_str) +
		                                        (alloc_size + 1) * sizeof(char));
		if unlikely(!string) {
			string = me->ap_string;
			if (alloc_size != min_alloc) {
				alloc_size = min_alloc;
				goto realloc_again;
			}
			if (Dee_CollectMemory(offsetof(String, s_str) +
			                      (alloc_size + 1) * sizeof(char)))
				goto realloc_again;
			return -1;
		}
		me->ap_string = string;
		string->s_len = alloc_size;
	}
	/* Copy text into the dynamic string. */
	/*Dee_DPRINTF("PRINT: %" PRFXSIZ " - `%.*s'\n", datalen, (int)datalen, data);*/
	memcpyc(string->s_str + me->ap_length,
	        data, datalen, sizeof(char));
	me->ap_length += datalen;
done:
	return (dssize_t)datalen;
}

PUBLIC WUNUSED NONNULL((1)) DREF DeeObject *
(DCALL Dee_ascii_printer_pack)(struct ascii_printer *__restrict self) {
	DREF String *result = self->ap_string;
	if unlikely(!result)
		return_reference_(Dee_EmptyString);
	/* Deallocate unused memory. */
	if likely(self->ap_length != result->s_len) {
		DREF String *reloc;
		reloc = (DREF String *)DeeObject_TryRealloc(result,
		                                            offsetof(String, s_str) +
		                                            (self->ap_length + 1) * sizeof(char));
		if likely(reloc)
			result = reloc;
		result->s_len = self->ap_length;
	}

	/* Make sure to terminate the c-string representation. */
	result->s_str[self->ap_length] = '\0';

	/* Do final object initialization. */
	DeeObject_Init(result, &DeeString_Type);
	result->s_hash = (dhash_t)-1;
	result->s_data = NULL;
	DBG_memset(self, 0xcc, sizeof(*self));
	return (DREF DeeObject *)result;
}

PUBLIC WUNUSED NONNULL((1, 2)) char *
(DCALL Dee_ascii_printer_allocstr)(struct ascii_printer *__restrict self,
                                   char const *__restrict str, size_t length) {
	char *result;
	dssize_t error;
	if (self->ap_string) {
		result = (char *)memmem(self->ap_string->s_str,
		                        self->ap_length, str, length);
		if (result)
			return result;
	}

	/* Append a new string. */
	error = ascii_printer_print(self, str, length);
	if unlikely(error < 0)
		goto err;
	ASSERT(self->ap_string || (!self->ap_length && !length));
	ASSERT(self->ap_length >= length);
	return self->ap_string->s_str + (self->ap_length - length);
err:
	return NULL;
}

STATIC_ASSERT(STRING_WIDTH_1BYTE < 1);

/* Resize a single-byte string to have a length of `num_bytes' bytes.
 * You may pass `NULL' for `self', or a reference to `Dee_EmptyString'
 * in order to allocate and return a new buffer. */
PUBLIC WUNUSED DREF DeeObject *DCALL
DeeString_ResizeBuffer(DREF DeeObject *self, size_t num_bytes) {
	DREF String *result;
	if unlikely(self == Dee_EmptyString)
		self = NULL;
	ASSERTF(!self || DeeString_Check(self), "Not a string buffer");
	ASSERTF(!self || !DeeObject_IsShared(self), "String buffers cannot be shared");

	/* Special case: Resize-to-zero. */
	if unlikely(!num_bytes) {
		if (self)
			Dee_DecrefDokill(self);
		Dee_Incref(Dee_EmptyString);
		return Dee_EmptyString;
	}

	/* Re-allocate the buffer. */
	result = (DREF String *)DeeObject_Realloc(self,
	                                          offsetof(String, s_str) +
	                                          (num_bytes + 1) * sizeof(char));
	if likely(result) {
		if (!self) {
			/* Do the initial init when `self' was `NULL'. */
			DeeObject_Init(result, &DeeString_Type);
			result->s_data = NULL;
			result->s_hash = (dhash_t)-1;
		}
		result->s_len            = num_bytes;
		result->s_str[num_bytes] = '\0';
	}
	return (DREF DeeObject *)result;
}

PUBLIC WUNUSED DREF DeeObject *DCALL
DeeString_TryResizeBuffer(DREF DeeObject *self, size_t num_bytes) {
	DREF String *result;
	if unlikely(self == Dee_EmptyString)
		self = NULL;
	ASSERTF(!self || DeeString_Check(self), "Not a string buffer");
	ASSERTF(!self || !DeeObject_IsShared(self), "String buffers cannot be shared");

	/* Special case: Resize-to-zero. */
	if unlikely(!num_bytes) {
		if (self)
			Dee_DecrefDokill(self);
		Dee_Incref(Dee_EmptyString);
		return Dee_EmptyString;
	}

	/* Re-allocate the buffer. */
	result = (DREF String *)DeeObject_TryRealloc(self,
	                                             offsetof(String, s_str) +
	                                             (num_bytes + 1) * sizeof(char));
	if likely(result) {
		if (!self) {
			/* Do the initial init when `self' was `NULL'. */
			DeeObject_Init(result, &DeeString_Type);
			result->s_data = NULL;
			result->s_hash = (dhash_t)-1;
		}
		result->s_len            = num_bytes;
		result->s_str[num_bytes] = '\0';
	}
	return (DREF DeeObject *)result;
}

/* Construct an uninitialized single-byte string,
 * capable of representing up to `num_bytes' bytes of text. */
PUBLIC WUNUSED DREF DeeObject *DCALL
DeeString_NewBuffer(size_t num_bytes) {
	DREF String *result;
	if unlikely(!num_bytes) {
		Dee_Incref(Dee_EmptyString);
		return Dee_EmptyString;
	}
	result = (DREF String *)DeeObject_Malloc(offsetof(String, s_str) +
	                                         (num_bytes + 1) * sizeof(char));
	if likely(result) {
		DeeObject_Init(result, &DeeString_Type);
		result->s_data           = NULL;
		result->s_hash           = (dhash_t)-1;
		result->s_len            = num_bytes;
		result->s_str[num_bytes] = '\0';
	}
	return (DREF DeeObject *)result;
}

PUBLIC NONNULL((1)) void DCALL
DeeString_FreeWidth(DeeObject *__restrict self) {
	DeeStringObject *me;
	struct string_utf *utf;
	me = (DeeStringObject *)self;
	ASSERTF(DeeString_Check(me), "Not a string buffer");
	ASSERTF(!DeeObject_IsShared(me), "String buffers cannot be shared");
	if ((utf = me->s_data) == NULL)
		return;
	ASSERTF(utf->u_width == STRING_WIDTH_1BYTE, "This isn't a 1-byte string");
	Dee_string_utf_fini(utf, me);
	me->s_data = NULL;
	Dee_string_utf_free(utf);
}

/* Construct strings with basic width-data. */
PUBLIC WUNUSED NONNULL((1)) DREF DeeObject *DCALL
DeeString_NewSized(/*unsigned latin-1*/ char const *__restrict str, size_t length) {
	DREF DeeObject *result;
	/* Optimization: use pre-allocated latin1 strings
	 *               for single-character sequences. */
	switch (length) {

	case 0:
		return_empty_string;

	case 1:
		return DeeString_Chr((uint8_t)str[0]);

	default:
		break;
	}
	result = DeeString_NewBuffer(length);
	if (result) {
		memcpyc(DeeString_STR(result), str,
		        length, sizeof(char));
	}
	return result;
}

/* Construct a new, non-decoded single-byte-per-character string `str'.
 * The string itself may contain characters above 127, which are then
 * interpreted as part of the unicode character-range U+0080...U+00FF. */
PUBLIC WUNUSED NONNULL((1)) DREF DeeObject *DCALL
DeeString_New(/*unsigned*/ char const *__restrict str) {
	return DeeString_NewSized(str, strlen(str));
}


PRIVATE WUNUSED DREF DeeObject *DCALL
string_new_empty(void) {
	return_empty_string;
}

PRIVATE WUNUSED DREF DeeObject *DCALL
string_new(size_t argc, DeeObject *const *argv) {
	DeeObject *ob;
	if (DeeArg_Unpack(argc, argv, "o:string", &ob))
		goto err;
	return DeeObject_Str(ob);
err:
	return NULL;
}

PUBLIC WUNUSED NONNULL((1)) DREF DeeObject *DCALL
DeeString_VNewf(/*utf-8*/ char const *__restrict format, va_list args) {
	struct unicode_printer printer = UNICODE_PRINTER_INIT;
	if unlikely(unicode_printer_vprintf(&printer, format, args) < 0)
		goto err;
	return unicode_printer_pack(&printer);
err:
	unicode_printer_fini(&printer);
	return NULL;
}

/* Construct a new string using printf-like (and deemon-enhanced) format-arguments. */
PUBLIC WUNUSED NONNULL((1)) DREF DeeObject *
DeeString_Newf(/*utf-8*/ char const *__restrict format, ...) {
	va_list args;
	DREF DeeObject *result;
	va_start(args, format);
	result = DeeString_VNewf(format, args);
	va_end(args);
	return result;
}


PRIVATE WUNUSED NONNULL((1)) int DCALL
string_bool(String *__restrict self) {
	return !DeeString_IsEmpty(self);
}

INTERN ATTR_PURE WUNUSED NONNULL((1)) dhash_t DCALL
DeeString_Hash(DeeObject *__restrict self) {
	dhash_t result;
	ASSERT_OBJECT_TYPE_EXACT(self, &DeeString_Type);
	result = DeeString_HASH(self);
	if (result == (dhash_t)-1) {
		SWITCH_SIZEOF_WIDTH(DeeString_WIDTH(self)) {

		CASE_WIDTH_1BYTE:
			result = Dee_HashPtr(DeeString_STR(self),
			                     DeeString_SIZE(self));
			break;

		CASE_WIDTH_2BYTE: {
			uint16_t *str;
			str    = DeeString_Get2Byte(self);
			result = Dee_Hash2Byte(str, WSTR_LENGTH(str));
		}	break;

		CASE_WIDTH_4BYTE: {
			uint32_t *str;
			str    = DeeString_Get4Byte(self);
			result = Dee_Hash4Byte(str, WSTR_LENGTH(str));
		}	break;

		}
		DeeString_HASH(self) = result;
	}
	return result;
}

PUBLIC ATTR_PURE WUNUSED NONNULL((1)) dhash_t DCALL
DeeString_HashCase(DeeObject *__restrict self) {
	dhash_t result;
	ASSERT_OBJECT_TYPE_EXACT(self, &DeeString_Type);
	SWITCH_SIZEOF_WIDTH(DeeString_WIDTH(self)) {

	CASE_WIDTH_1BYTE:
		result = Dee_HashCasePtr(DeeString_STR(self),
		                         DeeString_SIZE(self));
		break;

	CASE_WIDTH_2BYTE: {
		uint16_t *str;
		str    = DeeString_Get2Byte(self);
		result = Dee_HashCase2Byte(str, WSTR_LENGTH(str));
	}	break;

	CASE_WIDTH_4BYTE: {
		uint32_t *str;
		str    = DeeString_Get4Byte(self);
		result = Dee_HashCase4Byte(str, WSTR_LENGTH(str));
	}	break;

	}
	return result;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_repr(DeeObject *__restrict self) {
	struct ascii_printer printer = ASCII_PRINTER_INIT;
	if unlikely(DeeString_PrintRepr(self, &ascii_printer_print, &printer) < 0)
		goto err;
	return ascii_printer_pack(&printer);
err:
	ascii_printer_fini(&printer);
	return NULL;
}


/* Destroy the regex cache associated with `self'.
 * Called from `DeeString_Type.tp_fini' when `STRING_UTF_FREGEX' was set. */
INTDEF NONNULL((1)) void DCALL /* From "./unicode/regex.c" */
DeeString_DestroyRegex(String *__restrict self);

PRIVATE NONNULL((1)) void DCALL
string_fini(String *__restrict self) {
	struct string_utf *utf;
	/* Clean up UTF data. */
	if ((utf = self->s_data) != NULL) {
		/* If present */
		if unlikely(utf->u_flags & STRING_UTF_FREGEX)
			DeeString_DestroyRegex(self);

		Dee_string_utf_fini(utf, self);
		Dee_string_utf_free(utf);
	}
}

LOCAL ATTR_CONST int DCALL
fix_memcmp_return(int value) {
	if (value < -1) {
		value = -1;
	} else if (value > 1) {
		value = 1;
	}
	return value;
}


INTERN WUNUSED NONNULL((1, 2)) int DCALL
compare_string_bytes(String *__restrict lhs,
                     DeeBytesObject *__restrict rhs) {
	size_t lhs_len;
	size_t rhs_len;
	if (!lhs->s_data ||
	    lhs->s_data->u_width == STRING_WIDTH_1BYTE) {
		uint8_t *lhs_str;
		int result;
		/* Compare against single-byte string. */
		lhs_str = (uint8_t *)lhs->s_str;
		lhs_len = lhs->s_len;
		rhs_len = DeeBytes_SIZE(rhs);
		/* Most simple case: compare ascii/single-byte strings. */
		result = memcmp(lhs_str, DeeBytes_DATA(rhs), MIN(lhs_len, rhs_len));
		if (result != 0)
			return fix_memcmp_return(result);
	} else {
		uint8_t *rhs_str;
		struct string_utf *lhs_utf;
		/* Compare against single-byte string. */
		rhs_str = DeeBytes_DATA(rhs);
		rhs_len = DeeBytes_SIZE(rhs);
		lhs_utf = lhs->s_data;
		switch (lhs_utf->u_width) {

		CASE_WIDTH_2BYTE: {
			uint16_t *lhs_str;
			size_t i, common_len;
			lhs_str    = (uint16_t *)lhs_utf->u_data[lhs_utf->u_width];
			lhs_len    = WSTR_LENGTH(lhs_str);
			common_len = MIN(rhs_len, lhs_len);
			for (i = 0; i < common_len; ++i) {
				if (lhs_str[i] == (uint16_t)(rhs_str[i]))
					continue;
				return lhs_str[i] < (uint16_t)(rhs_str[i]) ? -1 : 1;
			}
		}	break;

		CASE_WIDTH_4BYTE: {
			uint32_t *lhs_str;
			size_t i, common_len;
			lhs_str    = (uint32_t *)lhs_utf->u_data[lhs_utf->u_width];
			lhs_len    = WSTR_LENGTH(lhs_str);
			common_len = MIN(rhs_len, lhs_len);
			for (i = 0; i < common_len; ++i) {
				if (lhs_str[i] == (uint32_t)(rhs_str[i]))
					continue;
				return lhs_str[i] < (uint32_t)(rhs_str[i]) ? -1 : 1;
			}
		}	break;

		default: __builtin_unreachable();
		}
	}

	/* If string contents are identical, leave off by comparing their lengths. */
	if (lhs_len == rhs_len)
		return 0;
	if (lhs_len < rhs_len)
		return -1;
	return 1;
}

#ifndef CONFIG_HAVE_memcmpw
#define CONFIG_HAVE_memcmpw
#define memcmpw dee_memcmpw
DeeSystem_DEFINE_memcmpw(dee_memcmpw)
#endif /* !CONFIG_HAVE_memcmpw */

#ifndef CONFIG_HAVE_memcmpl
#define CONFIG_HAVE_memcmpl
#define memcmpl dee_memcmpl
DeeSystem_DEFINE_memcmpl(dee_memcmpl)
#endif /* !CONFIG_HAVE_memcmpl */

PRIVATE WUNUSED NONNULL((1, 2)) int DCALL
compare_strings(String *__restrict lhs,
                String *__restrict rhs) {
	size_t lhs_len;
	size_t rhs_len;
	if (!lhs->s_data ||
	    lhs->s_data->u_width == STRING_WIDTH_1BYTE) {
		uint8_t *lhs_str;
		/* Compare against single-byte string. */
		lhs_str = (uint8_t *)lhs->s_str;
		lhs_len = lhs->s_len;
		if (!rhs->s_data ||
		    rhs->s_data->u_width == STRING_WIDTH_1BYTE) {
			int result;
			rhs_len = rhs->s_len;
			/* Most simple case: compare ascii/single-byte strings. */
			result = memcmp(lhs_str, rhs->s_str, MIN(lhs_len, rhs_len));
			if (result != 0)
				return fix_memcmp_return(result);
		} else {
			struct string_utf *rhs_utf = rhs->s_data;
			switch (rhs_utf->u_width) {

			CASE_WIDTH_2BYTE: {
				uint16_t *rhs_str;
				size_t i, common_len;
				rhs_str    = (uint16_t *)rhs_utf->u_data[rhs_utf->u_width];
				rhs_len    = WSTR_LENGTH(rhs_str);
				common_len = MIN(lhs_len, rhs_len);
				for (i = 0; i < common_len; ++i) {
					if ((uint16_t)(lhs_str[i]) == rhs_str[i])
						continue;
					return (uint16_t)(lhs_str[i]) < rhs_str[i] ? -1 : 1;
				}
			}	break;

			CASE_WIDTH_4BYTE: {
				uint32_t *rhs_str;
				size_t i, common_len;
				rhs_str    = (uint32_t *)rhs_utf->u_data[rhs_utf->u_width];
				rhs_len    = WSTR_LENGTH(rhs_str);
				common_len = MIN(lhs_len, rhs_len);
				for (i = 0; i < common_len; ++i) {
					if ((uint32_t)(lhs_str[i]) == rhs_str[i])
						continue;
					return (uint32_t)(lhs_str[i]) < rhs_str[i] ? -1 : 1;
				}
			}	break;

			default: __builtin_unreachable();
			}
		}
	} else if (!rhs->s_data ||
	           rhs->s_data->u_width == STRING_WIDTH_1BYTE) {
		uint8_t *rhs_str;
		struct string_utf *lhs_utf;
		/* Compare against single-byte string. */
		rhs_str = (uint8_t *)rhs->s_str;
		rhs_len = rhs->s_len;
		lhs_utf = lhs->s_data;
		switch (lhs_utf->u_width) {

		CASE_WIDTH_2BYTE: {
			uint16_t *lhs_str;
			size_t i, common_len;
			lhs_str    = (uint16_t *)lhs_utf->u_data[lhs_utf->u_width];
			lhs_len    = WSTR_LENGTH(lhs_str);
			common_len = MIN(rhs_len, lhs_len);
			for (i = 0; i < common_len; ++i) {
				if (lhs_str[i] == (uint16_t)(rhs_str[i]))
					continue;
				return lhs_str[i] < (uint16_t)(rhs_str[i]) ? -1 : 1;
			}
		}	break;

		CASE_WIDTH_4BYTE: {
			uint32_t *lhs_str;
			size_t i, common_len;
			lhs_str    = (uint32_t *)lhs_utf->u_data[lhs_utf->u_width];
			lhs_len    = WSTR_LENGTH(lhs_str);
			common_len = MIN(rhs_len, lhs_len);
			for (i = 0; i < common_len; ++i) {
				if (lhs_str[i] == (uint32_t)(rhs_str[i]))
					continue;
				return lhs_str[i] < (uint32_t)(rhs_str[i]) ? -1 : 1;
			}
		}	break;

		default: __builtin_unreachable();
		}
	} else {
		struct string_utf *lhs_utf;
		struct string_utf *rhs_utf;
		lhs_utf = lhs->s_data;
		rhs_utf = rhs->s_data;
		ASSERT(lhs_utf != NULL);
		ASSERT(rhs_utf != NULL);
		ASSERT(lhs_utf->u_width != STRING_WIDTH_1BYTE);
		ASSERT(rhs_utf->u_width != STRING_WIDTH_1BYTE);
		/* Complex string comparison. */
		switch (lhs_utf->u_width) {

		CASE_WIDTH_2BYTE: {
			uint16_t *lhs_str;
			lhs_str = (uint16_t *)lhs_utf->u_data[lhs_utf->u_width];
			lhs_len = WSTR_LENGTH(lhs_str);
			switch (rhs_utf->u_width) {

			CASE_WIDTH_2BYTE: {
				int result;
				uint16_t *rhs_str;
				size_t common_len;
				rhs_str    = (uint16_t *)rhs_utf->u_data[rhs_utf->u_width];
				rhs_len    = WSTR_LENGTH(rhs_str);
				common_len = MIN(lhs_len, rhs_len);
				result     = memcmpw(lhs_str, rhs_str, common_len);
				if (result != 0)
					return fix_memcmp_return(result);
			}	break;

			CASE_WIDTH_4BYTE: {
				uint32_t *rhs_str;
				size_t i, common_len;
				rhs_str    = (uint32_t *)rhs_utf->u_data[rhs_utf->u_width];
				rhs_len    = WSTR_LENGTH(rhs_str);
				common_len = MIN(lhs_len, rhs_len);
				for (i = 0; i < common_len; ++i) {
					if ((uint32_t)(lhs_str[i]) == rhs_str[i])
						continue;
					return (uint32_t)(lhs_str[i]) < rhs_str[i] ? -1 : 1;
				}
			}	break;

			default: __builtin_unreachable();
			}
		}	break;

		CASE_WIDTH_4BYTE: {
			uint32_t *lhs_str;
			lhs_str = (uint32_t *)lhs_utf->u_data[lhs_utf->u_width];
			lhs_len = WSTR_LENGTH(lhs_str);
			switch (rhs_utf->u_width) {

			CASE_WIDTH_2BYTE: {
				uint16_t *rhs_str;
				size_t i, common_len;
				rhs_str    = (uint16_t *)rhs_utf->u_data[rhs_utf->u_width];
				rhs_len    = WSTR_LENGTH(rhs_str);
				common_len = MIN(lhs_len, rhs_len);
				for (i = 0; i < common_len; ++i) {
					if (lhs_str[i] == (uint32_t)(rhs_str[i]))
						continue;
					return lhs_str[i] < (uint32_t)(rhs_str[i]) ? -1 : 1;
				}
			}	break;

			CASE_WIDTH_4BYTE: {
				int result;
				uint32_t *rhs_str;
				size_t common_len;
				rhs_str    = (uint32_t *)rhs_utf->u_data[rhs_utf->u_width];
				rhs_len    = WSTR_LENGTH(rhs_str);
				common_len = MIN(lhs_len, rhs_len);
				result     = memcmpl(lhs_str, rhs_str, common_len);
				if (result != 0)
					return fix_memcmp_return(result);
			}	break;

			default: __builtin_unreachable();
			}
		}	break;

		default: __builtin_unreachable();
		}
	}

	/* If string contents are identical, leave off by comparing their lengths. */
	if (lhs_len == rhs_len)
		return 0;
	if (lhs_len < rhs_len)
		return -1;
	return 1;
}


INTERN WUNUSED NONNULL((1, 2)) int DCALL
string_compare(String *lhs, DeeObject *rhs) {
	int result;
	if (DeeBytes_Check(rhs)) {
		result = compare_string_bytes(lhs, (DeeBytesObject *)rhs);
	} else {
		if (DeeObject_AssertTypeExact(rhs, &DeeString_Type))
			goto err;
		result = compare_strings(lhs, (String *)rhs);
	}
	return result;
err:
	return -2;
}

INTERN WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
string_lo(String *self, DeeObject *some_object) {
	int result = string_compare(self, some_object);
	if unlikely(result == -2)
		goto err;
	return_bool_(result < 0);
err:
	return NULL;
}

PRIVATE WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
string_le(String *self, DeeObject *some_object) {
	int result = string_compare(self, some_object);
	if unlikely(result == -2)
		goto err;
	return_bool_(result <= 0);
err:
	return NULL;
}

PRIVATE WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
string_gr(String *self, DeeObject *some_object) {
	int result = string_compare(self, some_object);
	if unlikely(result == -2)
		goto err;
	return_bool_(result > 0);
err:
	return NULL;
}

PRIVATE WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
string_ge(String *self, DeeObject *some_object) {
	int result = string_compare(self, some_object);
	if unlikely(result == -2)
		goto err;
	return_bool_(result >= 0);
err:
	return NULL;
}


INTDEF WUNUSED NONNULL((1, 2)) bool DCALL
string_eq_bytes(String *__restrict self,
                DeeBytesObject *__restrict other);

PRIVATE WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
string_eq(String *self, DeeObject *some_object) {
	/* Basic checks for same-object. */
	if (self == (String *)some_object)
		return_true;
	if (DeeBytes_Check(some_object))
		return_bool(string_eq_bytes(self, (DeeBytesObject *)some_object));
	if (DeeObject_AssertTypeExact(some_object, &DeeString_Type))
		goto err;
	if (DeeString_Hash((DeeObject *)self) !=
	    DeeString_Hash((DeeObject *)some_object))
		return_false;
	return_bool(compare_strings(self, (String *)some_object) == 0);
err:
	return NULL;
}

PRIVATE WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
string_ne(String *self, DeeObject *some_object) {
	/* Basic checks for same-object. */
	if (self == (String *)some_object)
		return_false;
	if (DeeBytes_Check(some_object))
		return_bool(!string_eq_bytes(self, (DeeBytesObject *)some_object));
	if (DeeObject_AssertTypeExact(some_object, &DeeString_Type))
		goto err;
	if (DeeString_Hash((DeeObject *)self) !=
	    DeeString_Hash((DeeObject *)some_object))
		return_true;
	/* XXX: This could be optimized! */
	return_bool(compare_strings(self, (String *)some_object) != 0);
err:
	return NULL;
}




typedef struct {
	OBJECT_HEAD
	DREF String   *si_string; /* [1..1][const] The string that is being iterated. */
	union dcharptr si_iter;   /* [1..1][weak] The current iterator position. */
	union dcharptr si_end;    /* [1..1][const] The string end pointer. */
	unsigned int   si_width;  /* [const] The stirng width used during iteration (One of `STRING_WIDTH_*'). */
} StringIterator;
#define READ_ITER_PTR(x) atomic_read(&(x)->si_iter.ptr)


PRIVATE NONNULL((1)) void DCALL
stringiter_fini(StringIterator *__restrict self) {
	Dee_Decref(self->si_string);
}

PRIVATE NONNULL((1, 2)) void DCALL
stringiter_visit(StringIterator *__restrict self, dvisit_t proc, void *arg) {
	Dee_Visit(self->si_string);
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
stringiter_next(StringIterator *__restrict self) {
	DREF DeeObject *result;
	union dcharptr pos, new_pos;

	/* Consume one character (atomically) */
	do {
		pos.ptr = atomic_read(&self->si_iter.ptr);
		if (pos.ptr >= self->si_end.ptr)
			return ITER_DONE;
		new_pos.cp8 = pos.cp8 + STRING_SIZEOF_WIDTH(self->si_width);
	} while (!atomic_cmpxch_weak_or_write(&self->si_iter.ptr, pos.ptr, new_pos.ptr));

	/* Create the single-character string. */
	SWITCH_SIZEOF_WIDTH(self->si_width) {

	CASE_WIDTH_1BYTE:
		result = DeeString_Chr(*pos.cp8);
		break;

	CASE_WIDTH_2BYTE:
		result = DeeString_Chr(*pos.cp16);
		break;

	CASE_WIDTH_4BYTE:
		result = DeeString_Chr(*pos.cp32);
		break;

	}
	return result;
}

PRIVATE WUNUSED NONNULL((1)) int DCALL
stringiter_bool(StringIterator *__restrict self) {
	return READ_ITER_PTR(self) < self->si_end.ptr;
}

PRIVATE WUNUSED NONNULL((1)) int DCALL
stringiter_ctor(StringIterator *__restrict self) {
	self->si_string   = (DREF String *)Dee_EmptyString;
	self->si_iter.ptr = DeeString_STR(Dee_EmptyString);
	self->si_end.ptr  = DeeString_STR(Dee_EmptyString);
	self->si_width    = STRING_WIDTH_1BYTE;
	Dee_Incref(Dee_EmptyString);
	return 0;
}

PRIVATE WUNUSED NONNULL((1, 2)) int DCALL
stringiter_copy(StringIterator *__restrict self,
                StringIterator *__restrict other) {
	self->si_string   = other->si_string;
	self->si_iter.ptr = READ_ITER_PTR(other);
	self->si_end      = other->si_end;
	self->si_width    = other->si_width;
	Dee_Incref(self->si_string);
	return 0;
}

PRIVATE WUNUSED NONNULL((1)) int DCALL
stringiter_init(StringIterator *__restrict self,
                size_t argc, DeeObject *const *argv) {
	String *str;
	if (DeeArg_Unpack(argc, argv, "o:string.Iterator", &str))
		goto err;
	if (DeeObject_AssertTypeExact(str, &DeeString_Type))
		goto err;
	self->si_string = str;
	Dee_Incref(str);
	self->si_width    = DeeString_WIDTH(str);
	self->si_iter.ptr = DeeString_WSTR(str);
	self->si_end.ptr = (void *)((uintptr_t)self->si_iter.ptr +
	                            WSTR_LENGTH(self->si_iter.ptr) *
	                            STRING_SIZEOF_WIDTH(self->si_width));
	return 0;
err:
	return -1;
}

PRIVATE struct type_member tpconst stringiter_members[] = {
	TYPE_MEMBER_FIELD_DOC(STR_seq, STRUCT_OBJECT, offsetof(StringIterator, si_string), "->?Dstring"),
	TYPE_MEMBER_FIELD("__width__", STRUCT_CONST | STRUCT_INT, offsetof(StringIterator, si_width)),
	TYPE_MEMBER_END
};

INTDEF DeeTypeObject StringIterator_Type;

#define DEFINE_STRINGITER_COMPARE(name, op)                                      \
	PRIVATE WUNUSED DREF DeeObject *DCALL                                                \
	name(StringIterator *__restrict self,                                        \
	     StringIterator *__restrict other) {                                     \
		if (DeeObject_AssertTypeExact(other, &StringIterator_Type)) \
			goto err;                                                            \
		return_bool(READ_ITER_PTR(self) op READ_ITER_PTR(other));                \
	err:                                                                         \
		return NULL;                                                             \
	}
DEFINE_STRINGITER_COMPARE(stringiter_eq, ==)
DEFINE_STRINGITER_COMPARE(stringiter_ne, !=)
DEFINE_STRINGITER_COMPARE(stringiter_lo, <)
DEFINE_STRINGITER_COMPARE(stringiter_le, <=)
DEFINE_STRINGITER_COMPARE(stringiter_gr, >)
DEFINE_STRINGITER_COMPARE(stringiter_ge, >=)
#undef DEFINE_STRINGITER_COMPARE

PRIVATE WUNUSED NONNULL((1)) DREF String *DCALL
stringiter_nii_getseq(StringIterator *__restrict self) {
	return_reference_(self->si_string);
}

PRIVATE WUNUSED NONNULL((1)) int DCALL
stringiter_nii_hasprev(StringIterator *__restrict self) {
	return READ_ITER_PTR(self) > (void *)DeeString_WSTR(self->si_string);
}

PRIVATE WUNUSED NONNULL((1)) size_t DCALL
stringiter_nii_getindex(StringIterator *__restrict self) {
	union dcharptr pos;
	pos.ptr = READ_ITER_PTR(self);
	return (size_t)(pos.cp8 - (uint8_t *)DeeString_WSTR(self->si_string)) /
	       STRING_SIZEOF_WIDTH(self->si_width);
}

PRIVATE WUNUSED NONNULL((1)) int DCALL
stringiter_nii_setindex(StringIterator *__restrict self, size_t index) {
	if (index > DeeString_WLEN(self->si_string))
		index = DeeString_WLEN(self->si_string);
	self->si_iter.cp8 = (uint8_t *)DeeString_WSTR(self->si_string) +
	                    (index * STRING_SIZEOF_WIDTH(self->si_width));
	return 0;
}

PRIVATE WUNUSED NONNULL((1)) int DCALL
stringiter_nii_rewind(StringIterator *__restrict self) {
	atomic_write(&self->si_iter.ptr,
	             DeeString_WSTR(self->si_string));
	return 0;
}

PRIVATE WUNUSED NONNULL((1)) int DCALL
stringiter_nii_prev(StringIterator *__restrict self) {
	union dcharptr pos, new_pos;
	do {
		pos.ptr = atomic_read(&self->si_iter.ptr);
		if (pos.ptr <= (void *)DeeString_WSTR(self->si_string))
			return 1;
		new_pos.cp8 = pos.cp8 - STRING_SIZEOF_WIDTH(self->si_width);
	} while (!atomic_cmpxch_weak_or_write(&self->si_iter.ptr, pos.ptr, new_pos.ptr));
	return 0;
}

PRIVATE WUNUSED NONNULL((1)) int DCALL
stringiter_nii_next(StringIterator *__restrict self) {
	union dcharptr pos, new_pos;

	/* Consume one character (atomically) */
	do {
		pos.ptr = atomic_read(&self->si_iter.ptr);
		if (pos.ptr >= self->si_end.ptr)
			return 1;
		new_pos.cp8 = pos.cp8 + STRING_SIZEOF_WIDTH(self->si_width);
	} while (!atomic_cmpxch_weak_or_write(&self->si_iter.ptr, pos.ptr, new_pos.ptr));
	return 0;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
stringiter_nii_peek(StringIterator *__restrict self) {
	DREF DeeObject *result;
	union dcharptr pos;
	pos.ptr = self->si_iter.ptr;
	if (pos.ptr >= self->si_end.ptr)
		return ITER_DONE;

	/* Create the single-character string. */
	SWITCH_SIZEOF_WIDTH(self->si_width) {

	CASE_WIDTH_1BYTE:
		result = DeeString_Chr(*pos.cp8);
		break;

	CASE_WIDTH_2BYTE:
		result = DeeString_Chr(*pos.cp16);
		break;

	CASE_WIDTH_4BYTE:
		result = DeeString_Chr(*pos.cp32);
		break;

	}
	return result;
}

PRIVATE struct type_nii tpconst stringiter_nii = {
	/* .nii_class = */ TYPE_ITERX_CLASS_BIDIRECTIONAL,
	/* .nii_flags = */ TYPE_ITERX_FNORMAL,
	{
		/* .nii_common = */ {
			/* .nii_getseq   = */ (dfunptr_t)&stringiter_nii_getseq,
			/* .nii_getindex = */ (dfunptr_t)&stringiter_nii_getindex,
			/* .nii_setindex = */ (dfunptr_t)&stringiter_nii_setindex,
			/* .nii_rewind   = */ (dfunptr_t)&stringiter_nii_rewind,
			/* .nii_revert   = */ (dfunptr_t)NULL, //TODO:&stringiter_nii_revert,
			/* .nii_advance  = */ (dfunptr_t)NULL, //TODO:&stringiter_nii_advance,
			/* .nii_prev     = */ (dfunptr_t)&stringiter_nii_prev,
			/* .nii_next     = */ (dfunptr_t)&stringiter_nii_next,
			/* .nii_hasprev  = */ (dfunptr_t)&stringiter_nii_hasprev,
			/* .nii_peek     = */ (dfunptr_t)&stringiter_nii_peek
		}
	}
};


PRIVATE struct type_cmp stringiter_cmp = {
	/* .tp_hash = */ NULL,
	/* .tp_eq   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&stringiter_eq,
	/* .tp_ne   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&stringiter_ne,
	/* .tp_lo   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&stringiter_lo,
	/* .tp_le   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&stringiter_le,
	/* .tp_gr   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&stringiter_gr,
	/* .tp_ge   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&stringiter_ge,
	/* .tp_nii  = */ &stringiter_nii
};

INTERN DeeTypeObject StringIterator_Type = {
	OBJECT_HEAD_INIT(&DeeType_Type),
	/* .tp_name     = */ "_StringIterator",
	/* .tp_doc      = */ DOC("(seq?:?Dstring)"),
	/* .tp_flags    = */ TP_FNORMAL,
	/* .tp_weakrefs = */ 0,
	/* .tp_features = */ TF_NONLOOPING,
	/* .tp_base     = */ &DeeIterator_Type,
	/* .tp_init = */ {
		{
			/* .tp_alloc = */ {
				/* .tp_ctor      = */ (dfunptr_t)&stringiter_ctor,
				/* .tp_copy_ctor = */ (dfunptr_t)&stringiter_copy,
				/* .tp_deep_ctor = */ (dfunptr_t)&stringiter_copy,
				/* .tp_any_ctor  = */ (dfunptr_t)&stringiter_init,
				TYPE_FIXED_ALLOCATOR(StringIterator)
			}
		},
		/* .tp_dtor        = */ (void (DCALL *)(DeeObject *__restrict))&stringiter_fini,
		/* .tp_assign      = */ NULL, /* TODO */
		/* .tp_move_assign = */ NULL
	},
	/* .tp_cast = */ {
		/* .tp_str  = */ NULL,
		/* .tp_repr = */ NULL,
		/* .tp_bool = */ (int (DCALL *)(DeeObject *__restrict))&stringiter_bool
	},
	/* .tp_call          = */ NULL,
	/* .tp_visit         = */ (void (DCALL *)(DeeObject *__restrict, dvisit_t, void *))&stringiter_visit,
	/* .tp_gc            = */ NULL,
	/* .tp_math          = */ NULL,
	/* .tp_cmp           = */ &stringiter_cmp,
	/* .tp_seq           = */ NULL,
	/* .tp_iter_next     = */ (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&stringiter_next,
	/* .tp_attr          = */ NULL,
	/* .tp_with          = */ NULL,
	/* .tp_buffer        = */ NULL,
	/* .tp_methods       = */ NULL,
	/* .tp_getsets       = */ NULL,
	/* .tp_members       = */ stringiter_members,
	/* .tp_class_methods = */ NULL,
	/* .tp_class_getsets = */ NULL,
	/* .tp_class_members = */ NULL
};



PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_iter(String *__restrict self) {
	DREF StringIterator *result;
	result = DeeObject_MALLOC(StringIterator);
	if unlikely(!result)
		goto done;
	DeeObject_Init(result, &StringIterator_Type);
	result->si_string = self;
	Dee_Incref(self);
	result->si_width    = DeeString_WIDTH(self);
	result->si_iter.ptr = DeeString_WSTR(self);
	result->si_end.ptr = (void *)((uintptr_t)result->si_iter.ptr +
	                              WSTR_LENGTH(result->si_iter.ptr) *
	                              STRING_SIZEOF_WIDTH(result->si_width));
done:
	return (DREF DeeObject *)result;
}

INTDEF WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
string_contains(String *self,
                DeeObject *some_object);

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_size(String *__restrict self) {
	size_t result = DeeString_WLEN(self);
	return DeeInt_NewSize(result);
}

PRIVATE WUNUSED NONNULL((1, 2)) DREF DeeObject *DCALL
string_get(String *self,
           DeeObject *index) {
	int width = DeeString_WIDTH(self);
	union dcharptr str;
	size_t i, len;
	str.ptr = DeeString_WSTR(self);
	len     = WSTR_LENGTH(str.ptr);
	if (DeeObject_AsSize(index, &i))
		goto err;
	if unlikely((size_t)i >= len)
		goto err_bound;
	SWITCH_SIZEOF_WIDTH(width) {

	CASE_WIDTH_1BYTE:
		return DeeString_Chr(str.cp8[i]);

	CASE_WIDTH_2BYTE:
		return DeeString_Chr(str.cp16[i]);

	CASE_WIDTH_4BYTE:
		return DeeString_Chr(str.cp32[i]);

	}
err_bound:
	err_index_out_of_bounds((DeeObject *)self, i, len);
err:
	return NULL;
}

INTERN WUNUSED NONNULL((1, 2, 3)) DREF DeeObject *DCALL
string_range_get(String *__restrict self,
                 DeeObject *__restrict begin,
                 DeeObject *__restrict end) {
	void *str  = DeeString_WSTR(self);
	int width  = DeeString_WIDTH(self);
	size_t len = WSTR_LENGTH(str);
	dssize_t i_begin, i_end = (dssize_t)len;
	if (DeeObject_AsSSize(begin, &i_begin))
		goto err;
	if (!DeeNone_Check(end)) {
		if (DeeObject_AsSSize(end, &i_end))
			goto err;
	}
	if (i_begin < 0)
		i_begin += len;
	if (i_end < 0)
		i_end += len;
	if unlikely((size_t)i_begin >= len ||
	            (size_t)i_begin >= (size_t)i_end)
		return_empty_string;
	if unlikely((size_t)i_end > len)
		i_end = (dssize_t)len;
	return DeeString_NewWithWidth((uint8_t *)str +
	                              ((size_t)i_begin * STRING_SIZEOF_WIDTH(width)),
	                              (size_t)i_end - (size_t)i_begin,
	                              width);
err:
	return NULL;
}


PRIVATE struct type_cmp string_cmp = {
	/* .tp_hash = */ &DeeString_Hash,
	/* .tp_eq   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&string_eq,
	/* .tp_ne   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&string_ne,
	/* .tp_lo   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&string_lo,
	/* .tp_le   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&string_le,
	/* .tp_gr   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&string_gr,
	/* .tp_ge   = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&string_ge,
};

PRIVATE WUNUSED NONNULL((1)) size_t DCALL
string_nsi_getsize(String *__restrict self) {
	ASSERT(DeeString_WLEN(self) != (size_t)-1);
	return DeeString_WLEN(self);
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_nsi_getitem(String *__restrict self, size_t index) {
	if unlikely(index >= DeeString_WLEN(self))
		goto err_index;
	return DeeString_Chr(DeeString_GetChar(self, index));
err_index:
	err_index_out_of_bounds((DeeObject *)self, index, DeeString_WLEN(self));
	return NULL;
}

PRIVATE struct type_nsi tpconst string_nsi = {
	/* .nsi_class   = */ TYPE_SEQX_CLASS_SEQ,
	/* .nsi_flags   = */ TYPE_SEQX_FNORMAL,
	{
		/* .nsi_seqlike = */ {
			/* .nsi_getsize      = */ (dfunptr_t)&string_nsi_getsize,
			/* .nsi_getsize_fast = */ (dfunptr_t)&string_nsi_getsize,
			/* .nsi_getitem      = */ (dfunptr_t)&string_nsi_getitem,
			/* .nsi_delitem      = */ (dfunptr_t)NULL,
			/* .nsi_setitem      = */ (dfunptr_t)NULL,
			/* .nsi_getitem_fast = */ (dfunptr_t)NULL,
			/* .nsi_getrange     = */ (dfunptr_t)NULL,
			/* .nsi_getrange_n   = */ (dfunptr_t)NULL,
			/* .nsi_setrange     = */ (dfunptr_t)NULL,
			/* .nsi_setrange_n   = */ (dfunptr_t)NULL,
			/* .nsi_find         = */ (dfunptr_t)NULL,
			/* .nsi_rfind        = */ (dfunptr_t)NULL,
			/* .nsi_xch          = */ (dfunptr_t)NULL,
			/* .nsi_insert       = */ (dfunptr_t)NULL,
			/* .nsi_insertall    = */ (dfunptr_t)NULL,
			/* .nsi_insertvec    = */ (dfunptr_t)NULL,
			/* .nsi_pop          = */ (dfunptr_t)NULL,
			/* .nsi_erase        = */ (dfunptr_t)NULL,
			/* .nsi_remove       = */ (dfunptr_t)NULL,
			/* .nsi_rremove      = */ (dfunptr_t)NULL,
			/* .nsi_removeall    = */ (dfunptr_t)NULL,
			/* .nsi_removeif     = */ (dfunptr_t)NULL
		}
	}
};

PRIVATE struct type_seq string_seq = {
	/* .tp_iter_self = */ (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&string_iter,
	/* .tp_size      = */ (DREF DeeObject *(DCALL *)(DeeObject *__restrict))&string_size,
	/* .tp_contains  = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&string_contains,
	/* .tp_get       = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *))&string_get,
	/* .tp_del       = */ NULL,
	/* .tp_set       = */ NULL,
	/* .tp_range_get = */ (DREF DeeObject *(DCALL *)(DeeObject *, DeeObject *, DeeObject *))&string_range_get,
	/* .tp_range_del = */ NULL,
	/* .tp_range_set = */ NULL,
	/* .tp_nsi       = */ &string_nsi
};

PRIVATE struct type_member tpconst string_class_members[] = {
	TYPE_MEMBER_CONST(STR_Iterator, &StringIterator_Type),
	TYPE_MEMBER_END
};

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_class_chr(DeeObject *UNUSED(self),
                 size_t argc, DeeObject *const *argv) {
	uint32_t ch;
	if (DeeArg_Unpack(argc, argv, UNPu32 ":chr", &ch))
		goto err;
	return DeeString_Chr(ch);
err:
	return NULL;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_class_fromseq(DeeObject *UNUSED(self),
                     size_t argc, DeeObject *const *argv) {
	DeeObject *seq;
	if (DeeArg_Unpack(argc, argv, "o:fromseq", &seq))
		goto err;
	/* XXX: Fast-sequence optimizations? */
	{
		struct unicode_printer printer = UNICODE_PRINTER_INIT;
		DREF DeeObject *iter, *elem;
		iter = DeeObject_IterSelf(seq);
		if unlikely(!iter)
			goto err_printer;
		while (ITER_ISOK(elem = DeeObject_IterNext(iter))) {
			uint32_t chr;
			int temp;
			temp = DeeObject_AsUInt32(elem, &chr);
			Dee_Decref(elem);
			if unlikely(temp)
				goto err_iter;
			if unlikely(unicode_printer_putc(&printer, chr))
				goto err_iter;
			if (DeeThread_CheckInterrupt())
				goto err_iter;
		}
		if unlikely(!elem)
			goto err_iter;
		Dee_Decref(iter);
		return unicode_printer_pack(&printer);
err_iter:
		Dee_Decref(iter);
err_printer:
		unicode_printer_fini(&printer);
	}
err:
	return NULL;
}

PRIVATE struct type_method tpconst string_class_methods[] = {
	TYPE_METHOD("chr", &string_class_chr,
	            "(ch:?Dint)->?.\n"
	            "#tIntegerOverflow{@ch is negative or greater than the greatest unicode-character}"
	            "#r{A single-character string matching the unicode-character @ch}"),
	TYPE_METHOD("fromseq", &string_class_fromseq,
	            "(ordinals:?S?Dint)->?.\n"
	            "#tIntegerOverflow{One of the ordinals is negative, or greater than $0xffffffff}"
	            "Construct a new string object from a sequence of ordinal values"),
	TYPE_METHOD_END
};

INTDEF struct type_method tpconst string_methods[];
INTDEF struct type_math string_math;

INTDEF WUNUSED NONNULL((1)) DREF DeeObject *DCALL
DeeString_Ordinals(DeeObject *__restrict self);

INTERN WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_hashed(String *__restrict self) {
	return_bool(self->s_hash != DEE_STRING_HASH_UNSET);
}

INTERN WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_hasutf(String *__restrict self) {
	return_bool(atomic_read(&self->s_data) != NULL);
}

INTERN WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_hasregex(String *__restrict self) {
	struct string_utf *utf;
	utf = atomic_read(&self->s_data);
	if (utf == NULL)
		return_false;
	return_bool((atomic_read(&utf->u_flags) & STRING_UTF_FREGEX) != 0);
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_getfirst(String *__restrict self) {
	void *str = DeeString_WSTR(self);
	int width = DeeString_WIDTH(self);
	if unlikely(!WSTR_LENGTH(str))
		goto err_empty;
	return DeeString_Chr(STRING_WIDTH_GETCHAR(width, str, 0));
err_empty:
	err_empty_sequence((DeeObject *)self);
	return NULL;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_getlast(String *__restrict self) {
	void *str     = DeeString_WSTR(self);
	int width     = DeeString_WIDTH(self);
	size_t length = WSTR_LENGTH(str);
	if unlikely(!length)
		goto err_empty;
	return DeeString_Chr(STRING_WIDTH_GETCHAR(width, str, length - 1));
err_empty:
	err_empty_sequence((DeeObject *)self);
	return NULL;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_sizeof(String *__restrict self) {
	size_t result;
	struct string_utf *utf;
	result = offsetof(String, s_str);
	result += (self->s_len + 1) * sizeof(char);
	utf = self->s_data;
	if (utf) {
		unsigned int i;
		result += sizeof(struct string_utf);
		for (i = 1; i < STRING_WIDTH_COUNT; ++i) {
			if (!utf->u_data[i])
				continue;
			result += STRING_MUL_SIZEOF_WIDTH(WSTR_LENGTH(utf->u_data[i]), i);
		}
		if (utf->u_data[STRING_WIDTH_1BYTE] &&
		    utf->u_data[STRING_WIDTH_1BYTE] != (size_t *)DeeString_STR(self))
			result += WSTR_LENGTH(utf->u_data[STRING_WIDTH_1BYTE]) * 1;
		if (utf->u_utf8 && utf->u_utf8 != (char *)DeeString_STR(self) &&
		    utf->u_utf8 != (char *)utf->u_data[STRING_WIDTH_1BYTE])
			result += WSTR_LENGTH(utf->u_utf8) * 1;
		if (utf->u_utf16 && (uint16_t *)utf->u_utf16 != (uint16_t *)utf->u_data[STRING_WIDTH_2BYTE])
			result += WSTR_LENGTH(utf->u_utf16) * 2;
	}
	return DeeInt_NewSize(result);
}


/* Expose auditing internals for `deemon.string' */
#undef CONFIG_HAVE_STRING_AUDITING_INTERNALS
#if 1
#define CONFIG_HAVE_STRING_AUDITING_INTERNALS
#endif

#ifdef CONFIG_HAVE_STRING_AUDITING_INTERNALS
PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_str_bytes(String *__restrict self) {
	return DeeBytes_NewView((DeeObject *)self,
	                        DeeString_STR(self),
	                        DeeString_SIZE(self) * sizeof(char),
	                        Dee_BUFFER_FREADONLY);
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_str_bytes_isutf8(String *__restrict self) {
	return_bool(DeeString_STR_ISUTF8(self));
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_str_bytes_islatin1(String *__restrict self) {
	return_bool(DeeString_STR_ISLATIN1(self));
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_str_width(String *__restrict self) {
	int width   = DeeString_WIDTH(self);
	size_t size = STRING_SIZEOF_WIDTH(width);
	return DeeInt_NewSize(size);
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_wstr_bytes(String *__restrict self) {
	return DeeBytes_NewView((DeeObject *)self,
	                        DeeString_WSTR(self),
	                        DeeString_WSIZ(self),
	                        Dee_BUFFER_FREADONLY);
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_utf8_bytes(String *__restrict self) {
	char *utf8 = DeeString_AsUtf8((DeeObject *)self);
	if unlikely(!utf8)
		goto err;
	return DeeBytes_NewView((DeeObject *)self,
	                        utf8, WSTR_LENGTH(utf8),
	                        Dee_BUFFER_FREADONLY);
err:
	return NULL;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_utf16_bytes(String *__restrict self) {
	uint16_t *utf16 = DeeString_AsUtf16((DeeObject *)self, STRING_ERROR_FSTRICT);
	if unlikely(!utf16)
		goto err;
	return DeeBytes_NewView((DeeObject *)self,
	                        utf16, WSTR_LENGTH(utf16) << 1,
	                        Dee_BUFFER_FREADONLY);
err:
	return NULL;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_utf32_bytes(String *__restrict self) {
	uint32_t *utf32 = DeeString_AsUtf32((DeeObject *)self);
	if unlikely(!utf32)
		goto err;
	return DeeBytes_NewView((DeeObject *)self,
	                        utf32, WSTR_LENGTH(utf32) << 2,
	                        Dee_BUFFER_FREADONLY);
err:
	return NULL;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_1byte_bytes(String *__restrict self) {
	uint8_t *b1;
	if (self->s_data && self->s_data->u_width != Dee_STRING_WIDTH_1BYTE)
		goto err_too_large;
	b1 = DeeString_As1Byte((DeeObject *)self);
	return DeeBytes_NewView((DeeObject *)self,
	                        b1, WSTR_LENGTH(b1),
	                        Dee_BUFFER_FREADONLY);
err_too_large:
	DeeError_Throwf(&DeeError_UnicodeEncodeError,
	                "String %r contains ordinals greater than U+00FF",
	                self);
	return NULL;
}

PRIVATE WUNUSED NONNULL((1)) DREF DeeObject *DCALL
string_audit_2byte_bytes(String *__restrict self) {
	uint16_t *b2;
	if (self->s_data && self->s_data->u_width > Dee_STRING_WIDTH_2BYTE)
		goto err_too_large;
	b2 = DeeString_As2Byte((DeeObject *)self);
	return DeeBytes_NewView((DeeObject *)self,
	                        b2, WSTR_LENGTH(b2) << 1,
	                        Dee_BUFFER_FREADONLY);
err_too_large:
	DeeError_Throwf(&DeeError_UnicodeEncodeError,
	                "String %r contains ordinals greater than U+FFFF",
	                self);
	return NULL;
}

#define string_audit_4byte_bytes string_audit_utf32_bytes
#endif /* CONFIG_HAVE_STRING_AUDITING_INTERNALS */


PRIVATE struct type_getset tpconst string_getsets[] = {
	TYPE_GETTER("ordinals", &DeeString_Ordinals,
	            "->?S?Dint\n"
	            "Returns a proxy view for the characters of @this ?. as a sequence of "
	            /**/ "integers referring to the ordinal values of each character (s.a. ?#ord)"),
	TYPE_GETTER("__hashed__", &string_hashed,
	            "->?Dbool\n"
	            "Evaluates to ?t if @this ?. has been hashed"),
	TYPE_GETTER("__hasutf__", &string_hasutf,
	            "->?Dbool\n"
	            "Evaluates to ?t if @this ?. owns a UTF container"),
	TYPE_GETTER("__hasregex__", &string_hasregex,
	            "->?Dbool\n"
	            "Evaluates to ?t if @this ?. has been compiled as a regex pattern in the past"),
	TYPE_GETTER(STR_first, &string_getfirst,
	            "->?.\n"
	            "#tValueError{@this ?. is empty}"
	            "Returns the first character of @this ?."),
	TYPE_GETTER(STR_last, &string_getlast,
	            "->?.\n"
	            "#tValueError{@this ?. is empty}"
	            "Returns the last character of @this ?."),
	TYPE_GETTER("__sizeof__", &string_sizeof, "->?Dint"),

#ifdef CONFIG_HAVE_STRING_AUDITING_INTERNALS
	TYPE_GETTER("__str_bytes__", &string_audit_str_bytes,
	            "->?DBytes\n"
	            "Internal function to view the bytes of ${DeeString_STR()}"),
	TYPE_GETTER("__str_bytes_isutf8__", &string_audit_str_bytes_isutf8,
	            "->?Dbool\n"
	            "Value of ${DeeString_STR_ISUTF8()}"),
	TYPE_GETTER("__str_bytes_islatin1__", &string_audit_str_bytes_islatin1,
	            "->?Dbool\n"
	            "Value of ${DeeString_STR_ISLATIN1()}"),
	TYPE_GETTER("__str_width__", &string_audit_str_width,
	            "->?Dint\n"
	            "Returns $1, $2 or $4 based on ${DeeString_WIDTH()}"),
	TYPE_GETTER("__wstr_bytes__", &string_audit_wstr_bytes,
	            "->?DBytes\n"
	            "Internal function to view the bytes of ${DeeString_WSTR()}"),
	TYPE_GETTER("__utf8_bytes__", &string_audit_utf8_bytes,
	            "->?DBytes\n"
	            "Internal function to view the bytes of ${DeeString_AsUtf8()}"),
	TYPE_GETTER("__utf16_bytes__", &string_audit_utf16_bytes,
	            "->?DBytes\n"
	            "#tUnicodeEncodeError{@this ?Dstring contains ordinals that can't be encoded as utf-16}"
	            "Internal function to view the bytes of ${DeeString_AsUtf16()}"),
	TYPE_GETTER("__utf32_bytes__", &string_audit_utf32_bytes,
	            "->?DBytes\n"
	            "Internal function to view the bytes of ${DeeString_AsUtf32()}"),
	TYPE_GETTER("__1byte_bytes__", &string_audit_1byte_bytes,
	            "->?DBytes\n"
	            "#tUnicodeEncodeError{@this ?Dstring contains ordinals greater than $0xff}"
	            "Internal function to view the bytes of ${DeeString_As1Byte()}"),
	TYPE_GETTER("__2byte_bytes__", &string_audit_2byte_bytes,
	            "->?DBytes\n"
	            "#tUnicodeEncodeError{@this ?Dstring contains ordinals greater than $0xffff}"
	            "Internal function to view the bytes of ${DeeString_As2Byte()}"),
	TYPE_GETTER("__4byte_bytes__", &string_audit_4byte_bytes,
	            "->?DBytes\n"
	            "Internal function to view the bytes of ${DeeString_As4Byte()}"),
#endif /* CONFIG_HAVE_STRING_AUDITING_INTERNALS */
	TYPE_GETSET_END
};


PRIVATE int DCALL
string_getbuf(String *__restrict self,
              DeeBuffer *__restrict info,
              unsigned int UNUSED(flags)) {
	info->bb_base = DeeString_AsBytes((DeeObject *)self, false);
	if unlikely(!info->bb_base)
		goto err;
	info->bb_size = WSTR_LENGTH(info->bb_base);
	return 0;
err:
	return -1;
}

PRIVATE struct type_buffer string_buffer = {
	/* .tp_getbuf       = */ (int (DCALL *)(DeeObject *__restrict, DeeBuffer *__restrict, unsigned int))&string_getbuf,
	/* .tp_putbuf       = */ NULL,
	/* .tp_buffer_flags = */ Dee_BUFFER_TYPE_FREADONLY
};


/* `string from deemon' */
PUBLIC DeeTypeObject DeeString_Type = {
	OBJECT_HEAD_INIT(&DeeType_Type),
	/* .tp_name     = */ DeeString_STR(&str_string),
	/* .tp_doc      = */ DOC("An encoding-neutral, immutable sequence of characters\n"
	                         "\n"

	                         "()\n"
	                         "Returns an empty string $\"\"\n"
	                         "\n"

	                         "(ob)\n"
	                         "Same as ${str ob}, returning the string representation of @ob\n"
	                         "\n"

	                         "str->\n"
	                         "Simply re-return @this ?.\n"
	                         "\n"

	                         "repr->\n"
	                         "Returns @this ?. as a C-style escaped string\n"
	                         "${"
	                         /**/ "operator repr() {\n"
	                         /**/ "	return \"\\\"{}\\\"\".format({ this.encode(\"c-escape\") });\n"
	                         /**/ "}"
	                         "}\n"
	                         "\n"

	                         "bool->\n"
	                         "Returns ?t if @this ?. is non-empty\n"
	                         "${"
	                         /**/ "operator bool() {\n"
	                         /**/ "	return ##this != 0;\n"
	                         /**/ "}"
	                         "}\n"
	                         "\n"

	                         "+(other:?X3?.?DBytes?O)->\n"
	                         "Return a new string that is the concatenation of @this and ${str other}\n"
	                         "${"
	                         /**/ "operator + (other) {\n"
	                         /**/ "	return \"{}{}\".format({ this, other });\n"
	                         /**/ "}"
	                         "}\n"
	                         "\n"

	                         "*(times:?Dint)->\n"
	                         "#tIntegerOverflow{@times is negative, or too large}"
	                         "Returns @this ?. repeated @times number of times\n"
	                         "\n"

	                         "%(args:?DTuple)->\n"
	                         "%(arg:?O)->\n"
	                         "Using @this ?. as a printf-style format string, use a tuple found "
	                         /**/ "in @args to format it into a new string, which is then returned\n"
	                         "If @arg isn't a tuple, it is packed into one and the call is identical "
	                         /**/ "to ${this.operator % (pack(arg))}\n"
	                         "${"
	                         /**/ "local x = 42;\n"
	                         /**/ "print \"x = %d\" % x; /* \"x = 42\" */"
	                         "}\n"
	                         "\n"

	                         "<(other:?X2?.?DBytes)->\n"
	                         "<=(other:?X2?.?DBytes)->\n"
	                         "==(other:?X2?.?DBytes)->\n"
	                         "!=(other:?X2?.?DBytes)->\n"
	                         ">(other:?X2?.?DBytes)->\n"
	                         ">=(other:?X2?.?DBytes)->\n"
	                         "Perform a lexicographical comparison between @this ?. "
	                         /**/ "and @other, and return the result\n"
	                         "\n"

	                         "iter->\n"
	                         "Return a string iterator that can be used to enumerate each of "
	                         /**/ "the string's characters individually\n"
	                         "\n"

	                         "#->\n"
	                         "Returns the length of @this ?. in characters\n"
	                         "\n"

	                         "contains(substr:?X2?.?DBytes)->\n"
	                         "Returns ?t if @substr is apart of @this ?.\n"
	                         "${"
	                         /**/ "print \"foo\" in \"bar\";    /* false */\n"
	                         /**/ "print \"foo\" in \"foobar\"; /* true */"
	                         "}\n"
	                         "\n"

	                         "[]->?.\n"
	                         "#tIntegerOverflow{@index is negative}"
	                         "#tIndexError{@index is greater than ${##this}}"
	                         "Returns the @{index}th character of @this ?.\n"
	                         "${"
	                         /**/ "print \"foo\"[0]; /* \"f\" */\n"
	                         /**/ "print \"foo\"[1]; /* \"o\" */"
	                         "}\n"
	                         "\n"

	                         "[:]->?.\n"
	                         "Return a sub-string of @this, that starts at @start and ends at @end\n"
	                         "If @end is greater than ${##this}, it is truncated to that value\n"
	                         "If @start is greater than, or equal to @end, an empty string is returned\n"
	                         "If either @start or @end is negative, ${##this} is added before "
	                         /**/ "further index transformations are performed\n"
	                         "As per convention, ?N may be passed for @end as an alias for ${##this}\n"
	                         "${"
	                         /**/ "print \"foo\"[:-1];      /* \"fo\" */\n"
	                         /**/ "print \"bar\"[1:];       /* \"ar\" */\n"
	                         /**/ "print \"foobar\"[3:123]; /* \"bar\" */\n"
	                         /**/ "print \"bizbuz\"[5:4];   /* \"\" */"
	                         "}"),
	/* .tp_flags    = */ TP_FNORMAL | TP_FVARIABLE | TP_FNAMEOBJECT | TP_FFINAL,
	/* .tp_weakrefs = */ 0,
	/* .tp_features = */ TF_NONE,
	/* .tp_base     = */ &DeeSeq_Type,
	/* .tp_init = */ {
		{
			/* .tp_var = */ {
				/* .tp_ctor      = */ (dfunptr_t)&string_new_empty,
				/* .tp_copy_ctor = */ (dfunptr_t)&DeeObject_NewRef,
				/* .tp_deep_ctor = */ (dfunptr_t)&DeeObject_NewRef,
				/* .tp_any_ctor  = */ (dfunptr_t)&string_new
			}
		},
		/* .tp_dtor        = */ (void (DCALL *)(DeeObject *__restrict))&string_fini,
		/* .tp_assign      = */ NULL,
		/* .tp_move_assign = */ NULL
	},
	/* .tp_cast = */ {
		/* .tp_str       = */ &DeeObject_NewRef,
		/* .tp_repr      = */ &string_repr,
		/* .tp_bool      = */ (int (DCALL *)(DeeObject *__restrict))&string_bool,
		/* .tp_print     = */ &DeeString_PrintUtf8,
		/* .tp_printrepr = */ &DeeString_PrintRepr
	},
	/* .tp_call          = */ NULL,
	/* .tp_visit         = */ NULL,
	/* .tp_gc            = */ NULL,
	/* .tp_math          = */ &string_math,
	/* .tp_cmp           = */ &string_cmp,
	/* .tp_seq           = */ &string_seq,
	/* .tp_iter_next     = */ NULL,
	/* .tp_attr          = */ NULL,
	/* .tp_with          = */ NULL,
	/* .tp_buffer        = */ &string_buffer,
	/* .tp_methods       = */ string_methods,
	/* .tp_getsets       = */ string_getsets,
	/* .tp_members       = */ NULL,
	/* .tp_class_methods = */ string_class_methods,
	/* .tp_class_getsets = */ NULL,
	/* .tp_class_members = */ string_class_members
};

PRIVATE struct {
	size_t len;
	uint32_t zero;
} empty_utf = { 0, 0 };

PRIVATE struct string_utf empty_string_utf = {
	/* .u_width = */ STRING_WIDTH_1BYTE, /* Every character fits into a single byte (because there are no characters) */
	/* .u_flags = */ STRING_UTF_FASCII,
	/* .u_data  = */ {
		/* [STRING_WIDTH_1BYTE] = */ (size_t *)&DeeString_Empty.s_zero,
		/* [STRING_WIDTH_2BYTE] = */ (size_t *)&empty_utf.zero,
		/* [STRING_WIDTH_4BYTE] = */ (size_t *)&empty_utf.zero
	},
	/* .u_utf8  = */ &DeeString_Empty.s_zero
};

PUBLIC struct Dee_empty_string_struct DeeString_Empty = {
	OBJECT_HEAD_INIT(&DeeString_Type),
	/* .s_data = */ &empty_string_utf,
	/* .s_hash = */ 0,
	/* .s_len  = */ 0,
	/* .s_zero = */ '\0'
};

DECL_END

#endif /* !GUARD_DEEMON_OBJECTS_STRING_C */
