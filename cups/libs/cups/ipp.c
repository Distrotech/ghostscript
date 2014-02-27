/*
 * "$Id: ipp.c 9357 2010-11-11 17:40:35Z mike $"
 *
 *   Internet Printing Protocol functions for CUPS.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ippAddBoolean()        - Add a boolean attribute to an IPP message.
 *   ippAddBooleans()       - Add an array of boolean values.
 *   ippAddDate()           - Add a date attribute to an IPP message.
 *   ippAddInteger()        - Add a integer attribute to an IPP message.
 *   ippAddIntegers()       - Add an array of integer values.
 *   ippAddOctetString()    - Add an octetString value to an IPP message.
 *   ippAddString()         - Add a language-encoded string to an IPP message.
 *   ippAddStrings()        - Add language-encoded strings to an IPP message.
 *   ippAddRange()          - Add a range of values to an IPP message.
 *   ippAddRanges()         - Add ranges of values to an IPP message.
 *   ippAddResolution()     - Add a resolution value to an IPP message.
 *   ippAddResolutions()    - Add resolution values to an IPP message.
 *   ippAddSeparator()      - Add a group separator to an IPP message.
 *   ippDateToTime()        - Convert from RFC 1903 Date/Time format to
 *                            UNIX time in seconds.
 *   ippDelete()            - Delete an IPP message.
 *   ippDeleteAttribute()   - Delete a single attribute in an IPP message.
 *   ippFindAttribute()     - Find a named attribute in a request...
 *   ippFindNextAttribute() - Find the next named attribute in a request...
 *   ippLength()            - Compute the length of an IPP message.
 *   ippNew()               - Allocate a new IPP message.
 *   ippNewRequest()        - Allocate a new IPP message.
 *   ippRead()              - Read data for an IPP message from a HTTP
 *                            connection.
 *   ippReadFile()          - Read data for an IPP message from a file.
 *   ippReadIO()            - Read data for an IPP message.
 *   ippTimeToDate()        - Convert from UNIX time to RFC 1903 format.
 *   ippWrite()             - Write data for an IPP message to a HTTP
 *                            connection.
 *   ippWriteFile()         - Write data for an IPP message to a file.
 *   ippWriteIO()           - Write data for an IPP message.
 *   _ippAddAttr()          - Add a new attribute to the request.
 *   _ippFreeAttr()         - Free an attribute.
 *   ipp_length()           - Compute the length of an IPP message or
 *                            collection value.
 *   ipp_read_http()        - Semi-blocking read on a HTTP connection...
 *   ipp_read_file()        - Read IPP data from a file.
 *   ipp_write_file()       - Write IPP data to a file.
 */

/*
 * Include necessary headers...
 */

#include "http-private.h"
#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <errno.h>
#ifdef WIN32
#  include <io.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

static unsigned char	*ipp_buffer_get(void);
static void		ipp_buffer_release(unsigned char *b);
static size_t		ipp_length(ipp_t *ipp, int collection);
static ssize_t		ipp_read_http(http_t *http, ipp_uchar_t *buffer,
			              size_t length);
static ssize_t		ipp_read_file(int *fd, ipp_uchar_t *buffer,
			              size_t length);
static ssize_t		ipp_write_file(int *fd, ipp_uchar_t *buffer,
			               size_t length);


/*
 * 'ippAddBoolean()' - Add a boolean attribute to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddBoolean(ipp_t      *ipp,		/* I - IPP message */
              ipp_tag_t  group,		/* I - IPP group */
              const char *name,		/* I - Name of attribute */
              char       value)		/* I - Value of attribute */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddBoolean(ipp=%p, group=%02x(%s), name=\"%s\", value=%d)",
                ipp, group, ippTagString(group), name, value));

  if (!ipp || !name)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 1)) == NULL)
    return (NULL);

  attr->name              = _cupsStrAlloc(name);
  attr->group_tag         = group;
  attr->value_tag         = IPP_TAG_BOOLEAN;
  attr->values[0].boolean = value;

  return (attr);
}


/*
 * 'ippAddBooleans()' - Add an array of boolean values.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddBooleans(ipp_t      *ipp,		/* I - IPP message */
               ipp_tag_t  group,	/* I - IPP group */
	       const char *name,	/* I - Name of attribute */
	       int        num_values,	/* I - Number of values */
	       const char *values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddBooleans(ipp=%p, group=%02x(%s), name=\"%s\", "
                "num_values=%d, values=%p)", ipp, group, ippTagString(group),
                name, num_values, values));

  if (!ipp || !name || num_values < 1)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = _cupsStrAlloc(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_BOOLEAN;

  if (values != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
      value->boolean = values[i];

  return (attr);
}


/*
 * 'ippAddCollection()' - Add a collection value.
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

ipp_attribute_t *			/* O - New attribute */
ippAddCollection(ipp_t      *ipp,	/* I - IPP message */
                 ipp_tag_t  group,	/* I - IPP group */
		 const char *name,	/* I - Name of attribute */
		 ipp_t      *value)	/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddCollection(ipp=%p, group=%02x(%s), name=\"%s\", "
                "value=%p)", ipp, group, ippTagString(group), name, value));

  if (!ipp || !name)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                 = _cupsStrAlloc(name);
  attr->group_tag            = group;
  attr->value_tag            = IPP_TAG_BEGIN_COLLECTION;
  attr->values[0].collection = value;

  value->use ++;

  return (attr);
}


/*
 * 'ippAddCollections()' - Add an array of collection values.
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

ipp_attribute_t *			/* O - New attribute */
ippAddCollections(
    ipp_t       *ipp,			/* I - IPP message */
    ipp_tag_t   group,			/* I - IPP group */
    const char  *name,			/* I - Name of attribute */
    int         num_values,		/* I - Number of values */
    const ipp_t **values)		/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddCollections(ipp=%p, group=%02x(%s), name=\"%s\", "
                "num_values=%d, values=%p)", ipp, group, ippTagString(group),
                name, num_values, values));

  if (!ipp || !name || num_values < 1)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = _cupsStrAlloc(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_BEGIN_COLLECTION;

  if (values != NULL)
  {
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
    {
      value->collection = (ipp_t *)values[i];
      value->collection->use ++;
    }
  }

  return (attr);
}


/*
 * 'ippAddDate()' - Add a date attribute to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddDate(ipp_t             *ipp,	/* I - IPP message */
           ipp_tag_t         group,	/* I - IPP group */
	   const char        *name,	/* I - Name of attribute */
	   const ipp_uchar_t *value)	/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddDate(ipp=%p, group=%02x(%s), name=\"%s\", value=%p)",
                ipp, group, ippTagString(group), name, value));

  if (!ipp || !name || !value)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 1)) == NULL)
    return (NULL);

  attr->name      = _cupsStrAlloc(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_DATE;
  memcpy(attr->values[0].date, value, 11);

  return (attr);
}


/*
 * 'ippAddInteger()' - Add a integer attribute to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddInteger(ipp_t      *ipp,		/* I - IPP message */
              ipp_tag_t  group,		/* I - IPP group */
	      ipp_tag_t  type,		/* I - Type of attribute */
              const char *name,		/* I - Name of attribute */
              int        value)		/* I - Value of attribute */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddInteger(ipp=%p, group=%02x(%s), type=%02x(%s), "
                "name=\"%s\", value=%d)", ipp, group, ippTagString(group),
		type, ippTagString(type), name, value));

  if (!ipp || !name)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 1)) == NULL)
    return (NULL);

  attr->name              = _cupsStrAlloc(name);
  attr->group_tag         = group;
  attr->value_tag         = type;
  attr->values[0].integer = value;

  return (attr);
}


/*
 * 'ippAddIntegers()' - Add an array of integer values.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddIntegers(ipp_t      *ipp,		/* I - IPP message */
               ipp_tag_t  group,	/* I - IPP group */
	       ipp_tag_t  type,		/* I - Type of attribute */
	       const char *name,	/* I - Name of attribute */
	       int        num_values,	/* I - Number of values */
	       const int  *values)	/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddIntegers(ipp=%p, group=%02x(%s), type=%02x(%s), "
                "name=\"%s\", num_values=%d, values=%p)", ipp,
		group, ippTagString(group), type, ippTagString(type), name,
		num_values, values));

  if (!ipp || !name || num_values < 1)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = _cupsStrAlloc(name);
  attr->group_tag = group;
  attr->value_tag = type;

  if (values != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
      value->integer = values[i];

  return (attr);
}


/*
 * 'ippAddOctetString()' - Add an octetString value to an IPP message.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ipp_attribute_t	*			/* O - New attribute */
ippAddOctetString(ipp_t      *ipp,	/* I - IPP message */
                  ipp_tag_t  group,	/* I - IPP group */
                  const char *name,	/* I - Name of attribute */
                  const void *data,	/* I - octetString data */
		  int        datalen)	/* I - Length of data in bytes */
{
  ipp_attribute_t	*attr;		/* New attribute */


  if (ipp == NULL || name == NULL)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 1)) == NULL)
    return (NULL);

 /*
  * Initialize the attribute data...
  */

  attr->name                     = _cupsStrAlloc(name);
  attr->group_tag                = group;
  attr->value_tag                = IPP_TAG_STRING;
  attr->values[0].unknown.length = datalen;

  if (data)
  {
    if ((attr->values[0].unknown.data = malloc(datalen)) == NULL)
    {
      ippDeleteAttribute(ipp, attr);
      return (NULL);
    }

    memcpy(attr->values[0].unknown.data, data, datalen);
  }

 /*
  * Return the new attribute...
  */

  return (attr);
}


