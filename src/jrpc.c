/**
 * This file is part of libjrpc library code.
 *
 * Copyright (C) 2014 Roman Yeryomin <roman@advem.lv>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENCE.txt file for more details.
 */
#include "jrpc.h"
#include "dbg.h"

static ssize_t jrpc_parse_error (ipsc_t *ipsc, json_t *jid)
{
	return jrpc_error (ipsc, jid,
			   JRPC_CODE_PARSE_ERROR,
			   JRPC_ERR_PARSE_ERROR );
}

static ssize_t jrpc_invalid_request (ipsc_t *ipsc, json_t *jid)
{
	return jrpc_error (ipsc, jid,
			   JRPC_CODE_INVALID_REQUEST,
			   JRPC_ERR_INVALID_REQUEST);
}

static ssize_t jrpc_method_not_found (ipsc_t *ipsc, json_t *jid)
{
	return jrpc_error (ipsc, jid,
			   JRPC_CODE_METHOD_NOT_FOUND,
			   JRPC_ERR_METHOD_NOT_FOUND);
}

ssize_t jrpc_invalid_params (ipsc_t *ipsc, json_t *jid)
{
	return jrpc_error (ipsc, jid,
			   JRPC_CODE_INVALID_PARAMS,
			   JRPC_ERR_INVALID_PARAMS);
}

ssize_t jrpc_internal_error (ipsc_t *ipsc, json_t *jid)
{
	return jrpc_error (ipsc, jid,
			   JRPC_CODE_INTERNAL_ERROR,
			   JRPC_ERR_INTERNAL_ERROR);
}

ssize_t jrpc_not_implemented (ipsc_t *ipsc, json_t *jid)
{
	return jrpc_error (ipsc, jid,
			   JRPC_CODE_NOT_IMPLEMENTED,
			   JRPC_ERR_NOT_IMPLEMENTED);
}

static int jrpc_check_version (json_t *jroot)
{
	char *version = NULL;

	if (json_unpack (jroot, "{s:s}", JRPC_KEY_JSONRPC, &version))
	{
		return -1;
	}

	if (strncmp (version, JRPC_KEY_VERSION,
		      strlen(JRPC_KEY_VERSION) + 1 ) )
	{
		return -1;
	}

	return 0;
}

void jrpc_add_version (json_t *jroot, json_t *jid)
{
	json_object_set_new (jroot, JRPC_KEY_JSONRPC, json_string (JRPC_KEY_VERSION));
	if (jid)
		json_object_set (jroot, JRPC_KEY_ID, jid);
	else
		json_object_set_new (jroot, JRPC_KEY_ID, json_null());
}

ssize_t jrpc_send_json( ipsc_t *ipsc, json_t *jroot )
{
	char *buf = NULL;
	ssize_t sb = 0;
	jrpc_runtime_t rt;

	if ( ipsc->flags & IPSC_FLAG_SERVER ) {
		rt = ((jrpc_t *)ipsc->cb_args)->rt;
	} else {
		rt = ((jrpc_req_t *)ipsc->cb_args)->rt;
	}

	if ((buf = json_dumps (jroot, JSON_COMPACT)) == NULL)
//	if ((buf = json_dumps (jroot, JSON_INDENT(2))) == NULL)
	{
		sb = JRPC_ERR_GENERIC;
		return sb;
	}

	//////////////////////////////////////
	_dbg ("JRPC", ">> \n%s\n", buf);
	//////////////////////////////////////

	sb = ipsc_send (ipsc, buf, strlen(buf));

	free (buf);
	return sb;
}

