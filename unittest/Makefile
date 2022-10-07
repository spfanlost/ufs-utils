CC=gcc
# CC=clang
AM_CFLAGS = -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=2
CFLAGS=-g -W -Wall -O2 -static

#CXXFLAGS = -DDEBUG
CHECKFLAGS = -Wall  -Wundef -Wno-missing-braces
override CFLAGS := $(CHECKFLAGS) $(CXXFLAGS)

APP_NAME := test

SOURCES := 

OBJECTS = \
	../ufs.o \
	../ufs_cmds.o \
	../options.o \
	../scsi_bsg_util.o \
	../ufs_err_hist.o \
	../unipro.o \
	../ufs_ffu.o \
	../ufs_vendor.o\
	../hmac_sha2.o \
	../sha2.o \
	../ufs_rpmb.o \
	../ufs_hmr.o \
	test.o \
	unittest.o \
	test_send_cmd.o \
	test_send_scsi.o \
	test_send_uic.o \
	test_tdm.o \
	test_0_full_disk_wr.o

INCLUDE :=

all: $(APP_NAME)
$(APP_NAME): $(OBJECTS)
	$(CC) $(AM_CFLAGS) $(CFLAGS) $(OBJECTS) -o $(APP_NAME) $(LDFLAGS)

clean:
	rm -f ../*.o*
	rm -f *.o
	rm -f *.bin
	rm -f *.dat
	rm -f $(APP_NAME)

.PHONY: all clean doc