/*
 * 'ippAddString()' - Add a language-encoded string to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddString(ipp_t      *ipp,		/* I - IPP message */
             ipp_tag_t  group,		/* I - IPP group */
	     ipp_tag_t  type,		/* I - Type of attribute */
             const char *name,		/* I - Name of attribute */
             const char *charset,	/* I - Character set */
             const char *value)		/* I - Value */
{
  ipp_attribute_t	*attr;		/* New attribute */
  char			buffer[1024],	/* Language/charset value buffer */
			*bufptr;	/* Pointer into buffer */


  DEBUG_printf(("ippAddString(ipp=%p, group=%02x(%s), type=%02x(%s), "
                "name=\"%s\", charset=\"%s\", value=\"%s\")", ipp,
		group, ippTagString(group), type, ippTagString(type), name,
		charset, value));

  if (!ipp || !name)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 1)) == NULL)
    return (NULL);

 /*
  * Force value to be English for the POSIX locale...
  */

  if (type == IPP_TAG_LANGUAGE && !strcasecmp(value, "C"))
    value = "en";

 /*
  * Convert language and charset values to lowercase and change _ to - as
  * needed...
  */

  if ((type == IPP_TAG_LANGUAGE || type == IPP_TAG_CHARSET) && value)
  {
    strlcpy(buffer, value, sizeof(buffer));
    value = buffer;

    for (bufptr = buffer; *bufptr; bufptr ++)
      if (*bufptr == '_')
        *bufptr = '-';
      else
        *bufptr = tolower(*bufptr & 255);
  }

 /*
  * Initialize the attribute data...
  */

  attr->name                     = _cupsStrAlloc(name);
  attr->group_tag                = group;
  attr->value_tag                = type;
  attr->values[0].string.charset = ((int)type & IPP_TAG_COPY) ? (char *)charset :
                                   charset ? _cupsStrAlloc(charset) : NULL;
  attr->values[0].string.text    = ((int)type & IPP_TAG_COPY) ? (char *)value :
                                   value ? _cupsStrAlloc(value) : NULL;

  return (attr);
}


/*
 * 'ippAddStrings()' - Add language-encoded strings to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddStrings(
    ipp_t              *ipp,		/* I - IPP message */
    ipp_tag_t          group,		/* I - IPP group */
    ipp_tag_t          type,		/* I - Type of attribute */
    const char         *name,		/* I - Name of attribute */
    int                num_values,	/* I - Number of values */
    const char         *charset,	/* I - Character set */
    const char * const *values)		/* I - Values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  ipp_value_t		*value;		/* Current value */
  char			buffer[1024],	/* Language/charset value buffer */
			*bufptr;	/* Pointer into buffer */


  DEBUG_printf(("ippAddStrings(ipp=%p, group=%02x(%s), type=%02x(%s), "
                "name=\"%s\", num_values=%d, charset=\"%s\", values=%p)", ipp,
		group, ippTagString(group), type, ippTagString(type), name,
		num_values, charset, values));

  if (!ipp || !name || num_values < 1)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, num_values)) == NULL)
    return (NULL);

 /*
  * Initialize the attribute data...
  */

  attr->name      = _cupsStrAlloc(name);
  attr->group_tag = group;
  attr->value_tag = type;

  for (i = 0, value = attr->values;
       i < num_values;
       i ++, value ++)
  {
    if (i == 0)
      value->string.charset = ((int)type & IPP_TAG_COPY) ? (char *)charset :
                                   charset ? _cupsStrAlloc(charset) : NULL;
    else
      value->string.charset = attr->values[0].string.charset;

    if (values != NULL)
    {
      if ((int)type & IPP_TAG_COPY)
        value->string.text = (char *)values[i];
      else if (type == IPP_TAG_LANGUAGE && !strcasecmp(values[i], "C"))
      {
       /*
	* Force language to be English for the POSIX locale...
	*/

	value->string.text = ((int)type & IPP_TAG_COPY) ? "en" :
                                      _cupsStrAlloc("en");
      }
      else if (type == IPP_TAG_LANGUAGE || type == IPP_TAG_CHARSET)
      {
       /*
	* Convert language values to lowercase and change _ to - as needed...
	*/

	strlcpy(buffer, values[i], sizeof(buffer));

	for (bufptr = buffer; *bufptr; bufptr ++)
	  if (*bufptr == '_')
	    *bufptr = '-';
	  else
	    *bufptr = tolower(*bufptr & 255);

	value->string.text = _cupsStrAlloc(buffer);
      }
      else
	value->string.text = _cupsStrAlloc(values[i]);

    }
  }

  return (attr);
}


/*
 * 'ippAddRange()' - Add a range of values to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddRange(ipp_t      *ipp,		/* I - IPP message */
            ipp_tag_t  group,		/* I - IPP group */
	    const char *name,		/* I - Name of attribute */
	    int        lower,		/* I - Lower value */
	    int        upper)		/* I - Upper value */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddRange(ipp=%p, group=%02x(%s), name=\"%s\", lower=%d, "
                "upper=%d)", ipp, group, ippTagString(group), name, lower,
		upper));

  if (!ipp || !name)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                  = _cupsStrAlloc(name);
  attr->group_tag             = group;
  attr->value_tag             = IPP_TAG_RANGE;
  attr->values[0].range.lower = lower;
  attr->values[0].range.upper = upper;

  return (attr);
}


/*
 * 'ippAddRanges()' - Add ranges of values to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddRanges(ipp_t      *ipp,		/* I - IPP message */
             ipp_tag_t  group,		/* I - IPP group */
	     const char *name,		/* I - Name of attribute */
	     int        num_values,	/* I - Number of values */
	     const int  *lower,		/* I - Lower values */
	     const int  *upper)		/* I - Upper values */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddRanges(ipp=%p, group=%02x(%s), name=\"%s\", "
                "num_values=%d, lower=%p, upper=%p)", ipp, group,
		ippTagString(group), name, num_values, lower, upper));

  if (!ipp || !name || num_values < 1)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = _cupsStrAlloc(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_RANGE;

  if (lower != NULL && upper != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
    {
      value->range.lower = lower[i];
      value->range.upper = upper[i];
    }

  return (attr);
}


/*
 * 'ippAddResolution()' - Add a resolution value to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddResolution(ipp_t      *ipp,	/* I - IPP message */
        	 ipp_tag_t  group,	/* I - IPP group */
		 const char *name,	/* I - Name of attribute */
		 ipp_res_t  units,	/* I - Units for resolution */
		 int        xres,	/* I - X resolution */
		 int        yres)	/* I - Y resolution */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddResolution(ipp=%p, group=%02x(%s), name=\"%s\", "
                "units=%d, xres=%d, yres=%d)", ipp, group,
		ippTagString(group), name, units, xres, yres));

  if (!ipp || !name)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 1)) == NULL)
    return (NULL);

  attr->name                       = _cupsStrAlloc(name);
  attr->group_tag                  = group;
  attr->value_tag                  = IPP_TAG_RESOLUTION;
  attr->values[0].resolution.xres  = xres;
  attr->values[0].resolution.yres  = yres;
  attr->values[0].resolution.units = units;

  return (attr);
}


/*
 * 'ippAddResolutions()' - Add resolution values to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddResolutions(ipp_t      *ipp,	/* I - IPP message */
        	  ipp_tag_t  group,	/* I - IPP group */
		  const char *name,	/* I - Name of attribute */
		  int        num_values,/* I - Number of values */
		  ipp_res_t  units,	/* I - Units for resolution */
		  const int  *xres,	/* I - X resolutions */
		  const int  *yres)	/* I - Y resolutions */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* New attribute */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippAddResolutions(ipp=%p, group=%02x(%s), name=\"%s\", "
                "num_value=%d, units=%d, xres=%p, yres=%p)", ipp, group,
		ippTagString(group), name, num_values, units, xres, yres));

  if (!ipp || !name || num_values < 1)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, num_values)) == NULL)
    return (NULL);

  attr->name      = _cupsStrAlloc(name);
  attr->group_tag = group;
  attr->value_tag = IPP_TAG_RESOLUTION;

  if (xres != NULL && yres != NULL)
    for (i = 0, value = attr->values;
	 i < num_values;
	 i ++, value ++)
    {
      value->resolution.xres  = xres[i];
      value->resolution.yres  = yres[i];
      value->resolution.units = units;
    }

  return (attr);
}


/*
 * 'ippAddSeparator()' - Add a group separator to an IPP message.
 */

ipp_attribute_t *			/* O - New attribute */
ippAddSeparator(ipp_t *ipp)		/* I - IPP message */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("ippAddSeparator(ipp=%p)", ipp));

  if (!ipp)
    return (NULL);

  if ((attr = _ippAddAttr(ipp, 0)) == NULL)
    return (NULL);

  attr->group_tag = IPP_TAG_ZERO;
  attr->value_tag = IPP_TAG_ZERO;

  return (attr);
}


/*
 * 'ippDateToTime()' - Convert from RFC 1903 Date/Time format to UNIX time
 *                     in seconds.
 */