ssize_t jrpc_recv_json (ipsc_t *ipsc, json_t **jp)
{
	char *buf = NULL;
	size_t buflen = 0;
	ssize_t rb = 0;
	ssize_t trb = 0;
	json_t *jobj; 
	int timeout;
	jrpc_runtime_t rt;

	json_error_t error;

	if ( ipsc->flags & IPSC_FLAG_SERVER ) {
		timeout = ((jrpc_t *)ipsc->cb_args)->conn.timeout;
		rt = ((jrpc_t *)ipsc->cb_args)->rt;
	} else {
		timeout = ((jrpc_req_t *)ipsc->cb_args)->conn.timeout;
		rt = ((jrpc_req_t *)ipsc->cb_args)->rt;
	}

	buflen = JRPC_DEFAULT_RCVBUF_STREAM;

	/* TODO: add hard memory limit */
	buf = (char *)malloc (buflen);

	if (buf == NULL)
	{
		return -1;
	}

	/* TODO: add hard memory limit */
	while ( (trb = ipsc_recv( ipsc, buf + rb,
					buflen - rb - 1, timeout )) > 0 )
	{
		/* lower the timeout after first data received */
		if ( timeout > 0 )
			timeout = 10;
		rb += trb;
		if ( rb > buflen - 2 ) {
			buflen += buflen;
			buf = (char *)realloc( buf, buflen );
		}
	}

	if ( rb < 2 )
		rb = 0;

	// TODO : ????
	// jobj = json_loadb (buf, (size_t)rb, JSON_DISABLE_EOF_CHECK, &error);
	jobj = json_loads (buf, JSON_DISABLE_EOF_CHECK, &error);
	if (!jobj)
	{
		rb = -1;
	}

	//////////////////////////////////////

	char *tmp;
	tmp = json_dumps (jobj, JSON_INDENT(2));
	if (tmp == NULL) 
	{
	}
	else 
	{
		_dbg ("JRPC", "<< \n%s\n", tmp);
		free (tmp);
	}

	//////////////////////////////////////

	free (buf);

	*jp = jobj;
	return rb;
}

ssize_t jrpc_process( ipsc_t *ipsc )
{
	int i, idx;
	size_t rb;
	size_t sb = 0;
	json_t *jp = NULL;
	json_t *jparams = NULL;
	json_t *jid = NULL;
	jrpc_cb_t cb;
	jrpc_t *jrpc = (jrpc_t *)ipsc->cb_args;
	char *method = NULL;

	ipsc->flags |= IPSC_FLAG_SERVER;

	rb = jrpc_recv_json (ipsc, &jp);
	if ( rb < 2 )
	{
		syslog( LOG_WARNING, "jrpc_process(recv): %m (%li)", rb );
		sb = jrpc_parse_error( ipsc, NULL );
		goto ret;
	}

	json_unpack (jp, "{s?:o}", JRPC_KEY_ID, &jid);

#ifndef JRPC_LITE
	/* check version string if we use standart fields */
	if (jrpc_check_version (jp))
	{
		sb = jrpc_invalid_request (ipsc, jid);
		goto ret;
	}
#endif

	/* send error back if 'method' key is not found */
	if (json_unpack (jp, "{s:s}", JRPC_KEY_METHOD, &method))
	{
		sb = jrpc_invalid_request (ipsc, jid);
		goto ret;
	}

	for ( i = 0; jrpc->methods[i].name; i++ )
	{
		if ( strncmp (method, jrpc->methods[i].name,
			      strlen(jrpc->methods[i].name) + 1 ) )
			continue;

		switch ( jrpc->methods[i].params )
		{
		case JRPC_CB_HAS_PARAMS:
			if (json_unpack (jp, "{s:o}", JRPC_KEY_PARAMS, &jparams))
			{
				sb = jrpc_invalid_params (ipsc, jid);
				goto ret;
			}
			break;
		case JRPC_CB_OPT_PARAMS:
			if (json_unpack (jp, "{s?:o}", JRPC_KEY_PARAMS, &jparams))
			{
				sb = jrpc_invalid_params (ipsc, jid);
				goto ret;
			}
			break;
		case JRPC_CB_NO_PARAMS:
		default:
			break;
		}

		if (!jrpc->methods[i].handlers)
		{
			sb = jrpc_not_implemented (ipsc, jid);
			goto ret;
		}

		if (!jrpc->methods[i].handlers[0])
		{
			sb = jrpc_not_implemented (ipsc, jid);
			goto ret;
		}

		for (idx = 0; jrpc->methods[i].handlers[idx]; idx++)
		{
			cb = jrpc->methods[i].handlers[idx];
			sb = cb (ipsc, jparams, jid);
			if ( sb == 0 )
				break;
			if ( sb < 0 ) {
				sb = jrpc_internal_error (ipsc, jid);
				break;
			}
		}

		goto ret;
	}

	/* no method defined, send standard error */
	sb = jrpc_method_not_found (ipsc, jid);

ret:
	if (sb < 0)
		syslog (LOG_WARNING, "jrpc_process(recv|send): %m (%li)", sb);

	json_decref (jp);

	return sb;
}

