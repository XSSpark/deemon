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
#ifndef GUARD_DEEMON_COMPILER_COMPILER_H
#define GUARD_DEEMON_COMPILER_COMPILER_H 1

#include "../api.h"
#include "../object.h"
#ifndef CONFIG_NO_THREADS
#include "../util/rwlock.h"
#include "../util/recursive-rwlock.h"
#endif

#ifdef CONFIG_BUILDING_DEEMON
#include "ast.h"
#include "tpp.h"
#include "lexer.h"
#include "error.h"
#endif /* CONFIG_BUILDING_DEEMON */

#include <stdint.h>

DECL_BEGIN

typedef struct compiler_object DeeCompilerObject;
struct compiler_options;

#ifdef CONFIG_BUILDING_DEEMON
#define COMPILER_ITEM_OBJECT_HEAD(T) \
        OBJECT_HEAD \
        DREF DeeCompilerObject *ci_compiler; /* [1..1][const] The associated compiler. */ \
        DeeCompilerItemObject **ci_pself;    /* [1..1][== self][1..1][lock(co_compiler->cp_item_lock)] Compiler item self-pointer. */ \
        DeeCompilerItemObject  *ci_next;     /* [0..1][lock(co_compiler->cp_item_lock)] Next compiler item. */ \
        T                      *ci_value;    /* [0..1][lock(DeeCompiler_Lock)] Pointer to the associated object. \
                                              * What exactly is pointed to depends on the compiler item type. \
                                              * NOTE: For compiler items implemented as `DeeCompilerObjItem_Type', \
                                              *       this field is [DREF][1..1][const] */

typedef struct compiler_item_object DeeCompilerItemObject;
typedef struct compiler_wrapper_object DeeCompilerWrapperObject;
struct compiler_item_object { COMPILER_ITEM_OBJECT_HEAD(void) };
struct compiler_wrapper_object {
    OBJECT_HEAD
    DREF DeeCompilerObject *cw_compiler; /* [1..1][const] The compiler being wrapped. */
};
#define COMPILER_ITEM_HASH(x) Dee_HashPointer((x)->ci_value)
INTDEF DeeTypeObject DeeCompilerItem_Type;
INTDEF DeeTypeObject DeeCompilerObjItem_Type;
INTDEF DeeTypeObject DeeCompilerWrapper_Type;

/* Construct and return a wrapper for a sub-component of the current compiler. */
INTDEF DREF DeeObject *DCALL DeeCompiler_GetWrapper(DeeCompilerObject *__restrict self,
                                                    DeeTypeObject *__restrict type);

/* Lookup or create a new compiler item for `value' */
INTDEF DREF DeeObject *DCALL DeeCompiler_GetItem(DeeTypeObject *__restrict type,
                                                 void *__restrict value);
INTDEF DREF DeeObject *DCALL DeeCompiler_GetObjItem(DeeTypeObject *__restrict type,
                                                    DeeObject *__restrict value);

/* Delete (clear) the compiler item associated with `value'. */
INTDEF bool DCALL DeeCompiler_DelItem(void *value);

/* Delete (clear) all compiler items matching the given `type'. */
INTDEF size_t DCALL DeeCompiler_DelItemType(DeeTypeObject *__restrict type);

/* Returns the value of a given compiler item.
 * @return: * :   A pointer to the item's value.
 * @return: NULL: The item got deleted (a ReferenceError was thrown) */
INTDEF void *(DCALL DeeCompilerItem_GetValue)(DeeObject *__restrict self);
#define DeeCompilerItem_VALUE(self,T) \
   ((T *)DeeCompilerItem_GetValue((DeeObject *)REQUIRES_OBJECT(self)))


struct compiler_items {
    size_t                  ci_size; /* [lock(ci_lock)] Amount of existing compiler items. */
    size_t                  ci_mask; /* [lock(ci_lock)] Hash-map mask. */
    DeeCompilerItemObject **ci_list; /* [0..1][0..ci_mask+1][owned] Hash-map of compiler items. */
#ifndef CONFIG_NO_THREADS
    rwlock_t                ci_lock; /* Lock for compiler items. */
#endif
};
#endif

