/*
 * "$Id: http.c 9233 2010-08-10 06:15:55Z mike $"
 *
 *   HTTP routines for CUPS.
 *
 *   Copyright 2007-2010 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   This file contains Kerberos support code, copyright 2006 by
 *   Jelmer Vernooij.
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
 *   _httpBIOMethods()    - Get the OpenSSL BIO methods for HTTP connections.
 *   httpBlocking()       - Set blocking/non-blocking behavior on a connection.
 *   httpCheck()          - Check to see if there is a pending response from the
 *                          server.
 *   httpClearCookie()    - Clear the cookie value(s).
 *   httpClearFields()    - Clear HTTP request fields.
 *   httpClose()          - Close an HTTP connection.
 *   httpConnect()        - Connect to a HTTP server.
 *   httpConnectEncrypt() - Connect to a HTTP server using encryption.
 *   _httpCreate()        - Create an unconnected HTTP connection.
 *   httpDelete()         - Send a DELETE request to the server.
 *   httpEncryption()     - Set the required encryption on the link.
 *   httpError()          - Get the last error on a connection.
 *   httpFlush()          - Flush data from a HTTP connection.
 *   httpFlushWrite()     - Flush data in write buffer.
 *   httpGet()            - Send a GET request to the server.
 *   httpGetAuthString()  - Get the current authorization string.
 *   httpGetBlocking()    - Get the blocking/non-block state of a connection.
 *   httpGetCookie()      - Get any cookie data from the response.
 *   httpGetFd()          - Get the file descriptor associated with a
 *                          connection.
 *   httpGetField()       - Get a field value from a request/response.
 *   httpGetLength()      - Get the amount of data remaining from the
 *                          content-length or transfer-encoding fields.
 *   httpGetLength2()     - Get the amount of data remaining from the
 *                          content-length or transfer-encoding fields.
 *   httpGetStatus()      - Get the status of the last HTTP request.
 *   httpGetSubField()    - Get a sub-field value.
 *   httpGetSubField2()   - Get a sub-field value.
 *   httpGets()           - Get a line of text from a HTTP connection.
 *   httpHead()           - Send a HEAD request to the server.
 *   httpInitialize()     - Initialize the HTTP interface library and set the
 *                          default HTTP proxy (if any).
 *   httpOptions()        - Send an OPTIONS request to the server.
 *   httpPost()           - Send a POST request to the server.
 *   httpPrintf()         - Print a formatted string to a HTTP connection.
 *   httpPut()            - Send a PUT request to the server.
 *   httpRead()           - Read data from a HTTP connection.
 *   httpRead2()          - Read data from a HTTP connection.
 *   _httpReadCDSA()      - Read function for the CDSA library.
 *   _httpReadGNUTLS()    - Read function for the GNU TLS library.
 *   httpReconnect()      - Reconnect to a HTTP server.
 *   httpSetAuthString()  - Set the current authorization string.
 *   httpSetCookie()      - Set the cookie value(s).
 *   httpSetExpect()      - Set the Expect: header in a request.
 *   httpSetField()       - Set the value of an HTTP header.
 *   httpSetLength()      - Set the content-length and content-encoding.
 *   httpTrace()          - Send an TRACE request to the server.
 *   httpUpdate()         - Update the current HTTP state for incoming data.
 *   _httpWait()          - Wait for data available on a connection (no flush).
 *   httpWait()           - Wait for data available on a connection.
 *   httpWrite()          - Write data to a HTTP connection.
 *   httpWrite2()         - Write data to a HTTP connection.
 *   _httpWriteCDSA()     - Write function for the CDSA library.
 *   _httpWriteGNUTLS()   - Write function for the GNU TLS library.
 *   http_bio_ctrl()      - Control the HTTP connection.
 *   http_bio_free()      - Free OpenSSL data.
 *   http_bio_new()       - Initialize an OpenSSL BIO structure.
 *   http_bio_puts()      - Send a string for OpenSSL.
 *   http_bio_read()      - Read data for OpenSSL.
 *   http_bio_write()     - Write data for OpenSSL.
 *   http_debug_hex()     - Do a hex dump of a buffer.
 *   http_field()         - Return the field index for a field name.
 *   http_read_ssl()      - Read from a SSL/TLS connection.
 *   http_locking_cb()    - Lock/unlock a thread's mutex.
 *   http_send()          - Send a request with all fields and the trailing
 *                          blank line.
 *   http_setup_ssl()     - Set up SSL/TLS support on a connection.
 *   http_shutdown_ssl()  - Shut down SSL/TLS on a connection.
 *   http_threadid_cb()   - Return the current thread ID.
 *   http_upgrade()       - Force upgrade to TLS encryption.
 *   http_write()         - Write a buffer to a HTTP connection.
 *   http_write_chunk()   - Write a chunked buffer.
 *   http_write_ssl()     - Write to a SSL/TLS connection.
 */

/*
 * Include necessary headers...
 */

#include "http-private.h"
#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#ifndef WIN32
#  include <signal.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#endif /* !WIN32 */
#ifdef HAVE_POLL
#  include <sys/poll.h>
#endif /* HAVE_POLL */


/*
 * Some operating systems have done away with the Fxxxx constants for
 * the fcntl() call; this works around that "feature"...
 */

#ifndef FNONBLK
#  define FNONBLK O_NONBLOCK
#endif /* !FNONBLK */


/*
 * Local functions...
 */

#ifdef DEBUG
static void		http_debug_hex(const char *prefix, const char *buffer,
			               int bytes);
#endif /* DEBUG */
static http_field_t	http_field(const char *name);
static int		http_send(http_t *http, http_state_t request,
			          const char *uri);
static int		http_write(http_t *http, const char *buffer,
			           int length);
static int		http_write_chunk(http_t *http, const char *buffer,
			                 int length);
#ifdef HAVE_SSL
static int		http_read_ssl(http_t *http, char *buf, int len);
static int		http_setup_ssl(http_t *http);
static void		http_shutdown_ssl(http_t *http);
static int		http_upgrade(http_t *http);
static int		http_write_ssl(http_t *http, const char *buf, int len);

#  ifdef HAVE_GNUTLS
#    ifdef HAVE_PTHREAD_H
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#    endif /* HAVE_PTHREAD_H */

#  elif defined(HAVE_LIBSSL) && defined(HAVE_PTHREAD_H)
static pthread_mutex_t	*http_locks;	/* OpenSSL lock mutexes */

static void		http_locking_cb(int mode, int type, const char *file,
					int line);
static unsigned long	http_threadid_cb(void);
#  endif /* HAVE_GNUTLS */
#endif /* HAVE_SSL */


/*
 * Local globals...
 */

static const char * const http_fields[] =
			{
			  "Accept-Language",
			  "Accept-Ranges",
			  "Authorization",
			  "Connection",
			  "Content-Encoding",
			  "Content-Language",
			  "Content-Length",
			  "Content-Location",
			  "Content-MD5",
			  "Content-Range",
			  "Content-Type",
			  "Content-Version",
			  "Date",
			  "Host",
			  "If-Modified-Since",
			  "If-Unmodified-since",
			  "Keep-Alive",
			  "Last-Modified",
			  "Link",
			  "Location",
			  "Range",
			  "Referer",
			  "Retry-After",
			  "Transfer-Encoding",
			  "Upgrade",
			  "User-Agent",
			  "WWW-Authenticate"
			};
#ifdef DEBUG
static const char * const http_states[] =
			{
			  "HTTP_WAITING",
			  "HTTP_OPTIONS",
			  "HTTP_GET",
			  "HTTP_GET_SEND",
			  "HTTP_HEAD",
			  "HTTP_POST",
			  "HTTP_POST_RECV",
			  "HTTP_POST_SEND",
			  "HTTP_PUT",
			  "HTTP_PUT_RECV",
			  "HTTP_DELETE",
			  "HTTP_TRACE",
			  "HTTP_CLOSE",
			  "HTTP_STATUS"
			};
#endif /* DEBUG */


#if defined(HAVE_SSL) && defined(HAVE_LIBSSL)
/*
 * BIO methods for OpenSSL...
 */

static int		http_bio_write(BIO *h, const char *buf, int num);
static int		http_bio_read(BIO *h, char *buf, int size);
static int		http_bio_puts(BIO *h, const char *str);
static long		http_bio_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int		http_bio_new(BIO *h);
static int		http_bio_free(BIO *data);

static BIO_METHOD	http_bio_methods =
			{
			  BIO_TYPE_SOCKET,
			  "http",
			  http_bio_write,
			  http_bio_read,
			  http_bio_puts,
			  NULL, /* http_bio_gets, */
			  http_bio_ctrl,
			  http_bio_new,
			  http_bio_free,
			  NULL,
			};


/*
 * '_httpBIOMethods()' - Get the OpenSSL BIO methods for HTTP connections.
 */

BIO_METHOD *				/* O - BIO methods for OpenSSL */
_httpBIOMethods(void)
{
  return (&http_bio_methods);
}
#endif /* HAVE_SSL && HAVE_LIBSSL */


/*
 * 'httpBlocking()' - Set blocking/non-blocking behavior on a connection.
 */

void
httpBlocking(http_t *http,		/* I - Connection to server */
             int    b)			/* I - 1 = blocking, 0 = non-blocking */
{
  if (http)
    http->blocking = b;
}


/*
 * 'httpCheck()' - Check to see if there is a pending response from the server.
 */

int					/* O - 0 = no data, 1 = data available */
httpCheck(http_t *http)			/* I - Connection to server */
{
  return (httpWait(http, 0));
}


/*
 * 'httpClearCookie()' - Clear the cookie value(s).
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

void
httpClearCookie(http_t *http)		/* I - Connection to server */
{
  if (!http)
    return;

  if (http->cookie)
  {
    free(http->cookie);
    http->cookie = NULL;
  }
}


/*
 * 'httpClearFields()' - Clear HTTP request fields.
 */

void
httpClearFields(http_t *http)		/* I - Connection to server */
{
  if (http)
  {
    memset(http->fields, 0, sizeof(http->fields));
    if (http->hostname[0] == '/')
      httpSetField(http, HTTP_FIELD_HOST, "localhost");
    else
      httpSetField(http, HTTP_FIELD_HOST, http->hostname);

    if (http->field_authorization)
    {
      free(http->field_authorization);
      http->field_authorization = NULL;
    }

    http->expect = (http_status_t)0;
  }
}


/*
 * 'httpClose()' - Close an HTTP connection.
 */

