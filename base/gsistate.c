/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Imager state housekeeping */
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"
#include "gscie.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxbitmap.h"
#include "gxcmap.h"
#include "gxdht.h"
#include "gxistate.h"
#include "gzht.h"
#include "gzline.h"
#include "gxfmap.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"
#include "gsicc_profilecache.h"

/******************************************************************************
 * See gsstate.c for a discussion of graphics/imager state memory management. *
 ******************************************************************************/

/* Imported values */
/* The following should include a 'const', but for some reason */
/* the Watcom compiler won't accept it, even though it happily accepts */
/* the same construct everywhere else. */
extern /*const*/ gx_color_map_procs *const cmap_procs_default;

/* GC procedures for gx_line_params */
static
ENUM_PTRS_WITH(line_params_enum_ptrs, gx_line_params *plp) return 0;
    case 0: return ENUM_OBJ((plp->dash.pattern_size == 0 ?
                             NULL : plp->dash.pattern));
ENUM_PTRS_END
static RELOC_PTRS_WITH(line_params_reloc_ptrs, gx_line_params *plp)
{
    if (plp->dash.pattern_size)
        RELOC_VAR(plp->dash.pattern);
} RELOC_PTRS_END
private_st_line_params();

/*
 * GC procedures for gs_imager_state
 *
 * See comments in gixstate.h before the definition of gs_cr_state_do_rc and
 * st_cr_state_num_ptrs for an explanation about why the effective_transfer
 * pointers are handled in this manner.
 */
public_st_imager_state();
static
ENUM_PTRS_BEGIN(imager_state_enum_ptrs)
    ENUM_SUPER(gs_imager_state, st_line_params, line_params, st_imager_state_num_ptrs - st_line_params_num_ptrs);
    ENUM_PTR(0, gs_imager_state, client_data);
    ENUM_PTR(1, gs_imager_state, trans_device);
    ENUM_PTR(2, gs_imager_state, icc_manager);
    ENUM_PTR(3, gs_imager_state, icc_link_cache);
    ENUM_PTR(4, gs_imager_state, icc_profile_cache);
#define E1(i,elt) ENUM_PTR(i+5,gs_imager_state,elt);
    gs_cr_state_do_ptrs(E1)
#undef E1
ENUM_PTRS_END
static RELOC_PTRS_BEGIN(imager_state_reloc_ptrs)
{
    RELOC_SUPER(gs_imager_state, st_line_params, line_params);
    RELOC_PTR(gs_imager_state, client_data);
    RELOC_PTR(gs_imager_state, trans_device);
    RELOC_PTR(gs_imager_state, icc_manager);
    RELOC_PTR(gs_imager_state, icc_link_cache);
    RELOC_PTR(gs_imager_state, icc_profile_cache);
#define R1(i,elt) RELOC_PTR(gs_imager_state,elt);
    gs_cr_state_do_ptrs(R1)
#undef R1
    {
        int i = GX_DEVICE_COLOR_MAX_COMPONENTS - 1;

        for (; i >= 0; i--)
            RELOC_PTR(gs_imager_state, effective_transfer[i]);
    }
} RELOC_PTRS_END

/* Initialize an imager state, other than the parts covered by */
/* gs_imager_state_initial. */
int
gs_imager_state_initialize(gs_imager_state * pis, gs_memory_t * mem)
{
    int i;
    pis->memory = mem;
    pis->client_data = 0;
    pis->trans_device = 0;
    /* Color rendering state */
    pis->halftone = 0;
    {
        int i;

        for (i = 0; i < gs_color_select_count; ++i)
            pis->screen_phase[i].x = pis->screen_phase[i].y = 0;
    }
    pis->dev_ht = 0;
    pis->cie_render = 0;
    pis->cie_to_xyz = false;
    pis->black_generation = 0;
    pis->undercolor_removal = 0;
    /* Allocate an initial transfer map. */
    rc_alloc_struct_n(pis->set_transfer.gray,
                      gx_transfer_map, &st_transfer_map,
                      mem, return_error(gs_error_VMerror),
                      "gs_imager_state_init(transfer)", 1);
    pis->set_transfer.gray->proc = gs_identity_transfer;
    pis->set_transfer.gray->id = gs_next_ids(pis->memory, 1);
    pis->set_transfer.gray->values[0] = frac_0;
    pis->set_transfer.red =
        pis->set_transfer.green =
        pis->set_transfer.blue = NULL;
    for (i = 0; i < GX_DEVICE_COLOR_MAX_COMPONENTS; i++)
        pis->effective_transfer[i] = pis->set_transfer.gray;
    pis->cie_joint_caches = NULL;
    pis->cie_joint_caches_alt = NULL;
    pis->cmap_procs = cmap_procs_default;
    pis->pattern_cache = NULL;
    pis->have_pattern_streams = false;
    pis->devicergb_cs = gs_cspace_new_DeviceRGB(mem);
    pis->devicecmyk_cs = gs_cspace_new_DeviceCMYK(mem);
    pis->icc_link_cache = gsicc_cache_new(pis->memory);
    pis->icc_manager = gsicc_manager_new(pis->memory);
    pis->icc_profile_cache = gsicc_profilecache_new(pis->memory);
    return 0;
}

