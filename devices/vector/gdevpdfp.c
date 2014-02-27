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


/* Get/put parameters for PDF-writing driver */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevpdfx.h"
#include "gdevpdfo.h"
#include "gdevpdfg.h"
#include "gsparamx.h"
#include "gsicc_manage.h"

/*
 * The pdfwrite device supports the following "real" parameters:
 *      OutputFile <string>
 *      all the Distiller parameters (also see gdevpsdp.c)
 * Only some of the Distiller parameters actually have any effect.
 *
 * The device also supports the following write-only pseudo-parameters that
 * serve only to communicate other information from the PostScript file.
 * Their "value" is an array of strings, some of which may be the result
 * of converting arbitrary PostScript objects to string form.
 *      pdfmark - see gdevpdfm.c
 *	DSC - processed in this file
 */
static int pdf_dsc_process(gx_device_pdf * pdev,
                            const gs_param_string_array * pma);

static const int CoreDistVersion = 5000;	/* Distiller 5.0 */
static const gs_param_item_t pdf_param_items[] = {
#define pi(key, type, memb) { key, type, offset_of(gx_device_pdf, memb) }

        /* Acrobat Distiller 4 parameters */

    /*
     * EndPage and StartPage are renamed because EndPage collides with
     * a page device parameter.
     */
    pi("PDFEndPage", gs_param_type_int, EndPage),
    pi("PDFStartPage", gs_param_type_int, StartPage),
    pi("Optimize", gs_param_type_bool, Optimize),
    pi("ParseDSCCommentsForDocInfo", gs_param_type_bool,
       ParseDSCCommentsForDocInfo),
    pi("ParseDSCComments", gs_param_type_bool, ParseDSCComments),
    pi("EmitDSCWarnings", gs_param_type_bool, EmitDSCWarnings),
    pi("CreateJobTicket", gs_param_type_bool, CreateJobTicket),
    pi("PreserveEPSInfo", gs_param_type_bool, PreserveEPSInfo),
    pi("AutoPositionEPSFiles", gs_param_type_bool, AutoPositionEPSFiles),
    pi("PreserveCopyPage", gs_param_type_bool, PreserveCopyPage),
    pi("UsePrologue", gs_param_type_bool, UsePrologue),

        /* Acrobat Distiller 5 parameters */

    pi("OffOptimizations", gs_param_type_int, OffOptimizations),

        /* Ghostscript-specific parameters */

    pi("ReAssignCharacters", gs_param_type_bool, ReAssignCharacters),
    pi("ReEncodeCharacters", gs_param_type_bool, ReEncodeCharacters),
    pi("FirstObjectNumber", gs_param_type_long, FirstObjectNumber),
    pi("CompressFonts", gs_param_type_bool, CompressFonts),
    pi("PrintStatistics", gs_param_type_bool, PrintStatistics),
    pi("MaxInlineImageSize", gs_param_type_long, MaxInlineImageSize),
    pi("DSCEncodingToUnicode", gs_param_type_int_array, DSCEncodingToUnicode),

        /* PDF Encryption */
    pi("OwnerPassword", gs_param_type_string, OwnerPassword),
    pi("UserPassword", gs_param_type_string, UserPassword),
    pi("KeyLength", gs_param_type_int, KeyLength),
    pi("Permissions", gs_param_type_int, Permissions),
    pi("EncryptionR", gs_param_type_int, EncryptionR),
    pi("NoEncrypt", gs_param_type_string, NoEncrypt),

        /* Target viewer capabilities (Ghostscript-specific)  */
 /* pi("ForOPDFRead", gs_param_type_bool, ForOPDFRead),			    pdfwrite-only */
    pi("ProduceDSC", gs_param_type_bool, ProduceDSC),
    pi("PatternImagemask", gs_param_type_bool, PatternImagemask),
    pi("MaxClipPathSize", gs_param_type_int, MaxClipPathSize),
    pi("MaxShadingBitmapSize", gs_param_type_int, MaxShadingBitmapSize),
#ifdef DEPRECATED_906
    pi("MaxViewerMemorySize", gs_param_type_int, MaxViewerMemorySize),
#endif
    pi("HaveTrueTypes", gs_param_type_bool, HaveTrueTypes),
    pi("HaveCIDSystem", gs_param_type_bool, HaveCIDSystem),
    pi("HaveTransparency", gs_param_type_bool, HaveTransparency),
    pi("CompressEntireFile", gs_param_type_bool, CompressEntireFile),
    pi("PDFX", gs_param_type_bool, PDFX),
    pi("PDFA", gs_param_type_int, PDFA),
    pi("DocumentUUID", gs_param_type_string, DocumentUUID),
    pi("InstanceUUID", gs_param_type_string, InstanceUUID),
    pi("DocumentTimeSeq", gs_param_type_int, DocumentTimeSeq),

    /* PDF/X parameters */
    pi("PDFXTrimBoxToMediaBoxOffset", gs_param_type_float_array, PDFXTrimBoxToMediaBoxOffset),
    pi("PDFXSetBleedBoxToMediaBox", gs_param_type_bool, PDFXSetBleedBoxToMediaBox),
    pi("PDFXBleedBoxToTrimBoxOffset", gs_param_type_float_array, PDFXBleedBoxToTrimBoxOffset),

    /* media selection parameters */
    pi("SetPageSize", gs_param_type_bool, SetPageSize),
    pi("RotatePages", gs_param_type_bool, RotatePages),
    pi("FitPages", gs_param_type_bool, FitPages),
    pi("CenterPages", gs_param_type_bool, CenterPages),
    pi("DoNumCopies", gs_param_type_bool, DoNumCopies),
    pi("PreserveSeparation", gs_param_type_bool, PreserveSeparation),
    pi("PreserveDeviceN", gs_param_type_bool, PreserveDeviceN),
    pi("PDFACompatibilityPolicy", gs_param_type_int, PDFACompatibilityPolicy),
    pi("DetectDuplicateImages", gs_param_type_bool, DetectDuplicateImages),
    pi("AllowIncrementalCFF", gs_param_type_bool, AllowIncrementalCFF),
    pi("WantsToUnicode", gs_param_type_bool, WantsToUnicode),
    pi("AllowPSRepeatFunctions", gs_param_type_bool, AllowPSRepeatFunctions),
    pi("IsDistiller", gs_param_type_bool, IsDistiller),
    pi("PreserveSMask", gs_param_type_bool, PreserveSMask),
    pi("PreserveTrMode", gs_param_type_bool, PreserveTrMode),
    pi("NoT3CCITT", gs_param_type_bool, NoT3CCITT),
    pi("PDFUseOldCMS", gs_param_type_bool, UseOldColor),
    pi("FastWebView", gs_param_type_bool, Linearise),
    pi("FirstPage", gs_param_type_int, FirstPage),
    pi("LastPage", gs_param_type_int, LastPage),
#undef pi
    gs_param_item_end
};

