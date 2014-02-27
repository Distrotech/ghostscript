/*
 * "$Id: mark.c 9293 2010-09-02 17:52:24Z mike $"
 *
 *   Option marking routines for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsMarkOptions()     - Mark command-line options in a PPD file.
 *   ppdFindChoice()       - Return a pointer to an option choice.
 *   ppdFindMarkedChoice() - Return the marked choice for the specified option.
 *   ppdFindOption()       - Return a pointer to the specified option.
 *   ppdIsMarked()         - Check to see if an option is marked.
 *   ppdMarkDefaults()     - Mark all default options in the PPD file.
 *   ppdMarkOption()       - Mark an option in a PPD file and return the number
 *                           of conflicts.
 *   ppdFirstOption()      - Return the first option in the PPD file.
 *   ppdNextOption()       - Return the next option in the PPD file.
 *   _ppdParseOptions()    - Parse options from a PPD file.
 *   ppd_debug_marked()    - Output the marked array to stdout...
 *   ppd_defaults()        - Set the defaults for this group and all sub-groups.
 *   ppd_mark_choices()    - Mark one or more option choices from a string.
 *   ppd_mark_option()     - Quickly mark an option without checking for
 *                           conflicts.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"
#include "debug.h"
#include "ppd-private.h"
#include "pwg-private.h"


/*
 * Local functions...
 */

#ifdef DEBUG
static void	ppd_debug_marked(ppd_file_t *ppd, const char *title);
#else
#  define	ppd_debug_marked(ppd,title)
#endif /* DEBUG */
static void	ppd_defaults(ppd_file_t *ppd, ppd_group_t *g);
static void	ppd_mark_choices(ppd_file_t *ppd, const char *s);
static void	ppd_mark_option(ppd_file_t *ppd, const char *option,
		                const char *choice);


/*
 * 'cupsMarkOptions()' - Mark command-line options in a PPD file.
 *
 * This function maps the IPP "finishings", "media", "mirror",
 * "multiple-document-handling", "output-bin", "printer-resolution", and
 * "sides" attributes to their corresponding PPD options and choices.
 */