struct compiler_object {
    /* >> Since the compiler is a fairly large system, divided into _a_ _lot_ of
     *    different functions all operating on the same objects, it proves to
     *    be quite efficient not to pass contexts and descriptors around through
     *    arguments when these functions call each other, but rather place them
     *    in various global variables.
     * However whenever we need to change which compiler is actually running,
     * we need to store the state of the previously active one somewhere, and
     * that is where `DeeCompilerObject' comes into place.
     * Despite the various very powerful members of this object, it is not meant
     * to operate upon them directly, but rather do the following whenever it
     * chooses to perform an operation:
     * >> COMPILER_BEGIN(self);
     * >> do_the_operation();
     * >> COMPILER_END();
     */
    OBJECT_HEAD
    DREF DeeCompilerObject *cp_prev;      /* [0..1][lock(DeeCompiler_Lock)]
                                           * The compiler that was active before this one and
                                           * will be restored when `DeeCompiler_End()' is called. */
    size_t                  cp_recursion; /* [lock(DeeCompiler_Lock)] Recursion counter for how often `DeeCompiler_Begin()' was invoked for this compiler. */
#define COMPILER_FNORMAL    0x0000        /* Normal compiler flags. */
#define COMPILER_FKEEPLEXER 0x0001        /* Do not save/restore the active TPP lexer. */
#define COMPILER_FKEEPERROR 0x0002        /* Do not save/restore the active parser error state. */
#define COMPILER_FMASK      0x0003        /* Mask of known flags. */
    uint16_t                cp_flags;     /* [const] Compiler flags (Set of `COMPILER_F*'). */
    uint16_t               _cp_pad[(sizeof(void *)/2)-1]; /* ... */
    WEAKREF_SUPPORT
#ifdef CONFIG_BUILDING_DEEMON
    /* [OVERRIDE(*,[valid_if(self != DeeCompiler_Active.wr_obj)])] */
    struct compiler_items    cp_items;           /* Hash-map of user-code compiler item wrappers. */
    DREF DeeScopeObject     *cp_scope;           /* [1..1] == ::current_scope */
    struct compiler_options *cp_inner_options;   /* [0..1] == ::inner_compiler_options */
    struct ast_tags          cp_tags;            /* == ::current_tags */
    struct TPPLexer          cp_lexer;           /* [valid_if(!COMPILER_FKEEPLEXER)] == ::TPPLexer_Global */
    struct parser_errors     cp_errors;          /* [valid_if(!COMPILER_FKEEPERROR)] == ::current_parser_errors */
    struct compiler_options *cp_options;         /* [0..1] User-defined compiler options. */
#ifndef CONFIG_LANGUAGE_NO_ASM
    size_t                   cp_uasm_unique;     /* Unique user-assembly ID. */
#endif /* !CONFIG_LANGUAGE_NO_ASM */
    uint16_t                 cp_parser_flags;    /* == ::parser_flags */
    uint16_t                 cp_optimizer_flags; /* == ::optimizer_flags */
    uint16_t                 cp_unwind_limit;    /* == ::optimizer_unwind_limit */
#endif /* CONFIG_BUILDING_DEEMON */
};


/* NOTE: Because of how large the user-code interface for the compiler is,
 *       combined with the fact that the internal implementation of the
 *       compiler is completely implementation-defined, the actual compiler
 *       type is not exported from `deemon', but rather exported from `rt',
 *       thus providing code that wishes to stick to our deemon implementation
 *       the ability to tinker around with the compiler, while still not having
 *       to standardize any aspect about its inner working what-so-ever. */
DDATDEF DeeTypeObject DeeCompiler_Type; /* compiler from rt */
#define DeeCompiler_Check(ob)      DeeObject_InstanceOf(ob,&DeeCompiler_Type)
#define DeeCompiler_CheckExact(ob) DeeObject_InstanceOfExact(ob,&DeeCompiler_Type)


/* Construct a new compiler for generating the source for the given `module'.
 * @param: flags: Set of `COMPILER_F*' (see above) */
DFUNDEF DREF DeeCompilerObject *DCALL
DeeCompiler_New(DeeObject *__restrict module,
                uint16_t flags);



#ifndef CONFIG_NO_THREADS
/* Lock held whenever the compiler is being used.
 * TODO: Use some blocking lock for this. - Don't use a spinlock. */
DDATDEF recursive_rwlock_t DeeCompiler_Lock;
#endif

/* A weak reference to the compiler associated with
 * the currently active global compiler context.
 * WARNING: Do _NOT_ attempt to write to this weak reference! _EVER_! */
#ifdef GUARD_DEEMON_COMPILER_COMPILER_C
DDATDEF weakref_t DeeCompiler_Active;
#else
DDATDEF weakref_t const DeeCompiler_Active;
#endif

/* [0..1][lock(DeeCompiler_Lock)] The currently active compiler.
 * This variable points to the current compiler while inside a
 * `DeeCompiler_Begin()...DeeCompiler_End()' block. */
DDATDEF DREF DeeCompilerObject *DeeCompiler_Current;


/* Ensure that `compiler' describes the currently active compiler context.
 * NOTE: The caller is responsible for holding a lock to `DeeCompiler_Lock'.
 * NOTE: It is possible to use sub-compilers, but it is not allowed to
 *       interweave top-level compilers below lower-level ones:
 *       >> DeeCompilerObject *a,b;
 *       >> DeeCompiler_Begin(a);
 *       >>    DeeCompiler_Begin(b); // OK!
 *       >>       DeeCompiler_Begin(a); // ILLEGAL: `a' is already apart of the ative compiler stack.
 *       >>       DeeCompiler_End();
 *       >>    DeeCompiler_End();
 *       >> DeeCompiler_End();
 */
DFUNDEF void DCALL DeeCompiler_Begin(DREF DeeCompilerObject *__restrict compiler);
DFUNDEF void DCALL DeeCompiler_End(void);

/* Check if `compiler' is the currently active one.
 * If it is, make sure to copy all context-sensitive data into
 * the compiler object and mark the global compiler context as
 * being unassigned.
 * NOTE: `DeeCompiler_Active' is _NOT_ used for this check,
 *        but `DeeCompiler_Current' is instead.
 *        This is because this function is intended to be called
 *        from the destructor of `DeeCompiler_Type', before the
 *        destructor will proceed to delete all compiler sub-components.
 * NOTE:  Unlike with `DeeCompiler_Begin()' and `DeeCompiler_End()', the
 *        caller must not be holding any kind of lock on `DeeCompiler_Lock'
 *        when calling this function! */
DFUNDEF void DCALL DeeCompiler_Unload(DREF DeeCompilerObject *__restrict compiler);

#ifdef CONFIG_NO_THREADS
#define COMPILER_BEGIN(c) DeeCompiler_Begin(c)
#define COMPILER_END()    DeeCompiler_End()
#else
#define COMPILER_BEGIN(c) (recursive_rwlock_write(&DeeCompiler_Lock),DeeCompiler_Begin(c))
#define COMPILER_END()    (DeeCompiler_End(),recursive_rwlock_endwrite(&DeeCompiler_Lock))
#endif


DECL_END

#endif /* !GUARD_DEEMON_COMPILER_COMPILER_H */
