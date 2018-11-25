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
#ifndef GUARD_DEX_IPC_WINDOWS_PIPE_C_INL
#define GUARD_DEX_IPC_WINDOWS_PIPE_C_INL 1
#define _KOS_SOURCE 1

#include "libipc.h"

#include <deemon/api.h>
#include <deemon/bool.h>
#include <deemon/file.h>
#include <deemon/filetypes.h>
#include <deemon/arg.h>
#include <deemon/tuple.h>
#include <deemon/error.h>

#include <string.h>

DECL_BEGIN

typedef DeeSystemFileObject SystemFile;

PRIVATE DREF SystemFile *DCALL
open_fd(DeeFileTypeObject *__restrict fType, HANDLE hHandle) {
 DREF SystemFile *result;
 result = DeeObject_FMALLOC(SystemFile);
 if unlikely(!result) goto done;
 /* Fill in the system file. */
 result->sf_filename  = NULL;
 result->sf_handle    = hHandle;
 result->sf_ownhandle = hHandle; /* Inherit */
 result->sf_filetype  = FILE_TYPE_PIPE;
 DeeLFileObject_Init(result,fType);
done:
 return result;
}


PRIVATE DREF DeeObject *DCALL
pipe_class_new(DeeObject *__restrict UNUSED(self),
               size_t argc, DeeObject **__restrict argv) {
 DWORD pipe_size = 0; HANDLE hReader,hWriter;
 DREF SystemFile *fReader,*fWriter;
 DREF DeeObject *result;
 if (DeeArg_Unpack(argc,argv,"|I32u:new",&pipe_size))
     goto err;
 DBG_ALIGNMENT_DISABLE();
 if (!CreatePipe(&hReader,&hWriter,NULL,pipe_size)) {
  DBG_ALIGNMENT_ENABLE();
  nt_ThrowLastError();
  goto err;
 }
 DBG_ALIGNMENT_ENABLE();
 /* Create file objects for the pipe handles. */
 fReader = open_fd(&DeePipeReader_Type,hReader);
 if unlikely(!fReader) goto err_hreadwrite;
 fWriter = open_fd(&DeePipeWriter_Type,hWriter);
 if unlikely(!fWriter) goto err_fread_hwrite;
 /* Pack the file into a tuple. */
 result = DeeTuple_PackSymbolic(2,fReader,fWriter);
 if unlikely(!result) goto err_freadwrite;
 return result;
err_freadwrite:
 Dee_Decref(fReader);
 Dee_Decref(fWriter);
 goto err;
err_hreadwrite:
 DBG_ALIGNMENT_DISABLE();
 CloseHandle(hReader);
err_hwriter:
 DBG_ALIGNMENT_DISABLE();
 CloseHandle(hWriter);
 DBG_ALIGNMENT_ENABLE();
err:
 return NULL;
err_fread_hwrite:
 Dee_Decref(fReader);
 goto err_hwriter;
}


PRIVATE struct type_member pipe_class_members[] = {
    TYPE_MEMBER_CONST("reader",(DeeObject *)&DeePipeReader_Type),
    TYPE_MEMBER_CONST("writer",(DeeObject *)&DeePipeWriter_Type),
    TYPE_MEMBER_END
};

PRIVATE struct type_method pipe_class_methods[] = {
    { "new", &pipe_class_new,
      DOC("(size_hint=!0)->?T2?#reader?#writer\n"
          "Creates a new pair of linked pipe files") },
    { NULL }
};

