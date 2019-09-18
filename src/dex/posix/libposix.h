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
#ifndef GUARD_DEX_POSIX_LIBPOSIX_H
#define GUARD_DEX_POSIX_LIBPOSIX_H 1
#define _KOS_SOURCE 1
#define _GNU_SOURCE 1
#define _XOPEN_SOURCE 700
#define _LARGEFILE64_SOURCE 1
#define _ATFILE_SOURCE 1

#include <deemon/api.h>
#include <deemon/dex.h>
#include <deemon/file.h>
#include <deemon/object.h>

#ifdef CONFIG_NO_IO_H
#undef CONFIG_HAVE_IO_H
#elif !defined(CONFIG_HAVE_IO_H) && \
      (defined(_MSC_VER) || __has_include(<io.h>))
#undef CONFIG_HAVE_IO_H
#define CONFIG_HAVE_IO_H 1
#endif

#ifdef CONFIG_NO_PROCESS_H
#undef CONFIG_HAVE_PROCESS_H
#elif !defined(CONFIG_HAVE_PROCESS_H) && \
      (defined(_MSC_VER) || __has_include(<process.h>))
#undef CONFIG_HAVE_PROCESS_H
#define CONFIG_HAVE_PROCESS_H 1
#endif

#ifdef CONFIG_NO_SYS_STAT_H
#undef CONFIG_HAVE_SYS_STAT_H
#elif !defined(CONFIG_HAVE_SYS_STAT_H) && \
      (defined(_MSC_VER) || \
       defined(__linux__) || defined(__linux) || defined(linux) || \
       defined(__unix__) || defined(__unix) || defined(unix) || \
       __has_include(<sys/stat.h>))
#undef CONFIG_HAVE_SYS_STAT_H
#define CONFIG_HAVE_SYS_STAT_H 1
#endif

#ifdef CONFIG_NO_FCNTL_H
#undef CONFIG_HAVE_FCNTL_H
#elif !defined(CONFIG_HAVE_FCNTL_H) && \
      (defined(_MSC_VER) || \
       defined(__linux__) || defined(__linux) || defined(linux) || \
       defined(__unix__) || defined(__unix) || defined(unix) || \
       __has_include(<fcntl.h>))
#undef CONFIG_HAVE_FCNTL_H
#define CONFIG_HAVE_FCNTL_H 1
#endif

#ifdef CONFIG_NO_SYS_IOCTL_H
#undef CONFIG_HAVE_SYS_IOCTL_H
#elif !defined(CONFIG_HAVE_SYS_IOCTL_H) && \
      (defined(__linux__) || defined(__linux) || defined(linux) || \
       defined(__unix__) || defined(__unix) || defined(unix) || \
       __has_include(<sys/ioctl.h>))
#undef CONFIG_HAVE_SYS_IOCTL_H
#define CONFIG_HAVE_SYS_IOCTL_H 1
#endif

#ifdef CONFIG_NO_UNISTD_H
#undef CONFIG_HAVE_UNISTD_H
#elif !defined(CONFIG_HAVE_UNISTD_H) && \
      (defined(__linux__) || defined(__linux) || defined(linux) || \
       defined(__unix__) || defined(__unix) || defined(unix) || \
       __has_include(<unistd.h>))
#undef CONFIG_HAVE_UNISTD_H
#define CONFIG_HAVE_UNISTD_H 1
#endif

#ifdef CONFIG_NO_ERRNO_H
#undef CONFIG_HAVE_ERRNO_H
#elif !defined(CONFIG_HAVE_ERRNO_H) && \
      (defined(_MSC_VER) || \
       defined(__linux__) || defined(__linux) || defined(linux) || \
       defined(__unix__) || defined(__unix) || defined(unix) || \
       __has_include(<errno.h>))
#undef CONFIG_HAVE_ERRNO_H
#define CONFIG_HAVE_ERRNO_H 1
#endif

#ifdef CONFIG_HAVE_IO_H
#include <io.h>
#endif /* CONFIG_HAVE_IO_H */

