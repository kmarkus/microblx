AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

pkglib_LTLIBRARIES = libubx.la \
                     cppdemo.la \
                     hexdump.la \
                     lfds_cyclic.la \
                     logger.la \
                     luablock.la \
                     mqueue.la \
                     ptrig.la \
                     random.la \
                     simple_fifo.la \
                     webif.la \
                     kdl_types.la \
                     stdtypes.la \
                     testtypes.la

#=======================================
# libubx : The microblx library
libubx_includes = libubx/ubx.h libubx/uthash.h libubx/uthash_ffi.h \
                  libubx/ubx_types.h libubx/ubx_proto.h
libubx_la_SOURCES = $(libubx_includes) \
                    libubx/ubx.c
#include_HEADERS = libubx/ubx.h
libubx/ubx_proto.h:
	cproto libubx/ubx.c > $@

%.h.hexarr: %.h
	tools/file2carr.lua $<

#======================================
%.lua.hexarr: %.lua
	tools/file2carr.lua $<

BUILT_SOURCES = std_blocks/logging/file_logger.lua.hexarr \
                std_blocks/ptrig/types/ptrig_config.h.hexarr \
                std_blocks/ptrig/types/ptrig_period.h.hexarr \
                std_blocks/ptrig/types/ptrig_tstat.h.hexarr \
                std_blocks/random/types/random_config.h.hexarr \
                std_blocks/webif/webif.lua.hexarr \
                std_types/kdl/types/kdl_frame.h.hexarr \
                std_types/kdl/types/kdl_rotation.h.hexarr \
                std_types/kdl/types/kdl_vector.h.hexarr \
                std_types/kdl/types/kdl.h.hexarr \
                std_types/kdl/types/kdl_twist.h.hexarr \
                std_types/kdl/types/kdl_wrench.h.hexarr \
                std_types/testtypes/types/test_trig_conf.h.hexarr

# cppdemo : CPP Demo block
cppdemo_la_includes = libubx/ubx.h
cppdemo_la_SOURCES = $(cppdemo_ls_includes) \
                     std_blocks/cppdemo/cppdemo.cpp
cppdemo_la_LIBADD = libubx.la
cppdemo_la_CPPFLAGS = -I$(top_srcdir)/libubx
cppdemo_la_LDFLAGS = -module -avoid-version

#======================================
# hexdump : Hexdump block
hexdump_la_includes = libubx/ubx.h
hexdump_la_SOURCES = $(hexdump_la_includes) \
                     std_blocks/hexdump/hexdump.c
hexdump_la_LIBADD = libubx.la
hexdump_la_CPPFLAGS = -I$(top_srcdir)/libubx
hexdump_la_LDFLAGS = -module -avoid-version

#======================================
# lfds_cyclic : A lock free interaction block
lfds_cyclic_la_includes = libubx/ubx.h
lfds_cyclic_la_SOURCES = $(lfds_cyclic_la_includes) \
                     std_blocks/lfds_buffers/lfds_cyclic.c
lfds_cyclic_la_LIBADD = std_blocks/lfds_buffers/liblfds611/bin/liblfds611.a \
                         libubx.la
lfds_cyclic_la_CPPFLAGS = -I$(top_srcdir)/libubx
lfds_cyclic_la_LDFLAGS = -module -avoid-version

std_blocks/lfds_buffers/liblfds611/bin/liblfds611.a:
	$(shell mkdir -p std_blocks/lfds_buffers/liblfds611/obj \
                         std_blocks/lfds_buffers/liblfds611/bin)
	make -C std_blocks/lfds_buffers/liblfds611

#======================================
# logger : Logger block
logger_la_includes = libubx/ubx.h libubx/ubx_types.h
logger_la_SOURCES = $(logger_la_includes) \
                    std_blocks/logging/file_logger.lua.hexarr \
                    std_blocks/logging/file_logger.c
logger_la_LIBADD = libubx.la
logger_la_CPPFLAGS = -I$(top_srcdir)/libubx
logger_la_LDFLAGS = -module -avoid-version

#======================================
# luablock : Lua block
luablock_la_includes = libubx/ubx.h
luablock_la_SOURCES = $(luablock_la_includes) \
                       std_blocks/luablock/luablock.c
luablock_la_LIBADD = libubx.la
luablock_la_CPPFLAGS = -I$(top_srcdir)/libubx
luablock_la_LDFLAGS = -module -avoid-version

#======================================
# mqueue : Message queue block
mqueue_la_includes = libubx/ubx.h
mqueue_la_SOURCES = $(mqueue_la_includes) \
                       std_blocks/mqueue/mqueue.c
mqueue_la_LIBADD = libubx.la
mqueue_la_CPPFLAGS = -I$(top_srcdir)/libubx
mqueue_la_LDFLAGS = -module -avoid-version

#======================================
# ptrig : Pthread based trigger block
PTRIG_HEXARRS = std_blocks/ptrig/types/ptrig_config.h.hexarr \
                std_blocks/ptrig/types/ptrig_period.h.hexarr \
                std_blocks/ptrig/types/ptrig_tstat.h.hexarr