time_t					/* O - UNIX time value */
ippDateToTime(const ipp_uchar_t *date)	/* I - RFC 1903 date info */
{
  struct tm	unixdate;		/* UNIX date/time info */
  time_t	t;			/* Computed time */


  if (!date)
    return (0);

  memset(&unixdate, 0, sizeof(unixdate));

 /*
  * RFC-1903 date/time format is:
  *
  *    Byte(s)  Description
  *    -------  -----------
  *    0-1      Year (0 to 65535)
  *    2        Month (1 to 12)
  *    3        Day (1 to 31)
  *    4        Hours (0 to 23)
  *    5        Minutes (0 to 59)
  *    6        Seconds (0 to 60, 60 = "leap second")
  *    7        Deciseconds (0 to 9)
  *    8        +/- UTC
  *    9        UTC hours (0 to 11)
  *    10       UTC minutes (0 to 59)
  */

  unixdate.tm_year = ((date[0] << 8) | date[1]) - 1900;
  unixdate.tm_mon  = date[2] - 1;
  unixdate.tm_mday = date[3];
  unixdate.tm_hour = date[4];
  unixdate.tm_min  = date[5];
  unixdate.tm_sec  = date[6];

  t = mktime(&unixdate);

  if (date[8] == '-')
    t += date[9] * 3600 + date[10] * 60;
  else
    t -= date[9] * 3600 + date[10] * 60;

  return (t);
}


/*
 * 'ippDelete()' - Delete an IPP message.
 */

void
ippDelete(ipp_t *ipp)			/* I - IPP message */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*next;		/* Next attribute */


  DEBUG_printf(("ippDelete(ipp=%p)", ipp));

  if (!ipp)
    return;

  ipp->use --;
  if (ipp->use > 0)
    return;

  for (attr = ipp->attrs; attr != NULL; attr = next)
  {
    next = attr->next;
    _ippFreeAttr(attr);
  }

  free(ipp);
}


/*
 * 'ippDeleteAttribute()' - Delete a single attribute in an IPP message.
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

void
ippDeleteAttribute(
    ipp_t           *ipp,		/* I - IPP message */
    ipp_attribute_t *attr)		/* I - Attribute to delete */
{
  ipp_attribute_t	*current,	/* Current attribute */
			*prev;		/* Previous attribute */


  DEBUG_printf(("ippDeleteAttribute(ipp=%p, attr=%p(%s))", ipp, attr,
                attr ? attr->name : "(null)"));

 /*
  * Find the attribute in the list...
  */

  for (current = ipp->attrs, prev = NULL;
       current != NULL && current != attr;
       prev = current, current = current->next);

  if (current)
  {
   /*
    * Found it, remove the attribute from the list...
    */

    if (prev)
      prev->next = current->next;
    else
      ipp->attrs = current->next;

    if (current == ipp->last)
      ipp->last = prev;

   /*
    * Free memory used by the attribute...
    */

    _ippFreeAttr(current);
  }
}


/*
 * 'ippFindAttribute()' - Find a named attribute in a request...
 */

ipp_attribute_t	*			/* O - Matching attribute */
ippFindAttribute(ipp_t      *ipp,	/* I - IPP message */
                 const char *name,	/* I - Name of attribute */
		 ipp_tag_t  type)	/* I - Type of attribute */
{
  DEBUG_printf(("2ippFindAttribute(ipp=%p, name=\"%s\", type=%02x(%s))", ipp,
                name, type, ippTagString(type)));

  if (!ipp || !name)
    return (NULL);

 /*
  * Reset the current pointer...
  */

  ipp->current = NULL;

 /*
  * Search for the attribute...
  */

  return (ippFindNextAttribute(ipp, name, type));
}


/*
 * 'ippFindNextAttribute()' - Find the next named attribute in a request...
 */

ipp_attribute_t	*			/* O - Matching attribute */
ippFindNextAttribute(ipp_t      *ipp,	/* I - IPP message */
                     const char *name,	/* I - Name of attribute */
		     ipp_tag_t  type)	/* I - Type of attribute */
{
  ipp_attribute_t	*attr;		/* Current atttribute */
  ipp_tag_t		value_tag;	/* Value tag */


  DEBUG_printf(("2ippFindNextAttribute(ipp=%p, name=\"%s\", type=%02x(%s))",
                ipp, name, type, ippTagString(type)));

  if (!ipp || !name)
    return (NULL);

  if (ipp->current)
  {
    ipp->prev = ipp->current;
    attr      = ipp->current->next;
  }
  else
  {
    ipp->prev = NULL;
    attr      = ipp->attrs;
  }

  for (; attr != NULL; ipp->prev = attr, attr = attr->next)
  {
    DEBUG_printf(("4ippFindAttribute: attr=%p, name=\"%s\"", attr,
                  attr->name));

    value_tag = (ipp_tag_t)(attr->value_tag & IPP_TAG_MASK);

    if (attr->name != NULL && strcasecmp(attr->name, name) == 0 &&
        (value_tag == type || type == IPP_TAG_ZERO ||
	 (value_tag == IPP_TAG_TEXTLANG && type == IPP_TAG_TEXT) ||
	 (value_tag == IPP_TAG_NAMELANG && type == IPP_TAG_NAME)))
    {
      ipp->current = attr;

      return (attr);
    }
  }

  ipp->current = NULL;
  ipp->prev    = NULL;

  return (NULL);
}


/*
 * 'ippLength()' - Compute the length of an IPP message.
 */

size_t					/* O - Size of IPP message */
ippLength(ipp_t *ipp)			/* I - IPP message */
{
  return (ipp_length(ipp, 0));
}


/*
 * 'ippNew()' - Allocate a new IPP message.
 */

ipp_t *					/* O - New IPP message */
ippNew(void)
{
  ipp_t	*temp;				/* New IPP message */


  DEBUG_puts("ippNew()");

  if ((temp = (ipp_t *)calloc(1, sizeof(ipp_t))) != NULL)
  {
   /*
    * Default to IPP 1.1...
    */

    temp->request.any.version[0] = 1;
    temp->request.any.version[1] = 1;
    temp->use                    = 1;
  }

  DEBUG_printf(("1ippNew: Returning %p", temp));

  return (temp);
}


/*
 *  'ippNewRequest()' - Allocate a new IPP request message.
 *
 * The new request message is initialized with the attributes-charset and
 * attributes-natural-language attributes added. The
 * attributes-natural-language value is derived from the current locale.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ipp_t *					/* O - IPP request message */
ippNewRequest(ipp_op_t op)		/* I - Operation code */
{
  ipp_t		*request;		/* IPP request message */
  cups_lang_t	*language;		/* Current language localization */


  DEBUG_printf(("ippNewRequest(op=%02x(%s))", op, ippOpString(op)));

 /*
  * Create a new IPP message...
  */

  if ((request = ippNew()) == NULL)
    return (NULL);

 /*
  * Set the operation and request ID...
  */

  request->request.op.operation_id = op;
  request->request.op.request_id   = 1;

 /*
  * Use UTF-8 as the character set...
  */

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, "utf-8");

 /*
  * Get the language from the current locale...
  */

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Return the new request...
  */

  return (request);
}


/*
 * 'ippRead()' - Read data for an IPP message from a HTTP connection.
 */

ipp_state_t				/* O - Current state */
ippRead(http_t *http,			/* I - HTTP connection */
        ipp_t  *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippRead(http=%p, ipp=%p), data_remaining=" CUPS_LLFMT,
                http, ipp, CUPS_LLCAST (http ? http->data_remaining : -1)));

  if (!http)
    return (IPP_ERROR);

  DEBUG_printf(("2ippRead: http->state=%d, http->used=%d", http->state,
                http->used));

  return (ippReadIO(http, (ipp_iocb_t)ipp_read_http, http->blocking, NULL,
                    ipp));
}


/*
 * 'ippReadFile()' - Read data for an IPP message from a file.
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

ipp_state_t				/* O - Current state */
ippReadFile(int   fd,			/* I - HTTP data */
            ipp_t *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippReadFile(fd=%d, ipp=%p)", fd, ipp));

  return (ippReadIO(&fd, (ipp_iocb_t)ipp_read_file, 1, NULL, ipp));
}


/*
 * 'ippReadIO()' - Read data for an IPP message.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ipp_state_t				/* O - Current state */