/*
 * Make a temporary copy of a gs_imager_state.  Note that this does not
 * do all the necessary reference counting, etc.  However, it does
 * clear out the transparency stack in the destination.
 */
gs_imager_state *
gs_imager_state_copy(const gs_imager_state * pis, gs_memory_t * mem)
{
    gs_imager_state *pis_copy =
        gs_alloc_struct(mem, gs_imager_state, &st_imager_state,
                        "gs_imager_state_copy");

    if (pis_copy) {
        *pis_copy = *pis;
    }
    return pis_copy;
}

/* Increment reference counts to note that an imager state has been copied. */
void
gs_imager_state_copied(gs_imager_state * pis)
{
    rc_increment(pis->halftone);
    rc_increment(pis->dev_ht);
    rc_increment(pis->cie_render);
    rc_increment(pis->black_generation);
    rc_increment(pis->undercolor_removal);
    rc_increment(pis->set_transfer.gray);
    rc_increment(pis->set_transfer.red);
    rc_increment(pis->set_transfer.green);
    rc_increment(pis->set_transfer.blue);
    rc_increment(pis->cie_joint_caches);
    rc_increment(pis->cie_joint_caches_alt);
    rc_increment(pis->devicergb_cs);
    rc_increment(pis->devicecmyk_cs);
    rc_increment(pis->icc_link_cache);
    rc_increment(pis->icc_profile_cache);
    rc_increment(pis->icc_manager);
}

/* Adjust reference counts before assigning one imager state to another. */
void
gs_imager_state_pre_assign(gs_imager_state *pto, const gs_imager_state *pfrom)
{
    const char *const cname = "gs_imager_state_pre_assign";

#define RCCOPY(element)\
    rc_pre_assign(pto->element, pfrom->element, cname)

    RCCOPY(cie_joint_caches);
    RCCOPY(cie_joint_caches_alt);
    RCCOPY(set_transfer.blue);
    RCCOPY(set_transfer.green);
    RCCOPY(set_transfer.red);
    RCCOPY(set_transfer.gray);
    RCCOPY(undercolor_removal);
    RCCOPY(black_generation);
    RCCOPY(cie_render);
    RCCOPY(dev_ht);
    RCCOPY(halftone);
    RCCOPY(devicergb_cs);
    RCCOPY(devicecmyk_cs);
    RCCOPY(icc_link_cache);
    RCCOPY(icc_profile_cache);
    RCCOPY(icc_manager);
#undef RCCOPY
}

/* Release an imager state. */
void
gs_imager_state_release(gs_imager_state * pis)
{
    const char *const cname = "gs_imager_state_release";
    gx_device_halftone *pdht = pis->dev_ht;

#define RCDECR(element)\
    rc_decrement(pis->element, cname);\
    pis->element = NULL	/* prevent subsequent decrements from this imager state */

    RCDECR(cie_joint_caches);
    RCDECR(set_transfer.gray);
    RCDECR(set_transfer.blue);
    RCDECR(set_transfer.green);
    RCDECR(set_transfer.red);
    RCDECR(undercolor_removal);
    RCDECR(black_generation);
    RCDECR(cie_render);
    /*
     * If we're going to free the device halftone, make sure we free the
     * dependent structures as well.
     */
    if (pdht != 0 && pdht->rc.ref_count == 1) {
        gx_device_halftone_release(pdht, pdht->rc.memory);
    }
    RCDECR(dev_ht);
    RCDECR(halftone);
    RCDECR(devicergb_cs);
    RCDECR(devicecmyk_cs);
    RCDECR(icc_link_cache);
    RCDECR(icc_profile_cache);
    RCDECR(icc_manager);
#undef RCDECR
}
