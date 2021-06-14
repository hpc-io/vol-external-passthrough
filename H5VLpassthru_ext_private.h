/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:	The private header file for the pass-through VOL connector.
 */

#ifndef _H5VLpassthru_ext_private_H
#define _H5VLpassthru_ext_private_H

/* Public headers needed by this file */
#include "H5VLpassthru_ext.h"        /* Public header for connector */

/* Private characteristics of the pass-through VOL connector */
#define H5VL_PASSTHRU_EXT_VERSION     0

/* Names for dynamically registered operations */
#define H5VL_PASSTHRU_EXT_DYN_FOO        "org.example.ext_passthru.foo"
#define H5VL_PASSTHRU_EXT_DYN_BAR        "org.example.ext_passthru.bar"
#define H5VL_PASSTHRU_EXT_DYN_FIDDLE     "org.example.ext_passthru.fiddle"

/* Parameters for each of the dynamically registered operations */

/* H5VL_PASSTHRU_EXT_DYN_FOO */
typedef struct H5VL_passthru_ext_dataset_foo_args_t {
    int i;
    double d;
} H5VL_passthru_ext_dataset_foo_args_t;

/* H5VL_PASSTHRU_EXT_DYN_BAR */
typedef struct H5VL_passthru_ext_dataset_bar_args_t {
    double *dp;
    unsigned *up;
} H5VL_passthru_ext_dataset_bar_args_t;

/* H5VL_PASSTHRU_EXT_DYN_FIDDLE */
    /* No args */


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif /* _H5VLpassthru_ext_private_H */

