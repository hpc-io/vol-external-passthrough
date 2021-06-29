#include "hdf5.h"
#include "mpi.h"
#include "stdlib.h"
#include "stdio.h"
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
void int2char(int a, char str[255]) {
  sprintf(str, "%d", a);
}


int main(int argc, char **argv) {
  // Assuming that the dataset is a two dimensional array of 8x5 dimension;
  size_t d1 = 2048; 
  size_t d2 = 2048;
  hsize_t ldims[2] = {d1, d2};
  hsize_t oned = d1*d2;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Info info = MPI_INFO_NULL;
  int rank, nproc, provided; 
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  MPI_Comm_size(comm, &nproc);
  MPI_Comm_rank(comm, &rank);
  hsize_t gdims[2] = {d1*nproc, d2};
  if (rank==0) {
    printf("=============================================\n");
    printf(" Buf dim: %llu x %llu\n",  ldims[0], ldims[1]);
    printf("   nproc: %d\n", nproc);
    printf("=============================================\n");
  }
  hsize_t offset[2] = {0, 0};
  // setup file access property list for mpio
  hid_t plist_id = H5Pcreate(H5P_FILE_ACCESS);
  H5Pset_fapl_mpio(plist_id, comm, info);
  bool p = true; 
  char f[255];
  strcpy(f, "parallel_file.h5");
  // create memory space
  hid_t memspace = H5Screate_simple(2, ldims, NULL);
  // define local data
  int* data = (int*)malloc(ldims[0]*ldims[1]*sizeof(int));
  // set up dataset access property list
  for(int i=0; i<ldims[0]*ldims[1]; i++)
    data[i] = rank+1; 
  hid_t dxf_id = H5Pcreate(H5P_DATASET_XFER);
  herr_t ret = H5Pset_dxpl_mpio(dxf_id, H5FD_MPIO_COLLECTIVE);
  printf("set dxpl: %d\n",ret);
  
  hid_t filespace = H5Screate_simple(2, gdims, NULL);
  hid_t dt = H5Tcopy(H5T_NATIVE_INT);

  hsize_t count[2] = {1, 1};
  hid_t file_id = H5Fcreate(f, H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);
  int niter = 1;
  offset[0]= rank*ldims[0];
  H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, ldims, count);
  char str[255];
  for(int it=0; it<niter; it++) {
    int2char(it, str);
    hid_t grp_id = H5Gcreate(file_id, str, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    hid_t dset = H5Dcreate(grp_id, "dset_test", H5T_NATIVE_INT, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    //hid_t dset2 = H5Dcreate(grp_id, "dset_test2", H5T_NATIVE_INT, filespace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    hid_t status = H5Dwrite(dset, H5T_NATIVE_INT, memspace, filespace, dxf_id, data); // write memory to file
    //hid_t status2 = H5Dwrite(dset2, H5T_NATIVE_INT, memspace, filespace, dxf_id, data); // write memory to file
    H5Dclose(dset);
    //H5Dclose(dset2); 
    H5Gclose(grp_id);
  }
  H5Fflush(file_id, H5F_SCOPE_LOCAL);
  H5Fclose(file_id);
  H5Pclose(dxf_id);
  H5Sclose(filespace);
  H5Sclose(memspace); 
  MPI_Finalize();
  return 0;
}