void
httpClose(http_t *http)			/* I - Connection to server */
{
#ifdef HAVE_GSSAPI
  OM_uint32	minor_status;		/* Minor status code */
#endif /* HAVE_GSSAPI */


  DEBUG_printf(("httpClose(http=%p)", http));

  if (!http)
    return;

  httpAddrFreeList(http->addrlist);

  if (http->cookie)
    free(http->cookie);

#ifdef HAVE_SSL
  if (http->tls)
    http_shutdown_ssl(http);
#endif /* HAVE_SSL */

#ifdef WIN32
  closesocket(http->fd);
#else
  close(http->fd);
#endif /* WIN32 */

#ifdef HAVE_GSSAPI
  if (http->gssctx != GSS_C_NO_CONTEXT)
    gss_delete_sec_context(&minor_status, &http->gssctx, GSS_C_NO_BUFFER);

  if (http->gssname != GSS_C_NO_NAME)
    gss_release_name(&minor_status, &http->gssname);
#endif /* HAVE_GSSAPI */

#ifdef HAVE_AUTHORIZATION_H
  if (http->auth_ref)
    AuthorizationFree(http->auth_ref, kAuthorizationFlagDefaults);
#endif /* HAVE_AUTHORIZATION_H */

  httpClearFields(http);

  if (http->authstring && http->authstring != http->_authstring)
    free(http->authstring);

  free(http);
}


/*
 * 'httpConnect()' - Connect to a HTTP server.
 *
 * This function is deprecated - use @link httpConnectEncrypt@ instead.
 *
 * @deprecated@
 */

http_t *				/* O - New HTTP connection */
httpConnect(const char *host,		/* I - Host to connect to */
            int        port)		/* I - Port number */
{
  return (httpConnectEncrypt(host, port, HTTP_ENCRYPT_IF_REQUESTED));
}


/*
 * 'httpConnectEncrypt()' - Connect to a HTTP server using encryption.
 */

http_t *				/* O - New HTTP connection */
httpConnectEncrypt(
    const char        *host,		/* I - Host to connect to */
    int               port,		/* I - Port number */
    http_encryption_t encryption)	/* I - Type of encryption to use */
{
  http_t	*http;			/* New HTTP connection */


  DEBUG_printf(("httpConnectEncrypt(host=\"%s\", port=%d, encryption=%d)",
                host, port, encryption));

 /*
  * Create the HTTP structure...
  */

  if ((http = _httpCreate(host, port, encryption)) == NULL)
    return (NULL);

 /*
  * Connect to the remote system...
  */

  if (!httpReconnect(http))
    return (http);

 /*
  * Could not connect to any known address - bail out!
  */

  httpAddrFreeList(http->addrlist);

  free(http);

  return (NULL);
}


/*
 * '_httpCreate()' - Create an unconnected HTTP connection.
 */

http_t *				/* O - HTTP connection */
_httpCreate(
    const char        *host,		/* I - Hostname */
    int               port,		/* I - Port number */
    http_encryption_t encryption)	/* I - Encryption to use */
{
  http_t		*http;		/* New HTTP connection */
  http_addrlist_t	*addrlist;	/* Host address data */
  char			service[255];	/* Service name */


  DEBUG_printf(("4_httpCreate(host=\"%s\", port=%d, encryption=%d)",
                host, port, encryption));

  if (!host)
    return (NULL);

  httpInitialize();

 /*
  * Lookup the host...
  */

  sprintf(service, "%d", port);

  if ((addrlist = httpAddrGetList(host, AF_UNSPEC, service)) == NULL)
    return (NULL);

 /*
  * Allocate memory for the structure...
  */

  if ((http = calloc(sizeof(http_t), 1)) == NULL)
  {
    httpAddrFreeList(addrlist);
    return (NULL);
  }

 /*
  * Initialize the HTTP data...
  */

  http->activity = time(NULL);
  http->addrlist = addrlist;
  http->blocking = 1;
  http->fd       = -1;
#ifdef HAVE_GSSAPI
  http->gssctx   = GSS_C_NO_CONTEXT;
  http->gssname  = GSS_C_NO_NAME;
#endif /* HAVE_GSSAPI */
  http->version  = HTTP_1_1;

  strlcpy(http->hostname, host, sizeof(http->hostname));

  if (port == 443)			/* Always use encryption for https */
    http->encryption = HTTP_ENCRYPT_ALWAYS;
  else
    http->encryption = encryption;

 /*
  * Return the new structure...
  */

  return (http);
}


/*
 * 'httpDelete()' - Send a DELETE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpDelete(http_t     *http,		/* I - Connection to server */
           const char *uri)		/* I - URI to delete */
{
  return (http_send(http, HTTP_DELETE, uri));
}


/*
 * 'httpEncryption()' - Set the required encryption on the link.
 */

int					/* O - -1 on error, 0 on success */
httpEncryption(http_t            *http,	/* I - Connection to server */
               http_encryption_t e)	/* I - New encryption preference */
{
  DEBUG_printf(("httpEncryption(http=%p, e=%d)", http, e));

#ifdef HAVE_SSL
  if (!http)
    return (0);

  http->encryption = e;

  if ((http->encryption == HTTP_ENCRYPT_ALWAYS && !http->tls) ||
      (http->encryption == HTTP_ENCRYPT_NEVER && http->tls))
    return (httpReconnect(http));
  else if (http->encryption == HTTP_ENCRYPT_REQUIRED && !http->tls)
    return (http_upgrade(http));
  else
    return (0);
#else
  if (e == HTTP_ENCRYPT_ALWAYS || e == HTTP_ENCRYPT_REQUIRED)
    return (-1);
  else
    return (0);
#endif /* HAVE_SSL */
}


/*
 * 'httpError()' - Get the last error on a connection.
 */

int					/* O - Error code (errno) value */
httpError(http_t *http)			/* I - Connection to server */
{
  if (http)
    return (http->error);
  else
    return (EINVAL);
}


/*
 * 'httpFlush()' - Flush data from a HTTP connection.
 */

void
httpFlush(http_t *http)			/* I - Connection to server */
{
  char		buffer[8192];		/* Junk buffer */
  int		blocking;		/* To block or not to block */
  http_state_t	oldstate;		/* Old state */


  DEBUG_printf(("httpFlush(http=%p), state=%s", http,
                http_states[http->state]));

 /*
  * Temporarily set non-blocking mode so we don't get stuck in httpRead()...
  */

  blocking = http->blocking;
  http->blocking = 0;

 /*
  * Read any data we can...
  */

  oldstate = http->state;
  while (httpRead2(http, buffer, sizeof(buffer)) > 0);

 /*
  * Restore blocking and reset the connection if we didn't get all of
  * the remaining data...
  */

  http->blocking = blocking;

  if (http->state == oldstate && http->state != HTTP_WAITING && http->fd >= 0)
  {
   /*
    * Didn't get the data back, so close the current connection.
    */

    http->state = HTTP_WAITING;

#ifdef HAVE_SSL
    if (http->tls)
      http_shutdown_ssl(http);
#endif /* HAVE_SSL */

#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif /* WIN32 */

    http->fd = -1;
  }
}


/*
 * 'httpFlushWrite()' - Flush data in write buffer.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

int					/* O - Bytes written or -1 on error */
httpFlushWrite(http_t *http)		/* I - Connection to server */
{
  int	bytes;				/* Bytes written */


  DEBUG_printf(("httpFlushWrite(http=%p)", http));

  if (!http || !http->wused)
    return (0);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED)
    bytes = http_write_chunk(http, http->wbuffer, http->wused);
  else
    bytes = http_write(http, http->wbuffer, http->wused);

  http->wused = 0;

  return (bytes);
}


/*
 * 'httpGet()' - Send a GET request to the server.
 */

int					/* O - Status of call (0 = success) */
httpGet(http_t     *http,		/* I - Connection to server */
        const char *uri)		/* I - URI to get */
{
  return (http_send(http, HTTP_GET, uri));
}


/*
 * 'httpGetAuthString()' - Get the current authorization string.
 *
 * The authorization string is set by cupsDoAuthentication() and
 * httpSetAuthString().  Use httpGetAuthString() to retrieve the
 * string to use with httpSetField() for the HTTP_FIELD_AUTHORIZATION
 * value.
 *
 * @since CUPS 1.3/Mac OS X 10.5@
 */

char *					/* O - Authorization string */
httpGetAuthString(http_t *http)		/* I - Connection to server */
{
  if (http)
    return (http->authstring);
  else
    return (NULL);
}


/*
 * 'httpGetBlocking()' - Get the blocking/non-block state of a connection.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

int					/* O - 1 if blocking, 0 if non-blocking */
httpGetBlocking(http_t *http)		/* I - Connection to server */
{
  return (http ? http->blocking : 0);
}


/*
 * 'httpGetCookie()' - Get any cookie data from the response.
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

const char *				/* O - Cookie data or NULL */
httpGetCookie(http_t *http)		/* I - HTTP connecion */
{
  return (http ? http->cookie : NULL);
}


/*
 * 'httpGetFd()' - Get the file descriptor associated with a connection.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

int					/* O - File descriptor or -1 if none */
httpGetFd(http_t *http)			/* I - Connection to server */
{
  return (http ? http->fd : -1);
}


/*
 * 'httpGetField()' - Get a field value from a request/response.
 */

const char *				/* O - Field value */
httpGetField(http_t       *http,	/* I - Connection to server */
             http_field_t field)	/* I - Field to get */
{
  if (!http || field <= HTTP_FIELD_UNKNOWN || field >= HTTP_FIELD_MAX)
    return (NULL);
  else if (field == HTTP_FIELD_AUTHORIZATION &&
	   http->field_authorization)
  {
   /*
    * Special case for WWW-Authenticate: as its contents can be
    * longer than HTTP_MAX_VALUE...
    */

    return (http->field_authorization);
  }
  else
    return (http->fields[field]);
}


/*
 * 'httpGetLength()' - Get the amount of data remaining from the
 *                     content-length or transfer-encoding fields.
 *
 * This function is deprecated and will not return lengths larger than
 * 2^31 - 1; use httpGetLength2() instead.
 *
 * @deprecated@
 */

int					/* O - Content length */
httpGetLength(http_t *http)		/* I - Connection to server */
{
 /*
  * Get the read content length and return the 32-bit value.
  */

  if (http)
  {
    httpGetLength2(http);

    return (http->_data_remaining);
  }
  else
    return (-1);
}