#ifdef CONFIG_HAVE_PROCESS_H
#include <process.h>
#endif /* CONFIG_HAVE_PROCESS_H */

#ifdef CONFIG_HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* CONFIG_HAVE_SYS_STAT_H */

#ifdef CONFIG_HAVE_FCNTL_H
#include <fcntl.h>
#endif /* CONFIG_HAVE_FCNTL_H */

#ifdef CONFIG_HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* CONFIG_HAVE_SYS_IOCTL_H */

#ifdef CONFIG_HAVE_UNISTD_H
#include <unistd.h>
#endif /* CONFIG_HAVE_UNISTD_H */

#ifdef CONFIG_HAVE_ERRNO_H
#include <errno.h>
#endif /* CONFIG_HAVE_ERRNO_H */

#ifndef CONFIG_NO_STDIO
#include <stdio.h>
#endif /* !CONFIG_NO_STDIO */

#ifndef CONFIG_NO_STDLIB
#include <stdlib.h>
#endif /* !CONFIG_NO_STDLIB */

#ifdef _MSC_VER         /* Microsoft compiler */
#define HAVE_EXECV      1
#define execv     _execv
#define execve    _execve
#define execvp    _execvp
#define execvpe   _execvpe

#define HAVE_WEXECV     1
#define wexecv    _wexecv
#define wexecve   _wexecve
#define wexecvp   _wexecvp
#define wexecvpe  _wexecvpe

#define HAVE_SPAWNV     1
#define spawnv    _spawnv
#define spawnve   _spawnve
#define spawnvp   _spawnvp
#define spawnvpe  _spawnvpe

#define HAVE_WSPAWNV    1
#define wspawnv   _wspawnv
#define wspawnve  _wspawnve
#define wspawnvp  _wspawnvp
#define wspawnvpe _wspawnvpe

#define HAVE_CWAIT      1
#define cwait     _cwait

#define HAVE_SYSTEM     1
#define HAVE_WSYSTEM    1
#define wsystem   _wsystem

#define HAVE_FILEIO 1
#define HAVE_LSEEK64 1
#define HAVE_CREAT 1
#define HAVE_WCREAT 1
#define HAVE_WOPEN 1
#define creat     _creat
#define wcreat    _wcreat
#define open      _open
#define wopen     _wopen
#define read(fd,buf,size)     _read(fd,buf,(int)(size))
#define write(fd,buf,size)    _write(fd,buf,(int)(size))
#define lseek     _lseek
#define lseek64   _lseeki64
#define close     _close

#define HAVE_FSYNC      1
#define fsync     _commit

#define HAVE_GETPID 1
#define getpid    _getpid

#define HAVE_FTRUNCATE 1
#define HAVE_FTRUNCATE64 1
#define ftruncate   _chsize
#define ftruncate64 _chsize_s

#define HAVE_UMASK 1
#define umask    _umask

#define HAVE_DUP 1
#define dup      _dup

#define HAVE_DUP2 1
#define dup2     _dup2

#define HAVE_ISATTY 1
#define isatty   _isatty

#define HAVE_ACCESS 1
#define HAVE_WACCESS 1
#define access   _access
#define waccess  _waccess
#define F_OK     0
#define X_OK     1 /* Not supported? */
#define W_OK     2
#define R_OK     4

#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_APPEND
#undef O_CREAT
#undef O_TRUNC
#undef O_EXCL
#undef O_TEXT
#undef O_BINARY
#undef O_WTEXT
#undef O_U16TEXT
#undef O_U8TEXT
#undef O_RAW
#undef O_NOINHERIT
#undef O_CLOEXEC
#undef O_TEMPORARY
#undef O_SHORT_LIVED
#undef O_OBTAIN_DIR
#undef O_SEQUENTIAL
#undef O_RANDOM
#else