int					/* O - 1 if conflicts exist, 0 otherwise */
cupsMarkOptions(
    ppd_file_t    *ppd,			/* I - PPD file */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  int		i, j;			/* Looping vars */
  char		*ptr,			/* Pointer into string */
		s[255];			/* Temporary string */
  const char	*val,			/* Pointer into value */
		*media,			/* media option */
		*page_size,		/* PageSize option */
		*ppd_keyword;		/* PPD keyword */
  cups_option_t	*optptr;		/* Current option */
  ppd_attr_t	*attr;			/* PPD attribute */


 /*
  * Check arguments...
  */

  if (!ppd || num_options <= 0 || !options)
    return (0);

  ppd_debug_marked(ppd, "Before...");

 /*
  * Do special handling for media and PageSize...
  */

  media     = cupsGetOption("media", num_options, options);
  page_size = cupsGetOption("PageSize", num_options, options);

  if (media)
  {
   /*
    * Load PWG mapping data as needed...
    */

    if (!ppd->pwg)
      ppd->pwg = _pwgCreateWithPPD(ppd);

   /*
    * Loop through the option string, separating it at commas and
    * marking each individual option as long as the corresponding
    * PPD option (PageSize, InputSlot, etc.) is not also set.
    *
    * For PageSize, we also check for an empty option value since
    * some versions of MacOS X use it to specify auto-selection
    * of the media based solely on the size.
    */

    for (val = media; *val;)
    {
     /*
      * Extract the sub-option from the string...
      */

      for (ptr = s; *val && *val != ',' && (ptr - s) < (sizeof(s) - 1);)
	*ptr++ = *val++;
      *ptr++ = '\0';

      if (*val == ',')
	val ++;

     /*
      * Mark it...
      */

      if (!page_size || !page_size[0])
      {
        if (!strncasecmp(s, "Custom.", 7) || ppdPageSize(ppd, s))
          ppd_mark_option(ppd, "PageSize", s);
        else if ((ppd_keyword = _pwgGetPageSize((_pwg_t *)ppd->pwg, NULL, s,
	                                        NULL)) != NULL)
	  ppd_mark_option(ppd, "PageSize", ppd_keyword);
      }

      if (!cupsGetOption("InputSlot", num_options, options) &&
	  (ppd_keyword = _pwgGetInputSlot((_pwg_t *)ppd->pwg, NULL, s)) != NULL)
	ppd_mark_option(ppd, "InputSlot", ppd_keyword);

      if (!cupsGetOption("MediaType", num_options, options) &&
	  (ppd_keyword = _pwgGetMediaType((_pwg_t *)ppd->pwg, NULL, s)) != NULL)
	ppd_mark_option(ppd, "MediaType", ppd_keyword);
    }
  }

 /*
  * Mark other options...
  */

  for (i = num_options, optptr = options; i > 0; i --, optptr ++)
    if (!strcasecmp(optptr->name, "media") ||
        !strcasecmp(optptr->name, "output-bin") ||
	!strcasecmp(optptr->name, "output-mode") ||
	!strcasecmp(optptr->name, "sides"))
      continue;
    else if (!strcasecmp(optptr->name, "resolution") ||
             !strcasecmp(optptr->name, "printer-resolution"))
    {
      ppd_mark_option(ppd, "Resolution", optptr->value);
      ppd_mark_option(ppd, "SetResolution", optptr->value);
      	/* Calcomp, Linotype, QMS, Summagraphics, Tektronix, Varityper */
      ppd_mark_option(ppd, "JCLResolution", optptr->value);
      	/* HP */
      ppd_mark_option(ppd, "CNRes_PGP", optptr->value);
      	/* Canon */
    }
    else if (!strcasecmp(optptr->name, "multiple-document-handling"))
    {
      if (!cupsGetOption("Collate", num_options, options) &&
          ppdFindOption(ppd, "Collate"))
      {
        if (strcasecmp(optptr->value, "separate-documents-uncollated-copies"))
	  ppd_mark_option(ppd, "Collate", "True");
	else
	  ppd_mark_option(ppd, "Collate", "False");
      }
    }
    else if (!strcasecmp(optptr->name, "finishings"))
    {
     /*
      * Lookup cupsIPPFinishings attributes for each value...
      */

      for (ptr = optptr->value; *ptr;)
      {
       /*
        * Get the next finishings number...
	*/

        if (!isdigit(*ptr & 255))
	  break;

        if ((j = strtol(ptr, &ptr, 10)) < 3)
	  break;

       /*
        * Skip separator as needed...
	*/

        if (*ptr == ',')
	  ptr ++;

       /*
        * Look it up in the PPD file...
	*/

	sprintf(s, "%d", j);

        if ((attr = ppdFindAttr(ppd, "cupsIPPFinishings", s)) == NULL)
	  continue;

       /*
        * Apply "*Option Choice" settings from the attribute value...
	*/

        ppd_mark_choices(ppd, attr->value);
      }
    }
    else if (!strcasecmp(optptr->name, "print-quality"))
    {
      ppd_option_t	*output_mode = ppdFindOption(ppd, "OutputMode");
					/* OutputMode option */

      if (!strcmp(optptr->value, "3"))
      {
       /*
        * Draft quality...
	*/

	if (ppdFindChoice(output_mode, "Draft"))
	  ppd_mark_option(ppd, "OutputMode", "Draft");
	else if (ppdFindChoice(output_mode, "Fast"))
	  ppd_mark_option(ppd, "OutputMode", "Fast");

        if ((attr = ppdFindAttr(ppd, "APPrinterPreset",
	                        "DraftGray_with_Paper_Auto-Detect")) != NULL)
          ppd_mark_choices(ppd, attr->value);
      }
      else if (!strcmp(optptr->value, "4"))
      {
       /*
        * Normal quality...
	*/

	if (ppdFindChoice(output_mode, "Normal"))
	  ppd_mark_option(ppd, "OutputMode", "Normal");
	else if (ppdFindChoice(output_mode, "Good"))
	  ppd_mark_option(ppd, "OutputMode", "Good");

        if ((attr = ppdFindAttr(ppd, "APPrinterPreset",
	                        "Color_with_Paper_Auto-Detect")) != NULL)
          ppd_mark_choices(ppd, attr->value);
        else if ((attr = ppdFindAttr(ppd, "APPrinterPreset",
	                        "Gray_with_Paper_Auto-Detect")) != NULL)
          ppd_mark_choices(ppd, attr->value);
      }
      else if (!strcmp(optptr->value, "5"))
      {
       /*
        * High/best/photo quality...
	*/

	if (ppdFindChoice(output_mode, "Best"))
	  ppd_mark_option(ppd, "OutputMode", "Best");
	else if (ppdFindChoice(output_mode, "High"))
	  ppd_mark_option(ppd, "OutputMode", "High");

        if ((attr = ppdFindAttr(ppd, "APPrinterPreset",
	                        "Photo_on_Photo_Paper")) != NULL)
          ppd_mark_choices(ppd, attr->value);
      }
    }
    else if (!strcasecmp(optptr->name, "APPrinterPreset"))
    {
     /*
      * Lookup APPrinterPreset value...
      */

      if ((attr = ppdFindAttr(ppd, "APPrinterPreset", optptr->value)) != NULL)
      {
       /*
        * Apply "*Option Choice" settings from the attribute value...
	*/

        ppd_mark_choices(ppd, attr->value);
      }
    }
    else if (!strcasecmp(optptr->name, "mirror"))
      ppd_mark_option(ppd, "MirrorPrint", optptr->value);
    else
      ppd_mark_option(ppd, optptr->name, optptr->value);

  ppd_debug_marked(ppd, "After...");

  return (ppdConflicts(ppd) > 0);
}