/*
 * 'httpGetLength2()' - Get the amount of data remaining from the
 *                      content-length or transfer-encoding fields.
 *
 * This function returns the complete content length, even for
 * content larger than 2^31 - 1.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

off_t					/* O - Content length */
httpGetLength2(http_t *http)		/* I - Connection to server */
{
  DEBUG_printf(("2httpGetLength2(http=%p), state=%s", http,
                http_states[http->state]));

  if (!http)
    return (-1);

  if (!strcasecmp(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked"))
  {
    DEBUG_puts("4httpGetLength2: chunked request!");

    http->data_encoding  = HTTP_ENCODE_CHUNKED;
    http->data_remaining = 0;
  }
  else
  {
    http->data_encoding = HTTP_ENCODE_LENGTH;

   /*
    * The following is a hack for HTTP servers that don't send a
    * content-length or transfer-encoding field...
    *
    * If there is no content-length then the connection must close
    * after the transfer is complete...
    */

    if (!http->fields[HTTP_FIELD_CONTENT_LENGTH][0])
    {
     /*
      * Default content length is 0 for errors and 2^31-1 for other
      * successful requests...
      */

      if (http->status >= HTTP_MULTIPLE_CHOICES)
        http->data_remaining = 0;
      else
        http->data_remaining = 2147483647;
    }
    else
      http->data_remaining = strtoll(http->fields[HTTP_FIELD_CONTENT_LENGTH],
                                     NULL, 10);

    DEBUG_printf(("4httpGetLength2: content_length=" CUPS_LLFMT,
                  CUPS_LLCAST http->data_remaining));
  }

  if (http->data_remaining <= INT_MAX)
    http->_data_remaining = (int)http->data_remaining;
  else
    http->_data_remaining = INT_MAX;

  return (http->data_remaining);
}


/*
 * 'httpGetStatus()' - Get the status of the last HTTP request.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

http_status_t				/* O - HTTP status */
httpGetStatus(http_t *http)		/* I - Connection to server */
{
  return (http ? http->status : HTTP_ERROR);
}


/*
 * 'httpGetSubField()' - Get a sub-field value.
 *
 * @deprecated@
 */

char *					/* O - Value or NULL */
httpGetSubField(http_t       *http,	/* I - Connection to server */
                http_field_t field,	/* I - Field index */
                const char   *name,	/* I - Name of sub-field */
		char         *value)	/* O - Value string */
{
  return (httpGetSubField2(http, field, name, value, HTTP_MAX_VALUE));
}


/*
 * 'httpGetSubField2()' - Get a sub-field value.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

char *					/* O - Value or NULL */
httpGetSubField2(http_t       *http,	/* I - Connection to server */
                 http_field_t field,	/* I - Field index */
                 const char   *name,	/* I - Name of sub-field */
		 char         *value,	/* O - Value string */
		 int          valuelen)	/* I - Size of value buffer */
{
  const char	*fptr;			/* Pointer into field */
  char		temp[HTTP_MAX_VALUE],	/* Temporary buffer for name */
		*ptr,			/* Pointer into string buffer */
		*end;			/* End of value buffer */

  DEBUG_printf(("2httpGetSubField2(http=%p, field=%d, name=\"%s\", value=%p, "
                "valuelen=%d)", http, field, name, value, valuelen));

  if (!http || !name || !value || valuelen < 2 ||
      field <= HTTP_FIELD_UNKNOWN || field >= HTTP_FIELD_MAX)
    return (NULL);

  end = value + valuelen - 1;

  for (fptr = http->fields[field]; *fptr;)
  {
   /*
    * Skip leading whitespace...
    */

    while (_cups_isspace(*fptr))
      fptr ++;

    if (*fptr == ',')
    {
      fptr ++;
      continue;
    }

   /*
    * Get the sub-field name...
    */

    for (ptr = temp;
         *fptr && *fptr != '=' && !_cups_isspace(*fptr) &&
	     ptr < (temp + sizeof(temp) - 1);
         *ptr++ = *fptr++);

    *ptr = '\0';

    DEBUG_printf(("4httpGetSubField2: name=\"%s\"", temp));

   /*
    * Skip trailing chars up to the '='...
    */

    while (_cups_isspace(*fptr))
      fptr ++;

    if (!*fptr)
      break;

    if (*fptr != '=')
      continue;

   /*
    * Skip = and leading whitespace...
    */

    fptr ++;

    while (_cups_isspace(*fptr))
      fptr ++;

    if (*fptr == '\"')
    {
     /*
      * Read quoted string...
      */

      for (ptr = value, fptr ++;
           *fptr && *fptr != '\"' && ptr < end;
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && *fptr != '\"')
        fptr ++;

      if (*fptr)
        fptr ++;
    }
    else
    {
     /*
      * Read unquoted string...
      */

      for (ptr = value;
           *fptr && !_cups_isspace(*fptr) && *fptr != ',' && ptr < end;
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && !_cups_isspace(*fptr) && *fptr != ',')
        fptr ++;
    }

    DEBUG_printf(("4httpGetSubField2: value=\"%s\"", value));

   /*
    * See if this is the one...
    */

    if (!strcmp(name, temp))
    {
      DEBUG_printf(("3httpGetSubField2: Returning \"%s\"", value));
      return (value);
    }
  }

  value[0] = '\0';

  DEBUG_puts("3httpGetSubField2: Returning NULL");

  return (NULL);
}


/*
 * 'httpGets()' - Get a line of text from a HTTP connection.
 */

char *					/* O - Line or NULL */
httpGets(char   *line,			/* I - Line to read into */
         int    length,			/* I - Max length of buffer */
	 http_t *http)			/* I - Connection to server */
{
  char	*lineptr,			/* Pointer into line */
	*lineend,			/* End of line */
	*bufptr,			/* Pointer into input buffer */
	*bufend;			/* Pointer to end of buffer */
  int	bytes,				/* Number of bytes read */
	eol;				/* End-of-line? */


  DEBUG_printf(("2httpGets(line=%p, length=%d, http=%p)", line, length, http));

  if (http == NULL || line == NULL)
    return (NULL);

 /*
  * Read a line from the buffer...
  */

  http->error = 0;
  lineptr     = line;
  lineend     = line + length - 1;
  eol         = 0;

  while (lineptr < lineend)
  {
   /*
    * Pre-load the buffer as needed...
    */

#ifdef WIN32
    WSASetLastError(0);
#else
    errno = 0;
#endif /* WIN32 */

    while (http->used == 0)
    {
     /*
      * No newline; see if there is more data to be read...
      */

      if (!http->blocking && !_httpWait(http, 10000, 1))
      {
        DEBUG_puts("3httpGets: Timed out!");
#ifdef WIN32
        http->error = WSAETIMEDOUT;
#else
        http->error = ETIMEDOUT;
#endif /* WIN32 */
        return (NULL);
      }

#ifdef HAVE_SSL
      if (http->tls)
	bytes = http_read_ssl(http, http->buffer + http->used,
	                      HTTP_MAX_BUFFER - http->used);
      else
#endif /* HAVE_SSL */
        bytes = recv(http->fd, http->buffer + http->used,
	             HTTP_MAX_BUFFER - http->used, 0);

      DEBUG_printf(("4httpGets: read %d bytes...", bytes));

      if (bytes < 0)
      {
       /*
	* Nope, can't get a line this time...
	*/

#ifdef WIN32
        if (WSAGetLastError() != http->error)
	{
	  http->error = WSAGetLastError();
	  continue;
	}

        DEBUG_printf(("3httpGets: recv() error %d!", WSAGetLastError()));
#else
        DEBUG_printf(("3httpGets: recv() error %d!", errno));

        if (errno == EINTR || errno == EAGAIN)
	  continue;
	else if (errno != http->error)
	{
	  http->error = errno;
	  continue;
	}
#endif /* WIN32 */

        return (NULL);
      }
      else if (bytes == 0)
      {
	http->error = EPIPE;

        return (NULL);
      }

     /*
      * Yup, update the amount used...
      */

      http->used += bytes;
    }

   /*
    * Now copy as much of the current line as possible...
    */

    for (bufptr = http->buffer, bufend = http->buffer + http->used;
         lineptr < lineend && bufptr < bufend;)
    {
      if (*bufptr == 0x0a)
      {
        eol = 1;
	bufptr ++;
	break;
      }
      else if (*bufptr == 0x0d)
	bufptr ++;
      else
	*lineptr++ = *bufptr++;
    }

    http->used -= (int)(bufptr - http->buffer);
    if (http->used > 0)
      memmove(http->buffer, bufptr, http->used);

    if (eol)
    {
     /*
      * End of line...
      */

      http->activity = time(NULL);

      *lineptr = '\0';

      DEBUG_printf(("3httpGets: Returning \"%s\"", line));

      return (line);
    }
  }

  DEBUG_puts("3httpGets: No new line available!");

  return (NULL);
}


/*
 * 'httpHead()' - Send a HEAD request to the server.
 */

int					/* O - Status of call (0 = success) */
httpHead(http_t     *http,		/* I - Connection to server */
         const char *uri)		/* I - URI for head */
{
  DEBUG_printf(("httpHead(http=%p, uri=\"%s\")", http, uri));
  return (http_send(http, HTTP_HEAD, uri));
}


/*
 * 'httpInitialize()' - Initialize the HTTP interface library and set the
 *                      default HTTP proxy (if any).
 */

void
httpInitialize(void)
{
  static int	initialized = 0;	/* Have we been called before? */
#ifdef WIN32
  WSADATA	winsockdata;		/* WinSock data */
#endif /* WIN32 */
#ifdef HAVE_LIBSSL
  int		i;			/* Looping var */
  unsigned char	data[1024];		/* Seed data */
#endif /* HAVE_LIBSSL */


  if (initialized)
    return;

#ifdef WIN32
  WSAStartup(MAKEWORD(2,2), &winsockdata);

#elif !defined(SO_NOSIGPIPE)
 /*
  * Ignore SIGPIPE signals...
  */

#  ifdef HAVE_SIGSET
  sigset(SIGPIPE, SIG_IGN);

#  elif defined(HAVE_SIGACTION)
  struct sigaction	action;		/* POSIX sigaction data */


  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);

#  else
  signal(SIGPIPE, SIG_IGN);
#  endif /* !SO_NOSIGPIPE */
#endif /* WIN32 */

#ifdef HAVE_GNUTLS
 /*
  * Make sure we handle threading properly...
  */

#  ifdef HAVE_PTHREAD_H
  gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
#  endif /* HAVE_PTHREAD_H */

 /*
  * Initialize GNU TLS...
  */

  gnutls_global_init();

#elif defined(HAVE_LIBSSL)
 /*
  * Initialize OpenSSL...
  */

  SSL_load_error_strings();
  SSL_library_init();

 /*
  * Set the threading callbacks...
  */

#  ifdef HAVE_PTHREAD_H
  http_locks = calloc(CRYPTO_num_locks(), sizeof(pthread_mutex_t));

  for (i = 0; i < CRYPTO_num_locks(); i ++)
    pthread_mutex_init(http_locks + i, NULL);

  CRYPTO_set_id_callback(http_threadid_cb);
  CRYPTO_set_locking_callback(http_locking_cb);
#  endif /* HAVE_PTHREAD_H */

 /*
  * Using the current time is a dubious random seed, but on some systems
  * it is the best we can do (on others, this seed isn't even used...)
  */

  CUPS_SRAND(time(NULL));

  for (i = 0; i < sizeof(data); i ++)
    data[i] = CUPS_RAND();

  RAND_seed(data, sizeof(data));
#endif /* HAVE_GNUTLS */

  initialized = 1;
}


/*
 * 'httpOptions()' - Send an OPTIONS request to the server.
 */

int					/* O - Status of call (0 = success) */
httpOptions(http_t     *http,		/* I - Connection to server */
            const char *uri)		/* I - URI for options */
{
  return (http_send(http, HTTP_OPTIONS, uri));
}


/*
 * 'httpPost()' - Send a POST request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPost(http_t     *http,		/* I - Connection to server */
         const char *uri)		/* I - URI for post */
{
  return (http_send(http, HTTP_POST, uri));
}


