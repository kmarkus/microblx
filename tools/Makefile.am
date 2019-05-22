AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

bin_PROGRAMS = ubx_log

ubx_log_SOURCES = $(top_srcdir)/libubx/ubx.h ubx_log.c
ubx_log_LDADD = $(top_builddir)/libubx/librtlog_client.la

dist_bin_SCRIPTS = ubx_genblock \
		   ubx_tocarr \
		   ubx_launch \
                   ubx_ilaunch