/*
 * 'ppdFindChoice()' - Return a pointer to an option choice.
 */

ppd_choice_t *				/* O - Choice pointer or @code NULL@ */
ppdFindChoice(ppd_option_t *o,		/* I - Pointer to option */
              const char   *choice)	/* I - Name of choice */
{
  int		i;			/* Looping var */
  ppd_choice_t	*c;			/* Current choice */


  if (!o || !choice)
    return (NULL);

  if (choice[0] == '{' || !strncasecmp(choice, "Custom.", 7))
    choice = "Custom";

  for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
    if (!strcasecmp(c->choice, choice))
      return (c);

  return (NULL);
}


/*
 * 'ppdFindMarkedChoice()' - Return the marked choice for the specified option.
 */

ppd_choice_t *				/* O - Pointer to choice or @code NULL@ */
ppdFindMarkedChoice(ppd_file_t *ppd,	/* I - PPD file */
                    const char *option)	/* I - Keyword/option name */
{
  ppd_choice_t	key,			/* Search key for choice */
		*marked;		/* Marked choice */


  DEBUG_printf(("2ppdFindMarkedChoice(ppd=%p, option=\"%s\")", ppd, option));

  if ((key.option = ppdFindOption(ppd, option)) == NULL)
  {
    DEBUG_puts("3ppdFindMarkedChoice: Option not found, returning NULL");
    return (NULL);
  }

  marked = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key);

  DEBUG_printf(("3ppdFindMarkedChoice: Returning %p(%s)...", marked,
                marked ? marked->choice : "NULL"));

  return (marked);
}


/*
 * 'ppdFindOption()' - Return a pointer to the specified option.
 */

ppd_option_t *				/* O - Pointer to option or @code NULL@ */
ppdFindOption(ppd_file_t *ppd,		/* I - PPD file data */
              const char *option)	/* I - Option/Keyword name */
{
 /*
  * Range check input...
  */

  if (!ppd || !option)
    return (NULL);

  if (ppd->options)
  {
   /*
    * Search in the array...
    */

    ppd_option_t	key;		/* Option search key */


    strlcpy(key.keyword, option, sizeof(key.keyword));

    return ((ppd_option_t *)cupsArrayFind(ppd->options, &key));
  }
  else
  {
   /*
    * Search in each group...
    */

    int			i, j;		/* Looping vars */
    ppd_group_t		*group;		/* Current group */
    ppd_option_t	*optptr;	/* Current option */


    for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
      for (j = group->num_options, optptr = group->options;
           j > 0;
	   j --, optptr ++)
        if (!strcasecmp(optptr->keyword, option))
	  return (optptr);

    return (NULL);
  }
}