/*
 * 'httpPrintf()' - Print a formatted string to a HTTP connection.
 *
 * @private@
 */

int					/* O - Number of bytes written */
httpPrintf(http_t     *http,		/* I - Connection to server */
           const char *format,		/* I - printf-style format string */
	   ...)				/* I - Additional args as needed */
{
  int		bytes;			/* Number of bytes to write */
  char		buf[16384];		/* Buffer for formatted string */
  va_list	ap;			/* Variable argument pointer */


  DEBUG_printf(("2httpPrintf(http=%p, format=\"%s\", ...)", http, format));

  va_start(ap, format);
  bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  DEBUG_printf(("3httpPrintf: %s", buf));

  if (http->data_encoding == HTTP_ENCODE_FIELDS)
    return (httpWrite2(http, buf, bytes));
  else
  {
    if (http->wused)
    {
      DEBUG_puts("4httpPrintf: flushing existing data...");

      if (httpFlushWrite(http) < 0)
	return (-1);
    }

    return (http_write(http, buf, bytes));
  }
}


/*
 * 'httpPut()' - Send a PUT request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPut(http_t     *http,		/* I - Connection to server */
        const char *uri)		/* I - URI to put */
{
  DEBUG_printf(("httpPut(http=%p, uri=\"%s\")", http, uri));
  return (http_send(http, HTTP_PUT, uri));
}


/*
 * 'httpRead()' - Read data from a HTTP connection.
 *
 * This function is deprecated. Use the httpRead2() function which can
 * read more than 2GB of data.
 *
 * @deprecated@
 */

int					/* O - Number of bytes read */
httpRead(http_t *http,			/* I - Connection to server */
         char   *buffer,		/* I - Buffer for data */
	 int    length)			/* I - Maximum number of bytes */
{
  return ((int)httpRead2(http, buffer, length));
}


/*
 * 'httpRead2()' - Read data from a HTTP connection.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ssize_t					/* O - Number of bytes read */
httpRead2(http_t *http,			/* I - Connection to server */
          char   *buffer,		/* I - Buffer for data */
	  size_t length)		/* I - Maximum number of bytes */
{
  ssize_t	bytes;			/* Bytes read */
  char		len[32];		/* Length string */


  DEBUG_printf(("httpRead2(http=%p, buffer=%p, length=" CUPS_LLFMT ")",
                http, buffer, CUPS_LLCAST length));

  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);
  http->error    = 0;

  if (length <= 0)
    return (0);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED &&
      http->data_remaining <= 0)
  {
    DEBUG_puts("2httpRead2: Getting chunk length...");

    if (httpGets(len, sizeof(len), http) == NULL)
    {
      DEBUG_puts("1httpRead2: Could not get length!");
      return (0);
    }

    http->data_remaining = strtoll(len, NULL, 16);
    if (http->data_remaining < 0)
    {
      DEBUG_puts("1httpRead2: Negative chunk length!");
      return (0);
    }
  }

  DEBUG_printf(("2httpRead2: data_remaining=" CUPS_LLFMT,
                CUPS_LLCAST http->data_remaining));

  if (http->data_remaining <= 0)
  {
   /*
    * A zero-length chunk ends a transfer; unless we are reading POST
    * data, go idle...
    */

    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
      httpGets(len, sizeof(len), http);

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;

   /*
    * Prevent future reads for this request...
    */

    http->data_encoding = HTTP_ENCODE_LENGTH;

    return (0);
  }
  else if (length > (size_t)http->data_remaining)
    length = (size_t)http->data_remaining;

  if (http->used == 0 && length <= 256)
  {
   /*
    * Buffer small reads for better performance...
    */

    if (!http->blocking && !httpWait(http, 10000))
      return (0);

    if (http->data_remaining > sizeof(http->buffer))
      bytes = sizeof(http->buffer);
    else
      bytes = http->data_remaining;

#ifdef HAVE_SSL
    if (http->tls)
      bytes = http_read_ssl(http, http->buffer, bytes);
    else
#endif /* HAVE_SSL */
    {
      DEBUG_printf(("2httpRead2: reading %d bytes from socket into buffer...",
                    (int)bytes));

      bytes = recv(http->fd, http->buffer, bytes, 0);

      DEBUG_printf(("2httpRead2: read %d bytes from socket into buffer...",
                    (int)bytes));
    }

    if (bytes > 0)
      http->used = bytes;
    else if (bytes < 0)
    {
#ifdef WIN32
      http->error = WSAGetLastError();
      return (-1);
#else
      if (errno != EINTR && errno != EAGAIN)
      {
        http->error = errno;
        return (-1);
      }
#endif /* WIN32 */
    }
    else
    {
      http->error = EPIPE;
      return (0);
    }
  }

  if (http->used > 0)
  {
    if (length > (size_t)http->used)
      length = (size_t)http->used;

    bytes = (ssize_t)length;

    DEBUG_printf(("2httpRead2: grabbing %d bytes from input buffer...",
                  (int)bytes));

    memcpy(buffer, http->buffer, length);
    http->used -= (int)length;

    if (http->used > 0)
      memmove(http->buffer, http->buffer + length, http->used);
  }
#ifdef HAVE_SSL
  else if (http->tls)
  {
    if (!http->blocking && !httpWait(http, 10000))
      return (0);

    bytes = (ssize_t)http_read_ssl(http, buffer, (int)length);
  }
#endif /* HAVE_SSL */
  else
  {
    if (!http->blocking && !httpWait(http, 10000))
      return (0);

    DEBUG_printf(("2httpRead2: reading " CUPS_LLFMT " bytes from socket...",
                  CUPS_LLCAST length));

#ifdef WIN32
    bytes = (ssize_t)recv(http->fd, buffer, (int)length, 0);
#else
    while ((bytes = recv(http->fd, buffer, length, 0)) < 0)
      if (errno != EINTR && errno != EAGAIN)
        break;
#endif /* WIN32 */

    DEBUG_printf(("2httpRead2: read " CUPS_LLFMT " bytes from socket...",
                  CUPS_LLCAST bytes));
  }

  if (bytes > 0)
  {
    http->data_remaining -= bytes;

    if (http->data_remaining <= INT_MAX)
      http->_data_remaining = (int)http->data_remaining;
    else
      http->_data_remaining = INT_MAX;
  }
  else if (bytes < 0)
  {
#ifdef WIN32
    http->error = WSAGetLastError();
#else
    if (errno == EINTR || errno == EAGAIN)
      bytes = 0;
    else
      http->error = errno;
#endif /* WIN32 */
  }
  else
  {
    http->error = EPIPE;
    return (0);
  }

  if (http->data_remaining == 0)
  {
    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
      httpGets(len, sizeof(len), http);

    if (http->data_encoding != HTTP_ENCODE_CHUNKED)
    {
      if (http->state == HTTP_POST_RECV)
	http->state ++;
      else
	http->state = HTTP_WAITING;
    }
  }

#ifdef DEBUG
  http_debug_hex("httpRead2", buffer, (int)bytes);
#endif /* DEBUG */

  return (bytes);
}


#if defined(HAVE_SSL) && defined(HAVE_CDSASSL)
/*
 * '_httpReadCDSA()' - Read function for the CDSA library.
 */

OSStatus				/* O  - -1 on error, 0 on success */
_httpReadCDSA(
    SSLConnectionRef connection,	/* I  - SSL/TLS connection */
    void             *data,		/* I  - Data buffer */
    size_t           *dataLength)	/* IO - Number of bytes */
{
  OSStatus	result;			/* Return value */
  ssize_t	bytes;			/* Number of bytes read */
  http_t	*http;			/* HTTP connection */


  http = (http_t *)connection;

  if (!http->blocking)
  {
   /*
    * Make sure we have data before we read...
    */

    if (!_httpWait(http, 10000, 0))
    {
      http->error = ETIMEDOUT;
      return (-1);
    }
  }

  do
  {
    bytes = recv(http->fd, data, *dataLength, 0);
  }
  while (bytes == -1 && (errno == EINTR || errno == EAGAIN));

  if (bytes == *dataLength)
  {
    result = 0;
  }
  else if (bytes > 0)
  {
    *dataLength = bytes;
    result = errSSLWouldBlock;
  }
  else
  {
    *dataLength = 0;

    if (bytes == 0)
      result = errSSLClosedGraceful;
    else if (errno == EAGAIN)
      result = errSSLWouldBlock;
    else
      result = errSSLClosedAbort;
  }

  return (result);
}
#endif /* HAVE_SSL && HAVE_CDSASSL */


#if defined(HAVE_SSL) && defined(HAVE_GNUTLS)
/*
 * '_httpReadGNUTLS()' - Read function for the GNU TLS library.
 */

ssize_t					/* O - Number of bytes read or -1 on error */
_httpReadGNUTLS(
    gnutls_transport_ptr ptr,		/* I - Connection to server */
    void                 *data,		/* I - Buffer */
    size_t               length)	/* I - Number of bytes to read */
{
  http_t	*http;			/* HTTP connection */


  http = (http_t *)ptr;

  if (!http->blocking)
  {
   /*
    * Make sure we have data before we read...
    */

    if (!_httpWait(http, 10000, 0))
    {
      http->error = ETIMEDOUT;
      return (-1);
    }
  }

  return (recv(http->fd, data, length, 0));
}
#endif /* HAVE_SSL && HAVE_GNUTLS */


/*
 * 'httpReconnect()' - Reconnect to a HTTP server.
 */

int					/* O - 0 on success, non-zero on failure */
httpReconnect(http_t *http)		/* I - Connection to server */
{
  http_addrlist_t	*addr;		/* Connected address */
#ifdef DEBUG
  http_addrlist_t	*current;	/* Current address */
  char			temp[256];	/* Temporary address string */
#endif /* DEBUG */


  DEBUG_printf(("httpReconnect(http=%p)", http));

  if (!http)
    return (-1);

#ifdef HAVE_SSL
  if (http->tls)
  {
    DEBUG_puts("2httpReconnect: Shutting down SSL/TLS...");
    http_shutdown_ssl(http);
  }
#endif /* HAVE_SSL */

 /*
  * Close any previously open socket...
  */

  if (http->fd >= 0)
  {
    DEBUG_printf(("2httpReconnect: Closing socket %d...", http->fd));

#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif /* WIN32 */

    http->fd = -1;
  }

 /*
  * Connect to the server...
  */

#ifdef DEBUG
  for (current = http->addrlist; current; current = current->next)
    DEBUG_printf(("2httpReconnect: Address %s:%d",
                  httpAddrString(&(current->addr), temp, sizeof(temp)),
                  _httpAddrPort(&(current->addr))));
#endif /* DEBUG */

  if ((addr = httpAddrConnect(http->addrlist, &(http->fd))) == NULL)
  {
   /*
    * Unable to connect...
    */

#ifdef WIN32
    http->error  = WSAGetLastError();
#else
    http->error  = errno;
#endif /* WIN32 */
    http->status = HTTP_ERROR;

    DEBUG_printf(("1httpReconnect: httpAddrConnect failed: %s",
                  strerror(http->error)));

    return (-1);
  }

  DEBUG_printf(("2httpReconnect: New socket=%d", http->fd));

  http->hostaddr = &(addr->addr);
  http->error    = 0;
  http->status   = HTTP_CONTINUE;

#ifdef HAVE_SSL
  if (http->encryption == HTTP_ENCRYPT_ALWAYS)
  {
   /*
    * Always do encryption via SSL.
    */

    if (http_setup_ssl(http) != 0)
    {
#  ifdef WIN32
      closesocket(http->fd);
#  else
      close(http->fd);
#  endif /* WIN32 */

      return (-1);
    }
  }
  else if (http->encryption == HTTP_ENCRYPT_REQUIRED)
    return (http_upgrade(http));
#endif /* HAVE_SSL */

  DEBUG_printf(("1httpReconnect: Connected to %s:%d...",
		httpAddrString(http->hostaddr, temp, sizeof(temp)),
		_httpAddrPort(http->hostaddr)));

  return (0);
}


