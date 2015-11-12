/**
 * This file is part of libipsc library code.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include "ipsc.h"
#include "dbg.h"


inline int ipsc_set_nonblock( ipsc_t *ipsc )
{
	int opts = fcntl( ipsc->sd, F_GETFL );
	if ( opts <= 0 )
		return -1;

	opts |= O_NONBLOCK;
	if ( fcntl( ipsc->sd, F_SETFL, opts ) < 0 )
		return -1;

	return 0;
}

inline int ipsc_set_recv_timeout( ipsc_t *ipsc, unsigned int to )
{
	struct timeval tv;

	tv.tv_sec  = to / 1000;
	tv.tv_usec = (to%1000) * 1000;

	if ( setsockopt( ipsc->sd, SOL_SOCKET, SO_RCVTIMEO,
			 (char *)&tv, sizeof tv ) )
		return -1;

	return 0;
}

int ipsc_addr_un( ipsc_t **ipsc, uint16_t port )
{
	(*ipsc)->alen   = sizeof(struct sockaddr_un);
	(*ipsc)->addr   = (struct sockaddr *)malloc( (*ipsc)->alen );
	if ( !(*ipsc)->addr )
		return -1;

	((struct sockaddr_un *)(*ipsc)->addr)->sun_family = AF_LOCAL;
	snprintf( ((struct sockaddr_un *)(*ipsc)->addr)->sun_path,
		  sizeof(((struct sockaddr_un *)(*ipsc)->addr)->sun_path),
		  IPSC_SOCKET_FILE, port );

	return 0;
}

ipsc_t *ipsc_init(uint16_t port )
{
	ipsc_t *ipsc = (ipsc_t *)malloc( sizeof(ipsc_t) );
	if ( !ipsc )
		return NULL;

	ipsc->sd      = -1;
	ipsc->maxq    = 0;
	ipsc->flags   = 0;
	ipsc->alen    = 0;
	ipsc->addr    = NULL;
	ipsc->cb_args = NULL;

	if ( ipsc_addr_un( &ipsc, port ) )
		goto exit;

	ipsc->sd = socket( PF_LOCAL, SOCK_STREAM, 0 );
	if ( ipsc->sd == -1 )
		goto exit;

	return ipsc;

exit:
	ipsc_close( ipsc );
	return NULL;
}

int ipsc_bind( ipsc_t *ipsc )
{
	if ( !bind( ipsc->sd, ipsc->addr, ipsc->alen ) )
		return 0;

	if ( errno != EADDRINUSE )
		return -1;

	/* is there somebody using that socket already...? */
	if ( !connect( ipsc->sd, ipsc->addr, ipsc->alen) )
		return -1;

	/* ...seems not, nuke it */
	unlink( ((struct sockaddr_un *)ipsc->addr)->sun_path );

	/* try to bind again */
	if ( bind( ipsc->sd, ipsc->addr, ipsc->alen ) )
		return -1;

	return 0;
}

int ipsc_epoll_newfd( ipsc_t *ipsc, int epfd )
{
	struct epoll_event ev;

	ev.data.u64 = 0;
	ev.data.ptr = ipsc;

	// TODO : check
//	ev.events   = EPOLLIN | EPOLLPRI | EPOLLET | EPOLLRDHUP;
	ev.events   = EPOLLIN | EPOLLPRI | EPOLLET;

	if ( epoll_ctl (epfd, EPOLL_CTL_ADD, ipsc->sd, &ev ))
	{
		return -1;
	}

	return 0;
}

ipsc_t *ipsc_listen (uint16_t port, int maxq)
{
	ipsc_t *ipsc = ipsc_init (port);
	if ( !ipsc )
		return NULL;

	ipsc->maxq = maxq;
	if ( maxq > IPSC_MAX_QUEUE )
		ipsc->maxq = IPSC_MAX_QUEUE;
	if ( maxq < 1 )
		ipsc->maxq = IPSC_MAX_QUEUE_DEFAULT;

	if ( ipsc_bind( ipsc ) )
		goto exit;

	if ( listen( ipsc->sd, ipsc->maxq ) )
		goto exit;

	ipsc->flags |= IPSC_FLAG_SERVER;

	return ipsc;

exit:
	ipsc_close( ipsc );
	return NULL;
}

ipsc_t *ipsc_accept( ipsc_t *ipsc )
{
	if ( !ipsc )
		return NULL;

	ipsc_t *client = (ipsc_t *)malloc( sizeof(ipsc_t) );
	if ( !client )
		return NULL;

	client->sd      = -1;
	client->maxq    = 0;
	client->flags   = 0;
	client->alen    = ipsc->alen;
	client->addr    = (struct sockaddr *)malloc( client->alen );
	client->cb_args = ipsc->cb_args;

	if ( !client->addr )
		goto exit;

	client->sd = accept( ipsc->sd, client->addr,
			     (socklen_t *)&(client->alen) );
	if ( client->sd > 0 )
		return client;
exit:
	ipsc_close( client );
	return NULL;
}


ipsc_t *ipsc_connect( uint16_t port )
{
	ipsc_t *ipsc = ipsc_init( port );
	if ( !ipsc  )
		return NULL;

	if ( !connect( ipsc->sd, ipsc->addr, ipsc->alen ) )
		return ipsc;

	ipsc_close( ipsc );
	return NULL;
}