ptrig_la_includes = libubx/ubx.h libubx/ubx_types.h
ptrig_la_SOURCES = $(ptrig_la_includes) \
                   $(PTRIG_HEXARRS) \
                   std_blocks/ptrig/ptrig.c
ptrig_la_LIBADD = libubx.la
ptrig_la_CPPFLAGS = -I$(top_srcdir)/libubx
ptrig_la_LDFLAGS = -module -avoid-version

#======================================
# random : Random number generator block
RANDOM_HEXARRS = std_blocks/random/types/random_config.h.hexarr
random_la_includes = libubx/ubx.h
random_la_SOURCES = $(random_la_includes) \
                   $(RANDOM_HEXARRS) \
                   std_blocks/random/random.c
random_la_LIBADD = libubx.la
random_la_CPPFLAGS = -I$(top_srcdir)/libubx
random_la_LDFLAGS = -module -avoid-version

#======================================
# simple_fifo : Fifoed interaction block
simple_fifo_la_includes = libubx/ubx.h
simple_fifo_la_SOURCES = $(simple_fifo_la_includes) \
                   std_blocks/simple_fifo/simple_fifo.c
simple_fifo_la_LIBADD = libubx.la
simple_fifo_la_CPPFLAGS = -I$(top_srcdir)/libubx
simple_fifo_la_LDFLAGS = -module -avoid-version

#======================================
# webif : A system monitoring web-interface function block.
webif_la_includes = libubx/ubx.h
webif_la_SOURCES = $(webif_la_includes) \
                   std_blocks/webif/webif.lua.hexarr \
                   std_blocks/webif/webif.c \
                   std_blocks/webif/mongoose.c
webif_la_LIBADD = libubx.la
webif_la_CPPFLAGS = -I$(top_srcdir)/libubx
webif_la_LDFLAGS = -module -avoid-version

#======================================
# kdl_types : The basic types
KDL_TYPES_HEXARRS = std_types/kdl/types/kdl_frame.h.hexarr \
              std_types/kdl/types/kdl_rotation.h.hexarr \
              std_types/kdl/types/kdl_vector.h.hexarr \
              std_types/kdl/types/kdl.h.hexarr \
              std_types/kdl/types/kdl_twist.h.hexarr \
              std_types/kdl/types/kdl_wrench.h.hexarr
kdl_types_la_includes = libubx/ubx.h
kdl_types_la_SOURCES = $(kdl_types_la_includes) \
                   $(KDL_TYPES_HEXARRS) \
                   std_types/kdl/kdl_types.c
kdl_types_la_LIBADD = libubx.la
kdl_types_la_CPPFLAGS = -I$(top_srcdir)/libubx
kdl_types_la_LDFLAGS = -module -avoid-version

#======================================
# stdtypes : The standard types
stdtypes_la_includes = libubx/ubx.h
stdtypes_la_SOURCES = $(stdtypes_la_includes) \
                   std_types/stdtypes/stdtypes.c
stdtypes_la_LIBADD = libubx.la
stdtypes_la_CPPFLAGS = -I$(top_srcdir)/libubx
stdtypes_la_LDFLAGS = -module -avoid-version

#======================================
# testtypes : The basic types
TESTTYPES_HEXARRS = std_types/testtypes/types/test_trig_conf.h.hexarr
testtypes_la_includes = libubx/ubx.h
testtypes_la_SOURCES = $(testtypes_la_includes) \
                   $(TESTTYPES_HEXARRS) \
                   std_types/testtypes/testtypes.c
testtypes_la_LIBADD = libubx.la
testtypes_la_CPPFLAGS = -I$(top_srcdir)/libubx
testtypes_la_LDFLAGS = -module -avoid-version

#======================================
# Scripts etc
luadir = $(datadir)/lua/5.1
lua_SCRIPTS = lua/ansicolors.lua \
              lua/cdata.lua \
              lua/reflect.lua \
              lua/ubx.lua \
              lua/blockdiagram.lua \
              lua/cdata_tolua_test.lua \
              lua/strict.lua \
              lua/ubx_utils.lua \
              lua/cdata_layout.lua \
              lua/luaffi-introspection.lua \
              lua/time.lua \
              lua/utils.lua \
              tools/file2carr.lua \
              tools/ubx_genblock.lua \
              tools/ubx_launch.lua \
              tools/luaffi_load_file.lua \
              tools/ubx_launch \
              examples/block_model_example.lua \
              examples/mqueue_test.usc \
              examples/youbot_test.lua \
              examples/cpp_block_demo.usc \
              examples/trig_rnd_to_hexdump.lua \
              examples/minimal.usc \
              examples/youbot_basic.usc

ubxdir = $(datadir)/microblx
ubxincludedir = $(ubxdir)/include
ubxinclude_DATA = libubx/uthash_ffi.h \
                  libubx/ubx_types.h \
                  libubx/ubx_proto.h

CLEANFILES = $(BUILT_SOURCES)