/*
 * 'httpSetAuthString()' - Set the current authorization string.
 *
 * This function just stores a copy of the current authorization string in
 * the HTTP connection object.  You must still call httpSetField() to set
 * HTTP_FIELD_AUTHORIZATION prior to issuing a HTTP request using httpGet(),
 * httpHead(), httpOptions(), httpPost, or httpPut().
 *
 * @since CUPS 1.3/Mac OS X 10.5@
 */

void
httpSetAuthString(http_t     *http,	/* I - Connection to server */
                  const char *scheme,	/* I - Auth scheme (NULL to clear it) */
		  const char *data)	/* I - Auth data (NULL for none) */
{
 /*
  * Range check input...
  */

  if (!http)
    return;

  if (http->authstring && http->authstring != http->_authstring)
    free(http->authstring);

  http->authstring = http->_authstring;

  if (scheme)
  {
   /*
    * Set the current authorization string...
    */

    int len = (int)strlen(scheme) + (data ? (int)strlen(data) + 1 : 0) + 1;
    char *temp;

    if (len > (int)sizeof(http->_authstring))
    {
      if ((temp = malloc(len)) == NULL)
        len = sizeof(http->_authstring);
      else
        http->authstring = temp;
    }

    if (data)
      snprintf(http->authstring, len, "%s %s", scheme, data);
    else
      strlcpy(http->authstring, scheme, len);
  }
  else
  {
   /*
    * Clear the current authorization string...
    */

    http->_authstring[0] = '\0';
  }
}


/*
 * 'httpSetCookie()' - Set the cookie value(s).
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

void
httpSetCookie(http_t     *http,		/* I - Connection */
              const char *cookie)	/* I - Cookie string */
{
  if (!http)
    return;

  if (http->cookie)
    free(http->cookie);

  if (cookie)
    http->cookie = strdup(cookie);
  else
    http->cookie = NULL;
}


/*
 * 'httpSetExpect()' - Set the Expect: header in a request.
 *
 * Currently only HTTP_CONTINUE is supported for the "expect" argument.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

void
httpSetExpect(http_t        *http,	/* I - Connection to server */
              http_status_t expect)	/* I - HTTP status to expect (HTTP_CONTINUE) */
{
  if (http)
    http->expect = expect;
}


/*
 * 'httpSetField()' - Set the value of an HTTP header.
 */

void
httpSetField(http_t       *http,	/* I - Connection to server */
             http_field_t field,	/* I - Field index */
	     const char   *value)	/* I - Value */
{
  if (http == NULL ||
      field < HTTP_FIELD_ACCEPT_LANGUAGE ||
      field > HTTP_FIELD_WWW_AUTHENTICATE ||
      value == NULL)
    return;

  strlcpy(http->fields[field], value, HTTP_MAX_VALUE);

  if (field == HTTP_FIELD_AUTHORIZATION)
  {
   /*
    * Special case for Authorization: as its contents can be
    * longer than HTTP_MAX_VALUE
    */

    if (http->field_authorization)
      free(http->field_authorization);

    http->field_authorization = strdup(value);
  }
  else if (field == HTTP_FIELD_HOST)
  {
   /*
    * Special-case for Host: as we don't want a trailing "." on the hostname and
    * need to bracket IPv6 numeric addresses.
    */

    char *ptr = strchr(value, ':');

    if (value[0] != '[' && ptr && strchr(ptr + 1, ':'))
    {
     /*
      * Bracket IPv6 numeric addresses...
      *
      * This is slightly inefficient (basically copying twice), but is an edge
      * case and not worth optimizing...
      */

      snprintf(http->fields[HTTP_FIELD_HOST],
               sizeof(http->fields[HTTP_FIELD_HOST]), "[%s]", value);
    }
    else
    {
     /*
      * Check for a trailing dot on the hostname...
      */

      ptr = http->fields[HTTP_FIELD_HOST];

      if (*ptr)
      {
	ptr += strlen(ptr) - 1;

	if (*ptr == '.')
	  *ptr = '\0';
      }
    }
  }
}


/*
 * 'httpSetLength()' - Set the content-length and content-encoding.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

void
httpSetLength(http_t *http,		/* I - Connection to server */
              size_t length)		/* I - Length (0 for chunked) */
{
  if (!http)
    return;

  if (!length)
  {
    strcpy(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked");
    http->fields[HTTP_FIELD_CONTENT_LENGTH][0] = '\0';
  }
  else
  {
    http->fields[HTTP_FIELD_TRANSFER_ENCODING][0] = '\0';
    snprintf(http->fields[HTTP_FIELD_CONTENT_LENGTH], HTTP_MAX_VALUE,
             CUPS_LLFMT, CUPS_LLCAST length);
  }
}


/*
 * 'httpTrace()' - Send an TRACE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpTrace(http_t     *http,		/* I - Connection to server */
          const char *uri)		/* I - URI for trace */
{
  return (http_send(http, HTTP_TRACE, uri));
}


/*
 * 'httpUpdate()' - Update the current HTTP state for incoming data.
 */

http_status_t				/* O - HTTP status */
httpUpdate(http_t *http)		/* I - Connection to server */
{
  char		line[32768],		/* Line from connection... */
		*value;			/* Pointer to value on line */
  http_field_t	field;			/* Field index */
  int		major, minor,		/* HTTP version numbers */
		status;			/* Request status */


  DEBUG_printf(("httpUpdate(http=%p), state=%s", http,
                http_states[http->state]));

 /*
  * Flush pending data, if any...
  */

  if (http->wused)
  {
    DEBUG_puts("2httpUpdate: flushing buffer...");

    if (httpFlushWrite(http) < 0)
      return (HTTP_ERROR);
  }

 /*
  * If we haven't issued any commands, then there is nothing to "update"...
  */

  if (http->state == HTTP_WAITING)
    return (HTTP_CONTINUE);

 /*
  * Grab all of the lines we can from the connection...
  */

  while (httpGets(line, sizeof(line), http) != NULL)
  {
    DEBUG_printf(("2httpUpdate: Got \"%s\"", line));

    if (line[0] == '\0')
    {
     /*
      * Blank line means the start of the data section (if any).  Return
      * the result code, too...
      *
      * If we get status 100 (HTTP_CONTINUE), then we *don't* change states.
      * Instead, we just return HTTP_CONTINUE to the caller and keep on
      * tryin'...
      */

      if (http->status == HTTP_CONTINUE)
        return (http->status);

      if (http->status < HTTP_BAD_REQUEST)
        http->digest_tries = 0;

#ifdef HAVE_SSL
      if (http->status == HTTP_SWITCHING_PROTOCOLS && !http->tls)
      {
	if (http_setup_ssl(http) != 0)
	{
#  ifdef WIN32
	  closesocket(http->fd);
#  else
	  close(http->fd);
#  endif /* WIN32 */

	  return (HTTP_ERROR);
	}

        return (HTTP_CONTINUE);
      }
#endif /* HAVE_SSL */

      httpGetLength2(http);

      switch (http->state)
      {
        case HTTP_GET :
	case HTTP_POST :
	case HTTP_POST_RECV :
	case HTTP_PUT :
	    http->state ++;
	case HTTP_POST_SEND :
	case HTTP_HEAD :
	    break;

	default :
	    http->state = HTTP_WAITING;
	    break;
      }

      return (http->status);
    }
    else if (!strncmp(line, "HTTP/", 5))
    {
     /*
      * Got the beginning of a response...
      */

      if (sscanf(line, "HTTP/%d.%d%d", &major, &minor, &status) != 3)
        return (HTTP_ERROR);

      http->version = (http_version_t)(major * 100 + minor);
      http->status  = (http_status_t)status;
    }
    else if ((value = strchr(line, ':')) != NULL)
    {
     /*
      * Got a value...
      */

      *value++ = '\0';
      while (_cups_isspace(*value))
        value ++;

     /*
      * Be tolerants of servers that send unknown attribute fields...
      */

      if (!strcasecmp(line, "expect"))
      {
       /*
        * "Expect: 100-continue" or similar...
	*/

        http->expect = (http_status_t)atoi(value);
      }
      else if (!strcasecmp(line, "cookie"))
      {
       /*
        * "Cookie: name=value[; name=value ...]" - replaces previous cookies...
	*/

        httpSetCookie(http, value);
      }
      else if ((field = http_field(line)) == HTTP_FIELD_UNKNOWN)
      {
        DEBUG_printf(("1httpUpdate: unknown field %s seen!", line));
        continue;
      }
      else
        httpSetField(http, field, value);
    }
    else
    {
      DEBUG_printf(("1httpUpdate: Bad response line \"%s\"!", line));
      http->status = HTTP_ERROR;
      return (HTTP_ERROR);
    }
  }

 /*
  * See if there was an error...
  */

  if (http->error == EPIPE && http->status > HTTP_CONTINUE)
  {
    DEBUG_printf(("1httpUpdate: Returning status %d...", http->status));
    return (http->status);
  }

  if (http->error)
  {
    DEBUG_printf(("1httpUpdate: socket error %d - %s", http->error,
                  strerror(http->error)));
    http->status = HTTP_ERROR;
    return (HTTP_ERROR);
  }

 /*
  * If we haven't already returned, then there is nothing new...
  */

  return (HTTP_CONTINUE);
}


/*
 * '_httpWait()' - Wait for data available on a connection (no flush).
 */