ssize_t ipsc_send( ipsc_t *ipsc, const void *buf, size_t buflen )
{
	ssize_t sent = 0;
	size_t sent_sum = 0;

	while ( sent_sum < buflen ) {
		sent = send( ipsc->sd, (const char *)buf + sent_sum,
				buflen - sent_sum, MSG_NOSIGNAL );

		if ( sent == -1 ) {
			if ( errno == EAGAIN ||
			     errno == EWOULDBLOCK ||
			     errno == EINTR )
				continue;
			return sent;
		}
		sent_sum += sent;
	}

	return sent_sum;
}

ssize_t ipsc_recv( ipsc_t *ipsc, void *buf,
		   size_t buflen, unsigned int timeout )
{
	ssize_t rb = 0;
	ssize_t recvd = 0;

	if ( ipsc_set_recv_timeout( ipsc, timeout ) )
		return -1;

	while ( !recvd ) {
		rb = recv( ipsc->sd, (char *)buf + recvd,
				buflen - recvd, 0 );

		if ( rb < 1 ) {
			if ( ( (errno == EAGAIN || errno == EWOULDBLOCK) &&
						timeout == 0 ) ||
						errno == EINTR )
				continue;
			return rb;
		}
		recvd += rb;
	}

	return recvd;
}

int ipsc_epoll_init( ipsc_t *ipsc )
{
	int epfd;

	if ( ipsc_set_nonblock(ipsc) )
		return -1;

	epfd = epoll_create (ipsc->maxq);
	if ( epfd == -1 )
	{
		perror ("epoll_create");
		return -1;
	}

	if (ipsc_epoll_newfd (ipsc, epfd))
		return -1;

	return epfd;
}

int ipsc_epoll_wait (ipsc_t *ipsc, int epfd, ssize_t (*cb)(ipsc_t *))
{
	int i;
	int pool = 0;
	ipsc_t *client = NULL;
	struct epoll_event events[ipsc->maxq];

	pool = epoll_wait (epfd, events, ipsc->maxq, -1);
	if ( pool < 0 )
		return -1;

	for ( i = 0; i < pool; i++ ) {
		/* new client connected */
		if ( events[i].data.ptr == ipsc ) {
			/* accept clients, create new fd and add to the pool */
			while ( (client = ipsc_accept(ipsc)) ) {
				if ( ipsc_set_nonblock( client ) ) {
					ipsc_close( client );
					continue;
				}
				if (ipsc_epoll_newfd (client, epfd))
				{
					ipsc_close( client );
					continue;
				}
			}
			continue;
		}

		/* explicitly close connection, SCTP fails without this */
		// TODO : check
		// if ( events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) ) {
		if (events[i].events & (EPOLLHUP | EPOLLERR)) {
			ipsc_close (events[i].data.ptr);
			continue;
		}

		/* incoming event on previously accepted connection */
		if ( events[i].events & EPOLLIN ) {
			if ( !events[i].data.ptr )
				continue;
			if ( (*cb)( events[i].data.ptr ) < 0 )
				ipsc_close( events[i].data.ptr );
		}
	}

	return 0;
}

int ipsc_epoll_wait_timeout (ipsc_t *ipsc, int epfd,
		ssize_t (*cb)(ipsc_t *), int timeout)
{
	int i;
	int pool = 0;
	ipsc_t *client = NULL;
	struct epoll_event events[ipsc->maxq];

	pool = epoll_wait (epfd, events, ipsc->maxq, timeout);
	if ( pool < 0 )
		return -1;

	for ( i = 0; i < pool; i++ ) {
		/* new client connected */
		if ( events[i].data.ptr == ipsc ) {
			/* accept clients, create new fd and add to the pool */
			while ( (client = ipsc_accept(ipsc)) ) {
				if ( ipsc_set_nonblock( client ) ) {
					ipsc_close( client );
					continue;
				}
				if (ipsc_epoll_newfd (client, epfd))
				{
					ipsc_close( client );
					continue;
				}
			}
			continue;
		}

		/* explicitly close connection, SCTP fails without this */
		// TODO : check
		// if ( events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) ) {
		if (events[i].events & (EPOLLHUP | EPOLLERR)) {
			ipsc_close (events[i].data.ptr);
			continue;
		}

		/* incoming event on previously accepted connection */
		if ( events[i].events & EPOLLIN ) {
			if ( !events[i].data.ptr )
				continue;
			if ( (*cb)( events[i].data.ptr ) < 0 )
				ipsc_close( events[i].data.ptr );
		}
	}

	return 0;
}

void ipsc_close( ipsc_t *ipsc )
{
	if ( !ipsc )
		return;

	if ( ipsc->sd > 0 ) {
		shutdown( ipsc->sd, SHUT_RDWR );
		close( ipsc->sd );
	}

	if ( ipsc->flags & IPSC_FLAG_SERVER )
		unlink( ((struct sockaddr_un *)ipsc->addr)->sun_path );

	free( ipsc->addr );
	free( ipsc );
	ipsc = NULL;
}