#if defined(F_OK) && defined(X_OK) && defined(W_OK) && defined(R_OK)
#define HAVE_ACCESS 1
#ifdef __USE_GNU
#define HAVE_EUIDACCESS 1
#define HAVE_EACCESS    1
#endif /* __USE_GNU */
#ifdef __USE_ATFILE
#define HAVE_FACCESSAT  1
#endif /* __USE_ATFILE */
#endif

#ifdef __USE_ATFILE
#define HAVE_FCHOWNAT 1
#endif /* __USE_ATFILE */

#define HAVE_FILEIO 1 /* XXX: Add checks */

#if defined(__USE_UNIX98) || defined(__USE_XOPEN2K8)
#define HAVE_PREAD 1
#define HAVE_PWRITE 1
#endif

#ifdef __USE_LARGEFILE64
#define HAVE_OPEN64 1
#define HAVE_LSEEK64 1
#if defined(__USE_UNIX98) || defined(__USE_XOPEN2K8)
#define HAVE_PREAD64 1
#define HAVE_PWRITE64 1
#endif
#endif

#define HAVE_DUP 1  /* XXX: Add checks */
#define HAVE_DUP2 1 /* XXX: Add checks */
#ifdef __USE_GNU
#define HAVE_DUP3 1
#endif /* __USE_GNU */

#if defined(_POSIX_FSYNC) && (_POSIX_FSYNC+0) != 0
#define HAVE_FSYNC 1
#endif /* _POSIX_FSYNC */

#if defined(_POSIX_SAVED_IDS) && (_POSIX_SAVED_IDS+0) != 0
#define HAVE_SETREUID 1
#define HAVE_SETREGID 1
#endif /* _POSIX_SAVED_IDS */

#define HAVE_PIPE 1 /* XXX: Add checks */
#ifdef __USE_GNU
#define HAVE_PIPE2 1
#endif /* __USE_GNU */

#define HAVE_EXECV 1 /* XXX: Add checks */
#define HAVE_EXECVE 1 /* XXX: Add checks */
#ifdef __USE_GNU
#define HAVE_EXECVPE 1
#endif /* __USE_GNU */
#ifdef __USE_XOPEN2K8
#define HAVE_FEXECVE 1
#endif /* __USE_XOPEN2K8 */

#if defined(__USE_MISC) || defined(__USE_XOPEN)
#define HAVE_NICE 1
#endif

#define HAVE_PATHCONF 1 /* XXX: Add checks */
#define HAVE_FPATHCONF 1 /* XXX: Add checks */
#define HAVE_SYSCONF 1 /* XXX: Add checks */
#ifdef __USE_POSIX2
#define HAVE_CONFSTR 1
#endif

#define HAVE_GETPID 1 /* XXX: Add checks */
#define HAVE_GETPPID 1 /* XXX: Add checks */
#define HAVE_GETPGRP 1 /* XXX: Add checks */
#if defined(__USE_XOPEN_EXTENDED) || defined(__USE_XOPEN2K8)
#define HAVE_GETPGID 1
#define HAVE_GETSID 1
#endif
#define HAVE_SETPGID 1 /* XXX: Add checks */
#if defined(__USE_MISC) || defined(__USE_XOPEN_EXTENDED)
#define HAVE_SETPGRP 1
#endif
#define HAVE_SETSID 1 /* XXX: Add checks */
#define HAVE_GETUID 1 /* XXX: Add checks */
#define HAVE_GETEUID 1 /* XXX: Add checks */
#define HAVE_GETGID 1 /* XXX: Add checks */
#define HAVE_GETEGID 1 /* XXX: Add checks */

#endif 


/* Set optional flags to no-ops */
#ifndef O_BINARY
#ifdef __O_BINARY
#define O_BINARY __O_BINARY
#elif defined(_O_BINARY)
#define O_BINARY _O_BINARY
#elif defined(__O_RAW)
#define O_BINARY __O_RAW
#elif defined(_O_RAW)
#define O_BINARY _O_RAW
#elif defined(O_RAW)
#define O_BINARY O_RAW
#else
#define O_BINARY 0
#endif
#endif /* !O_BINARY */