/*
  Notes on implementing the remaining Distiller functionality
  ===========================================================

  Architectural issues
  --------------------

  Must optionally disable application of TR, BG, UCR similarly.  Affects:
    PreserveHalftoneInfo
    PreserveOverprintSettings
    TransferFunctionInfo
    UCRandBGInfo

  Current limitations
  -------------------

  Non-primary elements in HalftoneType 5 are not written correctly

  Acrobat Distiller 3
  -------------------

  ---- Image parameters ----

  AntiAlias{Color,Gray,Mono}Images

  ---- Other parameters ----

  CompressPages
    Compress things other than page contents
  * PreserveHalftoneInfo
  PreserveOPIComments
    ? see OPI spec?
  * PreserveOverprintSettings
  * TransferFunctionInfo
  * UCRandBGInfo
  ColorConversionStrategy
    Select color space for drawing commands
  ConvertImagesToIndexed
    Postprocess image data *after* downsampling (requires an extra pass)

  Acrobat Distiller 4
  -------------------

  ---- Other functionality ----

  Document structure pdfmarks

  ---- Parameters ----

  xxxDownsampleType = /Bicubic
    Add new filter (or use siscale?) & to setup (gdevpsdi.c)
  DetectBlends
    Idiom recognition?  PatternType 2 patterns / shfill?  (see AD4)
  DoThumbnails
    Also output to memory device -- resolution issue

  ---- Job-level control ----

  EmitDSCWarnings
    Require DSC parser / interceptor
  CreateJobTicket
    ?
  AutoPositionEPSFiles
    Require DSC parsing
  PreserveCopyPage
    Concatenate Contents streams
  UsePrologue
    Needs hack in top-level control?

*/