void *jrpc_server( void *args )
{
	if ( !args )
		return NULL;

	int epfd = -1;
	jrpc_t *jrpc = (jrpc_t *)args;
	ipsc_t *ipsc = ipsc_listen( jrpc->conn.port, jrpc->maxqueue );

	if ( !ipsc ) {
		syslog( LOG_WARNING,"jrpc_server(listen): %m" );
		_dbg ("LIBJRPC", "jrpc_server(listen): %m" );
		return NULL;
	}

	ipsc->cb_args = args;

	/* joinable thread callback helper */
	if ( jrpc->connreg )
		jrpc->connreg( ipsc );

	epfd = ipsc_epoll_init (ipsc);
	if ( epfd < 0 ) {
		ipsc_close(ipsc);
		syslog( LOG_WARNING, "jrpc_server(create): %m (%i)", epfd );
		_dbg ("LIBJRPC", "jrpc_server(create): %m (%i)", epfd );
		return NULL;
	}

	while (1) {
		/* do we actually need to check for error here? */
		ipsc_epoll_wait (ipsc, epfd, &jrpc_process);
		usleep (jrpc->epsleep);
	}

	/* should never get here */
	ipsc_close (ipsc);
	return NULL;
}

ssize_t jrpc_request( jrpc_req_t *req )
{
	if ( !req || !req->method )
		return JRPC_ERR_GENERIC;

	ssize_t sb = 0;
	ssize_t rb = 0;
	json_t *jp = NULL;;
	json_t *jroot = NULL;;
	ipsc_t *ipsc = NULL;

	ipsc = ipsc_connect( req->conn.port );
	if ( !ipsc )
	{
		sb = JRPC_ERR_GENERIC;
		goto exit;
	}

	jroot = json_object ();

#ifndef JRPC_LITE
	jrpc_add_version (jroot, req->jid);
#endif

	json_object_set_new (jroot, JRPC_KEY_METHOD, json_string (req->method));
	if (req->jparams)
		json_object_set_new (jroot, JRPC_KEY_PARAMS, req->jparams);

	/* send request */
	ipsc->cb_args = (void *)req;
	sb = jrpc_send_json (ipsc, jroot);
	if ( sb < 2 ) {
		sb = JRPC_ERR_SEND;
		goto exit;
	}

	/* get reply */
	rb = jrpc_recv_json (ipsc, &jp);
	if ( rb < 2 ) {
		syslog(LOG_WARNING, "jrpc_process(recv): %m (%li)", rb);
//		_dbg ("LIBJRPC", "jrpc_process(recv): %m (%li)", rb);
		sb = JRPC_ERR_RECV;
		goto exit;
	}

	sb = JRPC_SUCCESS;
	req->jres = json_copy (json_object_get (jp, JRPC_KEY_RESULT));
	if (req->jres == NULL)
	{
		sb = JRPC_ERR_NORESULT;

		req->jres = json_copy(json_object_get (jp, JRPC_KEY_ERROR));
		if (req->jres == NULL)
		{
			sb = JRPC_ERR_USER;
		}
	}

exit:
	ipsc_close (ipsc);
	json_decref (jp);
	json_decref (jroot);

	return sb;
}

ssize_t jrpc_send_reply ( ipsc_t *ipsc, json_t *jobj, json_t *jid, int type )
{
	if ( !ipsc || !jobj )
		return JRPC_ERR_GENERIC;

	ssize_t sb = 0;
	char msg_type[8]; /* either "error" or "result" */
	json_t *jroot = NULL;

	switch (type)
	{
	case JRPC_REPLY_TYPE_ERROR:
		snprintf( msg_type, sizeof msg_type, "%s", JRPC_KEY_ERROR );
		break;
	case JRPC_REPLY_TYPE_RESULT:
		snprintf( msg_type, sizeof msg_type, "%s", JRPC_KEY_RESULT );
		break;
	default:
		sb = JRPC_ERR_UNKNOWN_REPLY_TYPE;
		goto exit;
	}

	jroot = json_object ();

#ifndef JRPC_LITE
	jrpc_add_version (jroot, jid);
#endif

	if (json_object_set (jroot, msg_type, jobj))
	{
		sb = JRPC_ERR_GENERIC;
		goto exit;
	}

	sb = jrpc_send_json (ipsc, jroot);

exit:
	json_decref (jroot);

	return sb;
}

ssize_t jrpc_error (ipsc_t *ipsc, json_t *jid, int code, const char *message )
{
	json_t *err;
	err = json_object ();

	json_object_set_new (err, JRPC_KEY_ERROR_CODE, json_integer (code));
	json_object_set_new (err, JRPC_KEY_ERROR_TEXT, json_string (message));

	return jrpc_send_reply ( ipsc, err, jid, JRPC_REPLY_TYPE_ERROR );
}