int					/* O - 1 if data is available, 0 otherwise */
_httpWait(http_t *http,			/* I - Connection to server */
          int    msec,			/* I - Milliseconds to wait */
	  int    usessl)		/* I - Use SSL context? */
{
#ifdef HAVE_POLL
  struct pollfd		pfd;		/* Polled file descriptor */
#else
  fd_set		input_set;	/* select() input set */
  struct timeval	timeout;	/* Timeout */
#endif /* HAVE_POLL */
  int			nfds;		/* Result from select()/poll() */


  DEBUG_printf(("4_httpWait(http=%p, msec=%d, usessl=%d)", http, msec, usessl));

  if (http->fd < 0)
    return (0);

 /*
  * Check the SSL/TLS buffers for data first...
  */

#ifdef HAVE_SSL
  if (http->tls && usessl)
  {
#  ifdef HAVE_LIBSSL
    if (SSL_pending((SSL *)(http->tls)))
      return (1);
#  elif defined(HAVE_GNUTLS)
    if (gnutls_record_check_pending(((http_tls_t *)(http->tls))->session))
      return (1);
#  elif defined(HAVE_CDSASSL)
    size_t bytes;			/* Bytes that are available */

    if (!SSLGetBufferedReadSize(((http_tls_t *)(http->tls))->session, &bytes) &&
        bytes > 0)
      return (1);
#  endif /* HAVE_LIBSSL */
  }
#endif /* HAVE_SSL */

 /*
  * Then try doing a select() or poll() to poll the socket...
  */

#ifdef HAVE_POLL
  pfd.fd     = http->fd;
  pfd.events = POLLIN;

  while ((nfds = poll(&pfd, 1, msec)) < 0 &&
         (errno == EINTR || errno == EAGAIN));

#else
  do
  {
    FD_ZERO(&input_set);
    FD_SET(http->fd, &input_set);

    DEBUG_printf(("6_httpWait: msec=%d, http->fd=%d", msec, http->fd));

    if (msec >= 0)
    {
      timeout.tv_sec  = msec / 1000;
      timeout.tv_usec = (msec % 1000) * 1000;

      nfds = select(http->fd + 1, &input_set, NULL, NULL, &timeout);
    }
    else
      nfds = select(http->fd + 1, &input_set, NULL, NULL, NULL);

    DEBUG_printf(("6_httpWait: select() returned %d...", nfds));
  }
#  ifdef WIN32
  while (nfds < 0 && WSAGetLastError() == WSAEINTR);
#  else
  while (nfds < 0 && (errno == EINTR || errno == EAGAIN));
#  endif /* WIN32 */
#endif /* HAVE_POLL */

  DEBUG_printf(("5_httpWait: returning with nfds=%d...", nfds));

  return (nfds > 0);
}


/*
 * 'httpWait()' - Wait for data available on a connection.
 *
 * @since CUPS 1.1.19/Mac OS X 10.3@
 */

int					/* O - 1 if data is available, 0 otherwise */
httpWait(http_t *http,			/* I - Connection to server */
         int    msec)			/* I - Milliseconds to wait */
{
 /*
  * First see if there is data in the buffer...
  */

  if (http == NULL)
    return (0);

  if (http->used)
    return (1);

 /*
  * Flush pending data, if any...
  */

  if (http->wused)
  {
    if (httpFlushWrite(http) < 0)
      return (0);
  }

 /*
  * If not, check the SSL/TLS buffers and do a select() on the connection...
  */

  return (_httpWait(http, msec, 1));
}


/*
 * 'httpWrite()' - Write data to a HTTP connection.
 *
 * This function is deprecated. Use the httpWrite2() function which can
 * write more than 2GB of data.
 *
 * @deprecated@
 */

int					/* O - Number of bytes written */
httpWrite(http_t     *http,		/* I - Connection to server */
          const char *buffer,		/* I - Buffer for data */
	  int        length)		/* I - Number of bytes to write */
{
  return ((int)httpWrite2(http, buffer, length));
}


/*
 * 'httpWrite2()' - Write data to a HTTP connection.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

ssize_t					/* O - Number of bytes written */
httpWrite2(http_t     *http,		/* I - Connection to server */
           const char *buffer,		/* I - Buffer for data */
	   size_t     length)		/* I - Number of bytes to write */
{
  ssize_t	bytes;			/* Bytes written */


  DEBUG_printf(("httpWrite2(http=%p, buffer=%p, length=" CUPS_LLFMT ")", http,
                buffer, CUPS_LLCAST length));

 /*
  * Range check input...
  */

  if (http == NULL || buffer == NULL)
    return (-1);

 /*
  * Mark activity on the connection...
  */

  http->activity = time(NULL);

 /*
  * Buffer small writes for better performance...
  */

  if (length > 0)
  {
    if (http->wused && (length + http->wused) > sizeof(http->wbuffer))
    {
      DEBUG_printf(("2httpWrite2: Flushing buffer (wused=%d, length="
                    CUPS_LLFMT ")", http->wused, CUPS_LLCAST length));

      httpFlushWrite(http);
    }

    if ((length + http->wused) <= sizeof(http->wbuffer) &&
        length < sizeof(http->wbuffer))
    {
     /*
      * Write to buffer...
      */

      DEBUG_printf(("2httpWrite2: Copying " CUPS_LLFMT " bytes to wbuffer...",
                    CUPS_LLCAST length));

      memcpy(http->wbuffer + http->wused, buffer, length);
      http->wused += (int)length;
      bytes = (ssize_t)length;
    }
    else
    {
     /*
      * Otherwise write the data directly...
      */

      DEBUG_printf(("2httpWrite2: Writing " CUPS_LLFMT " bytes to socket...",
                    CUPS_LLCAST length));

      if (http->data_encoding == HTTP_ENCODE_CHUNKED)
	bytes = (ssize_t)http_write_chunk(http, buffer, (int)length);
      else
	bytes = (ssize_t)http_write(http, buffer, (int)length);

      DEBUG_printf(("2httpWrite2: Wrote " CUPS_LLFMT " bytes...",
                    CUPS_LLCAST bytes));
    }

    if (http->data_encoding == HTTP_ENCODE_LENGTH)
      http->data_remaining -= bytes;
  }
  else
    bytes = 0;

 /*
  * Handle end-of-request processing...
  */

  if ((http->data_encoding == HTTP_ENCODE_CHUNKED && length == 0) ||
      (http->data_encoding == HTTP_ENCODE_LENGTH && http->data_remaining == 0))
  {
   /*
    * Finished with the transfer; unless we are sending POST or PUT
    * data, go idle...
    */

    DEBUG_puts("2httpWrite: changing states...");

    if (http->wused)
      httpFlushWrite(http);

    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
    {
     /*
      * Send a 0-length chunk at the end of the request...
      */

      http_write(http, "0\r\n\r\n", 5);

     /*
      * Reset the data state...
      */

      http->data_encoding  = HTTP_ENCODE_LENGTH;
      http->data_remaining = 0;
    }

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else if (http->state == HTTP_PUT_RECV)
      http->state = HTTP_STATUS;
    else
      http->state = HTTP_WAITING;
  }

  return (bytes);
}


#if defined(HAVE_SSL) && defined(HAVE_CDSASSL)
/*
 * '_httpWriteCDSA()' - Write function for the CDSA library.
 */

OSStatus				/* O  - -1 on error, 0 on success */
_httpWriteCDSA(
    SSLConnectionRef connection,	/* I  - SSL/TLS connection */
    const void       *data,		/* I  - Data buffer */
    size_t           *dataLength)	/* IO - Number of bytes */
{
  OSStatus	result;			/* Return value */
  ssize_t	bytes;			/* Number of bytes read */
  http_t	*http;			/* HTTP connection */


  http = (http_t *)connection;

  do
  {
    bytes = write(http->fd, data, *dataLength);
  }
  while (bytes == -1 && (errno == EINTR || errno == EAGAIN));

  if (bytes == *dataLength)
  {
    result = 0;
  }
  else if (bytes >= 0)
  {
    *dataLength = bytes;
    result = errSSLWouldBlock;
  }
  else
  {
    *dataLength = 0;

    if (errno == EAGAIN)
      result = errSSLWouldBlock;
    else
      result = errSSLClosedAbort;
  }

  return (result);
}
#endif /* HAVE_SSL && HAVE_CDSASSL */


#if defined(HAVE_SSL) && defined(HAVE_GNUTLS)
/*
 * '_httpWriteGNUTLS()' - Write function for the GNU TLS library.
 */

ssize_t					/* O - Number of bytes written or -1 on error */
_httpWriteGNUTLS(
    gnutls_transport_ptr ptr,		/* I - Connection to server */
    const void           *data,		/* I - Data buffer */
    size_t               length)	/* I - Number of bytes to write */
{
  return (send(((http_t *)ptr)->fd, data, length, 0));
}
#endif /* HAVE_SSL && HAVE_GNUTLS */


#if defined(HAVE_SSL) && defined(HAVE_LIBSSL)
/*
 * 'http_bio_ctrl()' - Control the HTTP connection.
 */

static long				/* O - Result/data */
http_bio_ctrl(BIO  *h,			/* I - BIO data */
              int  cmd,			/* I - Control command */
	      long arg1,		/* I - First argument */
	      void *arg2)		/* I - Second argument */
{
  switch (cmd)
  {
    default :
        return (0);

    case BIO_CTRL_RESET :
        h->ptr = NULL;
	return (0);

    case BIO_C_SET_FILE_PTR :
        h->ptr  = arg2;
	h->init = 1;
	return (1);

    case BIO_C_GET_FILE_PTR :
        if (arg2)
	{
	  *((void **)arg2) = h->ptr;
	  return (1);
	}
	else
	  return (0);

    case BIO_CTRL_DUP :
    case BIO_CTRL_FLUSH :
        return (1);
  }
}


/*
 * 'http_bio_free()' - Free OpenSSL data.
 */

static int				/* O - 1 on success, 0 on failure */
http_bio_free(BIO *h)			/* I - BIO data */
{
  if (!h)
    return (0);

  if (h->shutdown)
  {
    h->init  = 0;
    h->flags = 0;
  }

  return (1);
}


/*
 * 'http_bio_new()' - Initialize an OpenSSL BIO structure.
 */

static int				/* O - 1 on success, 0 on failure */
http_bio_new(BIO *h)			/* I - BIO data */
{
  if (!h)
    return (0);

  h->init  = 0;
  h->num   = 0;
  h->ptr   = NULL;
  h->flags = 0;

  return (1);
}


/*
 * 'http_bio_puts()' - Send a string for OpenSSL.
 */

static int				/* O - Bytes written */
http_bio_puts(BIO        *h,		/* I - BIO data */
              const char *str)		/* I - String to write */
{
#ifdef WIN32
  return (send(((http_t *)h->ptr)->fd, str, (int)strlen(str), 0));
#else
  return (send(((http_t *)h->ptr)->fd, str, strlen(str), 0));
#endif /* WIN32 */
}


/*
 * 'http_bio_read()' - Read data for OpenSSL.
 */

