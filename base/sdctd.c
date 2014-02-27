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


/* DCT decoding filter stream */
#include "memory_.h"
#include "stdio_.h"
#include "jpeglib_.h"
#include "jerror_.h"
#include "gdebug.h"
#include "gsmemory.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

/* ------ DCTDecode ------ */

/* JPEG source manager procedures */
static void
dctd_init_source(j_decompress_ptr dinfo)
{
}
static const JOCTET fake_eoi[2] =
{0xFF, JPEG_EOI};
static boolean
dctd_fill_input_buffer(j_decompress_ptr dinfo)
{
    jpeg_decompress_data *jddp =
    (jpeg_decompress_data *) ((char *)dinfo -
                              offset_of(jpeg_decompress_data, dinfo));

    if (!jddp->input_eod)
        return FALSE;		/* normal case: suspend processing */
    /* Reached end of source data without finding EOI */
    WARNMS(dinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    dinfo->src->next_input_byte = fake_eoi;
    dinfo->src->bytes_in_buffer = 2;
    jddp->faked_eoi = true;	/* so process routine doesn't use next_input_byte */
    return TRUE;
}
static void
dctd_skip_input_data(j_decompress_ptr dinfo, long num_bytes)
{
    struct jpeg_source_mgr *src = dinfo->src;
    jpeg_decompress_data *jddp =
    (jpeg_decompress_data *) ((char *)dinfo -
                              offset_of(jpeg_decompress_data, dinfo));

    if (num_bytes > 0) {
        if (num_bytes > src->bytes_in_buffer) {
            jddp->skip += num_bytes - src->bytes_in_buffer;
            src->next_input_byte += src->bytes_in_buffer;
            src->bytes_in_buffer = 0;
            return;
        }
        src->next_input_byte += num_bytes;
        src->bytes_in_buffer -= num_bytes;
    }
}
static void
dctd_term_source(j_decompress_ptr dinfo)
{
}

/* Set the defaults for the DCTDecode filter. */
static void
s_DCTD_set_defaults(stream_state * st)
{
    s_DCT_set_defaults(st);
}

/* Initialize DCTDecode filter */
static int
s_DCTD_init(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    struct jpeg_source_mgr *src = &ss->data.decompress->source;

    src->init_source = dctd_init_source;
    src->fill_input_buffer = dctd_fill_input_buffer;
    src->skip_input_data = dctd_skip_input_data;
    src->term_source = dctd_term_source;
    src->resync_to_restart = jpeg_resync_to_restart;	/* use default method */
    ss->data.common->memory = ss->jpeg_memory;
    ss->data.decompress->dinfo.src = src;
    ss->data.decompress->skip = 0;
    ss->data.decompress->input_eod = false;
    ss->data.decompress->faked_eoi = false;
    ss->phase = 0;
    return 0;
}

static int
compact_jpeg_buffer(stream_cursor_read *pr)
{
    byte *o, *i;

    /* Search backwards from the end for 2 consecutive 0xFFs */
    o = (byte *)pr->limit;
    while (o - pr->ptr >= 2) {
        if (*o-- == 0xFF) {
            if (*o == 0xFF)
                goto compact;
            o--;
        }
    }
    return 0;
compact:
    i = o-1;
    do {
        /* Skip i backwards over 0xFFs */
        while ((i != pr->ptr) && (*i == 0xFF))
            i--;
        /* Repeatedly copy from i to o */
        while (i != pr->ptr) {
            byte c = *i--;
            *o-- = c;
            if (c == 0xFF)
                break;
        }
    } while (i != pr->ptr);

    pr->ptr = o;
    return o - i;
}

/* Process a buffer */
static int
s_DCTD_process(stream_state * st, stream_cursor_read * pr,
               stream_cursor_write * pw, bool last)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;
    jpeg_decompress_data *jddp = ss->data.decompress;
    struct jpeg_source_mgr *src = jddp->dinfo.src;
    int code;

    if_debug3m('w', st->memory, "[wdd]process avail=%u, skip=%u, last=%d\n",
               (uint) (pr->limit - pr->ptr), (uint) jddp->skip, last);
    if (jddp->skip != 0) {
        long avail = pr->limit - pr->ptr;

        if (avail < jddp->skip) {
            jddp->skip -= avail;
            pr->ptr = pr->limit;
            if (!last)
                return 0;	/* need more data */
            jddp->skip = 0;	/* don't skip past input EOD */
        }
        pr->ptr += jddp->skip;
        jddp->skip = 0;
    }
    src->next_input_byte = pr->ptr + 1;
    src->bytes_in_buffer = pr->limit - pr->ptr;
    jddp->input_eod = last;
    switch (ss->phase) {
        case 0:		/* not initialized yet */
            /*
             * Adobe implementations seem to ignore leading garbage bytes,
             * even though neither the standard nor Adobe's own
             * documentation mention this.
             */
            while (pr->ptr < pr->limit && pr->ptr[1] != 0xff)
                pr->ptr++;
            if (pr->ptr == pr->limit)
                return 0;
            src->next_input_byte = pr->ptr + 1;
            src->bytes_in_buffer = pr->limit - pr->ptr;
            ss->phase = 1;
            /* falls through */
        case 1:		/* reading header markers */
            if ((code = gs_jpeg_read_header(ss, TRUE)) < 0)
                return ERRC;
            pr->ptr =
                (jddp->faked_eoi ? pr->limit : src->next_input_byte - 1);
            switch (code) {
                case JPEG_SUSPENDED:
                    return 0;
                    /*case JPEG_HEADER_OK: */
            }

            /* 
             * Default the color transform if not set and check for
             * the Adobe marker and use Adobe's transform if the
             * marker is set.
             */
            if (ss->ColorTransform == -1) {
                if (jddp->dinfo.num_components == 3)
                    ss->ColorTransform = 1;
                else
                    ss->ColorTransform = 0;
            }
            
            if (jddp->dinfo.saw_Adobe_marker)
                ss->ColorTransform = jddp->dinfo.Adobe_transform;
            
            switch (jddp->dinfo.num_components) {
            case 3:
                jddp->dinfo.jpeg_color_space =
                    (ss->ColorTransform ? JCS_YCbCr : JCS_RGB);
                        /* out_color_space will default to JCS_RGB */
                        break;
            case 4:
                jddp->dinfo.jpeg_color_space =
                    (ss->ColorTransform ? JCS_YCCK : JCS_CMYK);
                /* out_color_space will default to JCS_CMYK */
                break;
            }
            ss->phase = 2;
            /* falls through */
        case 2:		/* start_decompress */
            if ((code = gs_jpeg_start_decompress(ss)) < 0)
                return ERRC;
            pr->ptr =
                (jddp->faked_eoi ? pr->limit : src->next_input_byte - 1);
            if (code == 0)
                return 0;
            ss->scan_line_size =
                jddp->dinfo.output_width * jddp->dinfo.output_components;
            if_debug4m('w', ss->memory, "[wdd]width=%u, components=%d, scan_line_size=%u, min_out_size=%u\n",
                       jddp->dinfo.output_width,
                       jddp->dinfo.output_components,
                       ss->scan_line_size, jddp->templat.min_out_size);
            if (ss->scan_line_size > (uint) jddp->templat.min_out_size) {
                /* Create a spare buffer for oversize scanline */
                jddp->scanline_buffer =
                    gs_alloc_bytes_immovable(gs_memory_stable(jddp->memory),
                                             ss->scan_line_size,
                                         "s_DCTD_process(scanline_buffer)");
                if (jddp->scanline_buffer == NULL)
                    return ERRC;
            }
            jddp->bytes_in_scanline = 0;
            ss->phase = 3;
            /* falls through */
        case 3:		/* reading data */
          dumpbuffer:
            if (jddp->bytes_in_scanline != 0) {
                uint avail = pw->limit - pw->ptr;
                uint tomove = min(jddp->bytes_in_scanline,
                                  avail);

                if_debug2m('w', ss->memory, "[wdd]moving %u/%u\n",
                           tomove, avail);
                memcpy(pw->ptr + 1, jddp->scanline_buffer +
                       (ss->scan_line_size - jddp->bytes_in_scanline),
                       tomove);
                pw->ptr += tomove;
                jddp->bytes_in_scanline -= tomove;
                if (jddp->bytes_in_scanline != 0)
                    return 1;	/* need more room */
            }
            while (jddp->dinfo.output_height > jddp->dinfo.output_scanline) {
                int read;
                byte *samples;

                if (jddp->scanline_buffer != NULL)
                    samples = jddp->scanline_buffer;
                else {
                    if ((uint) (pw->limit - pw->ptr) < ss->scan_line_size)
                        return 1;	/* need more room */
                    samples = pw->ptr + 1;
                }
                read = gs_jpeg_read_scanlines(ss, &samples, 1);
                if (read < 0)
                    return ERRC;
                if_debug3m('w', ss->memory, "[wdd]read returns %d, used=%u, faked_eoi=%d\n",
                           read,
                           (uint) (src->next_input_byte - 1 - pr->ptr),
                           (int)jddp->faked_eoi);
                pr->ptr =
                    (jddp->faked_eoi ? pr->limit : src->next_input_byte - 1);
                if (!read) {
                    /* We are suspending. If nothing was consumed, and the
                     * buffer was full, compact the data in the buffer. If
                     * this fails to save anything, then we'll never succeed;
                     * throw an error to avoid an infinite loop.
                     * The tricky part here is knowing "if the buffer is
                     * full"; we do that by comparing the number of bytes in
                     * the buffer with the min_in_size set for the stream.
                     */
                    /* TODO: If we ever find a file with valid data that trips
                     * this test, we should implement a scheme whereby we keep
                     * a local buffer and copy the data into it. The local
                     * buffer can be grown as required. */
                    if ((src->next_input_byte-1 == pr->ptr) &&
                        (pr->limit - pr->ptr >= ss->templat->min_in_size) &&
                        (compact_jpeg_buffer(pr) == 0))
                        return ERRC;
                    return 0;	/* need more data */
                }
                if (jddp->scanline_buffer != NULL) {
                    jddp->bytes_in_scanline = ss->scan_line_size;
                    goto dumpbuffer;
                }
                pw->ptr += ss->scan_line_size;
            }
            ss->phase = 4;
            /* falls through */
        case 4:		/* end of image; scan for EOI */
            if ((code = gs_jpeg_finish_decompress(ss)) < 0)
                return ERRC;
            pr->ptr =
                (jddp->faked_eoi ? pr->limit : src->next_input_byte - 1);
            if (code == 0)
                return 0;
            ss->phase = 5;
            /* falls through */
        case 5:		/* we are DONE */
            return EOFC;
    }
    /* Default case can't happen.... */
    return ERRC;
}

/* Release the stream */
static void
s_DCTD_release(stream_state * st)
{
    stream_DCT_state *const ss = (stream_DCT_state *) st;

    gs_jpeg_destroy(ss);
    if (ss->data.decompress->scanline_buffer != NULL)
        gs_free_object(gs_memory_stable(ss->data.common->memory),
                       ss->data.decompress->scanline_buffer,
                       "s_DCTD_release(scanline_buffer)");
    gs_free_object(ss->data.common->memory, ss->data.decompress,
                   "s_DCTD_release");
    /* Switch the template pointer back in case we still need it. */
    st->templat = &s_DCTD_template;
}

/* Stream template */
const stream_template s_DCTD_template =
{&st_DCT_state, s_DCTD_init, s_DCTD_process, 2000, 4000, s_DCTD_release,
 s_DCTD_set_defaults
};