/* ---------------- Get parameters ---------------- */

/* Get parameters. */
int
gdev_pdf_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    float cl = (float)pdev->CompatibilityLevel;
    int code;
    int cdv = CoreDistVersion;

    pdev->ParamCompatibilityLevel = cl;
    code = gdev_psdf_get_params(dev, plist);
    if (code < 0 ||
        (code = param_write_int(plist, "CoreDistVersion", &cdv)) < 0 ||
        (code = param_write_float(plist, "CompatibilityLevel", &cl)) < 0 ||
        (!pdev->is_ps2write && (code = param_write_bool(plist, "ForOPDFRead", &pdev->ForOPDFRead)) < 0) ||
        /* Indicate that we can process pdfmark and DSC. */
        (param_requested(plist, "pdfmark") > 0 &&
         (code = param_write_null(plist, "pdfmark")) < 0) ||
        (param_requested(plist, "DSC") > 0 &&
         (code = param_write_null(plist, "DSC")) < 0) ||
        (code = gs_param_write_items(plist, pdev, NULL, pdf_param_items)) < 0
        )
    {}
    return code;
}

/* ---------------- Put parameters ---------------- */

/* Put parameters, implementation */
static int
gdev_pdf_put_params_impl(gx_device * dev, const gx_device_pdf * save_dev, gs_param_list * plist)
{
    int ecode, code;
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    float cl = (float)pdev->CompatibilityLevel;
    bool locked = pdev->params.LockDistillerParams;
    gs_param_name param_name;
    enum psdf_color_conversion_strategy save_ccs = pdev->params.ColorConversionStrategy;

    pdev->pdf_memory = gs_memory_stable(pdev->memory);
    /*
     * If this is a pseudo-parameter (pdfmark or DSC),
     * don't bother checking for any real ones.
     */

    {
        gs_param_string_array ppa;
        gs_param_string pps;

        code = param_read_string_array(plist, (param_name = "pdfmark"), &ppa);
        switch (code) {
            case 0:
                code = pdfwrite_pdf_open_document(pdev);
                if (code < 0)
                    return code;
                code = pdfmark_process(pdev, &ppa);
                if (code >= 0)
                    return code;
                /* falls through for errors */
            default:
                param_signal_error(plist, param_name, code);
                return code;
            case 1:
                break;
        }

        code = param_read_string_array(plist, (param_name = "DSC"), &ppa);
        switch (code) {
            case 0:
                code = pdfwrite_pdf_open_document(pdev);
                if (code < 0)
                    return code;
                code = pdf_dsc_process(pdev, &ppa);
                if (code >= 0)
                    return code;
                /* falls through for errors */
            default:
                param_signal_error(plist, param_name, code);
                return code;
            case 1:
                break;
        }

        code = param_read_string(plist, (param_name = "pdfpagelabels"), &pps);
        switch (code) {
            case 0:
                {
                    if (!pdev->ForOPDFRead) {
                        cos_dict_t *const pcd = pdev->Catalog;
                        code = pdfwrite_pdf_open_document(pdev);
                        if (code < 0)
                            return code;
                        code = cos_dict_put_string(pcd, (const byte *)"/PageLabels", 11,
                                   pps.data, pps.size);
                        if (code >= 0)
                            return code;
                    } else
                        return 0;
                 }
                /* falls through for errors */
            default:
                param_signal_error(plist, param_name, code);
                return code;
            case 1:
                break;
        }
    }

    /*
     * Check for LockDistillerParams before doing anything else.
     * If LockDistillerParams is true and is not being set to false,
     * ignore all resettings of PDF-specific parameters.  Note that
     * LockDistillerParams is read again, and reset if necessary, in
     * psdf_put_params.
     */
    ecode = code = param_read_bool(plist, "LockDistillerParams", &locked);
    if (ecode < 0)
        param_signal_error(plist, param_name, ecode);

    if (!(locked && pdev->params.LockDistillerParams)) {
        /* General parameters. */

        {
            int efo = 1;

            ecode = param_put_int(plist, (param_name = ".EmbedFontObjects"), &efo, ecode);
            if (ecode < 0)
                param_signal_error(plist, param_name, ecode);
            if (efo != 1)
                param_signal_error(plist, param_name, ecode = gs_error_rangecheck);
        }
        {
            int cdv = CoreDistVersion;

            ecode = param_put_int(plist, (param_name = "CoreDistVersion"), &cdv, ecode);
            if (ecode < 0)
                return gs_note_error(ecode);
            if (cdv != CoreDistVersion)
                param_signal_error(plist, param_name, ecode = gs_error_rangecheck);
        }

        switch (code = param_read_float(plist, (param_name = "CompatibilityLevel"), &cl)) {
            default:
                ecode = code;
                param_signal_error(plist, param_name, ecode);
            case 0:
                /*
                 * Must be 1.2, 1.3, 1.4, or 1.5.  Per Adobe documentation, substitute
                 * the nearest achievable value.
                 */
                if (cl < (float)1.15)
                    cl = (float)1.1;
                else if (cl < (float)1.25)
                    cl = (float)1.2;
                else if (cl < (float)1.35)
                    cl = (float)1.3;
                else if (cl < (float)1.45)
                    cl = (float)1.4;
                else if (cl < (float)1.55)
                    cl = (float)1.5;
                else if (cl < (float)1.65)
                    cl = (float)1.6;
                else
                    cl = (float)1.7;
            case 1:
                break;
        }
        {   /* HACK : gs_param_list_s::memory is documented in gsparam.h as
               "for allocating coerced arrays". Not sure why zputdeviceparams
               sets it to the current memory space, while the device
               assumes to store them in the device's memory space.
               As a hackish workaround we temporary replace it here.
               Doing so because we don't want to change the global code now
               because we're unable to test it with all devices.
               Bug 688531 "Segmentation fault running pdfwrite from 219-01.ps".

               This solution to be reconsidered after fixing
               the bug 688533 "zputdeviceparams specifies a wrong memory space.".
            */
            gs_memory_t *mem = plist->memory;

            plist->memory = pdev->pdf_memory;
            code = gs_param_read_items(plist, pdev, pdf_param_items);
            if (code < 0 ||
                (!pdev->is_ps2write && (code = param_read_bool(plist, "ForOPDFRead", &pdev->ForOPDFRead)) < 0)
                ){
            }
            plist->memory = mem;
        }
        if (code < 0)
            ecode = code;
        {
            /*
             * Setting FirstObjectNumber is only legal if the file
             * has just been opened and nothing has been written,
             * or if we are setting it to the same value.
             */
            long fon = pdev->FirstObjectNumber;

            if (fon != save_dev->FirstObjectNumber) {
                if (fon <= 0 || fon > 0x7fff0000 ||
                    (pdev->next_id != 0 &&
                     pdev->next_id !=
                     save_dev->FirstObjectNumber + pdf_num_initial_ids)
                    ) {
                    ecode = gs_error_rangecheck;
                    param_signal_error(plist, "FirstObjectNumber", ecode);
                }
            }
        }
        {
            /*
             * Set ProcessColorModel now, because gx_default_put_params checks
             * it.
             */
            static const char *const pcm_names[] = {
                "DeviceGray", "DeviceRGB", "DeviceCMYK", "DeviceN", 0
            };
            int pcm = -1;

            ecode = param_put_enum(plist, "ProcessColorModel", &pcm,
                                   pcm_names, ecode);
            if (pcm >= 0) {
                pdf_set_process_color_model(pdev, pcm);
                pdf_set_initial_color(pdev, &pdev->saved_fill_color, &pdev->saved_stroke_color,
                                &pdev->fill_used_process_color, &pdev->stroke_used_process_color);
            }
        }
    }
    if (ecode < 0)
        goto fail;

    if (pdev->is_ps2write && (code = param_read_bool(plist, "ProduceDSC", &pdev->ProduceDSC)) < 0) {
        param_signal_error(plist, param_name, code);
    }

    /* PDFA and PDFX are stored in the page device dictionary and therefore
     * set on every setpagedevice. However, if we have encountered a file which
     * can't be made this way, and the PDFACompatibilityPolicy is 1, we want to
     * continue producing the file, but not as a PDF/A or PDF/X file. Its more
     * or less impossible to alter the setting in the (potentially saved) page
     * device dictionary, so we use this rather clunky method.
     */
    if (pdev->PDFA < 0 || pdev->PDFA > 2){
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "PDFA", ecode);
        goto fail;
    }
    if(pdev->PDFA != 0 && pdev->AbortPDFAX)
        pdev->PDFA = 0;
    if(pdev->PDFX && pdev->AbortPDFAX)
        pdev->PDFX = 0;
    if (pdev->PDFX && pdev->PDFA != 0) {
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "PDFA", ecode);
        goto fail;
    }
    if (pdev->PDFX && pdev->ForOPDFRead) {
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "PDFX", ecode);
        goto fail;
    }
    if (pdev->PDFA != 0 && pdev->ForOPDFRead) {
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "PDFA", ecode);
        goto fail;
    }
    if (pdev->PDFA == 1 || pdev->PDFX || pdev->CompatibilityLevel < 1.4) {
         pdev->HaveTransparency = false;
         pdev->PreserveSMask = false;
    }
    /*
     * We have to set version to the new value, because the set of
     * legal parameter values for psdf_put_params varies according to
     * the version.
     */
    if (pdev->PDFX)
        cl = (float)1.3; /* Instead pdev->CompatibilityLevel = 1.2; - see below. */
    if (pdev->PDFA != 0 && cl < 1.4)
        cl = (float)1.4;
    pdev->version = (cl < 1.2 ? psdf_version_level2 : psdf_version_ll3);
    if (pdev->ForOPDFRead) {
        pdev->ResourcesBeforeUsage = true;
        pdev->HaveCFF = false;
        pdev->HavePDFWidths = false;
        pdev->HaveStrokeColor = false;
        cl = (float)1.2; /* Instead pdev->CompatibilityLevel = 1.2; - see below. */
        pdev->MaxInlineImageSize = max_long; /* Save printer's RAM from saving temporary image data.
                                                Immediate images doen't need buffering. */
        pdev->version = psdf_version_level2;
    } else {
        pdev->ResourcesBeforeUsage = false;
        pdev->HaveCFF = true;
        pdev->HavePDFWidths = true;
        pdev->HaveStrokeColor = true;
    }
    pdev->ParamCompatibilityLevel = cl;
    if (cl < 1.2) {
        pdev->HaveCFF = false;
    }
    ecode = gdev_psdf_put_params(dev, plist);
    if (ecode < 0)
        goto fail;
    if (!pdev->UseOldColor) {
        if (pdev->params.ConvertCMYKImagesToRGB) {
            if (pdev->params.ColorConversionStrategy == ccs_CMYK) {
                emprintf(pdev->memory, "ConvertCMYKImagesToRGB is not compatible with ColorConversionStrategy of CMYK\n");
            } else {
                if (pdev->params.ColorConversionStrategy == ccs_Gray) {
                    emprintf(pdev->memory, "ConvertCMYKImagesToRGB is not compatible with ColorConversionStrategy of Gray\n");
                } else {
                    if (pdev->icc_struct)
                        rc_decrement(pdev->icc_struct,
                                     "reset default profile\n");
                    pdf_set_process_color_model(pdev,1);
                    ecode = gsicc_init_device_profile_struct((gx_device *)pdev, NULL, 0);
                    if (ecode < 0)
                        goto fail;
                }
            }
        }
        switch (pdev->params.ColorConversionStrategy) {
            case ccs_LeaveColorUnchanged:
            case ccs_UseDeviceDependentColor:
            case ccs_UseDeviceIndependentColor:
            case ccs_UseDeviceIndependentColorForImages:
            case ccs_ByObjectType:
            case ccs_sRGB:
                break;
            case ccs_CMYK:
                if (pdev->icc_struct)
                    rc_decrement(pdev->icc_struct,
                                 "reset default profile\n");
                pdf_set_process_color_model(pdev, 2);
                ecode = gsicc_init_device_profile_struct((gx_device *)pdev, NULL, 0);
                if (ecode < 0)
                    goto fail;
                break;
            case ccs_Gray:
                if (pdev->icc_struct)
                    rc_decrement(pdev->icc_struct,
                                 "reset default profile\n");
                pdf_set_process_color_model(pdev,0);
                ecode = gsicc_init_device_profile_struct((gx_device *)pdev, NULL, 0);
                if (ecode < 0)
                    goto fail;
                break;
            case ccs_RGB:
                /* Only bother to do this if we didn't handle it above */
                if (!pdev->params.ConvertCMYKImagesToRGB) {
                    if (pdev->icc_struct)
                        rc_decrement(pdev->icc_struct,
                                     "reset default profile\n");
                    pdf_set_process_color_model(pdev,1);
                    ecode = gsicc_init_device_profile_struct((gx_device *)pdev, NULL, 0);
                    if (ecode < 0)
                        goto fail;
                }
                break;
            default:
                break;
        }
    } else {
        if ((pdev->params.ColorConversionStrategy == ccs_CMYK &&
             strcmp(pdev->color_info.cm_name, "DeviceCMYK")) ||
            (pdev->params.ColorConversionStrategy == ccs_sRGB &&
              strcmp(pdev->color_info.cm_name, "DeviceRGB")) ||
            (pdev->params.ColorConversionStrategy == ccs_Gray &&
              strcmp(pdev->color_info.cm_name, "DeviceGray"))) {
            emprintf(pdev->memory,
                     "ColorConversionStrategy is incompatible to ProcessColorModel.\n");
            ecode = gs_note_error(gs_error_rangecheck);
            pdev->params.ColorConversionStrategy = save_ccs;
        }
        if (pdev->params.ColorConversionStrategy == ccs_UseDeviceIndependentColor) {
            if (!pdev->UseCIEColor) {
                emprintf(pdev->memory,
                         "Set UseCIEColor for UseDeviceIndependentColor to work properly.\n");
                ecode = gs_note_error(gs_error_rangecheck);
                pdev->UseCIEColor = true;
            }
        }
        if (pdev->params.ColorConversionStrategy == ccs_UseDeviceIndependentColorForImages) {
            if (!pdev->UseCIEColor) {
                emprintf(pdev->memory,
                         "UseDeviceDependentColorForImages is not supported. Use UseDeviceIndependentColor.\n");
                pdev->params.ColorConversionStrategy = ccs_UseDeviceIndependentColor;
                if (!pdev->UseCIEColor) {
                    emprintf(pdev->memory,
                             "Set UseCIEColor for UseDeviceIndependentColor to work properly.\n");
                    ecode = gs_note_error(gs_error_rangecheck);
                    pdev->UseCIEColor = true;
                }
            }
        }
        if (pdev->params.ColorConversionStrategy == ccs_UseDeviceDependentColor) {
            if (!strcmp(pdev->color_info.cm_name, "DeviceCMYK")) {
                emprintf(pdev->memory,
                         "Replacing the deprecated device parameter value UseDeviceDependentColor with CMYK.\n");
                pdev->params.ColorConversionStrategy = ccs_CMYK;
            } else if (!strcmp(pdev->color_info.cm_name, "DeviceRGB")) {
                emprintf(pdev->memory,
                         "Replacing the deprecated device parameter value UseDeviceDependentColor with sRGB.\n");
                pdev->params.ColorConversionStrategy = ccs_sRGB;
            } else {
                emprintf(pdev->memory,
                         "Replacing the deprecated device parameter value UseDeviceDependentColor with Gray.\n");
                pdev->params.ColorConversionStrategy = ccs_Gray;
            }
        }
    }
    if (cl < 1.5 && pdev->params.ColorImage.Filter != NULL &&
            !strcmp(pdev->params.ColorImage.Filter, "JPXEncode")) {
        emprintf(pdev->memory,
                 "JPXEncode requires CompatibilityLevel >= 1.5 .\n");
        ecode = gs_note_error(gs_error_rangecheck);
    }
    if (cl < 1.5 && pdev->params.GrayImage.Filter != NULL &&
            !strcmp(pdev->params.GrayImage.Filter, "JPXEncode")) {
        emprintf(pdev->memory,
                 "JPXEncode requires CompatibilityLevel >= 1.5 .\n");
        ecode = gs_note_error(gs_error_rangecheck);
    }
    if (cl < 1.4  && pdev->params.MonoImage.Filter != NULL &&
            !strcmp(pdev->params.MonoImage.Filter, "JBIG2Encode")) {
        emprintf(pdev->memory,
                 "JBIG2Encode requires CompatibilityLevel >= 1.4 .\n");
        ecode = gs_note_error(gs_error_rangecheck);
    }
    if (pdev->HaveTrueTypes && pdev->version == psdf_version_level2) {
        pdev->version = psdf_version_level2_with_TT ;
    }
    if (ecode < 0)
        goto fail;
    /*
     * Acrobat Reader doesn't handle user-space coordinates larger than
     * MAX_USER_COORD.  To compensate for this, reduce the resolution so
     * that the page size in device space (which we equate to user space) is
     * significantly less than MAX_USER_COORD.  Note that this still does
     * not protect us against input files that use coordinates far outside
     * the page boundaries.
     */
