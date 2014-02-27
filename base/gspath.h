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


/* Graphics state path procedures */
/* Requires gsstate.h */

#ifndef gspath_INCLUDED
#  define gspath_INCLUDED

#include "gspenum.h"

/*
 * Define clamped values for out-of-range coordinates.
 * Currently the path drawing routines can't handle values
 * close to the edge of the representable space.
 */
#define max_coord_fixed (max_fixed - int2fixed(1000))	/* arbitrary */
#define min_coord_fixed (-max_coord_fixed)
#define clamp_coord(xy)\
    (xy > fixed2float(max_coord_fixed) ? max_coord_fixed :\
     xy < fixed2float(min_coord_fixed) ? min_coord_fixed :\
     float2fixed(xy))

/* Path constructors */
int gs_newpath(gs_state *),
    gs_moveto(gs_state *, floatp, floatp),
    gs_rmoveto(gs_state *, floatp, floatp),
    gs_lineto(gs_state *, floatp, floatp),
    gs_rlineto(gs_state *, floatp, floatp),
    gs_arc(gs_state *, floatp, floatp, floatp, floatp, floatp),
    gs_arcn(gs_state *, floatp, floatp, floatp, floatp, floatp),
    /*
     * Because of an obscure bug in the IBM RS/6000 compiler, one (but not
     * both) bool argument(s) for gs_arc_add must come before the floatp
     * arguments.
     */
    gs_arc_add(gs_state *, bool, floatp, floatp, floatp, floatp, floatp, bool),
    gs_arcto(gs_state *, floatp, floatp, floatp, floatp, floatp, float[4]),
    gs_curveto(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp),
    gs_rcurveto(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp),
    gs_closepath(gs_state *);

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;
#endif
#ifndef gs_matrix_fixed_DEFINED
#define gs_matrix_fixed_DEFINED
typedef struct gs_matrix_fixed_s gs_matrix_fixed;
#endif

/* Imager-level procedures */
void make_quadrant_arc(gs_point *p, const gs_point *c,
        const gs_point *p0, const gs_point *p1, double r);

/* Add the current path to the path in the previous graphics state. */
int gs_upmergepath(gs_state *);

/* Path accessors and transformers */
int gs_currentpoint(gs_state *, gs_point *),
      gs_upathbbox(gs_state *, gs_rect *, bool),
      gs_dashpath(gs_state *),
      gs_flattenpath(gs_state *),
      gs_reversepath(gs_state *),
      gs_strokepath(gs_state *),
      gs_strokepath2(gs_state *);

/* The extra argument for gs_upathbbox controls whether to include */
/* a trailing moveto in the bounding box. */
#define gs_pathbbox(pgs, prect)\
  gs_upathbbox(pgs, prect, false)

/* Path enumeration */

/* This interface conditionally makes a copy of the path. */
gs_path_enum *gs_path_enum_alloc(gs_memory_t *, client_name_t);
int gs_path_enum_copy_init(gs_memory_t *mem, gs_path_enum *, const gs_state *, bool);

#define gs_path_enum_init(mem, penum, pgs)\
  gs_path_enum_copy_init(mem, penum, pgs, true)
int gs_path_enum_next(gs_path_enum *, gs_point[3]);  /* 0 when done */
void gs_path_enum_cleanup(gs_path_enum *);

/* Clipping */
int gs_clippath(gs_state *),
    gs_initclip(gs_state *),
    gs_clip(gs_state *),
    gs_eoclip(gs_state *);

#endif /* gspath_INCLUDED */