#ifndef O_SHORT_LIVED
#ifdef __O_SHORT_LIVED
#define O_SHORT_LIVED __O_SHORT_LIVED
#elif defined(_O_SHORT_LIVED)
#define O_SHORT_LIVED _O_SHORT_LIVED
#else
#define O_SHORT_LIVED 0
#endif
#endif /* !O_SHORT_LIVED */

#ifndef O_SEQUENTIAL
#ifdef __O_SEQUENTIAL
#define O_SEQUENTIAL __O_SEQUENTIAL
#elif defined(_O_SEQUENTIAL)
#define O_SEQUENTIAL _O_SEQUENTIAL
#else
#define O_SEQUENTIAL 0
#endif
#endif /* !O_SEQUENTIAL */

#ifndef O_RANDOM
#ifdef __O_RANDOM
#define O_RANDOM __O_RANDOM
#elif defined(_O_RANDOM)
#define O_RANDOM _O_RANDOM
#else
#define O_RANDOM 0
#endif
#endif /* !O_RANDOM */

#ifndef O_PATH
#ifdef __O_PATH
#define O_PATH __O_PATH
#elif defined(_O_PATH)
#define O_PATH _O_PATH
#else
#define O_PATH 0
#endif
#endif /* !O_PATH */

#ifndef O_NOATIME
#ifdef __O_NOATIME
#define O_NOATIME __O_NOATIME
#elif defined(_O_NOATIME)
#define O_NOATIME _O_NOATIME
#else
#define O_NOATIME 0
#endif
#endif /* !O_NOATIME */

#ifndef O_NOCTTY
#ifdef __O_NOCTTY
#define O_NOCTTY __O_NOCTTY
#elif defined(_O_NOCTTY)
#define O_NOCTTY _O_NOCTTY
#else
#define O_NOCTTY 0
#endif
#endif /* !O_NOCTTY */


#ifndef O_TEXT
#ifdef __O_TEXT
#define O_TEXT __O_TEXT
#elif defined(_O_TEXT)
#define O_TEXT _O_TEXT
#endif
#endif /* !O_TEXT */

#ifndef O_WTEXT
#ifdef __O_WTEXT
#define O_WTEXT __O_WTEXT
#elif defined(_O_WTEXT)
#define O_WTEXT _O_WTEXT
#endif
#endif /* !O_WTEXT */

#ifndef O_U16TEXT
#ifdef __O_U16TEXT
#define O_U16TEXT __O_U16TEXT
#elif defined(_O_U16TEXT)
#define O_U16TEXT _O_U16TEXT
#endif
#endif /* !O_U16TEXT */

#ifndef O_U8TEXT
#ifdef __O_U8TEXT
#define O_U8TEXT __O_U8TEXT
#elif defined(_O_U8TEXT)
#define O_U8TEXT _O_U8TEXT
#endif
#endif /* !O_U8TEXT */

#ifndef O_TEMPORARY
#ifdef __O_TEMPORARY
#define O_TEMPORARY __O_TEMPORARY
#elif defined(_O_TEMPORARY)
#define O_TEMPORARY _O_TEMPORARY
#endif
#endif /* !O_TEMPORARY */

#ifndef O_OBTAIN_DIR
#ifdef __O_OBTAIN_DIR
#define O_OBTAIN_DIR __O_OBTAIN_DIR
#elif defined(_O_OBTAIN_DIR)
#define O_OBTAIN_DIR _O_OBTAIN_DIR
#endif
#endif /* !O_OBTAIN_DIR */

#ifndef O_CREAT
#ifdef __O_CREAT
#define O_CREAT __O_CREAT
#elif defined(_O_CREAT)
#define O_CREAT _O_CREAT
#endif
#endif /* !O_CREAT */

#ifndef O_TRUNC
#ifdef __O_TRUNC
#define O_TRUNC __O_TRUNC
#elif defined(_O_TRUNC)
#define O_TRUNC _O_TRUNC
#endif
#endif /* !O_TRUNC */

