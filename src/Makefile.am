MAINTAINERCLEANFILES = Makefile.in

#Library specific CPPFLAGS

if DEBUG
	AM_CFLAGS = -g3 -O0
	AM_CXXFLAGS = -g3 -O0
else
	AM_CFLAGS = -O2
	AM_CXXFLAGS = -O2
endif

hp8970_CPPFLAGS = "-I$(top_srcdir)/include"
hp8970_CPPFLAGS += @GLIB_CFLAGS@ @GTK4_CFLAGS@ @json_CFLAGS@ @librsvg_CFLAGS@

hp8970_CFLAGS = $(AM_CFLAGS)
hp8970_CXXFLAGS = $(AM_CXXFLAGS)

hp8970_LDFLAGS = -lgpib -lm -lgs -rdynamic
hp8970_LDFLAGS += @GLIB_LIBS@ @GTK4_LIBS@ @json_LIBS@ @librsvg_LIBS@

#
# bin program
#

# Program name
bin_PROGRAMS = hp8970

hp8970_SOURCES = CairoPlot.c catalogWidgets.c g_settings-save+restore.c GPIBcommsThread.c GTKmainDialog.c \
				 GTKpageExtLO.c GTKpageGPIB.c GTKpageNoiseSource.c GTKpageNotes.c \
				 GTKpageOptions.c GTKpagePlot.c GTKpageSpecialFns.c \
				 HP8970.c HP8970comms.c HP8970-GTK4.c HP8970sweep.c HPlogo.c  \
				 JSON-save+restore.c messageEvent.c PDF+SVG+PNGwidgetCallback.c \
				 printWidgetCallback.c utility.c 


hp8970_SOURCES += $(top_srcdir)/include/GPIBcomms.h \
				  $(top_srcdir)/include/GTKcallbacks.h \
				  $(top_srcdir)/include/HP8790.h \
				  $(top_srcdir)/include/HP8970comms.h \
				  $(top_srcdir)/include/messageEvent.h \
				  $(top_srcdir)/include/widgetID.h

				  

