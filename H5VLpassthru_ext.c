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
 * Purpose:     This is a "pass through" VOL connector, which forwards each
 *              VOL callback to an underlying connector.
 *
 *              It is designed as an example VOL connector for developers to
 *              use when creating new connectors, especially connectors that
 *              are outside of the HDF5 library.  As such, it should _NOT_
 *              include _any_ private HDF5 header files.  This connector should
 *              therefore only make public HDF5 API calls and use standard C /
 *              POSIX calls.
 *
 *              Note that the HDF5 error stack must be preserved on code paths
 *              that could be invoked when the underlying VOL connector's
 *              callback can fail.
 *
 */


/* Header files needed */
/* Do NOT include private HDF5 files here! */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "H5Pdevelop.h"
/* Public HDF5 headers */
#include "hdf5.h"
#include "hdf5dev.h"
#include "h5_async_lib.h"
#include "h5_async_vol.h"
/* This connector's private header */
#include "H5VLpassthru_ext_private.h"
#define TRUE true
/**********/
/* Macros */
/**********/

/* Whether to display log messge when callback is invoked */
/* (Uncomment to enable) */
/* #define ENABLE_EXT_PASSTHRU_LOGGING */

/* Hack for missing va_copy() in old Visual Studio editions
 * (from H5win2_defs.h - used on VS2012 and earlier)
 */
#if defined(_WIN32) && defined(_MSC_VER) && (_MSC_VER < 1800)
#define va_copy(D,S)      ((D) = (S))
#endif

/************/
/* Typedefs */
/************/

/* The pass through VOL info object */
typedef struct H5VL_pass_through_ext_t {
    hid_t  under_vol_id;        /* ID for underlying VOL connector */
    void   *under_object;       /* Info object for underlying VOL connector */
    void   *parent;
    hid_t  m_id; 
    hid_t  es_id; 
} H5VL_pass_through_ext_t;

/* The pass through VOL wrapper context */
typedef struct H5VL_pass_through_ext_wrap_ctx_t {
    hid_t under_vol_id;         /* VOL ID for under VOL */
    void *under_wrap_ctx;       /* Object wrapping context for under VOL */
} H5VL_pass_through_ext_wrap_ctx_t;


/********************* */
/* Function prototypes */
/********************* */

/* Helper routines */
static H5VL_pass_through_ext_t *H5VL_pass_through_ext_new_obj(void *under_obj,
    hid_t under_vol_id);
static herr_t H5VL_pass_through_ext_free_obj(H5VL_pass_through_ext_t *obj);

/* "Management" callbacks */
static herr_t H5VL_pass_through_ext_init(hid_t vipl_id);
static herr_t H5VL_pass_through_ext_term(void);

/* VOL info callbacks */
static void *H5VL_pass_through_ext_info_copy(const void *info);
static herr_t H5VL_pass_through_ext_info_cmp(int *cmp_value, const void *info1, const void *info2);
static herr_t H5VL_pass_through_ext_info_free(void *info);
static herr_t H5VL_pass_through_ext_info_to_str(const void *info, char **str);
static herr_t H5VL_pass_through_ext_str_to_info(const char *str, void **info);

/* VOL object wrap / retrieval callbacks */
static void *H5VL_pass_through_ext_get_object(const void *obj);
static herr_t H5VL_pass_through_ext_get_wrap_ctx(const void *obj, void **wrap_ctx);
static void *H5VL_pass_through_ext_wrap_object(void *obj, H5I_type_t obj_type,
    void *wrap_ctx);
static void *H5VL_pass_through_ext_unwrap_object(void *obj);
static herr_t H5VL_pass_through_ext_free_wrap_ctx(void *obj);