/*
 * 'ppdIsMarked()' - Check to see if an option is marked.
 */

int					/* O - Non-zero if option is marked */
ppdIsMarked(ppd_file_t *ppd,		/* I - PPD file data */
            const char *option,		/* I - Option/Keyword name */
            const char *choice)		/* I - Choice name */
{
  ppd_choice_t	key,			/* Search key */
		*c;			/* Choice pointer */


  if (!ppd)
    return (0);

  if ((key.option = ppdFindOption(ppd, option)) == NULL)
    return (0);

  if ((c = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) == NULL)
    return (0);

  return (!strcmp(c->choice, choice));
}


/*
 * 'ppdMarkDefaults()' - Mark all default options in the PPD file.
 */

void
ppdMarkDefaults(ppd_file_t *ppd)	/* I - PPD file record */
{
  int		i;			/* Looping variables */
  ppd_group_t	*g;			/* Current group */
  ppd_choice_t	*c;			/* Current choice */


  if (!ppd)
    return;

 /*
  * Clean out the marked array...
  */

  for (c = (ppd_choice_t *)cupsArrayFirst(ppd->marked);
       c;
       c = (ppd_choice_t *)cupsArrayNext(ppd->marked))
  {
    cupsArrayRemove(ppd->marked, c);
    c->marked = 0;
  }

 /*
  * Then repopulate it with the defaults...
  */

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
    ppd_defaults(ppd, g);
}


/*
 * 'ppdMarkOption()' - Mark an option in a PPD file and return the number of
 *                     conflicts.
 */

int					/* O - Number of conflicts */
ppdMarkOption(ppd_file_t *ppd,		/* I - PPD file record */
              const char *option,	/* I - Keyword */
              const char *choice)	/* I - Option name */
{
  DEBUG_printf(("ppdMarkOption(ppd=%p, option=\"%s\", choice=\"%s\")",
        	ppd, option, choice));

 /*
  * Range check input...
  */

  if (!ppd || !option || !choice)
    return (0);

 /*
  * Mark the option...
  */

  ppd_mark_option(ppd, option, choice);

 /*
  * Return the number of conflicts...
  */

  return (ppdConflicts(ppd));
}


/*
 * 'ppdFirstOption()' - Return the first option in the PPD file.
 *
 * Options are returned from all groups in ascending alphanumeric order.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ppd_option_t *				/* O - First option or @code NULL@ */
ppdFirstOption(ppd_file_t *ppd)		/* I - PPD file */
{
  if (!ppd)
    return (NULL);
  else
    return ((ppd_option_t *)cupsArrayFirst(ppd->options));
}


/*
 * 'ppdNextOption()' - Return the next option in the PPD file.
 *
 * Options are returned from all groups in ascending alphanumeric order.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ppd_option_t *				/* O - Next option or @code NULL@ */
ppdNextOption(ppd_file_t *ppd)		/* I - PPD file */
{
  if (!ppd)
    return (NULL);
  else
    return ((ppd_option_t *)cupsArrayNext(ppd->options));
}


/*
 * '_ppdParseOptions()' - Parse options from a PPD file.
 *
 * This function looks for strings of the form:
 *
 *     *option choice ... *optionN choiceN
 *     property value ... propertyN valueN
 *
 * It stops when it finds a string that doesn't match this format.
 */