#ifndef O_RDONLY
#ifdef __O_RDONLY
#define O_RDONLY __O_RDONLY
#elif defined(_O_RDONLY)
#define O_RDONLY _O_RDONLY
#endif
#endif /* !O_RDONLY */

#ifndef O_WRONLY
#ifdef __O_WRONLY
#define O_WRONLY __O_WRONLY
#elif defined(_O_WRONLY)
#define O_WRONLY _O_WRONLY
#endif
#endif /* !O_WRONLY */

#ifndef O_RDWR
#ifdef __O_RDWR
#define O_RDWR __O_RDWR
#elif defined(_O_RDWR)
#define O_RDWR _O_RDWR
#endif
#endif /* !O_RDWR */

#ifndef O_ACCMODE
#ifdef __O_ACCMODE
#define O_ACCMODE __O_ACCMODE
#elif defined(_O_ACCMODE)
#define O_ACCMODE _O_ACCMODE
#elif 0 /* The combination might overlap with something else... */
#define O_ACCMODE (O_RDONLY|O_WRONLY|O_RDWR)
#endif
#endif /* !O_ACCMODE */

#ifndef O_CLOEXEC
#ifdef __O_NOINHERIT
#define O_CLOEXEC __O_NOINHERIT
#elif defined(_O_NOINHERIT)
#define O_CLOEXEC _O_NOINHERIT
#elif defined(O_NOINHERIT)
#define O_CLOEXEC O_NOINHERIT
#elif defined(__O_CLOEXEC)
#define O_CLOEXEC __O_CLOEXEC
#elif defined(_O_CLOEXEC)
#define O_CLOEXEC _O_CLOEXEC
#endif
#endif /* !O_CLOEXEC */

#ifndef O_EXCL
#ifdef __O_EXCL
#define O_EXCL __O_EXCL
#elif defined(_O_EXCL)
#define O_EXCL _O_EXCL
#endif
#endif /* !O_EXCL */

#ifndef O_APPEND
#ifdef __O_APPEND
#define O_APPEND __O_APPEND
#elif defined(_O_APPEND)
#define O_APPEND _O_APPEND
#endif
#endif /* !O_APPEND */

#ifndef O_NONBLOCK
#ifdef __O_NONBLOCK
#define O_NONBLOCK __O_NONBLOCK
#elif defined(_O_NONBLOCK)
#define O_NONBLOCK _O_NONBLOCK
#elif defined(__O_NDELAY)
#define O_NONBLOCK __O_NDELAY
#elif defined(_O_NDELAY)
#define O_NONBLOCK _O_NDELAY
#elif defined(O_NDELAY)
#define O_NONBLOCK O_NDELAY
#endif
#endif /* !O_NONBLOCK */

#ifndef O_RSYNC
#ifdef __O_RSYNC
#define O_RSYNC __O_RSYNC
#elif defined(_O_RSYNC)
#define O_RSYNC _O_RSYNC
#endif
#endif /* !O_RSYNC */

#ifndef O_SYNC
#ifdef __O_SYNC
#define O_SYNC __O_SYNC
#elif defined(_O_SYNC)
#define O_SYNC _O_SYNC
#endif
#endif /* !O_SYNC */

#ifndef O_DSYNC
#ifdef __O_DSYNC
#define O_DSYNC __O_DSYNC
#elif defined(_O_DSYNC)
#define O_DSYNC _O_DSYNC
#endif
#endif /* !O_DSYNC */

#ifndef O_ASYNC
#ifdef __O_ASYNC
#define O_ASYNC __O_ASYNC
#elif defined(_O_ASYNC)
#define O_ASYNC _O_ASYNC
#endif
#endif /* !O_ASYNC */

#ifndef O_DIRECT
#ifdef __O_DIRECT
#define O_DIRECT __O_DIRECT
#elif defined(_O_DIRECT)
#define O_DIRECT _O_DIRECT
#endif
#endif /* O_DIRECT */