INTERN DeeFileTypeObject DeePipe_Type = {
    /* .ft_base = */{
        OBJECT_HEAD_INIT(&DeeFileType_Type),
        /* .tp_name     = */"pipe",
        /* .tp_doc      = */NULL,
        /* .tp_flags    = */TP_FNORMAL,
        /* .tp_weakrefs = */0,
        /* .tp_features = */TF_NONE,
        /* .tp_base     = */(DeeTypeObject *)&DeeSystemFile_Type,
        /* .tp_init = */{
            {
                /* .tp_alloc = */{
                    /* .tp_ctor      = */NULL,
                    /* .tp_copy_ctor = */NULL,
                    /* .tp_deep_ctor = */NULL,
                    /* .tp_any_ctor  = */NULL,
                    TYPE_FIXED_ALLOCATOR(SystemFile)
                }
            },
            /* .tp_dtor        = */NULL,
            /* .tp_assign      = */NULL,
            /* .tp_move_assign = */NULL
        },
        /* .tp_cast = */{
            /* .tp_str  = */NULL,
            /* .tp_repr = */NULL,
            /* .tp_bool = */NULL
        },
        /* .tp_call          = */NULL,
        /* .tp_visit         = */NULL,
        /* .tp_gc            = */NULL,
        /* .tp_math          = */NULL,
        /* .tp_cmp           = */NULL,
        /* .tp_seq           = */NULL,
        /* .tp_iter_next     = */NULL,
        /* .tp_attr          = */NULL,
        /* .tp_with          = */NULL,
        /* .tp_buffer        = */NULL,
        /* .tp_methods       = */NULL,
        /* .tp_getsets       = */NULL,
        /* .tp_members       = */NULL,
        /* .tp_class_methods = */pipe_class_methods,
        /* .tp_class_getsets = */NULL,
        /* .tp_class_members = */pipe_class_members
    },
    /* .ft_read   = */NULL,
    /* .ft_write  = */NULL,
    /* .ft_seek   = */NULL,
    /* .ft_sync   = */NULL,
    /* .ft_trunc  = */NULL,
    /* .ft_close  = */NULL,
    /* .ft_pread  = */NULL,
    /* .ft_pwrite = */NULL,
    /* .ft_getc   = */NULL,
    /* .ft_ungetc = */NULL,
    /* .ft_putc   = */NULL
};
INTERN DeeFileTypeObject DeePipeReader_Type = {
    /* .ft_base = */{
        OBJECT_HEAD_INIT(&DeeFileType_Type),
        /* .tp_name     = */"pipe.reader",
        /* .tp_doc      = */NULL,
        /* .tp_flags    = */TP_FNORMAL,
        /* .tp_weakrefs = */0,
        /* .tp_features = */TF_NONE,
        /* .tp_base     = */(DeeTypeObject *)&DeePipe_Type,
        /* .tp_init = */{
            {
                /* .tp_alloc = */{
                    /* .tp_ctor      = */NULL,
                    /* .tp_copy_ctor = */NULL,
                    /* .tp_deep_ctor = */NULL,
                    /* .tp_any_ctor  = */NULL,
                    TYPE_FIXED_ALLOCATOR(SystemFile)
                }
            },
            /* .tp_dtor        = */NULL,
            /* .tp_assign      = */NULL,
            /* .tp_move_assign = */NULL
        },
        /* .tp_cast = */{
            /* .tp_str  = */NULL,
            /* .tp_repr = */NULL,
            /* .tp_bool = */NULL
        },
        /* .tp_call          = */NULL,
        /* .tp_visit         = */NULL,
        /* .tp_gc            = */NULL,
        /* .tp_math          = */NULL,
        /* .tp_cmp           = */NULL,
        /* .tp_seq           = */NULL,
        /* .tp_iter_next     = */NULL,
        /* .tp_attr          = */NULL,
        /* .tp_with          = */NULL,
        /* .tp_buffer        = */NULL,
        /* .tp_methods       = */NULL,
        /* .tp_getsets       = */NULL,
        /* .tp_members       = */NULL,
        /* .tp_class_methods = */NULL,
        /* .tp_class_getsets = */NULL,
        /* .tp_class_members = */NULL
    },
    /* .ft_read   = */NULL,
    /* .ft_write  = */NULL,
    /* .ft_seek   = */NULL,
    /* .ft_sync   = */NULL,
    /* .ft_trunc  = */NULL,
    /* .ft_close  = */NULL,
    /* .ft_pread  = */NULL,
    /* .ft_pwrite = */NULL,
    /* .ft_getc   = */NULL,
    /* .ft_ungetc = */NULL,
    /* .ft_putc   = */NULL
};
INTERN DeeFileTypeObject DeePipeWriter_Type = {
    /* .ft_base = */{
        OBJECT_HEAD_INIT(&DeeFileType_Type),
        /* .tp_name     = */"pipe.writer",
        /* .tp_doc      = */NULL,
        /* .tp_flags    = */TP_FNORMAL,
        /* .tp_weakrefs = */0,
        /* .tp_features = */TF_NONE,
        /* .tp_base     = */(DeeTypeObject *)&DeePipe_Type,
        /* .tp_init = */{
            {
                /* .tp_alloc = */{
                    /* .tp_ctor      = */NULL,
                    /* .tp_copy_ctor = */NULL,
                    /* .tp_deep_ctor = */NULL,
                    /* .tp_any_ctor  = */NULL,
                    TYPE_FIXED_ALLOCATOR(SystemFile)
                }
            },
            /* .tp_dtor        = */NULL,
            /* .tp_assign      = */NULL,
            /* .tp_move_assign = */NULL
        },
        /* .tp_cast = */{
            /* .tp_str  = */NULL,
            /* .tp_repr = */NULL,
            /* .tp_bool = */NULL
        },
        /* .tp_call          = */NULL,
        /* .tp_visit         = */NULL,
        /* .tp_gc            = */NULL,
        /* .tp_math          = */NULL,
        /* .tp_cmp           = */NULL,
        /* .tp_seq           = */NULL,
        /* .tp_iter_next     = */NULL,
        /* .tp_attr          = */NULL,
        /* .tp_with          = */NULL,
        /* .tp_buffer        = */NULL,
        /* .tp_methods       = */NULL,
        /* .tp_getsets       = */NULL,
        /* .tp_members       = */NULL,
        /* .tp_class_methods = */NULL,
        /* .tp_class_getsets = */NULL,
        /* .tp_class_members = */NULL
    },
    /* .ft_read   = */NULL,
    /* .ft_write  = */NULL,
    /* .ft_seek   = */NULL,
    /* .ft_sync   = */NULL,
    /* .ft_trunc  = */NULL,
    /* .ft_close  = */NULL,
    /* .ft_pread  = */NULL,
    /* .ft_pwrite = */NULL,
    /* .ft_getc   = */NULL,
    /* .ft_ungetc = */NULL,
    /* .ft_putc   = */NULL
};


DECL_END

#endif /* !GUARD_DEX_IPC_WINDOWS_PIPE_C_INL */
