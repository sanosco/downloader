###############################################################################
#
#  test project make file.
#
#  INCLDIRS = include1 include2 etc...
#  LIBNAMES = name1 name2 name3 etc...
#  LIBDIRS = dir1 dir2 dir3 etc...
#
#  31.01.2017
#
###############################################################################

APPLNAME = download-file

LIBNAMES = asan stdc++fs

LIBDIRS =

INCLDIRS =

CXXFLAGS = -fsanitize=address

include ./common-appl.mk
