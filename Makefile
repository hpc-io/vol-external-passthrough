#Your HDF5 install path
#HDF5_DIR=../build_hdf5/hdf5
HDF5_DIR=${HDF5_ROOT}

#MPI_DIR=/usr/local

CC=mpicc
#CC=gcc-9
AR=ar

DEBUG=-DENABLE_EXT_PASSTHRU_LOGGING -g -O0
#INCLUDES=-I$(MPI_DIR)/include -I$(HDF5_DIR)/include
INCLUDES=-I$(HDF5_DIR)/include -I$(HDF5_DIR)/../vol/include
CFLAGS = $(DEBUG) -fPIC $(INCLUDES) -Wall
#LIBS=-L$(HDF5_DIR)/lib -L$(MPI_DIR)/lib -lhdf5 -lz
LIBS=-L$(HDF5_DIR)/lib -lhdf5 -lz 
DYNLDFLAGS = $(DEBUG) -fPIC $(LIBS)
LDFLAGS = $(DEBUG) $(LIBS)
ARFLAGS = rs

DYNSRC = H5VLpassthru_ext.c
DYNOBJ = $(DYNSRC:.c=.o)
DYNLIB = libh5passthrough_vol.dylib

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

all: $(EXEXE) $(ASYNC_EXEXE) $(DYNLIB) $(STATLIB)

$(EXEXE): $(EXSRC) $(STATLIB) $(DYNLIB)
	$(CC) $(CFLAGS) $^ -o $(EXEXE) $(LDFLAGS) -L. -lnew_h5api

$(ASYNC_EXEXE): $(ASYNC_EXSRC) $(STATLIB) $(DYNLIB)
	$(CC) $(CFLAGS) $^ -o $(ASYNC_EXEXE) $(LDFLAGS) -L. -lnew_h5api

$(DYNLIB): $(DYNSRC)
	$(CC) -shared $(CFLAGS) $(DYNLDFLAGS) $^ -o $@
	cp $(DYNLIB) $(HDF5_ROOT)/../vol/lib

$(STATOBJ): $(STATSRC)
	$(CC) -c $(CFLAGS) $^ -o $(STATOBJ)

$(STATLIB): $(STATOBJ)
	$(AR) $(ARFLAGS) $@ $^
test_dataset: test_dataset.o 
	$(CC) -o test_dataset test_dataset.o -L$(HDF5_ROOT)/lib -lhdf5

test_dataset_empty: test_dataset_empty.o 
	$(CC) -o test_dataset_empty test_dataset_empty.o -L$(HDF5_ROOT)/lib -lhdf5

test_file: test_file.o 
	$(CC) -o test_file test_file.o -L$(HDF5_ROOT)/lib -lhdf5


test_group: test_group.o 
	$(CC) -o test_group test_group.o -L$(HDF5_ROOT)/lib -lhdf5

.PHONY: clean all
clean:
	rm -rf $(DYNOBJ) $(DYNLIB) $(DYNDBG) \
            $(STATOBJ) $(STATLIB) \
            $(EXOBJ) $(EXEXE) $(EXDBG) \
            $(ASYNC_EXOBJ) $(ASYNC_EXEXE) $(ASYNC_EXDBG) \
            $(DATAFILE)
