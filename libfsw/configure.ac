#                                               -*- Autoconf -*-
#
# Copyright (C) 2014, Enrico M. Crisostomo
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Process this file with autoconf to produce a configure script.
#
AC_PREREQ([2.69])
AC_INIT([libfsw], [1.4.0], [enrico.m.crisostomo@gmail.com], [], [https://github.com/emcrisostomo/fsw])
AC_COPYRIGHT([2014 (C) Enrico M. Crisostomo])
AC_REVISION([$Revision: 1.4.0-1 $])
AC_CONFIG_SRCDIR([c/libfsw.cpp])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([libfsw_config.h])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_MACRO_DIR([m4])

# Initialize Automake
AM_INIT_AUTOMAKE([-Wall -Werror gnu subdir-objects])
AM_PROG_AR

# Initialize libtool 
LT_PREREQ([2.4.2])
LT_INIT
AC_SUBST([LIBTOOL_DEPS])

# Configure language
AC_LANG(C++)

# Checks for programs.
AC_PROG_CXX([clang++ g++])

# Configure C++ compiler
AX_CXX_COMPILE_STDCXX_11(noext)
AX_CXXFLAGS_WARN_ALL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h unistd.h fcntl.h])
AC_CHECK_HEADERS([sys/event.h sys/inotify.h])
AC_CHECK_HEADERS([CoreServices/CoreServices.h])
AC_CHECK_HEADERS([unordered_map unordered_set])

AM_CONDITIONAL([USE_CORESERVICES], [test "x${ac_cv_header_CoreServices_CoreServices_h}" = "xyes"])
AM_CONDITIONAL([USE_KQUEUE], [test "x${ac_cv_header_sys_event_h}" = "xyes"])
AM_CONDITIONAL([USE_INOTIFY], [test "x${ac_cv_header_sys_inotify_h}" = "xyes"])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_MODE_T

AC_CHECK_MEMBERS([struct stat.st_mtime],
   [],
   [],
   [
     AC_INCLUDES_DEFAULT
     #include <sys/stat.h>
   ])

AC_CHECK_MEMBERS([struct stat.st_mtimespec],
   [],
   [],
   [
     AC_INCLUDES_DEFAULT
     #include <sys/stat.h>
   ])

# Checks for library functions.
AC_CHECK_FUNCS(
  [realpath],
  ,
  AC_MSG_ERROR([The realpath function cannot be found.])
)
AC_CHECK_FUNCS(
  [modf],
  ,
  AC_MSG_ERROR([The modf function cannot be found.]) 
)
AC_CHECK_FUNCS([regcomp])

AC_CHECK_DECLS(
  [kqueue, kevent],
  ,
  ,
  [
    AC_INCLUDES_DEFAULT
    [#include <sys/event.h>]
    [#include <sys/types.h>]
    [#include <sys/time.h>]
  ]
)

AX_CXX_HAVE_THREAD_LOCAL

if ! test "x${ax_cv_cxx_have_thread_local}" = "xyes" ; then
  AC_WARN(AC_PACKAGE_NAME [is missing some functions because the current combination of compiler and libraries do not support the thread_local storage specifier].)
fi

AC_OUTPUT
