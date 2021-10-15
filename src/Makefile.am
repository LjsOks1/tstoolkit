# name of your binary
bin_PROGRAMS = tstk

# list of source files
# the prefix is the name of the binary
tstk_SOURCES = main.c buffer.c x31.c scte35.c ait.c pmt.c tdt.c eit.c

# list of headers we're not going to install
noinst_HEADERS = buffer.h 


# our CFLAGS and LDFLAGS used for compiling and linking
# make sure you prefix these with the name of your binary
tstk_CFLAGS = $(GST_CFLAGS)
tstk_LDFLAGS = $(GST_LIBS) -lgstapp-1.0 -lgstmpegts-1.0