/* Attribute callbacks */
static void *H5VL_pass_through_ext_attr_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t type_id, hid_t space_id, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req);
static void *H5VL_pass_through_ext_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t aapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_attr_read(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_attr_write(void *attr, hid_t mem_type_id, const void *buf, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_attr_get(void *obj, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_attr_specific(void *obj, const H5VL_loc_params_t *loc_params, H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_attr_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_attr_close(void *attr, hid_t dxpl_id, void **req);

/* Dataset callbacks */
static void *H5VL_pass_through_ext_dataset_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req);
static void *H5VL_pass_through_ext_dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t dapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_dataset_read(void *dset, hid_t mem_type_id, hid_t mem_space_id,
                                    hid_t file_space_id, hid_t plist_id, void *buf, void **req);
static herr_t H5VL_pass_through_ext_dataset_write(void *dset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t plist_id, const void *buf, void **req);
static herr_t H5VL_pass_through_ext_dataset_get(void *dset, H5VL_dataset_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_dataset_specific(void *obj, H5VL_dataset_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_dataset_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_dataset_close(void *dset, hid_t dxpl_id, void **req);

/* Datatype callbacks */
static void *H5VL_pass_through_ext_datatype_commit(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id, void **req);
static void *H5VL_pass_through_ext_datatype_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t tapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_datatype_get(void *dt, H5VL_datatype_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_datatype_specific(void *obj, H5VL_datatype_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_datatype_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_datatype_close(void *dt, hid_t dxpl_id, void **req);

/* File callbacks */
static void *H5VL_pass_through_ext_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id, void **req);
static void *H5VL_pass_through_ext_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_file_get(void *file, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_file_specific(void *file, H5VL_file_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_file_optional(void *file, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_file_close(void *file, hid_t dxpl_id, void **req);

/* Group callbacks */
static void *H5VL_pass_through_ext_group_create(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req);
static void *H5VL_pass_through_ext_group_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name, hid_t gapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_group_get(void *obj, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_group_specific(void *obj, H5VL_group_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_group_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_group_close(void *grp, hid_t dxpl_id, void **req);

/* Link callbacks */
static herr_t H5VL_pass_through_ext_link_create(H5VL_link_create_args_t *args, void *obj, const H5VL_loc_params_t *loc_params, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_link_copy(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_link_move(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_link_get(void *obj, const H5VL_loc_params_t *loc_params, H5VL_link_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_link_specific(void *obj, const H5VL_loc_params_t *loc_params, H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_link_optional(void *obj, const H5VL_loc_params_t *loc_params, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);

/* Object callbacks */
static void *H5VL_pass_through_ext_object_open(void *obj, const H5VL_loc_params_t *loc_params, H5I_type_t *opened_type, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_object_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, const char *src_name, void *dst_obj, const H5VL_loc_params_t *dst_loc_params, const char *dst_name, hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_object_get(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_object_specific(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL_pass_through_ext_object_optional(void *obj, const H5VL_loc_params_t *loc_params, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);

/* Container/connector introspection callbacks */
static herr_t H5VL_pass_through_ext_introspect_get_conn_cls(void *obj, H5VL_get_conn_lvl_t lvl, const H5VL_class_t **conn_cls);
static herr_t H5VL_pass_through_ext_introspect_get_cap_flags(const void *info, unsigned *cap_flags);
static herr_t H5VL_pass_through_ext_introspect_opt_query(void *obj, H5VL_subclass_t cls, int op_type, uint64_t *flags);

/* Async request callbacks */
static herr_t H5VL_pass_through_ext_request_wait(void *req, uint64_t timeout, H5VL_request_status_t *status);
static herr_t H5VL_pass_through_ext_request_notify(void *obj, H5VL_request_notify_t cb, void *ctx);
static herr_t H5VL_pass_through_ext_request_cancel(void *req, H5VL_request_status_t *status);
static herr_t H5VL_pass_through_ext_request_specific(void *req, H5VL_request_specific_args_t *args);
static herr_t H5VL_pass_through_ext_request_optional(void *req, H5VL_optional_args_t *args);
static herr_t H5VL_pass_through_ext_request_free(void *req);

/* Blob callbacks */
static herr_t H5VL_pass_through_ext_blob_put(void *obj, const void *buf, size_t size, void *blob_id, void *ctx);
static herr_t H5VL_pass_through_ext_blob_get(void *obj, const void *blob_id, void *buf, size_t size, void *ctx);
static herr_t H5VL_pass_through_ext_blob_specific(void *obj, void *blob_id, H5VL_blob_specific_args_t *args);
static herr_t H5VL_pass_through_ext_blob_optional(void *obj, void *blob_id, H5VL_optional_args_t *args);

/* Token callbacks */
static herr_t H5VL_pass_through_ext_token_cmp(void *obj, const H5O_token_t *token1, const H5O_token_t *token2, int *cmp_value);
static herr_t H5VL_pass_through_ext_token_to_str(void *obj, H5I_type_t obj_type, const H5O_token_t *token, char **token_str);
static herr_t H5VL_pass_through_ext_token_from_str(void *obj, H5I_type_t obj_type, const char *token_str, H5O_token_t *token);

/* Generic optional callback */
static herr_t H5VL_pass_through_ext_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req);

/*******************/
/* Local variables */
/*******************/

/* Pass through VOL connector class struct */
static const H5VL_class_t H5VL_pass_through_ext_g = {
    H5VL_VERSION,                                       /* VOL class struct version */
    (H5VL_class_value_t)H5VL_PASSTHRU_EXT_VALUE,        /* value        */
    H5VL_PASSTHRU_EXT_NAME,                             /* name         */
    H5VL_PASSTHRU_EXT_VERSION,                          /* connector version */
    0,                                                  /* capability flags */
    H5VL_pass_through_ext_init,                         /* initialize   */
    H5VL_pass_through_ext_term,                         /* terminate    */
    {                                           /* info_cls */
        sizeof(H5VL_pass_through_ext_info_t),           /* size    */
        H5VL_pass_through_ext_info_copy,                /* copy    */
        H5VL_pass_through_ext_info_cmp,                 /* compare */
        H5VL_pass_through_ext_info_free,                /* free    */
        H5VL_pass_through_ext_info_to_str,              /* to_str  */
        H5VL_pass_through_ext_str_to_info               /* from_str */
    },
    {                                           /* wrap_cls */
        H5VL_pass_through_ext_get_object,               /* get_object   */
        H5VL_pass_through_ext_get_wrap_ctx,             /* get_wrap_ctx */
        H5VL_pass_through_ext_wrap_object,              /* wrap_object  */
        H5VL_pass_through_ext_unwrap_object,            /* unwrap_object */
        H5VL_pass_through_ext_free_wrap_ctx             /* free_wrap_ctx */
    },
    {                                           /* attribute_cls */
        H5VL_pass_through_ext_attr_create,              /* create */
        H5VL_pass_through_ext_attr_open,                /* open */
        H5VL_pass_through_ext_attr_read,                /* read */
        H5VL_pass_through_ext_attr_write,               /* write */
        H5VL_pass_through_ext_attr_get,                 /* get */
        H5VL_pass_through_ext_attr_specific,            /* specific */
        H5VL_pass_through_ext_attr_optional,            /* optional */
        H5VL_pass_through_ext_attr_close                /* close */
    },
    {                                           /* dataset_cls */
        H5VL_pass_through_ext_dataset_create,           /* create */
        H5VL_pass_through_ext_dataset_open,             /* open */
        H5VL_pass_through_ext_dataset_read,             /* read */
        H5VL_pass_through_ext_dataset_write,            /* write */
        H5VL_pass_through_ext_dataset_get,              /* get */
        H5VL_pass_through_ext_dataset_specific,         /* specific */
        H5VL_pass_through_ext_dataset_optional,         /* optional */
        H5VL_pass_through_ext_dataset_close             /* close */
    },
    {                                           /* datatype_cls */
        H5VL_pass_through_ext_datatype_commit,          /* commit */
        H5VL_pass_through_ext_datatype_open,            /* open */
        H5VL_pass_through_ext_datatype_get,             /* get_size */
        H5VL_pass_through_ext_datatype_specific,        /* specific */
        H5VL_pass_through_ext_datatype_optional,        /* optional */
        H5VL_pass_through_ext_datatype_close            /* close */
    },
    {                                           /* file_cls */
        H5VL_pass_through_ext_file_create,              /* create */
        H5VL_pass_through_ext_file_open,                /* open */
        H5VL_pass_through_ext_file_get,                 /* get */
        H5VL_pass_through_ext_file_specific,            /* specific */
        H5VL_pass_through_ext_file_optional,            /* optional */
        H5VL_pass_through_ext_file_close                /* close */
    },
    {                                           /* group_cls */
        H5VL_pass_through_ext_group_create,             /* create */
        H5VL_pass_through_ext_group_open,               /* open */
        H5VL_pass_through_ext_group_get,                /* get */
        H5VL_pass_through_ext_group_specific,           /* specific */
        H5VL_pass_through_ext_group_optional,           /* optional */
        H5VL_pass_through_ext_group_close               /* close */
    },
    {                                           /* link_cls */
        H5VL_pass_through_ext_link_create,              /* create */
        H5VL_pass_through_ext_link_copy,                /* copy */
        H5VL_pass_through_ext_link_move,                /* move */
        H5VL_pass_through_ext_link_get,                 /* get */
        H5VL_pass_through_ext_link_specific,            /* specific */
        H5VL_pass_through_ext_link_optional             /* optional */
    },
    {                                           /* object_cls */
        H5VL_pass_through_ext_object_open,              /* open */
        H5VL_pass_through_ext_object_copy,              /* copy */
        H5VL_pass_through_ext_object_get,               /* get */
        H5VL_pass_through_ext_object_specific,          /* specific */
        H5VL_pass_through_ext_object_optional           /* optional */
    },
    {                                           /* introspect_cls */
        H5VL_pass_through_ext_introspect_get_conn_cls,  /* get_conn_cls */
        H5VL_pass_through_ext_introspect_get_cap_flags, /* get_cap_flags */
        H5VL_pass_through_ext_introspect_opt_query,     /* opt_query */
    },
    {                                           /* request_cls */
        H5VL_pass_through_ext_request_wait,             /* wait */
        H5VL_pass_through_ext_request_notify,           /* notify */
        H5VL_pass_through_ext_request_cancel,           /* cancel */
        H5VL_pass_through_ext_request_specific,         /* specific */
        H5VL_pass_through_ext_request_optional,         /* optional */
        H5VL_pass_through_ext_request_free              /* free */
    },
    {                                           /* blob_cls */
        H5VL_pass_through_ext_blob_put,                 /* put */
        H5VL_pass_through_ext_blob_get,                 /* get */
        H5VL_pass_through_ext_blob_specific,            /* specific */
        H5VL_pass_through_ext_blob_optional             /* optional */
    },
    {                                           /* token_cls */
        H5VL_pass_through_ext_token_cmp,                /* cmp */
        H5VL_pass_through_ext_token_to_str,             /* to_str */
        H5VL_pass_through_ext_token_from_str              /* from_str */
    },
    H5VL_pass_through_ext_optional                  /* optional */
};

/* The connector identification number, initialized at runtime */
static hid_t H5VL_PASSTHRU_EXT_g = H5I_INVALID_HID;

/* Operation values for new "API" routines */
/* These are initialized in the VOL connector's 'init' callback at runtime.
 *      It's good practice to reset them back to -1 in the 'term' callback.
 */
static int H5VL_passthru_dataset_foo_op_g = -1;
static int H5VL_passthru_dataset_bar_op_g = -1;
static int H5VL_passthru_group_fiddle_op_g = -1;

/* Required shim routines, to enable dynamic loading of shared library */
/* The HDF5 library _must_ find routines with these names and signatures
 *      for a shared library that contains a VOL connector to be detected
 *      and loaded at runtime.
 */
H5PL_type_t H5PLget_plugin_type(void) {return H5PL_TYPE_VOL;}
const void *H5PLget_plugin_info(void) {return &H5VL_pass_through_ext_g;}


/*-------------------------------------------------------------------------
 * Function:    H5VL__pass_through_new_obj
 *
 * Purpose:     Create a new pass through object for an underlying object
 *
 * Return:      Success:    Pointer to the new pass through object
 *              Failure:    NULL
 *
 * Programmer:  Quincey Koziol
 *              Monday, December 3, 2018
 *
 *-------------------------------------------------------------------------
 */
static H5VL_pass_through_ext_t *
H5VL_pass_through_ext_new_obj(void *under_obj, hid_t under_vol_id)
{
    H5VL_pass_through_ext_t *new_obj;

    new_obj = (H5VL_pass_through_ext_t *)calloc(1, sizeof(H5VL_pass_through_ext_t));
    new_obj->under_object = under_obj;
    new_obj->under_vol_id = under_vol_id;
    H5Iinc_ref(new_obj->under_vol_id);

    return new_obj;
} /* end H5VL__pass_through_new_obj() */


/*-------------------------------------------------------------------------
 * Function:    H5VL__pass_through_free_obj
 *
 * Purpose:     Release a pass through object
 *
 * Note:	Take care to preserve the current HDF5 error stack
 *		when calling HDF5 API calls.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 * Programmer:  Quincey Koziol
 *              Monday, December 3, 2018
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_free_obj(H5VL_pass_through_ext_t *obj)
{
    hid_t err_id;

    err_id = H5Eget_current_stack();

    H5Idec_ref(obj->under_vol_id);

    H5Eset_current_stack(err_id);

    free(obj);

    return 0;
} /* end H5VL__pass_through_free_obj() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_register
 *
 * Purpose:     Register the pass-through VOL connector and retrieve an ID
 *              for it.
 *
 * Return:      Success:    The ID for the pass-through VOL connector
 *              Failure:    -1
 *
 * Programmer:  Quincey Koziol
 *              Wednesday, November 28, 2018
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL_pass_through_ext_register(void)
{
    /* Singleton register the pass-through VOL connector ID */
    if(H5VL_PASSTHRU_EXT_g < 0)
        H5VL_PASSTHRU_EXT_g = H5VLregister_connector(&H5VL_pass_through_ext_g, H5P_DEFAULT);

    return H5VL_PASSTHRU_EXT_g;
} /* end H5VL_pass_through_ext_register() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_init
 *
 * Purpose:     Initialize this VOL connector, performing any necessary
 *              operations for the connector that will apply to all containers
 *              accessed with the connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_init(hid_t vipl_id)
{
#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INIT\n");
#endif

    /* Shut compiler up about unused parameter */
    (void)vipl_id;

    /* Acquire operation values for new "API" routines to use */
    assert(-1 == H5VL_passthru_dataset_foo_op_g);
    if(H5VLregister_opt_operation(H5VL_SUBCLS_DATASET, H5VL_PASSTHRU_EXT_DYN_FOO, &H5VL_passthru_dataset_foo_op_g) < 0)
        return(-1);
    assert(-1 != H5VL_passthru_dataset_foo_op_g);
    assert(-1 == H5VL_passthru_dataset_bar_op_g);
    if(H5VLregister_opt_operation(H5VL_SUBCLS_DATASET, H5VL_PASSTHRU_EXT_DYN_BAR, &H5VL_passthru_dataset_bar_op_g) < 0)
        return(-1);
    assert(-1 != H5VL_passthru_dataset_bar_op_g);
    assert(-1 == H5VL_passthru_group_fiddle_op_g);
    if(H5VLregister_opt_operation(H5VL_SUBCLS_GROUP, H5VL_PASSTHRU_EXT_DYN_FIDDLE, &H5VL_passthru_group_fiddle_op_g) < 0)
        return(-1);
    assert(-1 != H5VL_passthru_group_fiddle_op_g);

    return 0;
} /* end H5VL_pass_through_ext_init() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_term
 *
 * Purpose:     Terminate this VOL connector, performing any necessary
 *              operations for the connector that release connector-wide
 *              resources (usually created / initialized with the 'init'
 *              callback).
 *
 * Return:      Success:    0
 *              Failure:    (Can't fail)
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_term(void)
{
#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL TERM\n");
#endif

    /* Reset VOL ID */
    H5VL_PASSTHRU_EXT_g = H5I_INVALID_HID;

    /* Reset operation values for new "API" routines */
    if(-1 != H5VL_passthru_dataset_foo_op_g) {
        if(H5VLunregister_opt_operation(H5VL_SUBCLS_DATASET, H5VL_PASSTHRU_EXT_DYN_FOO) < 0)
            return(-1);
        H5VL_passthru_dataset_foo_op_g = (-1);
    } /* end if */
    if(-1 != H5VL_passthru_dataset_bar_op_g) {
        if(H5VLunregister_opt_operation(H5VL_SUBCLS_DATASET, H5VL_PASSTHRU_EXT_DYN_BAR) < 0)
            return(-1);
        H5VL_passthru_dataset_bar_op_g = (-1);
    } /* end if */
    if(-1 != H5VL_passthru_group_fiddle_op_g) {
        if(H5VLunregister_opt_operation(H5VL_SUBCLS_GROUP, H5VL_PASSTHRU_EXT_DYN_FIDDLE) < 0)
            return(-1);
        H5VL_passthru_group_fiddle_op_g = (-1);
    } /* end if */

    return 0;
} /* end H5VL_pass_through_ext_term() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_info_copy
 *
 * Purpose:     Duplicate the connector's info object.
 *
 * Returns:     Success:    New connector info object
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_info_copy(const void *_info)
{
    const H5VL_pass_through_ext_info_t *info = (const H5VL_pass_through_ext_info_t *)_info;
    H5VL_pass_through_ext_info_t *new_info;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INFO Copy\n");
#endif

    /* Allocate new VOL info struct for the pass through connector */
    new_info = (H5VL_pass_through_ext_info_t *)calloc(1, sizeof(H5VL_pass_through_ext_info_t));

    /* Increment reference count on underlying VOL ID, and copy the VOL info */
    new_info->under_vol_id = info->under_vol_id;
    H5Iinc_ref(new_info->under_vol_id);
    if(info->under_vol_info)
        H5VLcopy_connector_info(new_info->under_vol_id, &(new_info->under_vol_info), info->under_vol_info);

    return new_info;
} /* end H5VL_pass_through_ext_info_copy() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_info_cmp
 *
 * Purpose:     Compare two of the connector's info objects, setting *cmp_value,
 *              following the same rules as strcmp().
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_info_cmp(int *cmp_value, const void *_info1, const void *_info2)
{
    const H5VL_pass_through_ext_info_t *info1 = (const H5VL_pass_through_ext_info_t *)_info1;
    const H5VL_pass_through_ext_info_t *info2 = (const H5VL_pass_through_ext_info_t *)_info2;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INFO Compare\n");
#endif

    /* Sanity checks */
    assert(info1);
    assert(info2);

    /* Initialize comparison value */
    *cmp_value = 0;

    /* Compare under VOL connector classes */
    H5VLcmp_connector_cls(cmp_value, info1->under_vol_id, info2->under_vol_id);
    if(*cmp_value != 0)
        return 0;

    /* Compare under VOL connector info objects */
    H5VLcmp_connector_info(cmp_value, info1->under_vol_id, info1->under_vol_info, info2->under_vol_info);
    if(*cmp_value != 0)
        return 0;

    return 0;
} /* end H5VL_pass_through_ext_info_cmp() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_info_free
 *
 * Purpose:     Release an info object for the connector.
 *
 * Note:	Take care to preserve the current HDF5 error stack
 *		when calling HDF5 API calls.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_info_free(void *_info)
{
    H5VL_pass_through_ext_info_t *info = (H5VL_pass_through_ext_info_t *)_info;
    hid_t err_id;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INFO Free\n");
#endif

    err_id = H5Eget_current_stack();

    /* Release underlying VOL ID and info */
    if(info->under_vol_info)
        H5VLfree_connector_info(info->under_vol_id, info->under_vol_info);
    H5Idec_ref(info->under_vol_id);

    H5Eset_current_stack(err_id);

    /* Free pass through info object itself */
    free(info);

    return 0;
} /* end H5VL_pass_through_ext_info_free() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_info_to_str
 *
 * Purpose:     Serialize an info object for this connector into a string
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_info_to_str(const void *_info, char **str)
{
    const H5VL_pass_through_ext_info_t *info = (const H5VL_pass_through_ext_info_t *)_info;
    H5VL_class_value_t under_value = (H5VL_class_value_t)-1;
    char *under_vol_string = NULL;
    size_t under_vol_str_len = 0;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INFO To String\n");
#endif

    /* Get value and string for underlying VOL connector */
    H5VLget_value(info->under_vol_id, &under_value);
    H5VLconnector_info_to_str(info->under_vol_info, info->under_vol_id, &under_vol_string);

    /* Determine length of underlying VOL info string */
    if(under_vol_string)
        under_vol_str_len = strlen(under_vol_string);

    /* Allocate space for our info */
    *str = (char *)H5allocate_memory(32 + under_vol_str_len, (hbool_t)0);
    assert(*str);

    /* Encode our info
     * Normally we'd use snprintf() here for a little extra safety, but that
     * call had problems on Windows until recently. So, to be as platform-independent
     * as we can, we're using sprintf() instead.
     */
    sprintf(*str, "under_vol=%u;under_info={%s}", (unsigned)under_value, (under_vol_string ? under_vol_string : ""));

    return 0;
} /* end H5VL_pass_through_ext_info_to_str() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_str_to_info
 *
 * Purpose:     Deserialize a string into an info object for this connector.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_str_to_info(const char *str, void **_info)
{
    H5VL_pass_through_ext_info_t *info;
    unsigned under_vol_value;
    const char *under_vol_info_start, *under_vol_info_end;
    hid_t under_vol_id;
    void *under_vol_info = NULL;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INFO String To Info\n");
#endif

    /* Retrieve the underlying VOL connector value and info */
    sscanf(str, "under_vol=%u;", &under_vol_value);
    under_vol_id = H5VLregister_connector_by_value((H5VL_class_value_t)under_vol_value, H5P_DEFAULT);

    under_vol_info_start = strchr(str, '{');
    under_vol_info_end = strrchr(str, '}');
    assert(under_vol_info_end > under_vol_info_start);
    if(under_vol_info_end != (under_vol_info_start + 1)) {
        char *under_vol_info_str;

        under_vol_info_str = (char *)malloc((size_t)(under_vol_info_end - under_vol_info_start));
        memcpy(under_vol_info_str, under_vol_info_start + 1, (size_t)((under_vol_info_end - under_vol_info_start) - 1));
        *(under_vol_info_str + (under_vol_info_end - under_vol_info_start)) = '\0';

        H5VLconnector_str_to_info(under_vol_info_str, under_vol_id, &under_vol_info);

        free(under_vol_info_str);
    } /* end else */

    /* Allocate new pass-through VOL connector info and set its fields */
    info = (H5VL_pass_through_ext_info_t *)calloc(1, sizeof(H5VL_pass_through_ext_info_t));
    info->under_vol_id = under_vol_id;
    info->under_vol_info = under_vol_info;

    /* Set return value */
    *_info = info;

    return 0;
} /* end H5VL_pass_through_ext_str_to_info() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_get_object
 *
 * Purpose:     Retrieve the 'data' for a VOL object.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_get_object(const void *obj)
{
    const H5VL_pass_through_ext_t *o = (const H5VL_pass_through_ext_t *)obj;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL Get object\n");
#endif

    return H5VLget_object(o->under_object, o->under_vol_id);
} /* end H5VL_pass_through_ext_get_object() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_get_wrap_ctx
 *
 * Purpose:     Retrieve a "wrapper context" for an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_get_wrap_ctx(const void *obj, void **wrap_ctx)
{
    const H5VL_pass_through_ext_t *o = (const H5VL_pass_through_ext_t *)obj;
    H5VL_pass_through_ext_wrap_ctx_t *new_wrap_ctx;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL WRAP CTX Get\n");
#endif

    /* Allocate new VOL object wrapping context for the pass through connector */
    new_wrap_ctx = (H5VL_pass_through_ext_wrap_ctx_t *)calloc(1, sizeof(H5VL_pass_through_ext_wrap_ctx_t));

    /* Increment reference count on underlying VOL ID, and copy the VOL info */
    new_wrap_ctx->under_vol_id = o->under_vol_id;
    H5Iinc_ref(new_wrap_ctx->under_vol_id);
    H5VLget_wrap_ctx(o->under_object, o->under_vol_id, &new_wrap_ctx->under_wrap_ctx);

    /* Set wrap context to return */
    *wrap_ctx = new_wrap_ctx;

    return 0;
} /* end H5VL_pass_through_ext_get_wrap_ctx() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_wrap_object
 *
 * Purpose:     Use a "wrapper context" to wrap a data object
 *
 * Return:      Success:    Pointer to wrapped object
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_wrap_object(void *obj, H5I_type_t obj_type, void *_wrap_ctx)
{
    H5VL_pass_through_ext_wrap_ctx_t *wrap_ctx = (H5VL_pass_through_ext_wrap_ctx_t *)_wrap_ctx;
    H5VL_pass_through_ext_t *new_obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL WRAP Object\n");
#endif

    /* Wrap the object with the underlying VOL */
    under = H5VLwrap_object(obj, obj_type, wrap_ctx->under_vol_id, wrap_ctx->under_wrap_ctx);
    if(under)
        new_obj = H5VL_pass_through_ext_new_obj(under, wrap_ctx->under_vol_id);
    else
        new_obj = NULL;

    return new_obj;
} /* end H5VL_pass_through_ext_wrap_object() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_unwrap_object
 *
 * Purpose:     Unwrap a wrapped object, discarding the wrapper, but returning
 *		underlying object.
 *
 * Return:      Success:    Pointer to unwrapped object
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_unwrap_object(void *obj)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL UNWRAP Object\n");
#endif

    /* Unrap the object with the underlying VOL */
    under = H5VLunwrap_object(o->under_object, o->under_vol_id);

    if(under)
        H5VL_pass_through_ext_free_obj(o);

    return under;
} /* end H5VL_pass_through_ext_unwrap_object() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_free_wrap_ctx
 *
 * Purpose:     Release a "wrapper context" for an object
 *
 * Note:	Take care to preserve the current HDF5 error stack
 *		when calling HDF5 API calls.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_free_wrap_ctx(void *_wrap_ctx)
{
    H5VL_pass_through_ext_wrap_ctx_t *wrap_ctx = (H5VL_pass_through_ext_wrap_ctx_t *)_wrap_ctx;
    hid_t err_id;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL WRAP CTX Free\n");
#endif

    err_id = H5Eget_current_stack();

    /* Release underlying VOL ID and wrap context */
    if(wrap_ctx->under_wrap_ctx)
        H5VLfree_wrap_ctx(wrap_ctx->under_wrap_ctx, wrap_ctx->under_vol_id);
    H5Idec_ref(wrap_ctx->under_vol_id);

    H5Eset_current_stack(err_id);

    /* Free pass through wrap context object itself */
    free(wrap_ctx);

    return 0;
} /* end H5VL_pass_through_ext_free_wrap_ctx() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_attr_create
 *
 * Purpose:     Creates an attribute on an object.
 *
 * Return:      Success:    Pointer to attribute object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_attr_create(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t type_id, hid_t space_id, hid_t acpl_id,
    hid_t aapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *attr;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL ATTRIBUTE Create\n");
#endif

    under = H5VLattr_create(o->under_object, loc_params, o->under_vol_id, name, type_id, space_id, acpl_id, aapl_id, dxpl_id, req);
    if(under) {
        attr = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        attr = NULL;

    return (void*)attr;
} /* end H5VL_pass_through_ext_attr_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_attr_open
 *
 * Purpose:     Opens an attribute on an object.
 *
 * Return:      Success:    Pointer to attribute object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_attr_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t aapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *attr;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL ATTRIBUTE Open\n");
#endif

    under = H5VLattr_open(o->under_object, loc_params, o->under_vol_id, name, aapl_id, dxpl_id, req);
    if(under) {
        attr = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        attr = NULL;

    return (void *)attr;
} /* end H5VL_pass_through_ext_attr_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_attr_read
 *
 * Purpose:     Reads data from attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_attr_read(void *attr, hid_t mem_type_id, void *buf,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)attr;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL ATTRIBUTE Read\n");
#endif

    ret_value = H5VLattr_read(o->under_object, o->under_vol_id, mem_type_id, buf, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_attr_read() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_attr_write
 *
 * Purpose:     Writes data to attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_attr_write(void *attr, hid_t mem_type_id, const void *buf,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)attr;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL ATTRIBUTE Write\n");
#endif

    ret_value = H5VLattr_write(o->under_object, o->under_vol_id, mem_type_id, buf, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_attr_write() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_attr_get
 *
 * Purpose:     Gets information about an attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_attr_get(void *obj, H5VL_attr_get_args_t *args, hid_t dxpl_id,
    void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL ATTRIBUTE Get\n");
#endif

    ret_value = H5VLattr_get(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_attr_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_attr_specific
 *
 * Purpose:     Specific operation on attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL ATTRIBUTE Specific\n");
#endif

    ret_value = H5VLattr_specific(o->under_object, loc_params, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_attr_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_attr_optional
 *
 * Purpose:     Perform a connector-specific operation on an attribute
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_attr_optional(void *obj, H5VL_optional_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL ATTRIBUTE Optional\n");
#endif

    ret_value = H5VLattr_optional(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_attr_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_attr_close
 *
 * Purpose:     Closes an attribute.
 *
 * Return:      Success:    0
 *              Failure:    -1, attr not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_attr_close(void *attr, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)attr;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL ATTRIBUTE Close\n");
#endif

    ret_value = H5VLattr_close(o->under_object, o->under_vol_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    /* Release our wrapper, if underlying attribute was closed */
    if(ret_value >= 0)
        H5VL_pass_through_ext_free_obj(o);

    return ret_value;
} /* end H5VL_pass_through_ext_attr_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_dataset_create
 *
 * Purpose:     Creates a dataset in a container
 *
 * Return:      Success:    Pointer to a dataset object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_dataset_create(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id,
    hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *dset;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATASET Create\n");
#endif

    under = H5VLdataset_create(o->under_object, loc_params, o->under_vol_id, name, lcpl_id, type_id, space_id, dcpl_id,  dapl_id, dxpl_id, req);
    if(under) {
        dset = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);
	dset->m_id = H5Dcreate(o->m_id, name, type_id, space_id, lcpl_id, dcpl_id, dapl_id); 
	dset->parent = obj;
	//dset->es_id = H5EScreate();
        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        dset = NULL;

    return (void *)dset;
} /* end H5VL_pass_through_ext_dataset_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_dataset_open
 *
 * Purpose:     Opens a dataset in a container
 *
 * Return:      Success:    Pointer to a dataset object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_dataset_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *dset;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATASET Open\n");
#endif

    under = H5VLdataset_open(o->under_object, loc_params, o->under_vol_id, name, dapl_id, dxpl_id, req);
    if(under) {
        dset = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        dset = NULL;

    return (void *)dset;
} /* end H5VL_pass_through_ext_dataset_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_dataset_read
 *
 * Purpose:     Reads data elements from a dataset into a buffer.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_dataset_read(void *dset, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, hid_t plist_id, void *buf, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)dset;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATASET Read\n");
#endif

    ret_value = H5VLdataset_read(o->under_object, o->under_vol_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_dataset_read() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_dataset_write
 *
 * Purpose:     Writes data elements from a buffer into a dataset.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_dataset_write(void *dset, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, hid_t plist_id, const void *buf, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)dset;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATASET Write\n");
#endif
    hid_t xpl = H5Pcopy(plist_id);
    void *req1=NULL;
    void *req2=NULL;
    H5Dwrite(o->m_id, mem_type_id, mem_space_id, file_space_id, xpl, buf);

    int *p=(int *)buf;
    for (int i=0; i<10; i++)
      p[i] = 100;
    H5Dread_async(o->m_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, o->es_id);
    void **requests;
    size_t count;
    hid_t *connector_ids;
    ret_value = H5ESget_requests(o->es_id, connector_ids, requests, &count);
    req1 = requests[count-1]; 
    ret_value = H5VLdataset_write(o->under_object, o->under_vol_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, &req2); // writing data to the parallel file system.
    H5VL_async_set_request_dep(req2, req1);
    H5ESinsert_request(o->es_id, o->under_vol_id, req2);
    H5VL_request_status_t *status;
    H5VLrequest_wait(req2, o->under_vol_id, UINT64_MAX, status);
    H5Pclose(xpl);
    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_dataset_write() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_dataset_get
 *
 * Purpose:     Gets information about a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_dataset_get(void *dset, H5VL_dataset_get_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)dset;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATASET Get\n");
#endif

    ret_value = H5VLdataset_get(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_dataset_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_dataset_specific
 *
 * Purpose:     Specific operation on a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_dataset_specific(void *obj, H5VL_dataset_specific_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    hid_t under_vol_id;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL H5Dspecific\n");
#endif

    // Save copy of underlying VOL connector ID and prov helper, in case of
    // refresh destroying the current object
    under_vol_id = o->under_vol_id;

    ret_value = H5VLdataset_specific(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_dataset_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_dataset_optional
 *
 * Purpose:     Perform a connector-specific operation on a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_dataset_optional(void *obj, H5VL_optional_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATASET Optional\n");
#endif

    /* Sanity check */
    assert(-1 != H5VL_passthru_dataset_foo_op_g);
    assert(-1 != H5VL_passthru_dataset_bar_op_g);

    /* Capture and perform connector-specific 'foo' and 'bar' operations */
    if(args->op_type == H5VL_passthru_dataset_foo_op_g) {
        H5VL_passthru_ext_dataset_foo_args_t *foo_args;      /* Parameters for 'foo' operation */

        /* Set up access to parameters for 'foo' operation */
        foo_args = (H5VL_passthru_ext_dataset_foo_args_t *)args->args;
printf("foo: foo_args->i = %d, foo_args->d = %f\n", foo_args->i, foo_args->d);

        /* <do 'foo', with 'i' and 'd'> */

        /* Set return value */
        ret_value = 0;

    } else if(args->op_type == H5VL_passthru_dataset_bar_op_g) {
        H5VL_passthru_ext_dataset_bar_args_t *bar_args;      /* Parameters for 'bar' operation */

        /* Set up access to parameters for 'bar' operation */
        bar_args = (H5VL_passthru_ext_dataset_bar_args_t *)args->args;
printf("bar: bar_args->dp = %p, bar_args->up = %p\n", bar_args->dp, bar_args->up);

        /* <do 'bar', possibly with 'dp' and 'up'> */

        /* Set values to return to application in parameters */
        if(bar_args->dp)
            *bar_args->dp = 3.14159;
        if(bar_args->up)
            *bar_args->up = 42;

        /* Set return value */
        ret_value = 0;

    } else
        ret_value = H5VLdataset_optional(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_dataset_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_dataset_close
 *
 * Purpose:     Closes a dataset.
 *
 * Return:      Success:    0
 *              Failure:    -1, dataset not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_dataset_close(void *dset, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)dset;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATASET Close\n");
#endif

    ret_value = H5VLdataset_close(o->under_object, o->under_vol_id, dxpl_id, req);
    H5Dclose(o->m_id); 

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    /* Release our wrapper, if underlying dataset was closed */
    if(ret_value >= 0)
        H5VL_pass_through_ext_free_obj(o);

    return ret_value;
} /* end H5VL_pass_through_ext_dataset_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_datatype_commit
 *
 * Purpose:     Commits a datatype inside a container.
 *
 * Return:      Success:    Pointer to datatype object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_datatype_commit(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *dt;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATATYPE Commit\n");
#endif

    under = H5VLdatatype_commit(o->under_object, loc_params, o->under_vol_id, name, type_id, lcpl_id, tcpl_id, tapl_id, dxpl_id, req);
    if(under) {
        dt = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        dt = NULL;

    return (void *)dt;
} /* end H5VL_pass_through_ext_datatype_commit() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_datatype_open
 *
 * Purpose:     Opens a named datatype inside a container.
 *
 * Return:      Success:    Pointer to datatype object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_datatype_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t tapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *dt;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATATYPE Open\n");
#endif

    under = H5VLdatatype_open(o->under_object, loc_params, o->under_vol_id, name, tapl_id, dxpl_id, req);
    if(under) {
        dt = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        dt = NULL;

    return (void *)dt;
} /* end H5VL_pass_through_ext_datatype_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_datatype_get
 *
 * Purpose:     Get information about a datatype
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_datatype_get(void *dt, H5VL_datatype_get_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)dt;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATATYPE Get\n");
#endif

    ret_value = H5VLdatatype_get(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_datatype_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_datatype_specific
 *
 * Purpose:     Specific operations for datatypes
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_datatype_specific(void *obj, H5VL_datatype_specific_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    hid_t under_vol_id;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATATYPE Specific\n");
#endif

    // Save copy of underlying VOL connector ID and prov helper, in case of
    // refresh destroying the current object
    under_vol_id = o->under_vol_id;

    ret_value = H5VLdatatype_specific(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_datatype_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_datatype_optional
 *
 * Purpose:     Perform a connector-specific operation on a datatype
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_datatype_optional(void *obj, H5VL_optional_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATATYPE Optional\n");
#endif

    ret_value = H5VLdatatype_optional(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_datatype_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_datatype_close
 *
 * Purpose:     Closes a datatype.
 *
 * Return:      Success:    0
 *              Failure:    -1, datatype not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_datatype_close(void *dt, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)dt;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL DATATYPE Close\n");
#endif

    assert(o->under_object);

    ret_value = H5VLdatatype_close(o->under_object, o->under_vol_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    /* Release our wrapper, if underlying datatype was closed */
    if(ret_value >= 0)
        H5VL_pass_through_ext_free_obj(o);

    return ret_value;
} /* end H5VL_pass_through_ext_datatype_close() */


static herr_t native_vol_info(void **_info) {
  const char *str = "under_vol=0;under_vol_info={}";
  H5VL_pass_through_ext_info_t *info;
  unsigned under_vol_value;
  const char *under_vol_info_start, *under_vol_info_end;
  hid_t under_vol_id;
  void *under_vol_info = NULL;

  /* Retrieve the underlying VOL connector value and info */
  sscanf(str, "under_vol=%u;", &under_vol_value);
  under_vol_id = H5VLregister_connector_by_value(
      (H5VL_class_value_t)under_vol_value, H5P_DEFAULT);

  under_vol_info_start = strchr(str, '{');
  under_vol_info_end = strrchr(str, '}');
  assert(under_vol_info_end > under_vol_info_start);
  if (under_vol_info_end != (under_vol_info_start + 1)) {
    char *under_vol_info_str;

    under_vol_info_str =
        (char *)malloc((size_t)(under_vol_info_end - under_vol_info_start));
    memcpy(under_vol_info_str, under_vol_info_start + 1,
           (size_t)((under_vol_info_end - under_vol_info_start) - 1));
    *(under_vol_info_str + (under_vol_info_end - under_vol_info_start)) = '\0';

    H5VLconnector_str_to_info(under_vol_info_str, under_vol_id,
                              &under_vol_info);

    free(under_vol_info_str);
  } /* end else */

  /* Allocate new pass-through VOL connector info and set its fields */
  info = (H5VL_pass_through_ext_info_t *)calloc(1, sizeof(H5VL_pass_through_ext_info_t));
  info->under_vol_id = under_vol_id;
  info->under_vol_info = under_vol_info;

  /* Set return value */
  *_info = info;
  return 0;
}


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_file_create
 *
 * Purpose:     Creates a container using this connector
 *
 * Return:      Success:    Pointer to a file object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_file_create(const char *name, unsigned flags, hid_t fcpl_id,
    hid_t fapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_info_t *info;
    H5VL_pass_through_ext_t *file;
    hid_t under_fapl_id;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL FILE Create\n");
#endif

    /* Get copy of our VOL info from FAPL */
    H5Pget_vol_info(fapl_id, (void **)&info);

    /* Make sure we have info about the underlying VOL to be used */
    if (!info)
        return NULL;

    /* Copy the FAPL */
    under_fapl_id = H5Pcopy(fapl_id);

    /* Set the VOL ID and info for the underlying FAPL */
    H5Pset_vol(under_fapl_id, info->under_vol_id, info->under_vol_info);

    /* Open the file with the underlying VOL connector */
    char tmp[255];
    strcpy(tmp, name); 
    strcat(tmp, "-map");
    under = H5VLfile_create(name, flags, fcpl_id, under_fapl_id, dxpl_id, req);


    // get default fapl_id calling native

    hid_t fapl_id_default = H5Pcopy(fapl_id);
    hid_t async_vol_id = H5VLget_connector_id_by_value(H5VL_ASYNC_VALUE);

    void *p = NULL;
    native_vol_info(&p);
    H5Pset_vol(fapl_id_default, async_vol_id, p);

    printf("creating file ...\n");
    file->m_id = H5Fcreate(tmp, H5F_ACC_TRUNC, fcpl_id, fapl_id_default);
    printf("creating file done ...\n");
    if(under) {
        file = H5VL_pass_through_ext_new_obj(under, info->under_vol_id);
        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, info->under_vol_id);
    } /* end if */
    else
        file = NULL;

    /* Close underlying FAPL */
    H5Pclose(under_fapl_id);
    H5Pclose(fapl_id_default);

    /* Release copy of our VOL info */
    H5VL_pass_through_ext_info_free(info);

    return (void *)file;
} /* end H5VL_pass_through_ext_file_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_file_open
 *
 * Purpose:     Opens a container created with this connector
 *
 * Return:      Success:    Pointer to a file object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_file_open(const char *name, unsigned flags, hid_t fapl_id,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_info_t *info;
    H5VL_pass_through_ext_t *file;
    hid_t under_fapl_id;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL FILE Open\n");
#endif

    /* Get copy of our VOL info from FAPL */
    H5Pget_vol_info(fapl_id, (void **)&info);

    /* Make sure we have info about the underlying VOL to be used */
    if (!info)
        return NULL;

    /* Copy the FAPL */
    under_fapl_id = H5Pcopy(fapl_id);

    /* Set the VOL ID and info for the underlying FAPL */
    H5Pset_vol(under_fapl_id, info->under_vol_id, info->under_vol_info);

    /* Open the file with the underlying VOL connector */
    under = H5VLfile_open(name, flags, under_fapl_id, dxpl_id, req);
    if(under) {
        file = H5VL_pass_through_ext_new_obj(under, info->under_vol_id);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, info->under_vol_id);
    } /* end if */
    else
        file = NULL;

    /* Close underlying FAPL */
    H5Pclose(under_fapl_id);

    /* Release copy of our VOL info */
    H5VL_pass_through_ext_info_free(info);

    return (void *)file;
} /* end H5VL_pass_through_ext_file_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_file_get
 *
 * Purpose:     Get info about a file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_file_get(void *file, H5VL_file_get_args_t *args, hid_t dxpl_id,
    void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)file;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL FILE Get\n");
#endif

    ret_value = H5VLfile_get(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_file_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_file_specific
 *
 * Purpose:     Specific operation on file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_file_specific(void *file, H5VL_file_specific_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)file;
    H5VL_pass_through_ext_t *new_o;
    H5VL_file_specific_args_t my_args;
    H5VL_file_specific_args_t *new_args;
    H5VL_pass_through_ext_info_t *info;
    hid_t under_vol_id = -1;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL FILE Specific\n");
#endif

    /* Check for 'is accessible' operation */
    if(args->op_type == H5VL_FILE_IS_ACCESSIBLE) {
        /* Make a (shallow) copy of the arguments */
        memcpy(&my_args, args, sizeof(my_args));

        /* Set up the new FAPL for the updated arguments */

        /* Get copy of our VOL info from FAPL */
        H5Pget_vol_info(args->args.is_accessible.fapl_id, (void **)&info);

        /* Make sure we have info about the underlying VOL to be used */
        if (!info)
            return (-1);

        /* Keep the correct underlying VOL ID for later */
        under_vol_id = info->under_vol_id;

        /* Copy the FAPL */
        my_args.args.is_accessible.fapl_id = H5Pcopy(args->args.is_accessible.fapl_id);

        /* Set the VOL ID and info for the underlying FAPL */
        H5Pset_vol(my_args.args.is_accessible.fapl_id, info->under_vol_id, info->under_vol_info);

        /* Set argument pointer to new arguments */
        new_args = &my_args;

        /* Set object pointer for operation */
        new_o = NULL;
    } /* end else-if */
    /* Check for 'delete' operation */
    else if(args->op_type == H5VL_FILE_DELETE) {
        /* Make a (shallow) copy of the arguments */
        memcpy(&my_args, args, sizeof(my_args));

        /* Set up the new FAPL for the updated arguments */

        /* Get copy of our VOL info from FAPL */
        H5Pget_vol_info(args->args.del.fapl_id, (void **)&info);

        /* Make sure we have info about the underlying VOL to be used */
        if (!info)
            return (-1);

        /* Keep the correct underlying VOL ID for later */
        under_vol_id = info->under_vol_id;

        /* Copy the FAPL */
        my_args.args.del.fapl_id = H5Pcopy(args->args.del.fapl_id);

        /* Set the VOL ID and info for the underlying FAPL */
        H5Pset_vol(my_args.args.del.fapl_id, info->under_vol_id, info->under_vol_info);

        /* Set argument pointer to new arguments */
        new_args = &my_args;

        /* Set object pointer for operation */
        new_o = NULL;
    } /* end else-if */
    else {
        /* Keep the correct underlying VOL ID for later */
        under_vol_id = o->under_vol_id;

        /* Set argument pointer to current arguments */
        new_args = args;

        /* Set object pointer for operation */
        new_o = o->under_object;
    } /* end else */

    ret_value = H5VLfile_specific(new_o, under_vol_id, new_args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, under_vol_id);

    /* Check for 'is accessible' operation */
    if(args->op_type == H5VL_FILE_IS_ACCESSIBLE) {
        /* Close underlying FAPL */
        H5Pclose(my_args.args.is_accessible.fapl_id);

        /* Release copy of our VOL info */
        H5VL_pass_through_ext_info_free(info);
    } /* end else-if */
    /* Check for 'delete' operation */
    else if(args->op_type == H5VL_FILE_DELETE) {
        /* Close underlying FAPL */
        H5Pclose(my_args.args.del.fapl_id);

        /* Release copy of our VOL info */
        H5VL_pass_through_ext_info_free(info);
    } /* end else-if */
    else if(args->op_type == H5VL_FILE_REOPEN) {
        /* Wrap reopened file struct pointer, if we reopened one */
        if(ret_value >= 0 && args->args.reopen.file)
            *args->args.reopen.file = H5VL_pass_through_ext_new_obj(*args->args.reopen.file, o->under_vol_id);
    } /* end else */

    return ret_value;
} /* end H5VL_pass_through_ext_file_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_file_optional
 *
 * Purpose:     Perform a connector-specific operation on a file
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_file_optional(void *file, H5VL_optional_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)file;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL File Optional\n");
#endif

    ret_value = H5VLfile_optional(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_file_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_file_close
 *
 * Purpose:     Closes a file.
 *
 * Return:      Success:    0
 *              Failure:    -1, file not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_file_close(void *file, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)file;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL FILE Close\n");
#endif

    ret_value = H5VLfile_close(o->under_object, o->under_vol_id, dxpl_id, req);
    H5Fclose(o->m_id); 
    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    /* Release our wrapper, if underlying file was closed */
    if(ret_value >= 0)
        H5VL_pass_through_ext_free_obj(o);

    return ret_value;
} /* end H5VL_pass_through_ext_file_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_group_create
 *
 * Purpose:     Creates a group inside a container
 *
 * Return:      Success:    Pointer to a group object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_group_create(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *group;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL GROUP Create\n");
#endif

    under = H5VLgroup_create(o->under_object, loc_params, o->under_vol_id, name, lcpl_id, gcpl_id,  gapl_id, dxpl_id, req);

    if(under) {
        group = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);
	group->m_id = H5Gcreate(o->m_id, name, lcpl_id, gcpl_id, gapl_id);
	group->parent = obj; 
        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        group = NULL;

    return (void *)group;
} /* end H5VL_pass_through_ext_group_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_group_open
 *
 * Purpose:     Opens a group inside a container
 *
 * Return:      Success:    Pointer to a group object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_group_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t gapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *group;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL GROUP Open\n");
#endif

    under = H5VLgroup_open(o->under_object, loc_params, o->under_vol_id, name, gapl_id, dxpl_id, req);
    if(under) {
        group = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        group = NULL;

    return (void *)group;
} /* end H5VL_pass_through_ext_group_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_group_get
 *
 * Purpose:     Get info about a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_group_get(void *obj, H5VL_group_get_args_t *args, hid_t dxpl_id,
    void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL GROUP Get\n");
#endif

    ret_value = H5VLgroup_get(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_group_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_group_specific
 *
 * Purpose:     Specific operation on a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_group_specific(void *obj, H5VL_group_specific_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    H5VL_group_specific_args_t my_args;
    H5VL_group_specific_args_t *new_args;
    hid_t under_vol_id;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL GROUP Specific\n");
#endif

    // Save copy of underlying VOL connector ID and prov helper, in case of
    // refresh destroying the current object
    under_vol_id = o->under_vol_id;

    /* Unpack arguments to get at the child file pointer when mounting a file */
    if(args->op_type == H5VL_GROUP_MOUNT) {

        /* Make a (shallow) copy of the arguments */
        memcpy(&my_args, args, sizeof(my_args));

        /* Set the object for the child file */
        my_args.args.mount.child_file = ((H5VL_pass_through_ext_t *)args->args.mount.child_file)->under_object;

        /* Point to modified arguments */
        new_args = &my_args;
    } /* end if */
    else
        new_args = args;

    ret_value = H5VLgroup_specific(o->under_object, under_vol_id, new_args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_group_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_group_optional
 *
 * Purpose:     Perform a connector-specific operation on a group
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_group_optional(void *obj, H5VL_optional_args_t *args,
    hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL GROUP Optional\n");
#endif

    /* Sanity check */
    assert(-1 != H5VL_passthru_group_fiddle_op_g);

    /* Capture and perform connector-specific 'fiddle' operation */
    if(args->op_type == H5VL_passthru_group_fiddle_op_g) {
        /* No args for 'fiddle' operation */

printf("fiddle\n");

        /* <do 'fiddle'> */

        /* Set return value */
        ret_value = 0;

    } else
        ret_value = H5VLgroup_optional(o->under_object, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_group_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_group_close
 *
 * Purpose:     Closes a group.
 *
 * Return:      Success:    0
 *              Failure:    -1, group not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_group_close(void *grp, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)grp;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL GROUP Close\n");
#endif

    ret_value = H5VLgroup_close(o->under_object, o->under_vol_id, dxpl_id, req);
    H5Gclose(o->m_id); 

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    /* Release our wrapper, if underlying file was closed */
    if(ret_value >= 0)
        H5VL_pass_through_ext_free_obj(o);

    return ret_value;
} /* end H5VL_pass_through_ext_group_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_link_create
 *
 * Purpose:     Creates a hard / soft / UD / external link.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_link_create(H5VL_link_create_args_t *args, void *obj,
    const H5VL_loc_params_t *loc_params, hid_t lcpl_id, hid_t lapl_id,
    hid_t dxpl_id, void **req)
{
    H5VL_link_create_args_t my_args;
    H5VL_link_create_args_t *new_args;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    hid_t under_vol_id = -1;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL LINK Create\n");
#endif

    /* Try to retrieve the "under" VOL id */
    if(o)
        under_vol_id = o->under_vol_id;

    /* Fix up the link target object for hard link creation */
    if(H5VL_LINK_CREATE_HARD == args->op_type) {
        /* If it's a non-NULL pointer, find the 'under object' and re-set the args */
        if(args->args.hard.curr_obj) {
            /* Make a (shallow) copy of the arguments */
            memcpy(&my_args, args, sizeof(my_args));

            /* Check if we still need the "under" VOL ID */
            if(under_vol_id < 0)
                under_vol_id = ((H5VL_pass_through_ext_t *)args->args.hard.curr_obj)->under_vol_id;

            /* Set the object for the link target */
            my_args.args.hard.curr_obj = ((H5VL_pass_through_ext_t *)args->args.hard.curr_obj)->under_object;

            /* Set argument pointer to modified parameters */
            new_args = &my_args;
        } /* end if */
        else
            new_args = args;
    } /* end if */
    else
        new_args = args;

    /* Re-issue 'link create' call, possibly using the unwrapped pieces */
    ret_value = H5VLlink_create(new_args, (o ? o->under_object : NULL), loc_params, under_vol_id, lcpl_id, lapl_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_link_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_link_copy
 *
 * Purpose:     Renames an object within an HDF5 container and copies it to a new
 *              group.  The original name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_link_copy(void *src_obj, const H5VL_loc_params_t *loc_params1,
    void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl_id,
    hid_t lapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o_src = (H5VL_pass_through_ext_t *)src_obj;
    H5VL_pass_through_ext_t *o_dst = (H5VL_pass_through_ext_t *)dst_obj;
    hid_t under_vol_id = -1;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL LINK Copy\n");
#endif

    /* Retrieve the "under" VOL id */
    if(o_src)
        under_vol_id = o_src->under_vol_id;
    else if(o_dst)
        under_vol_id = o_dst->under_vol_id;
    assert(under_vol_id > 0);

    ret_value = H5VLlink_copy((o_src ? o_src->under_object : NULL), loc_params1, (o_dst ? o_dst->under_object : NULL), loc_params2, under_vol_id, lcpl_id, lapl_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_link_copy() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_link_move
 *
 * Purpose:     Moves a link within an HDF5 file to a new group.  The original
 *              name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_link_move(void *src_obj, const H5VL_loc_params_t *loc_params1,
    void *dst_obj, const H5VL_loc_params_t *loc_params2, hid_t lcpl_id,
    hid_t lapl_id, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o_src = (H5VL_pass_through_ext_t *)src_obj;
    H5VL_pass_through_ext_t *o_dst = (H5VL_pass_through_ext_t *)dst_obj;
    hid_t under_vol_id = -1;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL LINK Move\n");
#endif

    /* Retrieve the "under" VOL id */
    if(o_src)
        under_vol_id = o_src->under_vol_id;
    else if(o_dst)
        under_vol_id = o_dst->under_vol_id;
    assert(under_vol_id > 0);

    ret_value = H5VLlink_move((o_src ? o_src->under_object : NULL), loc_params1, (o_dst ? o_dst->under_object : NULL), loc_params2, under_vol_id, lcpl_id, lapl_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_link_move() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_link_get
 *
 * Purpose:     Get info about a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_link_get(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_link_get_args_t *args, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL LINK Get\n");
#endif

    ret_value = H5VLlink_get(o->under_object, loc_params, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_link_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_link_specific
 *
 * Purpose:     Specific operation on a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL LINK Specific\n");
#endif

    ret_value = H5VLlink_specific(o->under_object, loc_params, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_link_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_link_optional
 *
 * Purpose:     Perform a connector-specific operation on a link
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_link_optional(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL LINK Optional\n");
#endif

    ret_value = H5VLlink_optional(o->under_object, loc_params, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_link_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_object_open
 *
 * Purpose:     Opens an object inside a container.
 *
 * Return:      Success:    Pointer to object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL_pass_through_ext_object_open(void *obj, const H5VL_loc_params_t *loc_params,
    H5I_type_t *opened_type, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *new_obj;
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    void *under;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL OBJECT Open\n");
#endif

    under = H5VLobject_open(o->under_object, loc_params, o->under_vol_id, opened_type, dxpl_id, req);
    if(under) {
        new_obj = H5VL_pass_through_ext_new_obj(under, o->under_vol_id);

        /* Check for async request */
        if(req && *req)
            *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);
    } /* end if */
    else
        new_obj = NULL;

    return (void *)new_obj;
} /* end H5VL_pass_through_ext_object_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_object_copy
 *
 * Purpose:     Copies an object inside a container.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_object_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params,
    const char *src_name, void *dst_obj, const H5VL_loc_params_t *dst_loc_params,
    const char *dst_name, hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id,
    void **req)
{
    H5VL_pass_through_ext_t *o_src = (H5VL_pass_through_ext_t *)src_obj;
    H5VL_pass_through_ext_t *o_dst = (H5VL_pass_through_ext_t *)dst_obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL OBJECT Copy\n");
#endif

    ret_value = H5VLobject_copy(o_src->under_object, src_loc_params, src_name, o_dst->under_object, dst_loc_params, dst_name, o_src->under_vol_id, ocpypl_id, lcpl_id, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o_src->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_object_copy() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_object_get
 *
 * Purpose:     Get info about an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_object_get(void *obj, const H5VL_loc_params_t *loc_params, H5VL_object_get_args_t *args, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL OBJECT Get\n");
#endif

    ret_value = H5VLobject_get(o->under_object, loc_params, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_object_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_object_specific
 *
 * Purpose:     Specific operation on an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_object_specific(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    hid_t under_vol_id;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL OBJECT Specific\n");
#endif

    // Save copy of underlying VOL connector ID and prov helper, in case of
    // refresh destroying the current object
    under_vol_id = o->under_vol_id;

    ret_value = H5VLobject_specific(o->under_object, loc_params, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_object_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_object_optional
 *
 * Purpose:     Perform a connector-specific operation for an object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_object_optional(void *obj, const H5VL_loc_params_t *loc_params,
    H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL OBJECT Optional\n");
#endif

    ret_value = H5VLobject_optional(o->under_object, loc_params, o->under_vol_id, args, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = H5VL_pass_through_ext_new_obj(*req, o->under_vol_id);

    return ret_value;
} /* end H5VL_pass_through_ext_object_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_introspect_get_conn_clss
 *
 * Purpose:     Query the connector class.
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_pass_through_ext_introspect_get_conn_cls(void *obj, H5VL_get_conn_lvl_t lvl,
    const H5VL_class_t **conn_cls)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INTROSPECT GetConnCls\n");
#endif

    /* Check for querying this connector's class */
    if(H5VL_GET_CONN_LVL_CURR == lvl) {
        *conn_cls = &H5VL_pass_through_ext_g;
        ret_value = 0;
    } /* end if */
    else
        ret_value = H5VLintrospect_get_conn_cls(o->under_object, o->under_vol_id,
            lvl, conn_cls);

    return ret_value;
} /* end H5VL_pass_through_ext_introspect_get_conn_cls() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_introspect_get_cap_flags
 *
 * Purpose:     Query the capability flags for this connector and any
 *              underlying connector(s).
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_pass_through_ext_introspect_get_cap_flags(const void *_info, unsigned *cap_flags)
{
    const H5VL_pass_through_ext_info_t *info = (const H5VL_pass_through_ext_info_t *)_info;
    herr_t                          ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INTROSPECT GetCapFlags\n");
#endif

    /* Invoke the query on the underlying VOL connector */
    ret_value = H5VLintrospect_get_cap_flags(info->under_vol_info, info->under_vol_id, cap_flags);

    /* Bitwise OR our capability flags in */
    if (ret_value >= 0)
        *cap_flags |= H5VL_pass_through_ext_g.cap_flags;

    return ret_value;
} /* end H5VL_pass_through_introspect_ext_get_cap_flags() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_introspect_opt_query
 *
 * Purpose:     Query if an optional operation is supported by this connector
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_pass_through_ext_introspect_opt_query(void *obj, H5VL_subclass_t cls,
    int op_type, uint64_t *flags)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL INTROSPECT OptQuery\n");
#endif

    ret_value = H5VLintrospect_opt_query(o->under_object, o->under_vol_id, cls,
        op_type, flags);

    return ret_value;
} /* end H5VL_pass_through_ext_introspect_opt_query() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_request_wait
 *
 * Purpose:     Wait (with a timeout) for an async operation to complete
 *
 * Note:        Releases the request if the operation has completed and the
 *              connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_request_wait(void *obj, uint64_t timeout,
    H5VL_request_status_t *status)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL REQUEST Wait\n");
#endif

    ret_value = H5VLrequest_wait(o->under_object, o->under_vol_id, timeout, status);

    return ret_value;
} /* end H5VL_pass_through_ext_request_wait() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_request_notify
 *
 * Purpose:     Registers a user callback to be invoked when an asynchronous
 *              operation completes
 *
 * Note:        Releases the request, if connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_request_notify(void *obj, H5VL_request_notify_t cb, void *ctx)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL REQUEST Notify\n");
#endif

    ret_value = H5VLrequest_notify(o->under_object, o->under_vol_id, cb, ctx);

    return ret_value;
} /* end H5VL_pass_through_ext_request_notify() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_request_cancel
 *
 * Purpose:     Cancels an asynchronous operation
 *
 * Note:        Releases the request, if connector callback succeeds
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_request_cancel(void *obj, H5VL_request_status_t *status)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL REQUEST Cancel\n");
#endif

    ret_value = H5VLrequest_cancel(o->under_object, o->under_vol_id, status);

    return ret_value;
} /* end H5VL_pass_through_ext_request_cancel() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_request_specific
 *
 * Purpose:     Specific operation on a request
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_request_specific(void *obj, H5VL_request_specific_args_t *args)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value = -1;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL REQUEST Specific\n");
#endif

    ret_value = H5VLrequest_specific(o->under_object, o->under_vol_id, args);

    return ret_value;
} /* end H5VL_pass_through_ext_request_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_request_optional
 *
 * Purpose:     Perform a connector-specific operation for a request
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_request_optional(void *obj, H5VL_optional_args_t *args)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL REQUEST Optional\n");
#endif

    ret_value = H5VLrequest_optional(o->under_object, o->under_vol_id, args);

    return ret_value;
} /* end H5VL_pass_through_ext_request_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_request_free
 *
 * Purpose:     Releases a request, allowing the operation to complete without
 *              application tracking
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_request_free(void *obj)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL REQUEST Free\n");
#endif

    ret_value = H5VLrequest_free(o->under_object, o->under_vol_id);

    if(ret_value >= 0)
        H5VL_pass_through_ext_free_obj(o);

    return ret_value;
} /* end H5VL_pass_through_ext_request_free() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_blob_put
 *
 * Purpose:     Handles the blob 'put' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_pass_through_ext_blob_put(void *obj, const void *buf, size_t size,
    void *blob_id, void *ctx)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL BLOB Put\n");
#endif

    ret_value = H5VLblob_put(o->under_object, o->under_vol_id, buf, size,
        blob_id, ctx);

    return ret_value;
} /* end H5VL_pass_through_ext_blob_put() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_blob_get
 *
 * Purpose:     Handles the blob 'get' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_pass_through_ext_blob_get(void *obj, const void *blob_id, void *buf,
    size_t size, void *ctx)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL BLOB Get\n");
#endif

    ret_value = H5VLblob_get(o->under_object, o->under_vol_id, blob_id, buf,
        size, ctx);

    return ret_value;
} /* end H5VL_pass_through_ext_blob_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_blob_specific
 *
 * Purpose:     Handles the blob 'specific' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_pass_through_ext_blob_specific(void *obj, void *blob_id,
    H5VL_blob_specific_args_t *args)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL BLOB Specific\n");
#endif

    ret_value = H5VLblob_specific(o->under_object, o->under_vol_id, blob_id, args);

    return ret_value;
} /* end H5VL_pass_through_ext_blob_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_blob_optional
 *
 * Purpose:     Handles the blob 'optional' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_pass_through_ext_blob_optional(void *obj, void *blob_id, H5VL_optional_args_t *args)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL BLOB Optional\n");
#endif

    ret_value = H5VLblob_optional(o->under_object, o->under_vol_id, blob_id, args);

    return ret_value;
} /* end H5VL_pass_through_ext_blob_optional() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_token_cmp
 *
 * Purpose:     Compare two of the connector's object tokens, setting
 *              *cmp_value, following the same rules as strcmp().
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_token_cmp(void *obj, const H5O_token_t *token1,
    const H5O_token_t *token2, int *cmp_value)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL TOKEN Compare\n");
#endif

    /* Sanity checks */
    assert(obj);
    assert(token1);
    assert(token2);
    assert(cmp_value);

    ret_value = H5VLtoken_cmp(o->under_object, o->under_vol_id, token1, token2, cmp_value);

    return ret_value;
} /* end H5VL_pass_through_ext_token_cmp() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_token_to_str
 *
 * Purpose:     Serialize the connector's object token into a string.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_token_to_str(void *obj, H5I_type_t obj_type,
    const H5O_token_t *token, char **token_str)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL TOKEN To string\n");
#endif

    /* Sanity checks */
    assert(obj);
    assert(token);
    assert(token_str);

    ret_value = H5VLtoken_to_str(o->under_object, obj_type, o->under_vol_id, token, token_str);

    return ret_value;
} /* end H5VL_pass_through_ext_token_to_str() */


/*---------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_token_from_str
 *
 * Purpose:     Deserialize the connector's object token from a string.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5VL_pass_through_ext_token_from_str(void *obj, H5I_type_t obj_type,
    const char *token_str, H5O_token_t *token)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL TOKEN From string\n");
#endif

    /* Sanity checks */
    assert(obj);
    assert(token);
    assert(token_str);

    ret_value = H5VLtoken_from_str(o->under_object, obj_type, o->under_vol_id, token_str, token);

    return ret_value;
} /* end H5VL_pass_through_ext_token_from_str() */


/*-------------------------------------------------------------------------
 * Function:    H5VL_pass_through_ext_optional
 *
 * Purpose:     Handles the generic 'optional' callback
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_pass_through_ext_optional(void *obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    H5VL_pass_through_ext_t *o = (H5VL_pass_through_ext_t *)obj;
    herr_t ret_value;

#ifdef ENABLE_EXT_PASSTHRU_LOGGING
    printf("------- EXT PASS THROUGH VOL generic Optional\n");
#endif

    ret_value = H5VLoptional(o->under_object, o->under_vol_id, args, dxpl_id, req);

    return ret_value;
} /* end H5VL_pass_through_ext_optional() */