#define MAX_EXTENT ((int)(MAX_USER_COORD * 0.9))
    /* Changing resolution or page size requires closing the device, */
    if (dev->height > MAX_EXTENT || dev->width > MAX_EXTENT) {
        double factor =
            max(dev->height / (double)MAX_EXTENT,
                dev->width / (double)MAX_EXTENT);

        gx_device_set_resolution(dev, dev->HWResolution[0] / factor,
                                 dev->HWResolution[1] / factor);
    }
#undef MAX_EXTENT
    if (pdev->FirstObjectNumber != save_dev->FirstObjectNumber) {
        if (pdev->xref.file != 0) {
            gp_fseek_64(pdev->xref.file, 0L, SEEK_SET);
            pdf_initialize_ids(pdev);
        }
    }
    /* Handle the float/double mismatch. */
    pdev->CompatibilityLevel = (int)(cl * 10 + 0.5) / 10.0;
    if(pdev->OwnerPassword.size != save_dev->OwnerPassword.size ||
        (pdev->OwnerPassword.size != 0 &&
         memcmp(pdev->OwnerPassword.data, save_dev->OwnerPassword.data,
         pdev->OwnerPassword.size) != 0)) {
        if (pdev->is_open) {
            if (pdev->PageCount == 0) {
                gs_closedevice((gx_device *)save_dev);
                return 0;
            }
            else
                emprintf(pdev->memory, "Owner Password changed mid-job, ignoring.\n");
        }
    }

    if (pdev->Linearise && pdev->is_ps2write) {
        emprintf(pdev->memory, "Can't linearise PostScript output, ignoring\n");
        pdev->Linearise = false;
    }

    return 0;
 fail:
    /* Restore all the parameters to their original state. */
    pdev->version = save_dev->version;
    pdf_set_process_color_model(pdev, save_dev->pcm_color_info_index);
    pdev->saved_fill_color = save_dev->saved_fill_color;
    pdev->saved_stroke_color = save_dev->saved_fill_color;
    {
        const gs_param_item_t *ppi = pdf_param_items;

        for (; ppi->key; ++ppi)
            memcpy((char *)pdev + ppi->offset,
                   (char *)save_dev + ppi->offset,
                   gs_param_type_sizes[ppi->type]);
        pdev->ForOPDFRead = save_dev->ForOPDFRead;
    }
    return ecode;
}