static int				/* O - Bytes read */
http_bio_read(BIO  *h,			/* I - BIO data */
              char *buf,		/* I - Buffer */
	      int  size)		/* I - Number of bytes to read */
{
  http_t	*http;			/* HTTP connection */


  http = (http_t *)h->ptr;

  if (!http->blocking)
  {
   /*
    * Make sure we have data before we read...
    */

    if (!_httpWait(http, 10000, 0))
    {
#ifdef WIN32
      http->error = WSAETIMEDOUT;
#else
      http->error = ETIMEDOUT;
#endif /* WIN32 */

      return (-1);
    }
  }

  return (recv(http->fd, buf, size, 0));
}


/*
 * 'http_bio_write()' - Write data for OpenSSL.
 */

static int				/* O - Bytes written */
http_bio_write(BIO        *h,		/* I - BIO data */
               const char *buf,		/* I - Buffer to write */
	       int        num)		/* I - Number of bytes to write */
{
  return (send(((http_t *)h->ptr)->fd, buf, num, 0));
}
#endif /* HAVE_SSL && HAVE_LIBSSL */


#ifdef DEBUG
/*
 * 'http_debug_hex()' - Do a hex dump of a buffer.
 */

static void
http_debug_hex(const char *prefix,	/* I - Prefix for line */
               const char *buffer,	/* I - Buffer to dump */
               int        bytes)	/* I - Bytes to dump */
{
  int	i, j,				/* Looping vars */
	ch;				/* Current character */
  char	line[255],			/* Line buffer */
	*start,				/* Start of line after prefix */
	*ptr;				/* Pointer into line */


  if (_cups_debug_fd < 0 || _cups_debug_level < 6)
    return;

  DEBUG_printf(("6%s: %d bytes:\n", prefix, bytes));

  snprintf(line, sizeof(line), "6%s: ", prefix);
  start = line + strlen(line);

  for (i = 0; i < bytes; i += 16)
  {
    for (j = 0, ptr = start; j < 16 && (i + j) < bytes; j ++, ptr += 2)
      sprintf(ptr, "%02X", buffer[i + j] & 255);

    while (j < 16)
    {
      strcpy(ptr, "  ");
      ptr += 2;
      j ++;
    }

    strcpy(ptr, "  ");
    ptr += 2;

    for (j = 0; j < 16 && (i + j) < bytes; j ++)
    {
      ch = buffer[i + j] & 255;

      if (ch < ' ' || ch >= 127)
	ch = '.';

      *ptr++ = ch;
    }

    *ptr = '\0';
    DEBUG_puts(line);
  }
}
#endif /* DEBUG */


/*
 * 'http_field()' - Return the field index for a field name.
 */

static http_field_t			/* O - Field index */
http_field(const char *name)		/* I - String name */
{
  int	i;				/* Looping var */


  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (strcasecmp(name, http_fields[i]) == 0)
      return ((http_field_t)i);

  return (HTTP_FIELD_UNKNOWN);
}


#ifdef HAVE_SSL
/*
 * 'http_read_ssl()' - Read from a SSL/TLS connection.
 */

static int				/* O - Bytes read */
http_read_ssl(http_t *http,		/* I - Connection to server */
	      char   *buf,		/* I - Buffer to store data */
	      int    len)		/* I - Length of buffer */
{
#  if defined(HAVE_LIBSSL)
  return (SSL_read((SSL *)(http->tls), buf, len));

#  elif defined(HAVE_GNUTLS)
  ssize_t	result;			/* Return value */


  result = gnutls_record_recv(((http_tls_t *)(http->tls))->session, buf, len);

  if (result < 0 && !errno)
  {
   /*
    * Convert GNU TLS error to errno value...
    */

    switch (result)
    {
      case GNUTLS_E_INTERRUPTED :
	  errno = EINTR;
	  break;

      case GNUTLS_E_AGAIN :
          errno = EAGAIN;
          break;

      default :
          errno = EPIPE;
          break;
    }

    result = -1;
  }

  return ((int)result);

#  elif defined(HAVE_CDSASSL)
  int		result;			/* Return value */
  OSStatus	error;			/* Error info */
  size_t	processed;		/* Number of bytes processed */


  error = SSLRead(((http_tls_t *)http->tls)->session, buf, len, &processed);

  switch (error)
  {
    case 0 :
	result = (int)processed;
	break;
    case errSSLClosedGraceful :
	result = 0;
	break;
    case errSSLWouldBlock :
	if (processed)
	  result = (int)processed;
	else
	{
	  result = -1;
	  errno = EINTR;
	}
	break;
    default :
	errno = EPIPE;
	result = -1;
	break;
  }

  return (result);
#  endif /* HAVE_LIBSSL */
}
#endif /* HAVE_SSL */


#if defined(HAVE_LIBSSL) && defined(HAVE_PTHREAD_H)
/*
 * 'http_locking_cb()' - Lock/unlock a thread's mutex.
 */

static void
http_locking_cb(int        mode,	/* I - Lock mode */
		int        type,	/* I - Lock type */
		const char *file,	/* I - Source file */
		int        line)	/* I - Line number */
{
  if (mode & CRYPTO_LOCK)
    pthread_mutex_lock(http_locks + type);
  else
    pthread_mutex_unlock(http_locks + type);
}
#endif /* HAVE_LIBSSL && HAVE_PTHREAD_H */


/*
 * 'http_send()' - Send a request with all fields and the trailing blank line.
 */

static int			/* O - 0 on success, non-zero on error */
http_send(http_t       *http,	/* I - Connection to server */
          http_state_t request,	/* I - Request code */
	  const char   *uri)	/* I - URI */
{
  int		i;		/* Looping var */
  char		buf[1024];	/* Encoded URI buffer */
  static const char * const codes[] =
		{		/* Request code strings */
		  NULL,
		  "OPTIONS",
		  "GET",
		  NULL,
		  "HEAD",
		  "POST",
		  NULL,
		  NULL,
		  "PUT",
		  NULL,
		  "DELETE",
		  "TRACE",
		  "CLOSE"
		};


  DEBUG_printf(("7http_send(http=%p, request=HTTP_%s, uri=\"%s\")",
                http, codes[request], uri));

  if (http == NULL || uri == NULL)
    return (-1);

 /*
  * Set the User-Agent field if it isn't already...
  */

  if (!http->fields[HTTP_FIELD_USER_AGENT][0])
    httpSetField(http, HTTP_FIELD_USER_AGENT, CUPS_MINIMAL);

 /*
  * Encode the URI as needed...
  */

  _httpEncodeURI(buf, uri, sizeof(buf));

 /*
  * See if we had an error the last time around; if so, reconnect...
  */

  if (http->status == HTTP_ERROR || http->status >= HTTP_BAD_REQUEST)
    if (httpReconnect(http))
      return (-1);

 /*
  * Flush any written data that is pending...
  */

  if (http->wused)
  {
    if (httpFlushWrite(http) < 0)
      if (httpReconnect(http))
        return (-1);
  }

 /*
  * Send the request header...
  */

  http->state         = request;
  http->data_encoding = HTTP_ENCODE_FIELDS;

  if (request == HTTP_POST || request == HTTP_PUT)
    http->state ++;

  http->status = HTTP_CONTINUE;

#ifdef HAVE_SSL
  if (http->encryption == HTTP_ENCRYPT_REQUIRED && !http->tls)
  {
    httpSetField(http, HTTP_FIELD_CONNECTION, "Upgrade");
    httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.0,SSL/2.0,SSL/3.0");
  }
#endif /* HAVE_SSL */

  if (httpPrintf(http, "%s %s HTTP/1.1\r\n", codes[request], buf) < 1)
  {
    http->status = HTTP_ERROR;
    return (-1);
  }

  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (http->fields[i][0] != '\0')
    {
      DEBUG_printf(("9http_send: %s: %s", http_fields[i],
                    httpGetField(http, i)));

      if (httpPrintf(http, "%s: %s\r\n", http_fields[i],
		     httpGetField(http, i)) < 1)
      {
	http->status = HTTP_ERROR;
	return (-1);
      }
    }

  if (http->cookie)
    if (httpPrintf(http, "Cookie: $Version=0; %s\r\n", http->cookie) < 1)
    {
      http->status = HTTP_ERROR;
      return (-1);
    }

  if (http->expect == HTTP_CONTINUE &&
      (http->state == HTTP_POST_RECV || http->state == HTTP_PUT_RECV))
    if (httpPrintf(http, "Expect: 100-continue\r\n") < 1)
    {
      http->status = HTTP_ERROR;
      return (-1);
    }

  if (httpPrintf(http, "\r\n") < 1)
  {
    http->status = HTTP_ERROR;
    return (-1);
  }

  if (httpFlushWrite(http) < 0)
    return (-1);

  httpGetLength2(http);
  httpClearFields(http);

 /*
  * The Kerberos and AuthRef authentication strings can only be used once...
  */

  if (http->field_authorization && http->authstring &&
      (!strncmp(http->authstring, "Negotiate", 9) ||
       !strncmp(http->authstring, "AuthRef", 7)))
  {
    http->_authstring[0] = '\0';

    if (http->authstring != http->_authstring)
      free(http->authstring);

    http->authstring = http->_authstring;
  }

  return (0);
}


#ifdef HAVE_SSL
/*
 * 'http_setup_ssl()' - Set up SSL/TLS support on a connection.
 */