#ifndef O_LARGEFILE
#ifdef __O_LARGEFILE
#define O_LARGEFILE __O_LARGEFILE
#elif defined(_O_LARGEFILE)
#define O_LARGEFILE _O_LARGEFILE
#endif
#endif /* !O_LARGEFILE */

#ifndef O_DIRECTORY
#ifdef __O_DIRECTORY
#define O_DIRECTORY __O_DIRECTORY
#elif defined(_O_DIRECTORY)
#define O_DIRECTORY _O_DIRECTORY
#endif
#endif /* !O_DIRECTORY */

#ifndef O_NOFOLLOW
#ifdef __O_NOFOLLOW
#define O_NOFOLLOW __O_NOFOLLOW
#elif defined(_O_NOFOLLOW)
#define O_NOFOLLOW _O_NOFOLLOW
#endif
#endif /* !O_NOFOLLOW */

#ifndef O_TMPFILE
#ifdef __O_TMPFILE
#define O_TMPFILE __O_TMPFILE
#elif defined(_O_TMPFILE)
#define O_TMPFILE _O_TMPFILE
#endif
#endif /* !O_TMPFILE */

#ifndef O_CLOFORK
#ifdef __O_CLOFORK
#define O_CLOFORK __O_CLOFORK
#elif defined(_O_CLOFORK)
#define O_CLOFORK _O_CLOFORK
#endif
#endif /* !O_CLOFORK */

#ifndef O_SYMLINK
#ifdef __O_SYMLINK
#define O_SYMLINK __O_SYMLINK
#elif defined(_O_SYMLINK)
#define O_SYMLINK _O_SYMLINK
#endif
#endif /* !O_SYMLINK */

#ifndef O_DOSPATH
#ifdef __O_DOSPATH
#define O_DOSPATH __O_DOSPATH
#elif defined(_O_DOSPATH)
#define O_DOSPATH _O_DOSPATH
#elif defined(_MSC_VER)
#define O_DOSPATH 0
#endif
#endif /* !O_DOSPATH */

#ifndef O_SHLOCK
#ifdef __O_SHLOCK
#define O_SHLOCK __O_SHLOCK
#elif defined(_O_SHLOCK)
#define O_SHLOCK _O_SHLOCK
#endif
#endif /* !O_SHLOCK */

#ifndef O_EXLOCK
#ifdef __O_EXLOCK
#define O_EXLOCK __O_EXLOCK
#elif defined(_O_EXLOCK)
#define O_EXLOCK _O_EXLOCK
#endif
#endif /* !O_EXLOCK */

#ifndef O_XATTR
#ifdef __O_XATTR
#define O_XATTR __O_XATTR
#elif defined(_O_XATTR)
#define O_XATTR _O_XATTR
#endif
#endif /* !O_XATTR */

#ifndef O_EXEC
#ifdef __O_EXEC
#define O_EXEC __O_EXEC
#elif defined(_O_EXEC)
#define O_EXEC _O_EXEC
#endif
#endif /* !O_EXEC */

#ifndef O_SEARCH
#ifdef __O_SEARCH
#define O_SEARCH __O_SEARCH
#elif defined(_O_SEARCH)
#define O_SEARCH _O_SEARCH
#endif
#endif /* !O_SEARCH */

#ifndef O_TTY_INIT
#ifdef __O_TTY_INIT
#define O_TTY_INIT __O_TTY_INIT
#elif defined(_O_TTY_INIT)
#define O_TTY_INIT _O_TTY_INIT
#endif
#endif /* !O_TTY_INIT */

#ifndef O_NOLINKS
#ifdef __O_NOLINKS
#define O_NOLINKS __O_NOLINKS
#elif defined(_O_NOLINKS)
#define O_NOLINKS _O_NOLINKS
#endif
#endif /* !O_NOLINKS */


#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif /* !EXIT_SUCCESS */

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif /* !EXIT_FAILURE */

DECL_BEGIN

/* Imported module access. */
#define FS_MODULE   DEX.d_imports[0]

DECL_END

#endif /* !GUARD_DEX_POSIX_LIBPOSIX_H */
