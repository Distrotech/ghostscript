/*
 * "$Id: globals.c 9161 2010-06-16 22:34:03Z mike $"
 *
 *   Global variable access routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
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
 *   _cupsGlobals()	  - Return a pointer to thread local storage.
 *   cups_env_init()      - Initialize environment variables.
 *   globals_init()       - Initialize globals once.
 *   globals_destructor() - Free memory allocated by _cupsGlobals().
 */

/*
 * Include necessary headers...
 */

#include "http-private.h"
#include "globals.h"
#include <stdlib.h>


/*
 * 'cups_env_init()' - Initialize environment variables.
 */

static void
cups_env_init(_cups_globals_t *g)	/* I - Global data */
{
#ifndef WIN32
#  ifdef HAVE_GETEUID
  if ((geteuid() != getuid() && getuid()) || getegid() != getgid())
#  else
  if (!getuid())
#  endif /* HAVE_GETEUID */
  {
   /*
    * When running setuid/setgid, don't allow environment variables to override
    * the directories...
    */

    g->cups_datadir    = CUPS_DATADIR;
    g->cups_serverbin  = CUPS_SERVERBIN;
    g->cups_serverroot = CUPS_SERVERROOT;
    g->cups_statedir   = CUPS_STATEDIR;
    g->localedir       = CUPS_LOCALEDIR;
  }
  else
#endif /* !WIN32 */
  {
   /*
    * Allow directories to be overridden by environment variables.
    */

    if ((g->cups_datadir = getenv("CUPS_DATADIR")) == NULL)
      g->cups_datadir = CUPS_DATADIR;

    if ((g->cups_serverbin = getenv("CUPS_SERVERBIN")) == NULL)
      g->cups_serverbin = CUPS_SERVERBIN;

    if ((g->cups_serverroot = getenv("CUPS_SERVERROOT")) == NULL)
      g->cups_serverroot = CUPS_SERVERROOT;

    if ((g->cups_statedir = getenv("CUPS_STATEDIR")) == NULL)
      g->cups_statedir = CUPS_STATEDIR;

    if ((g->localedir = getenv("LOCALEDIR")) == NULL)
      g->localedir = CUPS_LOCALEDIR;
  }
}


#ifdef HAVE_PTHREAD_H
/*
 * Implement per-thread globals...
 */

/*
 * Local globals...
 */

static pthread_key_t	globals_key = -1;
					/* Thread local storage key */
static pthread_once_t	globals_key_once = PTHREAD_ONCE_INIT;
					/* One-time initialization object */


/*
 * Local functions...
 */

static void	globals_init();
static void	globals_destructor(void *value);


/*
 * '_cupsGlobals()' - Return a pointer to thread local storage
 */

_cups_globals_t *			/* O - Pointer to global data */
_cupsGlobals(void)
{
  _cups_globals_t *globals;		/* Pointer to global data */


 /* 
  * Initialize the global data exactly once...
  */

  pthread_once(&globals_key_once, globals_init);

 /*
  * See if we have allocated the data yet...
  */

  if ((globals = (_cups_globals_t *)pthread_getspecific(globals_key)) == NULL)
  {
   /*
    * No, allocate memory as set the pointer for the key...
    */

    globals = calloc(1, sizeof(_cups_globals_t));
    pthread_setspecific(globals_key, globals);

   /*
    * Initialize variables that have non-zero values
    */

    globals->encryption  = (http_encryption_t)-1;
    globals->password_cb = (cups_password_cb2_t)_cupsGetPassword;

    cups_env_init(globals);
  }

 /*
  * Return the pointer to the data...
  */

  return (globals);
}


/*
 * 'globals_init()' - Initialize globals once.
 */

static void
globals_init()
{
  pthread_key_create(&globals_key, globals_destructor);
}


/*
 * 'globals_destructor()' - Free memory allocated by _cupsGlobals().
 */

static void
globals_destructor(void *value)		/* I - Data to free */
{
  int			i;		/* Looping var */
  _ipp_buffer_t		*buffer,	/* Current IPP read/write buffer */
			*next;		/* Next buffer */
  _cups_globals_t	*cg;		/* Global data */


  cg = (_cups_globals_t *)value;

  httpClose(cg->http);

  for (i = 0; i < 3; i ++)
    cupsFileClose(cg->stdio_files[i]);

  if (cg->last_status_message)
    _cupsStrFree(cg->last_status_message);

#  ifndef CUPS_LITE
  cupsFreeOptions(cg->cupsd_num_settings, cg->cupsd_settings);
#  endif /* !CUPS_LITE */

  for (buffer = cg->ipp_buffers; buffer; buffer = next)
  {
    next = buffer->next;
    free(buffer);
  }

  cupsArrayDelete(cg->pwg_size_lut);
  cupsArrayDelete(cg->leg_size_lut);

  free(value);
}


#else
/*
 * Implement static globals...
 */

/*
 * '_cupsGlobals()' - Return a pointer to thread local storage.
 */

_cups_globals_t *			/* O - Pointer to global data */
_cupsGlobals(void)
{
  static _cups_globals_t globals;	/* Global data */
  static int		initialized = 0;/* Global data initialized? */


 /*
  * Initialize global data as needed...
  */

  if (!initialized)
  {
    initialized = 1;

   /*
    * Initialize global variables...
    */

    memset(&globals, 0, sizeof(globals));

    globals.encryption  = (http_encryption_t)-1;
    globals.password_cb = (cups_password_cb2_t)_cupsGetPassword;

    cups_env_init(&globals);
  }

  return (&globals);
}
#endif /* HAVE_PTHREAD_H */


/*
 * End of "$Id: globals.c 9161 2010-06-16 22:34:03Z mike $".
 */