int					/* O  - Number of options */
_ppdParseOptions(
    const char    *s,			/* I  - String to parse */
    int           num_options,		/* I  - Number of options */
    cups_option_t **options,		/* IO - Options */
    _ppd_parse_t  which)		/* I  - What to parse */
{
  char	option[PPD_MAX_NAME * 2 + 1],	/* Current option/property */
	choice[PPD_MAX_NAME],		/* Current choice/value */
	*ptr;				/* Pointer into option or choice */


  if (!s)
    return (num_options);

 /*
  * Read all of the "*Option Choice" and "property value" pairs from the
  * string, add them to an options array as we go...
  */

  while (*s)
  {
   /*
    * Skip leading whitespace...
    */

    while (_cups_isspace(*s))
      s ++;

   /*
    * Get the option/property name...
    */

    ptr = option;
    while (*s && !_cups_isspace(*s) && ptr < (option + sizeof(option) - 1))
      *ptr++ = *s++;

    if (ptr == s || !_cups_isspace(*s))
      break;

    *ptr = '\0';

   /*
    * Get the choice...
    */

    while (_cups_isspace(*s))
      s ++;

    if (!*s)
      break;

    ptr = choice;
    while (*s && !_cups_isspace(*s) && ptr < (choice + sizeof(choice) - 1))
      *ptr++ = *s++;

    if (*s && !_cups_isspace(*s))
      break;

    *ptr = '\0';

   /*
    * Add it to the options array...
    */

    if (option[0] == '*' && which != _PPD_PARSE_PROPERTIES)
      num_options = cupsAddOption(option + 1, choice, num_options, options);
    else if (option[0] != '*' && which != _PPD_PARSE_OPTIONS)
      num_options = cupsAddOption(option, choice, num_options, options);
  }

  return (num_options);
}


#ifdef DEBUG
/*
 * 'ppd_debug_marked()' - Output the marked array to stdout...
 */

static void
ppd_debug_marked(ppd_file_t *ppd,		/* I - PPD file data */
             const char *title)		/* I - Title for list */
{
  ppd_choice_t	*c;			/* Current choice */


  DEBUG_printf(("2cupsMarkOptions: %s", title));

  for (c = (ppd_choice_t *)cupsArrayFirst(ppd->marked);
       c;
       c = (ppd_choice_t *)cupsArrayNext(ppd->marked))
    DEBUG_printf(("2cupsMarkOptions: %s=%s", c->option->keyword, c->choice));
}
#endif /* DEBUG */


/*
 * 'ppd_defaults()' - Set the defaults for this group and all sub-groups.
 */

static void
ppd_defaults(ppd_file_t  *ppd,		/* I - PPD file */
             ppd_group_t *g)		/* I - Group to default */
{
  int		i;			/* Looping var */
  ppd_option_t	*o;			/* Current option */
  ppd_group_t	*sg;			/* Current sub-group */


  for (i = g->num_options, o = g->options; i > 0; i --, o ++)
    if (strcasecmp(o->keyword, "PageRegion") != 0)
      ppdMarkOption(ppd, o->keyword, o->defchoice);

  for (i = g->num_subgroups, sg = g->subgroups; i > 0; i --, sg ++)
    ppd_defaults(ppd, sg);
}


/*
 * 'ppd_mark_choices()' - Mark one or more option choices from a string.
 */

static void
ppd_mark_choices(ppd_file_t *ppd,	/* I - PPD file */
                 const char *s)		/* I - "*Option Choice ..." string */
{
  int		i,			/* Looping var */
		num_options;		/* Number of options */
  cups_option_t	*options,		/* Options */
		*option;		/* Current option */


  if (!s)
    return;

  options     = NULL;
  num_options = _ppdParseOptions(s, 0, &options, 0);

  for (i = num_options, option = options; i > 0; i --, option ++)
    ppd_mark_option(ppd, option->name, option->value);

  cupsFreeOptions(num_options, options);
}


/*
 * 'ppd_mark_option()' - Quick mark an option without checking for conflicts.
 */