ippReadIO(void       *src,		/* I - Data source */
          ipp_iocb_t cb,		/* I - Read callback function */
	  int        blocking,		/* I - Use blocking IO? */
	  ipp_t      *parent,		/* I - Parent request, if any */
          ipp_t      *ipp)		/* I - IPP data */
{
  int			n;		/* Length of data */
  unsigned char		*buffer,	/* Data buffer */
			string[IPP_MAX_NAME],
					/* Small string buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		tag;		/* Current tag */
  ipp_tag_t		value_tag;	/* Current value tag */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippReadIO(src=%p, cb=%p, blocking=%d, parent=%p, ipp=%p)",
                src, cb, blocking, parent, ipp));
  DEBUG_printf(("2ippReadIO: ipp->state=%d", ipp->state));

  if (!src || !ipp)
    return (IPP_ERROR);

  if ((buffer = ipp_buffer_get()) == NULL)
  {
    DEBUG_puts("1ippReadIO: Unable to get read buffer!");
    return (IPP_ERROR);
  }

  switch (ipp->state)
  {
    case IPP_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_HEADER :
        if (parent == NULL)
	{
	 /*
          * Get the request header...
	  */

          if ((*cb)(src, buffer, 8) < 8)
	  {
	    DEBUG_puts("1ippReadIO: Unable to read header!");
	    ipp_buffer_release(buffer);
	    return (IPP_ERROR);
	  }

	 /*
          * Then copy the request header over...
	  */

          ipp->request.any.version[0]  = buffer[0];
          ipp->request.any.version[1]  = buffer[1];
          ipp->request.any.op_status   = (buffer[2] << 8) | buffer[3];
          ipp->request.any.request_id  = (((((buffer[4] << 8) | buffer[5]) << 8) |
	                        	 buffer[6]) << 8) | buffer[7];

          DEBUG_printf(("2ippReadIO: version=%d.%d", buffer[0], buffer[1]));
	  DEBUG_printf(("2ippReadIO: op_status=%04x",
	                ipp->request.any.op_status));
	  DEBUG_printf(("2ippReadIO: request_id=%d",
	                ipp->request.any.request_id));
        }

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = NULL;
	ipp->curtag  = IPP_TAG_ZERO;
	ipp->prev    = ipp->last;

       /*
        * If blocking is disabled, stop here...
	*/

        if (!blocking)
	  break;

    case IPP_ATTRIBUTE :
        for (;;)
	{
	  if ((*cb)(src, buffer, 1) < 1)
	  {
	    DEBUG_puts("1ippReadIO: Callback returned EOF/error");
	    ipp_buffer_release(buffer);
	    return (IPP_ERROR);
	  }

	  DEBUG_printf(("2ippReadIO: ipp->current=%p, ipp->prev=%p",
	                ipp->current, ipp->prev));

	 /*
	  * Read this attribute...
	  */

          tag = (ipp_tag_t)buffer[0];

	  if (tag == IPP_TAG_END)
	  {
	   /*
	    * No more attributes left...
	    */

            DEBUG_puts("2ippReadIO: IPP_TAG_END!");

	    ipp->state = IPP_DATA;
	    break;
	  }
          else if (tag < IPP_TAG_UNSUPPORTED_VALUE)
	  {
	   /*
	    * Group tag...  Set the current group and continue...
	    */

            if (ipp->curtag == tag)
	      ipp->prev = ippAddSeparator(ipp);
            else if (ipp->current)
	      ipp->prev = ipp->current;

	    ipp->curtag  = tag;
	    ipp->current = NULL;
	    DEBUG_printf(("2ippReadIO: group tag=%x(%s), ipp->prev=%p", tag,
	                  ippTagString(tag), ipp->prev));
	    continue;
	  }

          DEBUG_printf(("2ippReadIO: value tag=%x(%s)", tag,
	                ippTagString(tag)));

         /*
	  * Get the name...
	  */

          if ((*cb)(src, buffer, 2) < 2)
	  {
	    DEBUG_puts("1ippReadIO: unable to read name length!");
	    ipp_buffer_release(buffer);
	    return (IPP_ERROR);
	  }

          n = (buffer[0] << 8) | buffer[1];

          if (n >= IPP_BUF_SIZE)
	  {
	    DEBUG_printf(("1ippReadIO: bad name length %d!", n));
	    ipp_buffer_release(buffer);
	    return (IPP_ERROR);
	  }

          DEBUG_printf(("2ippReadIO: name length=%d", n));

          if (n == 0 && tag != IPP_TAG_MEMBERNAME &&
	      tag != IPP_TAG_END_COLLECTION)
	  {
	   /*
	    * More values for current attribute...
	    */

            if (ipp->current == NULL)
	    {
	      DEBUG_puts("1ippReadIO: Attribute without name and no current");
	      ipp_buffer_release(buffer);
	      return (IPP_ERROR);
	    }

            attr      = ipp->current;
	    value_tag = (ipp_tag_t)(attr->value_tag & IPP_TAG_MASK);

	   /*
	    * Make sure we aren't adding a new value of a different
	    * type...
	    */

	    if (value_tag == IPP_TAG_ZERO)
	    {
	     /*
	      * Setting the value of a collection member...
	      */

	      attr->value_tag = tag;
	    }
	    else if (value_tag == IPP_TAG_TEXTLANG ||
	             value_tag == IPP_TAG_NAMELANG ||
		     (value_tag >= IPP_TAG_TEXT &&
		      value_tag <= IPP_TAG_MIMETYPE))
            {
	     /*
	      * String values can sometimes come across in different
	      * forms; accept sets of differing values...
	      */

	      if (tag != IPP_TAG_TEXTLANG && tag != IPP_TAG_NAMELANG &&
	          (tag < IPP_TAG_TEXT || tag > IPP_TAG_MIMETYPE) &&
		  tag != IPP_TAG_NOVALUE)
	      {
		DEBUG_printf(("1ippReadIO: 1setOf value tag %x(%s) != %x(%s)",
			      value_tag, ippTagString(value_tag), tag,
			      ippTagString(tag)));
		ipp_buffer_release(buffer);
	        return (IPP_ERROR);
	      }
            }
	    else if (value_tag != tag)
	    {
	      DEBUG_printf(("1ippReadIO: value tag %x(%s) != %x(%s)",
	                    value_tag, ippTagString(value_tag), tag,
			    ippTagString(tag)));
	      ipp_buffer_release(buffer);
	      return (IPP_ERROR);
            }

           /*
	    * Finally, reallocate the attribute array as needed...
	    */

	    if (attr->num_values == 1 ||
	        (attr->num_values > 0 &&
	         (attr->num_values & (IPP_MAX_VALUES - 1)) == 0))
	    {
	      ipp_attribute_t	*temp;	/* Pointer to new buffer */


              DEBUG_printf(("2ippReadIO: reallocating for up to %d values...",
	                    attr->num_values + IPP_MAX_VALUES));

             /*
	      * Reallocate memory...
	      */

              if ((temp = realloc(attr, sizeof(ipp_attribute_t) +
	                                (attr->num_values + IPP_MAX_VALUES - 1) *
					sizeof(ipp_value_t))) == NULL)
	      {
	        DEBUG_puts("1ippReadIO: Unable to resize attribute");
		ipp_buffer_release(buffer);
	        return (IPP_ERROR);
	      }

              if (temp != attr)
	      {
               /*
		* Reset pointers in the list...
		*/

        	if (ipp->prev)
	          ipp->prev->next = temp;
		else
	          ipp->attrs = temp;

        	attr = ipp->current = ipp->last = temp;
	      }
	    }
	  }
	  else if (tag == IPP_TAG_MEMBERNAME)
	  {
	   /*
	    * Name must be length 0!
	    */

	    if (n)
	    {
	      DEBUG_puts("1ippReadIO: member name not empty!");
	      ipp_buffer_release(buffer);
	      return (IPP_ERROR);
	    }

            if (ipp->current)
	      ipp->prev = ipp->current;

	    attr = ipp->current = _ippAddAttr(ipp, 1);

	    DEBUG_printf(("2ippReadIO: membername, ipp->current=%p, "
	                  "ipp->prev=%p", ipp->current, ipp->prev));

	    attr->group_tag  = ipp->curtag;
	    attr->value_tag  = IPP_TAG_ZERO;
	    attr->num_values = 0;
	  }
	  else if (tag != IPP_TAG_END_COLLECTION)
	  {
	   /*
	    * New attribute; read the name and add it...
	    */

	    if ((*cb)(src, buffer, n) < n)
	    {
	      DEBUG_puts("1ippReadIO: unable to read name!");
	      ipp_buffer_release(buffer);
	      return (IPP_ERROR);
	    }

	    buffer[n] = '\0';

            if (ipp->current)
	      ipp->prev = ipp->current;

	    if ((attr = ipp->current = _ippAddAttr(ipp, 1)) == NULL)
	    {
	      DEBUG_puts("1ippReadIO: unable to allocate attribute!");
	      ipp_buffer_release(buffer);
	      return (IPP_ERROR);
	    }

	    DEBUG_printf(("2ippReadIO: name=\"%s\", ipp->current=%p, "
	                  "ipp->prev=%p", buffer, ipp->current, ipp->prev));

	    attr->group_tag  = ipp->curtag;
	    attr->value_tag  = tag;
	    attr->name       = _cupsStrAlloc((char *)buffer);
	    attr->num_values = 0;
	  }
	  else
	    attr = NULL;

          if (tag != IPP_TAG_END_COLLECTION)
            value = attr->values + attr->num_values;
	  else
	    value = NULL;

	  if ((*cb)(src, buffer, 2) < 2)
	  {
	    DEBUG_puts("1ippReadIO: unable to read value length!");
	    ipp_buffer_release(buffer);
	    return (IPP_ERROR);
	  }

	  n = (buffer[0] << 8) | buffer[1];
          DEBUG_printf(("2ippReadIO: value length=%d", n));

	  switch (tag)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
		if (n != 4)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

	        if ((*cb)(src, buffer, 4) < 4)
		{
	          DEBUG_puts("1ippReadIO: Unable to read integer value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

		n = (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];

                value->integer = n;
	        break;

	    case IPP_TAG_BOOLEAN :
		if (n != 1)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

	        if ((*cb)(src, buffer, 1) < 1)
		{
	          DEBUG_puts("1ippReadIO: Unable to read boolean value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

                value->boolean = buffer[0];
	        break;

            case IPP_TAG_NOVALUE :
	    case IPP_TAG_NOTSETTABLE :
	    case IPP_TAG_DELETEATTR :
	    case IPP_TAG_ADMINDEFINE :
	       /*
	        * These value types are not supposed to have values, however
		* some vendors (Brother) do not implement IPP correctly and so
		* we need to map non-empty values to text...
		*/

	        if (attr->value_tag == tag)
		{
		  if (n == 0)
		    break;

		  attr->value_tag = IPP_TAG_TEXT;
		}

	    case IPP_TAG_TEXT :
	    case IPP_TAG_NAME :
	    case IPP_TAG_KEYWORD :
	    case IPP_TAG_URI :
	    case IPP_TAG_URISCHEME :
	    case IPP_TAG_CHARSET :
	    case IPP_TAG_LANGUAGE :
	    case IPP_TAG_MIMETYPE :
		if (n >= IPP_BUF_SIZE)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

		if ((*cb)(src, buffer, n) < n)
		{
		  DEBUG_puts("1ippReadIO: unable to read name!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

		buffer[n] = '\0';
		value->string.text = _cupsStrAlloc((char *)buffer);
		DEBUG_printf(("2ippReadIO: value=\"%s\"", value->string.text));
	        break;

	    case IPP_TAG_DATE :
		if (n != 11)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

	        if ((*cb)(src, value->date, 11) < 11)
		{
	          DEBUG_puts("1ippReadIO: Unable to read date value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}
	        break;

	    case IPP_TAG_RESOLUTION :
		if (n != 9)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

	        if ((*cb)(src, buffer, 9) < 9)
		{
	          DEBUG_puts("1ippReadIO: Unable to read resolution value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

                value->resolution.xres =
		    (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];
                value->resolution.yres =
		    (((((buffer[4] << 8) | buffer[5]) << 8) | buffer[6]) << 8) |
		    buffer[7];
                value->resolution.units =
		    (ipp_res_t)buffer[8];
	        break;

	    case IPP_TAG_RANGE :
		if (n != 8)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

	        if ((*cb)(src, buffer, 8) < 8)
		{
	          DEBUG_puts("1ippReadIO: Unable to read range value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

                value->range.lower =
		    (((((buffer[0] << 8) | buffer[1]) << 8) | buffer[2]) << 8) |
		    buffer[3];
                value->range.upper =
		    (((((buffer[4] << 8) | buffer[5]) << 8) | buffer[6]) << 8) |
		    buffer[7];
	        break;

	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	        if (n >= IPP_BUF_SIZE || n < 4)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

	        if ((*cb)(src, buffer, n) < n)
		{
	          DEBUG_puts("1ippReadIO: Unable to read string w/language "
		             "value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

                bufptr = buffer;

	       /*
	        * text-with-language and name-with-language are composite
		* values:
		*
		*    charset-length
		*    charset
		*    text-length
		*    text
		*/

		n = (bufptr[0] << 8) | bufptr[1];

		if ((bufptr + 2 + n) >= (buffer + IPP_BUF_SIZE) ||
		    n >= sizeof(string))
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

		memcpy(string, bufptr + 2, n);
		string[n] = '\0';

		value->string.charset = _cupsStrAlloc((char *)string);

                bufptr += 2 + n;
		n = (bufptr[0] << 8) | bufptr[1];

		if ((bufptr + 2 + n) >= (buffer + IPP_BUF_SIZE))
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

		bufptr[2 + n] = '\0';
                value->string.text = _cupsStrAlloc((char *)bufptr + 2);
	        break;

            case IPP_TAG_BEGIN_COLLECTION :
	       /*
	        * Oh, boy, here comes a collection value, so read it...
		*/

                value->collection = ippNew();

                if (n > 0)
		{
	          DEBUG_puts("1ippReadIO: begCollection tag with value length "
		             "> 0!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

		if (ippReadIO(src, cb, 1, ipp, value->collection) == IPP_ERROR)
		{
	          DEBUG_puts("1ippReadIO: Unable to read collection value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}
                break;

            case IPP_TAG_END_COLLECTION :
		ipp_buffer_release(buffer);

                if (n > 0)
		{
	          DEBUG_puts("1ippReadIO: endCollection tag with value length "
		             "> 0!");
		  return (IPP_ERROR);
		}

	        DEBUG_puts("1ippReadIO: endCollection tag...");
		return (ipp->state = IPP_DATA);

            case IPP_TAG_MEMBERNAME :
	       /*
	        * The value the name of the member in the collection, which
		* we need to carry over...
		*/

		if (n >= IPP_BUF_SIZE)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

	        if ((*cb)(src, buffer, n) < n)
		{
	          DEBUG_puts("1ippReadIO: Unable to read member name value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

		buffer[n] = '\0';
		attr->name = _cupsStrAlloc((char *)buffer);

               /*
	        * Since collection members are encoded differently than
		* regular attributes, make sure we don't start with an
		* empty value...
		*/

                attr->num_values --;

		DEBUG_printf(("2ippReadIO: member name=\"%s\"", attr->name));
		break;

            default : /* Other unsupported values */
		if (n > IPP_MAX_LENGTH)
		{
		  DEBUG_printf(("1ippReadIO: bad value length %d!", n));
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

		if (!value)
		{
		  DEBUG_puts("1ippReadIO: NULL value!");
		  ipp_buffer_release(buffer);
		  return (IPP_ERROR);
		}

                value->unknown.length = n;
	        if (n > 0)
		{
		  if ((value->unknown.data = malloc(n)) == NULL)
		  {
		    DEBUG_puts("1ippReadIO: Unable to allocate value");
		    ipp_buffer_release(buffer);
		    return (IPP_ERROR);
		  }

	          if ((*cb)(src, value->unknown.data, n) < n)
		  {
	            DEBUG_puts("1ippReadIO: Unable to read unsupported value!");
		    ipp_buffer_release(buffer);
		    return (IPP_ERROR);
		  }
		}
		else
		  value->unknown.data = NULL;
	        break;
	  }

          attr->num_values ++;

	 /*
          * If blocking is disabled, stop here...
	  */

          if (!blocking)
	    break;
	}
        break;

    case IPP_DATA :
        break;

    default :
        break; /* anti-compiler-warning-code */
  }

  DEBUG_printf(("1ippReadIO: returning ipp->state=%d!", ipp->state));
  ipp_buffer_release(buffer);

  return (ipp->state);
}


/*
 * 'ippTimeToDate()' - Convert from UNIX time to RFC 1903 format.
 */

const ipp_uchar_t *			/* O - RFC-1903 date/time data */
ippTimeToDate(time_t t)			/* I - UNIX time value */
{
  struct tm	*unixdate;		/* UNIX unixdate/time info */
  ipp_uchar_t	*date = _cupsGlobals()->ipp_date;
					/* RFC-1903 date/time data */


 /*
  * RFC-1903 date/time format is:
  *
  *    Byte(s)  Description
  *    -------  -----------
  *    0-1      Year (0 to 65535)
  *    2        Month (1 to 12)
  *    3        Day (1 to 31)
  *    4        Hours (0 to 23)
  *    5        Minutes (0 to 59)
  *    6        Seconds (0 to 60, 60 = "leap second")
  *    7        Deciseconds (0 to 9)
  *    8        +/- UTC
  *    9        UTC hours (0 to 11)
  *    10       UTC minutes (0 to 59)
  */

  unixdate = gmtime(&t);
  unixdate->tm_year += 1900;

  date[0]  = unixdate->tm_year >> 8;
  date[1]  = unixdate->tm_year;
  date[2]  = unixdate->tm_mon + 1;
  date[3]  = unixdate->tm_mday;
  date[4]  = unixdate->tm_hour;
  date[5]  = unixdate->tm_min;
  date[6]  = unixdate->tm_sec;
  date[7]  = 0;
  date[8]  = '+';
  date[9]  = 0;
  date[10] = 0;

  return (date);
}


/*
 * 'ippWrite()' - Write data for an IPP message to a HTTP connection.
 */

ipp_state_t				/* O - Current state */
ippWrite(http_t *http,			/* I - HTTP connection */
         ipp_t  *ipp)			/* I - IPP data */
{
  DEBUG_printf(("ippWrite(http=%p, ipp=%p)", http, ipp));

  if (!http)
    return (IPP_ERROR);

  return (ippWriteIO(http, (ipp_iocb_t)httpWrite2, http->blocking, NULL, ipp));
}


/*
 * 'ippWriteFile()' - Write data for an IPP message to a file.
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

ipp_state_t				/* O - Current state */
ippWriteFile(int   fd,			/* I - HTTP data */
             ipp_t *ipp)		/* I - IPP data */
{
  DEBUG_printf(("ippWriteFile(fd=%d, ipp=%p)", fd, ipp));

  ipp->state = IPP_IDLE;

  return (ippWriteIO(&fd, (ipp_iocb_t)ipp_write_file, 1, NULL, ipp));
}


/*
 * 'ippWriteIO()' - Write data for an IPP message.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ipp_state_t				/* O - Current state */
ippWriteIO(void       *dst,		/* I - Destination */
           ipp_iocb_t cb,		/* I - Write callback function */
	   int        blocking,		/* I - Use blocking IO? */
	   ipp_t      *parent,		/* I - Parent IPP message */
           ipp_t      *ipp)		/* I - IPP data */
{
  int			i;		/* Looping var */
  int			n;		/* Length of data */
  unsigned char		*buffer,	/* Data buffer */
			*bufptr;	/* Pointer into buffer */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_value_t		*value;		/* Current value */


  DEBUG_printf(("ippWriteIO(dst=%p, cb=%p, blocking=%d, parent=%p, ipp=%p)",
                dst, cb, blocking, parent, ipp));

  if (!dst || !ipp)
    return (IPP_ERROR);

  if ((buffer = ipp_buffer_get()) == NULL)
  {
    DEBUG_puts("1ippWriteIO: Unable to get write buffer");
    return (IPP_ERROR);
  }

  switch (ipp->state)
  {
    case IPP_IDLE :
        ipp->state ++; /* Avoid common problem... */

    case IPP_HEADER :
        if (parent == NULL)
	{
	 /*
	  * Send the request header:
	  *
	  *                 Version = 2 bytes
	  *   Operation/Status Code = 2 bytes
	  *              Request ID = 4 bytes
	  *                   Total = 8 bytes
	  */

          bufptr = buffer;

	  *bufptr++ = ipp->request.any.version[0];
	  *bufptr++ = ipp->request.any.version[1];
	  *bufptr++ = ipp->request.any.op_status >> 8;
	  *bufptr++ = ipp->request.any.op_status;
	  *bufptr++ = ipp->request.any.request_id >> 24;
	  *bufptr++ = ipp->request.any.request_id >> 16;
	  *bufptr++ = ipp->request.any.request_id >> 8;
	  *bufptr++ = ipp->request.any.request_id;

	  DEBUG_printf(("2ippWriteIO: version=%d.%d", buffer[0], buffer[1]));
	  DEBUG_printf(("2ippWriteIO: op_status=%04x",
			ipp->request.any.op_status));
	  DEBUG_printf(("2ippWriteIO: request_id=%d",
			ipp->request.any.request_id));

          if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	  {
	    DEBUG_puts("1ippWriteIO: Could not write IPP header...");
	    ipp_buffer_release(buffer);
	    return (IPP_ERROR);
	  }
	}

       /*
	* Reset the state engine to point to the first attribute
	* in the request/response, with no current group.
	*/

        ipp->state   = IPP_ATTRIBUTE;
	ipp->current = ipp->attrs;
	ipp->curtag  = IPP_TAG_ZERO;

	DEBUG_printf(("1ippWriteIO: ipp->current=%p", ipp->current));

       /*
        * If blocking is disabled, stop here...
	*/

        if (!blocking)
	  break;

    case IPP_ATTRIBUTE :
        while (ipp->current != NULL)
	{
	 /*
	  * Write this attribute...
	  */

	  bufptr = buffer;
	  attr   = ipp->current;

	  ipp->current = ipp->current->next;

          if (!parent)
	  {
	    if (ipp->curtag != attr->group_tag)
	    {
	     /*
	      * Send a group tag byte...
	      */

	      ipp->curtag = attr->group_tag;

	      if (attr->group_tag == IPP_TAG_ZERO)
		continue;

	      DEBUG_printf(("2ippWriteIO: wrote group tag=%x(%s)",
			    attr->group_tag, ippTagString(attr->group_tag)));
	      *bufptr++ = attr->group_tag;
	    }
	    else if (attr->group_tag == IPP_TAG_ZERO)
	      continue;
	  }

	  DEBUG_printf(("1ippWriteIO: %s (%s%s)", attr->name,
	                attr->num_values > 1 ? "1setOf " : "",
			ippTagString(attr->value_tag)));

         /*
	  * Write the attribute tag and name.  The current implementation
	  * does not support the extension value tags above 0x7f, so all
	  * value tags are 1 byte.
	  *
	  * The attribute name length does not include the trailing nul
	  * character in the source string.
	  *
	  * Collection values (parent != NULL) are written differently...
	  */

          if (parent == NULL)
	  {
           /*
	    * Get the length of the attribute name, and make sure it won't
	    * overflow the buffer...
	    */

            if ((n = (int)strlen(attr->name)) > (IPP_BUF_SIZE - 4))
	    {
	      DEBUG_printf(("1ippWriteIO: Attribute name too long (%d)", n));
	      ipp_buffer_release(buffer);
	      return (IPP_ERROR);
	    }

           /*
	    * Write the value tag, name length, and name string...
	    */

            DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)",
	                  attr->value_tag, ippTagString(attr->value_tag)));
            DEBUG_printf(("2ippWriteIO: writing name=%d,\"%s\"", n,
	                  attr->name));

            *bufptr++ = attr->value_tag;
	    *bufptr++ = n >> 8;
	    *bufptr++ = n;
	    memcpy(bufptr, attr->name, n);
	    bufptr += n;
          }
	  else
	  {
           /*
	    * Get the length of the attribute name, and make sure it won't
	    * overflow the buffer...
	    */

            if ((n = (int)strlen(attr->name)) > (IPP_BUF_SIZE - 7))
	    {
	      DEBUG_printf(("1ippWriteIO: Attribute name too long (%d)", n));
	      ipp_buffer_release(buffer);
	      return (IPP_ERROR);
	    }

           /*
	    * Write the member name tag, name length, name string, value tag,
	    * and empty name for the collection member attribute...
	    */

            DEBUG_printf(("2ippWriteIO: writing value tag=%x(memberName)",
	                  IPP_TAG_MEMBERNAME));
            DEBUG_printf(("2ippWriteIO: writing name=%d,\"%s\"", n,
	                  attr->name));
            DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)",
	                  attr->value_tag, ippTagString(attr->value_tag)));
            DEBUG_puts("2ippWriteIO: writing name=0,\"\"");

            *bufptr++ = IPP_TAG_MEMBERNAME;
	    *bufptr++ = 0;
	    *bufptr++ = 0;
	    *bufptr++ = n >> 8;
	    *bufptr++ = n;
	    memcpy(bufptr, attr->name, n);
	    bufptr += n;

            *bufptr++ = attr->value_tag;
            *bufptr++ = 0;
            *bufptr++ = 0;
	  }

         /*
	  * Now write the attribute value(s)...
	  */

	  switch (attr->value_tag & ~IPP_TAG_COPY)
	  {
	    case IPP_TAG_INTEGER :
	    case IPP_TAG_ENUM :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 9)
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

		 /*
	          * Integers and enumerations are both 4-byte signed
		  * (twos-complement) values.
		  *
		  * Put the 2-byte length and 4-byte value into the buffer...
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 4;
		  *bufptr++ = value->integer >> 24;
		  *bufptr++ = value->integer >> 16;
		  *bufptr++ = value->integer >> 8;
		  *bufptr++ = value->integer;
		}
		break;

	    case IPP_TAG_BOOLEAN :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 6)
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                 /*
		  * Boolean values are 1-byte; 0 = false, 1 = true.
		  *
		  * Put the 2-byte length and 1-byte value into the buffer...
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 1;
		  *bufptr++ = value->boolean;
		}
		break;

	    case IPP_TAG_TEXT :
	    case IPP_TAG_NAME :
	    case IPP_TAG_KEYWORD :
	    case IPP_TAG_URI :
	    case IPP_TAG_URISCHEME :
	    case IPP_TAG_CHARSET :
	    case IPP_TAG_LANGUAGE :
	    case IPP_TAG_MIMETYPE :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

        	    DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)",
		                  attr->value_tag,
				  ippTagString(attr->value_tag)));
        	    DEBUG_printf(("2ippWriteIO: writing name=0,\"\""));

                    if ((IPP_BUF_SIZE - (bufptr - buffer)) < 3)
		    {
                      if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	              {
	        	DEBUG_puts("1ippWriteIO: Could not write IPP "
			           "attribute...");
			ipp_buffer_release(buffer);
	        	return (IPP_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                  if (value->string.text != NULL)
                    n = (int)strlen(value->string.text);
		  else
		    n = 0;

                  if (n > (IPP_BUF_SIZE - 2))
		  {
		    DEBUG_printf(("1ippWriteIO: String too long (%d)", n));
		    ipp_buffer_release(buffer);
		    return (IPP_ERROR);
		  }

                  DEBUG_printf(("2ippWriteIO: writing string=%d,\"%s\"", n,
		                value->string.text));

                  if ((int)(IPP_BUF_SIZE - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

		 /*
		  * All simple strings consist of the 2-byte length and
		  * character data without the trailing nul normally found
		  * in C strings.  Also, strings cannot be longer than IPP_MAX_LENGTH
		  * bytes since the 2-byte length is a signed (twos-complement)
		  * value.
		  *
		  * Put the 2-byte length and string characters in the buffer.
		  */

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.text, n);
		    bufptr += n;
		  }
		}
		break;

	    case IPP_TAG_DATE :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 16)
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                 /*
		  * Date values consist of a 2-byte length and an
		  * 11-byte date/time structure defined by RFC 1903.
		  *
		  * Put the 2-byte length and 11-byte date/time
		  * structure in the buffer.
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 11;
		  memcpy(bufptr, value->date, 11);
		  bufptr += 11;
		}
		break;

	    case IPP_TAG_RESOLUTION :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 14)
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
		      return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                 /*
		  * Resolution values consist of a 2-byte length,
		  * 4-byte horizontal resolution value, 4-byte vertical
		  * resolution value, and a 1-byte units value.
		  *
		  * Put the 2-byte length and resolution value data
		  * into the buffer.
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 9;
		  *bufptr++ = value->resolution.xres >> 24;
		  *bufptr++ = value->resolution.xres >> 16;
		  *bufptr++ = value->resolution.xres >> 8;
		  *bufptr++ = value->resolution.xres;
		  *bufptr++ = value->resolution.yres >> 24;
		  *bufptr++ = value->resolution.yres >> 16;
		  *bufptr++ = value->resolution.yres >> 8;
		  *bufptr++ = value->resolution.yres;
		  *bufptr++ = value->resolution.units;
		}
		break;

	    case IPP_TAG_RANGE :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 13)
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                 /*
		  * Range values consist of a 2-byte length,
		  * 4-byte lower value, and 4-byte upper value.
		  *
		  * Put the 2-byte length and range value data
		  * into the buffer.
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 8;
		  *bufptr++ = value->range.lower >> 24;
		  *bufptr++ = value->range.lower >> 16;
		  *bufptr++ = value->range.lower >> 8;
		  *bufptr++ = value->range.lower;
		  *bufptr++ = value->range.upper >> 24;
		  *bufptr++ = value->range.upper >> 16;
		  *bufptr++ = value->range.upper >> 8;
		  *bufptr++ = value->range.upper;
		}
		break;

	    case IPP_TAG_TEXTLANG :
	    case IPP_TAG_NAMELANG :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    if ((IPP_BUF_SIZE - (bufptr - buffer)) < 3)
		    {
                      if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	              {
	        	DEBUG_puts("1ippWriteIO: Could not write IPP "
		                   "attribute...");
			ipp_buffer_release(buffer);
	        	return (IPP_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                 /*
		  * textWithLanguage and nameWithLanguage values consist
		  * of a 2-byte length for both strings and their
		  * individual lengths, a 2-byte length for the
		  * character string, the character string without the
		  * trailing nul, a 2-byte length for the character
		  * set string, and the character set string without
		  * the trailing nul.
		  */

                  n = 4;

		  if (value->string.charset != NULL)
                    n += (int)strlen(value->string.charset);

		  if (value->string.text != NULL)
                    n += (int)strlen(value->string.text);

                  if (n > (IPP_BUF_SIZE - 2))
		  {
		    DEBUG_printf(("1ippWriteIO: text/nameWithLanguage value "
		                  "too long (%d)", n));
		    ipp_buffer_release(buffer);
		    return (IPP_ERROR);
                  }

                  if ((int)(IPP_BUF_SIZE - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

                 /* Length of entire value */
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Length of charset */
		  if (value->string.charset != NULL)
		    n = (int)strlen(value->string.charset);
		  else
		    n = 0;

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Charset */
		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.charset, n);
		    bufptr += n;
		  }

                 /* Length of text */
                  if (value->string.text != NULL)
		    n = (int)strlen(value->string.text);
		  else
		    n = 0;

	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Text */
		  if (n > 0)
		  {
		    memcpy(bufptr, value->string.text, n);
		    bufptr += n;
		  }
		}
		break;

            case IPP_TAG_BEGIN_COLLECTION :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
		 /*
		  * Collections are written with the begin-collection
		  * tag first with a value of 0 length, followed by the
		  * attributes in the collection, then the end-collection
		  * value...
		  */

                  if ((IPP_BUF_SIZE - (bufptr - buffer)) < 5)
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                 /*
		  * Write a data length of 0 and flush the buffer...
		  */

	          *bufptr++ = 0;
		  *bufptr++ = 0;

                  if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	          {
	            DEBUG_puts("1ippWriteIO: Could not write IPP "
		               "attribute...");
		    ipp_buffer_release(buffer);
	            return (IPP_ERROR);
	          }

		  bufptr = buffer;

                 /*
		  * Then write the collection attribute...
		  */

                  value->collection->state = IPP_IDLE;

		  if (ippWriteIO(dst, cb, 1, ipp,
		                 value->collection) == IPP_ERROR)
		  {
		    DEBUG_puts("1ippWriteIO: Unable to write collection value");
		    ipp_buffer_release(buffer);
		    return (IPP_ERROR);
		  }
		}
		break;

            default :
	        for (i = 0, value = attr->values;
		     i < attr->num_values;
		     i ++, value ++)
		{
		  if (i)
		  {
		   /*
		    * Arrays and sets are done by sending additional
		    * values with a zero-length name...
		    */

                    if ((IPP_BUF_SIZE - (bufptr - buffer)) < 3)
		    {
                      if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	              {
	        	DEBUG_puts("1ippWriteIO: Could not write IPP "
		                   "attribute...");
			ipp_buffer_release(buffer);
	        	return (IPP_ERROR);
	              }

		      bufptr = buffer;
		    }

                    *bufptr++ = attr->value_tag;
		    *bufptr++ = 0;
		    *bufptr++ = 0;
		  }

                 /*
		  * An unknown value might some new value that a
		  * vendor has come up with. It consists of a
		  * 2-byte length and the bytes in the unknown
		  * value buffer.
		  */

                  n = value->unknown.length;

                  if (n > (IPP_BUF_SIZE - 2))
		  {
		    DEBUG_printf(("1ippWriteIO: Data length too long (%d)",
		                  n));
		    ipp_buffer_release(buffer);
		    return (IPP_ERROR);
		  }

                  if ((int)(IPP_BUF_SIZE - (bufptr - buffer)) < (n + 2))
		  {
                    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	            {
	              DEBUG_puts("1ippWriteIO: Could not write IPP "
		                 "attribute...");
		      ipp_buffer_release(buffer);
	              return (IPP_ERROR);
	            }

		    bufptr = buffer;
		  }

                 /* Length of unknown value */
	          *bufptr++ = n >> 8;
		  *bufptr++ = n;

                 /* Value */
		  if (n > 0)
		  {
		    memcpy(bufptr, value->unknown.data, n);
		    bufptr += n;
		  }
		}
		break;
	  }

         /*
	  * Write the data out...
	  */

	  if (bufptr > buffer)
	  {
	    if ((*cb)(dst, buffer, (int)(bufptr - buffer)) < 0)
	    {
	      DEBUG_puts("1ippWriteIO: Could not write IPP attribute...");
	      ipp_buffer_release(buffer);
	      return (IPP_ERROR);
	    }

	    DEBUG_printf(("2ippWriteIO: wrote %d bytes",
			  (int)(bufptr - buffer)));
	  }

	 /*
          * If blocking is disabled, stop here...
	  */

          if (!blocking)
	    break;
	}

	if (ipp->current == NULL)
	{
         /*
	  * Done with all of the attributes; add the end-of-attributes
	  * tag or end-collection attribute...
	  */

          if (parent == NULL)
	  {
            buffer[0] = IPP_TAG_END;
	    n         = 1;
	  }
	  else
	  {
            buffer[0] = IPP_TAG_END_COLLECTION;
	    buffer[1] = 0; /* empty name */
	    buffer[2] = 0;
	    buffer[3] = 0; /* empty value */
	    buffer[4] = 0;
	    n         = 5;
	  }

	  if ((*cb)(dst, buffer, n) < 0)
	  {
	    DEBUG_puts("1ippWriteIO: Could not write IPP end-tag...");
	    ipp_buffer_release(buffer);
	    return (IPP_ERROR);
	  }

	  ipp->state = IPP_DATA;
	}
        break;

    case IPP_DATA :
        break;

    default :
        break; /* anti-compiler-warning-code */
  }

  ipp_buffer_release(buffer);

  return (ipp->state);
}


/*
 * '_ippAddAttr()' - Add a new attribute to the request.
 */

ipp_attribute_t *			/* O - New attribute */
_ippAddAttr(ipp_t *ipp,			/* I - IPP message */
            int   num_values)		/* I - Number of values */
{
  ipp_attribute_t	*attr;		/* New attribute */


  DEBUG_printf(("4_ippAddAttr(ipp=%p, num_values=%d)", ipp, num_values));

  if (!ipp || num_values < 0)
    return (NULL);

  attr = calloc(sizeof(ipp_attribute_t) +
                (num_values - 1) * sizeof(ipp_value_t), 1);

  if (attr != NULL)
  {
    attr->num_values = num_values;

    if (ipp->last == NULL)
      ipp->attrs = attr;
    else
      ipp->last->next = attr;

    ipp->last = attr;
  }

  DEBUG_printf(("5_ippAddAttr: Returning %p", attr));

  return (attr);
}


/*
 * '_ippFreeAttr()' - Free an attribute.
 */

void
_ippFreeAttr(ipp_attribute_t *attr)	/* I - Attribute to free */
{
  int		i;			/* Looping var */
  ipp_value_t	*value;			/* Current value */


  DEBUG_printf(("4_ippFreeAttr(attr=%p)", attr));

  switch (attr->value_tag)
  {
    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_RESERVED_STRING :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
    case IPP_TAG_CHARSET :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
	for (i = 0, value = attr->values;
	     i < attr->num_values;
	     i ++, value ++)
	  _cupsStrFree(value->string.text);
	break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
	for (i = 0, value = attr->values;
	     i < attr->num_values;
	     i ++, value ++)
	{
	  if (value->string.charset && i == 0)
	    _cupsStrFree(value->string.charset);
	  _cupsStrFree(value->string.text);
	}
	break;

    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
    case IPP_TAG_BOOLEAN :
    case IPP_TAG_DATE :
    case IPP_TAG_RESOLUTION :
    case IPP_TAG_RANGE :
	break;

    case IPP_TAG_BEGIN_COLLECTION :
	for (i = 0, value = attr->values;
	     i < attr->num_values;
	     i ++, value ++)
          ippDelete(value->collection);
	break;

    case IPP_TAG_STRING :
	for (i = 0, value = attr->values;
	     i < attr->num_values;
	     i ++, value ++)
	  free(value->unknown.data);
        break;

    default :
        if (!((int)attr->value_tag & IPP_TAG_COPY))
	{
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
            if (value->unknown.data)
	      free(value->unknown.data);
        }
	break;
  }

  if (attr->name)
    _cupsStrFree(attr->name);

  free(attr);
}


/*
 * 'ipp_buffer_get()' - Get a read/write buffer.
 */

static unsigned char *			/* O - Buffer */
ipp_buffer_get(void)
{
  _ipp_buffer_t		*buffer;	/* Current buffer */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


  for (buffer = cg->ipp_buffers; buffer; buffer = buffer->next)
    if (!buffer->used)
    {
      buffer->used = 1;
      return (buffer->d);
    }

  if ((buffer = malloc(sizeof(_ipp_buffer_t))) == NULL)
    return (NULL);

  buffer->used    = 1;
  buffer->next    = cg->ipp_buffers;
  cg->ipp_buffers = buffer;

  return (buffer->d);
}


/*
 * 'ipp_buffer_release()' - Release a read/write buffer.
 */

static void
ipp_buffer_release(unsigned char *b)	/* I - Buffer to release */
{
  ((_ipp_buffer_t *)b)->used = 0;
}


/*
 * 'ipp_length()' - Compute the length of an IPP message or collection value.
 */

static size_t				/* O - Size of IPP message */
ipp_length(ipp_t *ipp,			/* I - IPP message or collection */
           int   collection)		/* I - 1 if a collection, 0 otherwise */
{
  int			i;		/* Looping var */
  int			bytes;		/* Number of bytes */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		group;		/* Current group */
  ipp_value_t		*value;		/* Current value */


  if (ipp == NULL)
    return (0);

 /*
  * Start with 8 bytes for the IPP message header...
  */

  bytes = collection ? 0 : 8;

 /*
  * Then add the lengths of each attribute...
  */

  group = IPP_TAG_ZERO;

  for (attr = ipp->attrs; attr != NULL; attr = attr->next)
  {
    if (attr->group_tag != group && !collection)
    {
      group = attr->group_tag;
      if (group == IPP_TAG_ZERO)
	continue;

      bytes ++;	/* Group tag */
    }

    if (!attr->name)
      continue;

    DEBUG_printf(("9ipp_length: attr->name=\"%s\", attr->num_values=%d, "
                  "bytes=%d", attr->name, attr->num_values, bytes));

    bytes += (int)strlen(attr->name);	/* Name */
    bytes += attr->num_values;		/* Value tag for each value */
    bytes += 2 * attr->num_values;	/* Name lengths */
    bytes += 2 * attr->num_values;	/* Value lengths */

    if (collection)
      bytes += 5;			/* Add membername overhead */

    switch (attr->value_tag & ~IPP_TAG_COPY)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
          bytes += 4 * attr->num_values;
	  break;

      case IPP_TAG_BOOLEAN :
          bytes += attr->num_values;
	  break;

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
      case IPP_TAG_URISCHEME :
      case IPP_TAG_CHARSET :
      case IPP_TAG_LANGUAGE :
      case IPP_TAG_MIMETYPE :
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
	    if (value->string.text != NULL)
	      bytes += (int)strlen(value->string.text);
	  break;

      case IPP_TAG_DATE :
          bytes += 11 * attr->num_values;
	  break;

      case IPP_TAG_RESOLUTION :
          bytes += 9 * attr->num_values;
	  break;

      case IPP_TAG_RANGE :
          bytes += 8 * attr->num_values;
	  break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
          bytes += 4 * attr->num_values;/* Charset + text length */

	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
	  {
	    if (value->string.charset != NULL)
	      bytes += (int)strlen(value->string.charset);

	    if (value->string.text != NULL)
	      bytes += (int)strlen(value->string.text);
	  }
	  break;

      case IPP_TAG_BEGIN_COLLECTION :
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
            bytes += (int)ipp_length(value->collection, 1);
	  break;

      default :
	  for (i = 0, value = attr->values;
	       i < attr->num_values;
	       i ++, value ++)
            bytes += value->unknown.length;
	  break;
    }
  }

 /*
  * Finally, add 1 byte for the "end of attributes" tag or 5 bytes
  * for the "end of collection" tag and return...
  */

  if (collection)
    bytes += 5;
  else
    bytes ++;

  DEBUG_printf(("8ipp_length: Returning %d bytes", bytes));

  return (bytes);
}


/*
 * 'ipp_read_http()' - Semi-blocking read on a HTTP connection...
 */

static ssize_t				/* O - Number of bytes read */
ipp_read_http(http_t      *http,	/* I - Client connection */
              ipp_uchar_t *buffer,	/* O - Buffer for data */
	      size_t      length)	/* I - Total length */
{
  int		tbytes,			/* Total bytes read */
		bytes;			/* Bytes read this pass */
  char		len[32];		/* Length string */


  DEBUG_printf(("7ipp_read_http(http=%p, buffer=%p, length=%d)",
                http, buffer, (int)length));

 /*
  * Loop until all bytes are read...
  */

  for (tbytes = 0, bytes = 0;
       tbytes < (int)length;
       tbytes += bytes, buffer += bytes)
  {
    DEBUG_printf(("9ipp_read_http: tbytes=%d, http->state=%d", tbytes,
                  http->state));

    if (http->state == HTTP_WAITING)
      break;

    if (http->used > 0 && http->data_encoding == HTTP_ENCODE_LENGTH)
    {
     /*
      * Do "fast read" from HTTP buffer directly...
      */

      if (http->used > (int)(length - tbytes))
        bytes = (int)(length - tbytes);
      else
        bytes = http->used;

      if (bytes == 1)
	buffer[0] = http->buffer[0];
      else
	memcpy(buffer, http->buffer, bytes);

      http->used           -= bytes;
      http->data_remaining -= bytes;

      if (http->data_remaining <= INT_MAX)
	http->_data_remaining = (int)http->data_remaining;
      else
	http->_data_remaining = INT_MAX;

      if (http->used > 0)
	memmove(http->buffer, http->buffer + bytes, http->used);

      if (http->data_remaining == 0)
      {
	if (http->data_encoding == HTTP_ENCODE_CHUNKED)
	{
	 /*
	  * Get the trailing CR LF after the chunk...
	  */

	  if (!httpGets(len, sizeof(len), http))
	    return (-1);
	}

	if (http->data_encoding != HTTP_ENCODE_CHUNKED)
	{
	  if (http->state == HTTP_POST_RECV)
	    http->state ++;
	  else
	    http->state = HTTP_WAITING;
	}
      }
    }
    else
    {
     /*
      * Wait a maximum of 1 second for data...
      */

      if (!http->blocking)
      {
       /*
        * Wait up to 10 seconds for more data on non-blocking sockets...
	*/

	if (!httpWait(http, 10000))
	{
	 /*
          * Signal no data...
	  */

          bytes = -1;
	  break;
	}
      }

      if ((bytes = httpRead2(http, (char *)buffer, length - tbytes)) < 0)
      {
#ifdef WIN32
        break;
#else
        if (errno != EAGAIN && errno != EINTR)
	  break;

	bytes = 0;
#endif /* WIN32 */
      }
      else if (bytes == 0)
        break;
    }
  }

 /*
  * Return the number of bytes read...
  */

  if (tbytes == 0 && bytes < 0)
    tbytes = -1;

  DEBUG_printf(("8ipp_read_http: Returning %d bytes", tbytes));

  return (tbytes);
}


/*
 * 'ipp_read_file()' - Read IPP data from a file.
 */

static ssize_t				/* O - Number of bytes read */
ipp_read_file(int         *fd,		/* I - File descriptor */
              ipp_uchar_t *buffer,	/* O - Read buffer */
	      size_t      length)	/* I - Number of bytes to read */
{
#ifdef WIN32
  return ((ssize_t)read(*fd, buffer, (unsigned)length));
#else
  return (read(*fd, buffer, length));
#endif /* WIN32 */
}


/*
 * 'ipp_write_file()' - Write IPP data to a file.
 */

static ssize_t				/* O - Number of bytes written */
ipp_write_file(int         *fd,		/* I - File descriptor */
               ipp_uchar_t *buffer,	/* I - Data to write */
               size_t      length)	/* I - Number of bytes to write */
{
#ifdef WIN32
  return ((ssize_t)write(*fd, buffer, (unsigned)length));
#else
  return (write(*fd, buffer, length));
#endif /* WIN32 */
}


#ifdef __linux
/*
 * The following symbol definitions are provided only for KDE
 * compatibility during the CUPS 1.2 testing period and will be
 * removed in a future release of CUPS.  These are PRIVATE APIs
 * from CUPS 1.1.x that the KDE developers chose to use...
 */

ipp_attribute_t *			/* O - New attribute */
_ipp_add_attr(ipp_t *ipp,		/* I - IPP message */
              int   num_values)		/* I - Number of values */
{
  return (_ippAddAttr(ipp, num_values));
}

void
_ipp_free_attr(ipp_attribute_t *attr)	/* I - Attribute to free */
{
  _ippFreeAttr(attr);
}
#endif /* __linux */


/*
 * End of "$Id: ipp.c 9357 2010-11-11 17:40:35Z mike $".
 */
