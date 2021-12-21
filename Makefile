#Your HDF5 install path
##HDF5_DIR=../build_hdf5/hdf5
#HDF5_DIR=/Users/koziol/HDF5/github/hpc-io/async_vol_register_optional/build_parallel_debug/hdf5
#MPI_DIR=/usr/local

CC=mpicc
#CC=gcc-9
AR=ar

DEBUG=-DENABLE_EXT_PASSTHRU_LOGGING -g -O0
#INCLUDES=-I$(MPI_DIR)/include -I$(HDF5_DIR)/include
INCLUDES=-I$(HDF5_DIR)/include
CFLAGS = $(DEBUG) -fPIC $(INCLUDES) -Wall
#LIBS=-L$(HDF5_DIR)/lib -L$(MPI_DIR)/lib -lhdf5 -lz
LIBS=-L$(HDF5_DIR)/lib -lhdf5 -lz -L$(HDF5_VOL_DIR)/lib/ -lh5async
# Uncomment this line Linux builds:
# DYNLDFLAGS = $(DEBUG) -shared -fPIC $(LIBS)
# Uncomment this line MacOS builds:
DYNLDFLAGS = $(DEBUG) -shared  -fPIC $(LIBS)
LDFLAGS = $(DEBUG) $(LIBS)
ARFLAGS = rs

DYNSRC = H5VLpassthru_ext.c
DYNOBJ = $(DYNSRC:.c=.o)
# Uncomment this line Linux builds:
# DYNLIB = libh5passthrough_vol.so
# Uncomment this line MacOS builds:
DYNLIB = libh5passthrough_vol.so
DYNDBG = libh5passthrough_vol.so.dSYM

STATSRC = new_h5api.c
STATOBJ = $(STATSRC:.c=.o)
STATLIB = libnew_h5api.a

EXSRC = new_h5api_ex.c
EXOBJ = $(EXSRC:.c=.o)
EXEXE = new_h5api_ex.exe
EXDBG = new_h5api_ex.exe.dSYM

ASYNC_EXSRC = async_new_h5api_ex.c
ASYNC_EXOBJ = $(ASYNC_EXSRC:.c=.o)
ASYNC_EXEXE = async_new_h5api_ex.exe
ASYNC_EXDBG = async_new_h5api_ex.exe.dSYM

DATAFILE = testfile.h5

all: $(EXEXE) $(ASYNC_EXEXE) $(DYNLIB) $(STATLIB) test_file test_group test_dataset test_dataset_write

$(EXEXE): $(EXSRC) $(STATLIB) $(DYNLIB)
	$(CC) $(CFLAGS) $^ -o $(EXEXE) $(LDFLAGS) -L. -lnew_h5api


test_file: test_file.o
	$(CC) $(CFLAGS) $^ -o test_file $(LDFLAGS) -L. -lnew_h5api

test_group: test_group.o
	$(CC) $(CFLAGS) $^ -o test_group $(LDFLAGS) -L. -lnew_h5api

test_dataset: test_dataset.o
	$(CC) $(CFLAGS) $^ -o test_dataset $(LDFLAGS) -L. -lnew_h5api

test_dataset_write: test_dataset_write.o 
	$(CC) -o test_dataset_write test_dataset_write.o -L$(HDF5_ROOT)/lib -lhdf5


$(ASYNC_EXEXE): $(ASYNC_EXSRC) $(STATLIB) $(DYNLIB)
	$(CC) $(CFLAGS) $^ -o $(ASYNC_EXEXE) $(LDFLAGS) -L. -lnew_h5api

$(DYNLIB): $(DYNSRC)
	$(CC) $(CFLAGS) $(DYNLDFLAGS) $^ -o $@
	cp -v *.h $(HDF5_VOL_DIR)/include
	cp -v $(DYNLIB) $(HDF5_VOL_DIR)/lib
	
$(STATOBJ): $(STATSRC)
	$(CC) -c $(CFLAGS) $^ -o $(STATOBJ)

$(STATLIB): $(STATOBJ)
	$(AR) $(ARFLAGS) $@ $^
	cp -v $(STATLIB) $(HDF5_VOL_DIR)/lib
	
.PHONY: clean all
clean:
	rm -rf $(DYNOBJ) $(DYNLIB) $(DYNDBG) \
            $(STATOBJ) $(STATLIB) \
            $(EXOBJ) $(EXEXE) $(EXDBG) \
            $(ASYNC_EXOBJ) $(ASYNC_EXEXE) $(ASYNC_EXDBG) \
            $(DATAFILE) *.80s-* test_dataset test_file test_group test_dataset_write