/* Put parameters */
int
gdev_pdf_put_params(gx_device * dev, gs_param_list * plist)
{
    int code;
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    gs_memory_t *mem = gs_memory_stable(pdev->memory);
    gx_device_pdf *save_dev = gs_malloc(mem, sizeof(gx_device_pdf), 1,
        "saved gx_device_pdf");

    if (!save_dev)
        return_error(gs_error_VMerror);
    memcpy(save_dev, pdev, sizeof(gx_device_pdf));
    code = gdev_pdf_put_params_impl(dev, save_dev, plist);
    gs_free(mem, save_dev, sizeof(gx_device_pdf), 1, "saved gx_device_pdf");
    return code;
}

/* ---------------- Process DSC comments ---------------- */

static int
pdf_dsc_process(gx_device_pdf * pdev, const gs_param_string_array * pma)
{
    /*
     * The Adobe "Distiller Parameters" documentation says that Distiller
     * looks at DSC comments, but it doesn't say which ones.  We look at
     * the ones that we see how to map directly to obvious PDF constructs.
     */
    int code = 0;
    uint i;

    /*
     * If ParseDSCComments is false, all DSC comments are ignored, even if
     * ParseDSCComentsForDocInfo or PreserveEPSInfo is true.
     */
    if (!pdev->ParseDSCComments)
        return 0;

    for (i = 0; i + 1 < pma->size && code >= 0; i += 2) {
        const gs_param_string *pkey = &pma->data[i];
        const gs_param_string *pvalue = &pma->data[i + 1];
        const char *key;

        /*
         * %%For, %%Creator, and %%Title are recognized only if either
         * ParseDSCCommentsForDocInfo or PreserveEPSInfo is true.
         * The other DSC comments are always recognized.
         *
         * Acrobat Distiller sets CreationDate and ModDate to the current
         * time, not the value of %%CreationDate.  We think this is wrong,
         * but we do the same -- we ignore %%CreationDate here.
         */

        if (pdf_key_eq(pkey, "Creator"))
            key = "/Creator";
        else if (pdf_key_eq(pkey, "Title"))
            key = "/Title";
        else if (pdf_key_eq(pkey, "For"))
            key = "/Author";
        else {
            pdf_page_dsc_info_t *ppdi;
            char scan_buf[200]; /* arbitrary */

            if ((ppdi = &pdev->doc_dsc_info,
                 pdf_key_eq(pkey, "Orientation")) ||
                (ppdi = &pdev->page_dsc_info,
                 pdf_key_eq(pkey, "PageOrientation"))
                ) {
                if (pvalue->size == 1 && pvalue->data[0] >= '0' &&
                    pvalue->data[0] <= '3'
                    )
                    ppdi->orientation = pvalue->data[0] - '0';
                else
                    ppdi->orientation = -1;
            } else if ((ppdi = &pdev->doc_dsc_info,
                        pdf_key_eq(pkey, "ViewingOrientation")) ||
                       (ppdi = &pdev->page_dsc_info,
                        pdf_key_eq(pkey, "PageViewingOrientation"))
                       ) {
                gs_matrix mat;
                int orient;

                if(pvalue->size >= sizeof(scan_buf) - 1)
                    continue;	/* error */
                memcpy(scan_buf, pvalue->data, pvalue->size);
                scan_buf[pvalue->size] = 0;
                if (sscanf(scan_buf, "[%g %g %g %g]",
                           &mat.xx, &mat.xy, &mat.yx, &mat.yy) != 4
                    )
                    continue;	/* error */
                for (orient = 0; orient < 4; ++orient) {
                    if (mat.xx == 1 && mat.xy == 0 && mat.yx == 0 && mat.yy == 1)
                        break;
                    gs_matrix_rotate(&mat, -90.0, &mat);
                }
                if (orient == 4) /* error */
                    orient = -1;
                ppdi->viewing_orientation = orient;
            } else {
                gs_rect box;

                if (pdf_key_eq(pkey, "EPSF")) {
                    pdev->is_EPS = (pvalue->size >= 1 && pvalue->data[0] != '0');
                    continue;
                }
                /*
                 * We only parse the BoundingBox for the sake of
                 * AutoPositionEPSFiles.
                 */
                if (pdf_key_eq(pkey, "BoundingBox"))
                    ppdi = &pdev->doc_dsc_info;
                else if (pdf_key_eq(pkey, "PageBoundingBox"))
                    ppdi = &pdev->page_dsc_info;
                else
                    continue;
                if(pvalue->size >= sizeof(scan_buf) - 1)
                    continue;	/* error */
                memcpy(scan_buf, pvalue->data, pvalue->size);
                scan_buf[pvalue->size] = 0;
                if (sscanf(scan_buf, "[%lg %lg %lg %lg]",
                           &box.p.x, &box.p.y, &box.q.x, &box.q.y) != 4
                    )
                    continue;	/* error */
                ppdi->bounding_box = box;
            }
            continue;
        }

        if (pdev->ParseDSCCommentsForDocInfo || pdev->PreserveEPSInfo)
            code = cos_dict_put_c_key_string(pdev->Info, key,
                                             pvalue->data, pvalue->size);
    }
    return code;
}