static int				/* O - Status of connection */
http_setup_ssl(http_t *http)		/* I - Connection to server */
{
#  ifdef HAVE_LIBSSL
  SSL_CTX	*context;		/* Context for encryption */
  SSL		*conn;			/* Connection for encryption */
  BIO		*bio;			/* BIO data */
#  elif defined(HAVE_GNUTLS)
  http_tls_t	*conn;			/* TLS session object */
  gnutls_certificate_client_credentials *credentials;
					/* TLS credentials */
#  elif defined(HAVE_CDSASSL)
  OSStatus	error;			/* Error code */
  http_tls_t	*conn;			/* CDSA connection information */
#  endif /* HAVE_LIBSSL */


  DEBUG_printf(("7http_setup_ssl(http=%p)", http));

#  ifdef HAVE_LIBSSL
  context = SSL_CTX_new(SSLv23_client_method());

  SSL_CTX_set_options(context, SSL_OP_NO_SSLv2); /* Only use SSLv3 or TLS */

  bio = BIO_new(_httpBIOMethods());
  BIO_ctrl(bio, BIO_C_SET_FILE_PTR, 0, (char *)http);

  conn = SSL_new(context);
  SSL_set_bio(conn, bio, bio);

  if (SSL_connect(conn) != 1)
  {
#    ifdef DEBUG
    unsigned long	error;	/* Error code */

    while ((error = ERR_get_error()) != 0)
      DEBUG_printf(("8http_setup_ssl: %s", ERR_error_string(error, NULL)));
#    endif /* DEBUG */

    SSL_CTX_free(context);
    SSL_free(conn);

#    ifdef WIN32
    http->error  = WSAGetLastError();
#    else
    http->error  = errno;
#    endif /* WIN32 */
    http->status = HTTP_ERROR;

    return (HTTP_ERROR);
  }

#  elif defined(HAVE_GNUTLS)
  if ((conn = (http_tls_t *)malloc(sizeof(http_tls_t))) == NULL)
  {
    http->error  = errno;
    http->status = HTTP_ERROR;

    return (-1);
  }

  credentials = (gnutls_certificate_client_credentials *)
                    malloc(sizeof(gnutls_certificate_client_credentials));
  if (credentials == NULL)
  {
    free(conn);

    http->error = errno;
    http->status = HTTP_ERROR;

    return (-1);
  }

  gnutls_certificate_allocate_credentials(credentials);

  gnutls_init(&(conn->session), GNUTLS_CLIENT);
  gnutls_set_default_priority(conn->session);
  gnutls_credentials_set(conn->session, GNUTLS_CRD_CERTIFICATE, *credentials);
  gnutls_transport_set_ptr(conn->session, (gnutls_transport_ptr)http);
  gnutls_transport_set_pull_function(conn->session, _httpReadGNUTLS);
  gnutls_transport_set_push_function(conn->session, _httpWriteGNUTLS);

  if ((gnutls_handshake(conn->session)) != GNUTLS_E_SUCCESS)
  {
    http->error  = errno;
    http->status = HTTP_ERROR;

    gnutls_deinit(conn->session);
    gnutls_certificate_free_credentials(*credentials);
    free(credentials);
    free(conn);

    return (-1);
  }

  conn->credentials = credentials;

#  elif defined(HAVE_CDSASSL)
  conn = (http_tls_t *)calloc(1, sizeof(http_tls_t));

  if (conn == NULL)
    return (-1);

  if ((error = SSLNewContext(false, &conn->session)))
  {
    http->error  = error;
    http->status = HTTP_ERROR;

    free(conn);
    return (-1);
  }

 /*
  * Use a union to resolve warnings about int/pointer size mismatches...
  */

  error = SSLSetConnection(conn->session, http);

  if (!error)
    error = SSLSetIOFuncs(conn->session, _httpReadCDSA, _httpWriteCDSA);

  if (!error)
    error = SSLSetAllowsExpiredCerts(conn->session, true);

  if (!error)
    error = SSLSetAllowsAnyRoot(conn->session, true);

  if (!error)
    error = SSLSetProtocolVersionEnabled(conn->session, kSSLProtocol2, false);

  if (!error)
  {
    while ((error = SSLHandshake(conn->session)) == errSSLWouldBlock)
      usleep(1000);
  }

  if (error)
  {
    http->error  = error;
    http->status = HTTP_ERROR;

    SSLDisposeContext(conn->session);

    free(conn);

    return (-1);
  }
#  endif /* HAVE_CDSASSL */

  http->tls = conn;
  return (0);
}
#endif /* HAVE_SSL */


#ifdef HAVE_SSL
/*
 * 'http_shutdown_ssl()' - Shut down SSL/TLS on a connection.
 */

static void
http_shutdown_ssl(http_t *http)		/* I - Connection to server */
{
#  ifdef HAVE_LIBSSL
  SSL_CTX	*context;		/* Context for encryption */
  SSL		*conn;			/* Connection for encryption */


  conn    = (SSL *)(http->tls);
  context = SSL_get_SSL_CTX(conn);

  SSL_shutdown(conn);
  SSL_CTX_free(context);
  SSL_free(conn);

#  elif defined(HAVE_GNUTLS)
  http_tls_t      *conn;		/* Encryption session */
  gnutls_certificate_client_credentials *credentials;
					/* TLS credentials */


  conn = (http_tls_t *)(http->tls);
  credentials = (gnutls_certificate_client_credentials *)(conn->credentials);

  gnutls_bye(conn->session, GNUTLS_SHUT_RDWR);
  gnutls_deinit(conn->session);
  gnutls_certificate_free_credentials(*credentials);
  free(credentials);
  free(conn);

#  elif defined(HAVE_CDSASSL)
  http_tls_t      *conn;		/* CDSA connection information */


  conn = (http_tls_t *)(http->tls);

  while (SSLClose(conn->session) == errSSLWouldBlock)
    usleep(1000);

  SSLDisposeContext(conn->session);

  if (conn->certsArray)
    CFRelease(conn->certsArray);

  free(conn);
#  endif /* HAVE_LIBSSL */

  http->tls = NULL;
}
#endif /* HAVE_SSL */


#if defined(HAVE_LIBSSL) && defined(HAVE_PTHREAD_H)
/*
 * 'http_threadid_cb()' - Return the current thread ID.
 */

static unsigned long			/* O - Thread ID */
http_threadid_cb(void)
{
  return ((unsigned long)pthread_self());
}
#endif /* HAVE_LIBSSL && HAVE_PTHREAD_H */


#ifdef HAVE_SSL
/*
 * 'http_upgrade()' - Force upgrade to TLS encryption.
 */

static int				/* O - Status of connection */
http_upgrade(http_t *http)		/* I - Connection to server */
{
  int		ret;			/* Return value */
  http_t	myhttp;			/* Local copy of HTTP data */


  DEBUG_printf(("7http_upgrade(%p)", http));

 /*
  * Flush the connection to make sure any previous "Upgrade" message
  * has been read.
  */

  httpFlush(http);

 /*
  * Copy the HTTP data to a local variable so we can do the OPTIONS
  * request without interfering with the existing request data...
  */

  memcpy(&myhttp, http, sizeof(myhttp));

 /*
  * Send an OPTIONS request to the server, requiring SSL or TLS
  * encryption on the link...
  */

  http->field_authorization = NULL;	/* Don't free the auth string */

  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONNECTION, "upgrade");
  httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.0, SSL/2.0, SSL/3.0");

  if ((ret = httpOptions(http, "*")) == 0)
  {
   /*
    * Wait for the secure connection...
    */

    while (httpUpdate(http) == HTTP_CONTINUE);
  }

 /*
  * Restore the HTTP request data...
  */

  memcpy(http->fields, myhttp.fields, sizeof(http->fields));
  http->data_encoding       = myhttp.data_encoding;
  http->data_remaining      = myhttp.data_remaining;
  http->_data_remaining     = myhttp._data_remaining;
  http->expect              = myhttp.expect;
  http->field_authorization = myhttp.field_authorization;
  http->digest_tries        = myhttp.digest_tries;

 /*
  * See if we actually went secure...
  */

  if (!http->tls)
  {
   /*
    * Server does not support HTTP upgrade...
    */

    DEBUG_puts("8http_upgrade: Server does not support HTTP upgrade!");

#  ifdef WIN32
    closesocket(http->fd);
#  else
    close(http->fd);
#  endif

    http->fd = -1;

    return (-1);
  }
  else
    return (ret);
}
#endif /* HAVE_SSL */


/*
 * 'http_write()' - Write a buffer to a HTTP connection.
 */

static int				/* O - Number of bytes written */
http_write(http_t     *http,		/* I - Connection to server */
           const char *buffer,		/* I - Buffer for data */
	   int        length)		/* I - Number of bytes to write */
{
  int	tbytes,				/* Total bytes sent */
	bytes;				/* Bytes sent */


  http->error = 0;
  tbytes      = 0;

  while (length > 0)
  {
#ifdef HAVE_SSL
    if (http->tls)
      bytes = http_write_ssl(http, buffer, length);
    else
#endif /* HAVE_SSL */
    bytes = send(http->fd, buffer, length, 0);

    if (bytes < 0)
    {
#ifdef WIN32
      if (WSAGetLastError() != http->error)
      {
        http->error = WSAGetLastError();
	continue;
      }
#else
      if (errno == EINTR || errno == EAGAIN)
        continue;
      else if (errno != http->error && errno != ECONNRESET)
      {
        http->error = errno;
	continue;
      }
#endif /* WIN32 */

      DEBUG_puts("8http_write: error writing data...");

      return (-1);
    }

    buffer += bytes;
    tbytes += bytes;
    length -= bytes;
  }

#ifdef DEBUG
  http_debug_hex("http_write", buffer - tbytes, tbytes);
#endif /* DEBUG */

  return (tbytes);
}


/*
 * 'http_write_chunk()' - Write a chunked buffer.
 */

static int				/* O - Number bytes written */
http_write_chunk(http_t     *http,	/* I - Connection to server */
                 const char *buffer,	/* I - Buffer to write */
		 int        length)	/* I - Length of buffer */
{
  char	header[255];			/* Chunk header */
  int	bytes;				/* Bytes written */


  DEBUG_printf(("7http_write_chunk(http=%p, buffer=%p, length=%d)",
                http, buffer, length));

 /*
  * Write the chunk header, data, and trailer.
  */

  sprintf(header, "%x\r\n", length);
  if (http_write(http, header, (int)strlen(header)) < 0)
  {
    DEBUG_puts("8http_write_chunk: http_write of length failed!");
    return (-1);
  }

  if ((bytes = http_write(http, buffer, length)) < 0)
  {
    DEBUG_puts("8http_write_chunk: http_write of buffer failed!");
    return (-1);
  }

  if (http_write(http, "\r\n", 2) < 0)
  {
    DEBUG_puts("8http_write_chunk: http_write of CR LF failed!");
    return (-1);
  }

  return (bytes);
}


#ifdef HAVE_SSL
/*
 * 'http_write_ssl()' - Write to a SSL/TLS connection.
 */

static int				/* O - Bytes written */
http_write_ssl(http_t     *http,	/* I - Connection to server */
	       const char *buf,		/* I - Buffer holding data */
	       int        len)		/* I - Length of buffer */
{
#  if defined(HAVE_LIBSSL)
  return (SSL_write((SSL *)(http->tls), buf, len));

#  elif defined(HAVE_GNUTLS)
  ssize_t	result;			/* Return value */

  result = gnutls_record_send(((http_tls_t *)(http->tls))->session, buf, len);

  if (result < 0 && !errno)
  {
   /*
    * Convert GNU TLS error to errno value...
    */

    switch (result)
    {
      case GNUTLS_E_INTERRUPTED :
	  errno = EINTR;
	  break;

      case GNUTLS_E_AGAIN :
          errno = EAGAIN;
          break;

      default :
          errno = EPIPE;
          break;
    }

    result = -1;
  }

  return ((int)result);

#  elif defined(HAVE_CDSASSL)
  int		result;			/* Return value */
  OSStatus	error;			/* Error info */
  size_t	processed;		/* Number of bytes processed */


  error = SSLWrite(((http_tls_t *)http->tls)->session, buf, len, &processed);

  switch (error)
  {
    case 0 :
	result = (int)processed;
	break;
    case errSSLClosedGraceful :
	result = 0;
	break;
    case errSSLWouldBlock :
	if (processed)
	  result = (int)processed;
	else
	{
	  result = -1;
	  errno  = EINTR;
	}
	break;
    default :
	errno  = EPIPE;
	result = -1;
	break;
  }

  return (result);
#  endif /* HAVE_LIBSSL */
}
#endif /* HAVE_SSL */


/*
 * End of "$Id: http.c 9233 2010-08-10 06:15:55Z mike $".
 */
