/* KallistiOS ##version##

   arpa/inet.h
   Copyright (C) 2006, 2007 Lawrence Sebald

*/

#ifndef __ARPA_INET_H
#define __ARPA_INET_H

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

uint32 htonl(uint32 value);
uint32 ntohl(uint32 value);

uint16 htons(uint16 value);
uint16 ntohs(uint16 value);

in_addr_t inet_addr(const char *cp);
int inet_aton(const char *cp, struct in_addr *pin);

int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

/* Non-reentrant */
char *inet_ntoa(struct in_addr addr);

__END_DECLS

#endif /* __ARPA_INET_H */
