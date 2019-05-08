AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

bin_PROGRAMS = ubxlog

ubxlog_SOURCES = ubx_log.c
ubxlog_LDADD = $(top_builddir)/libubx/librtlog_client.la

dist_bin_SCRIPTS = ubx_genblock \
		   ubx_tocarr \
		   ubx_launch \
                   ubx_ilaunch