static void
ppd_mark_option(ppd_file_t *ppd,	/* I - PPD file */
                const char *option,	/* I - Option name */
                const char *choice)	/* I - Choice name */
{
  int		i, j;			/* Looping vars */
  ppd_option_t	*o;			/* Option pointer */
  ppd_choice_t	*c,			/* Choice pointer */
		*oldc,			/* Old choice pointer */
		key;			/* Search key for choice */
  struct lconv	*loc;			/* Locale data */


  DEBUG_printf(("7ppd_mark_option(ppd=%p, option=\"%s\", choice=\"%s\")",
        	ppd, option, choice));

 /*
  * AP_D_InputSlot is the "default input slot" on MacOS X, and setting
  * it clears the regular InputSlot choices...
  */

  if (!strcasecmp(option, "AP_D_InputSlot"))
  {
    cupsArraySave(ppd->options);

    if ((o = ppdFindOption(ppd, "InputSlot")) != NULL)
    {
      key.option = o;
      if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
      {
        oldc->marked = 0;
        cupsArrayRemove(ppd->marked, oldc);
      }
    }

    cupsArrayRestore(ppd->options);
  }

 /*
  * Check for custom options...
  */

  cupsArraySave(ppd->options);

  o = ppdFindOption(ppd, option);

  cupsArrayRestore(ppd->options);

  if (!o)
    return;

  loc = localeconv();

  if (!strncasecmp(choice, "Custom.", 7))
  {
   /*
    * Handle a custom option...
    */

    if ((c = ppdFindChoice(o, "Custom")) == NULL)
      return;

    if (!strcasecmp(option, "PageSize"))
    {
     /*
      * Handle custom page sizes...
      */

      ppdPageSize(ppd, choice);
    }
    else
    {
     /*
      * Handle other custom options...
      */

      ppd_coption_t	*coption;	/* Custom option */
      ppd_cparam_t	*cparam;	/* Custom parameter */
      char		*units;		/* Custom points units */


      if ((coption = ppdFindCustomOption(ppd, option)) != NULL)
      {
        if ((cparam = (ppd_cparam_t *)cupsArrayFirst(coption->params)) == NULL)
	  return;

        switch (cparam->type)
	{
	  case PPD_CUSTOM_CURVE :
	  case PPD_CUSTOM_INVCURVE :
	  case PPD_CUSTOM_REAL :
	      cparam->current.custom_real = (float)_cupsStrScand(choice + 7,
	                                                         NULL, loc);
	      break;

	  case PPD_CUSTOM_POINTS :
	      cparam->current.custom_points = (float)_cupsStrScand(choice + 7,
	                                                           &units,
	                                                           loc);

              if (units)
	      {
        	if (!strcasecmp(units, "cm"))
	          cparam->current.custom_points *= 72.0f / 2.54f;
        	else if (!strcasecmp(units, "mm"))
	          cparam->current.custom_points *= 72.0f / 25.4f;
        	else if (!strcasecmp(units, "m"))
	          cparam->current.custom_points *= 72.0f / 0.0254f;
        	else if (!strcasecmp(units, "in"))
	          cparam->current.custom_points *= 72.0f;
        	else if (!strcasecmp(units, "ft"))
	          cparam->current.custom_points *= 12.0f * 72.0f;
              }
	      break;

	  case PPD_CUSTOM_INT :
	      cparam->current.custom_int = atoi(choice + 7);
	      break;

	  case PPD_CUSTOM_PASSCODE :
	  case PPD_CUSTOM_PASSWORD :
	  case PPD_CUSTOM_STRING :
	      if (cparam->current.custom_string)
	        _cupsStrFree(cparam->current.custom_string);

	      cparam->current.custom_string = _cupsStrAlloc(choice + 7);
	      break;
	}
      }
    }

   /*
    * Make sure that we keep the option marked below...
    */

    choice = "Custom";
  }
  else if (choice[0] == '{')
  {
   /*
    * Handle multi-value custom options...
    */

    ppd_coption_t	*coption;	/* Custom option */
    ppd_cparam_t	*cparam;	/* Custom parameter */
    char		*units;		/* Custom points units */
    int			num_vals;	/* Number of values */
    cups_option_t	*vals,		/* Values */
			*val;		/* Value */


    if ((c = ppdFindChoice(o, "Custom")) == NULL)
      return;

    if ((coption = ppdFindCustomOption(ppd, option)) != NULL)
    {
      num_vals = cupsParseOptions(choice, 0, &vals);

      for (i = 0, val = vals; i < num_vals; i ++, val ++)
      {
        if ((cparam = ppdFindCustomParam(coption, val->name)) == NULL)
	  continue;

	switch (cparam->type)
	{
	  case PPD_CUSTOM_CURVE :
	  case PPD_CUSTOM_INVCURVE :
	  case PPD_CUSTOM_REAL :
	      cparam->current.custom_real = (float)_cupsStrScand(val->value,
	                                                         NULL, loc);
	      break;

	  case PPD_CUSTOM_POINTS :
	      cparam->current.custom_points = (float)_cupsStrScand(val->value,
	                                                           &units,
	                                                           loc);

	      if (units)
	      {
        	if (!strcasecmp(units, "cm"))
		  cparam->current.custom_points *= 72.0f / 2.54f;
        	else if (!strcasecmp(units, "mm"))
		  cparam->current.custom_points *= 72.0f / 25.4f;
        	else if (!strcasecmp(units, "m"))
		  cparam->current.custom_points *= 72.0f / 0.0254f;
        	else if (!strcasecmp(units, "in"))
		  cparam->current.custom_points *= 72.0f;
        	else if (!strcasecmp(units, "ft"))
		  cparam->current.custom_points *= 12.0f * 72.0f;
	      }
	      break;

	  case PPD_CUSTOM_INT :
	      cparam->current.custom_int = atoi(val->value);
	      break;

	  case PPD_CUSTOM_PASSCODE :
	  case PPD_CUSTOM_PASSWORD :
	  case PPD_CUSTOM_STRING :
	      if (cparam->current.custom_string)
		_cupsStrFree(cparam->current.custom_string);

	      cparam->current.custom_string = _cupsStrRetain(val->value);
	      break;
	}
      }

      cupsFreeOptions(num_vals, vals);
    }
  }
  else
  {
    for (i = o->num_choices, c = o->choices; i > 0; i --, c ++)
      if (!strcasecmp(c->choice, choice))
        break;

    if (!i)
      return;
  }

 /*
  * Option found; mark it and then handle unmarking any other options.
  */

  if (o->ui != PPD_UI_PICKMANY)
  {
   /*
    * Unmark all other choices...
    */

    if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, c)) != NULL)
    {
      oldc->marked = 0;
      cupsArrayRemove(ppd->marked, oldc);
    }

    if (!strcasecmp(option, "PageSize") || !strcasecmp(option, "PageRegion"))
    {
     /*
      * Mark current page size...
      */

      for (j = 0; j < ppd->num_sizes; j ++)
	ppd->sizes[j].marked = !strcasecmp(ppd->sizes[j].name,
		                           choice);

     /*
      * Unmark the current PageSize or PageRegion setting, as
      * appropriate...
      */

      cupsArraySave(ppd->options);

      if (!strcasecmp(option, "PageSize"))
      {
	if ((o = ppdFindOption(ppd, "PageRegion")) != NULL)
        {
          key.option = o;
          if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
          {
            oldc->marked = 0;
            cupsArrayRemove(ppd->marked, oldc);
          }
        }
      }
      else
      {
	if ((o = ppdFindOption(ppd, "PageSize")) != NULL)
        {
          key.option = o;
          if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
          {
            oldc->marked = 0;
            cupsArrayRemove(ppd->marked, oldc);
          }
        }
      }

      cupsArrayRestore(ppd->options);
    }
    else if (!strcasecmp(option, "InputSlot"))
    {
     /*
      * Unmark ManualFeed option...
      */

      cupsArraySave(ppd->options);

      if ((o = ppdFindOption(ppd, "ManualFeed")) != NULL)
      {
        key.option = o;
        if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
        {
          oldc->marked = 0;
          cupsArrayRemove(ppd->marked, oldc);
        }
      }

      cupsArrayRestore(ppd->options);
    }
    else if (!strcasecmp(option, "ManualFeed") &&
	     !strcasecmp(choice, "True"))
    {
     /*
      * Unmark InputSlot option...
      */

      cupsArraySave(ppd->options);

      if ((o = ppdFindOption(ppd, "InputSlot")) != NULL)
      {
        key.option = o;
        if ((oldc = (ppd_choice_t *)cupsArrayFind(ppd->marked, &key)) != NULL)
        {
          oldc->marked = 0;
          cupsArrayRemove(ppd->marked, oldc);
        }
      }

      cupsArrayRestore(ppd->options);
    }
  }

  c->marked = 1;

  cupsArrayAdd(ppd->marked, c);
}


/*
 * End of "$Id: mark.c 9293 2010-09-02 17:52:24Z mike $".
 */
