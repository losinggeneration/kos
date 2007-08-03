/* KallistiOS ##version##

   netinet/in.h
   Copyright (C) 2006, 2007 Lawrence Sebald

*/

#ifndef __NETINET_IN_H
#define __NETINET_IN_H

#include <sys/cdefs.h>
#include <arch/types.h>

__BEGIN_DECLS

#ifndef __IN_PORT_T_DEFINED
#define __IN_PORT_T_DEFINED
typedef uint16 in_port_t;
#endif

#ifndef __IN_ADDR_T_DEFINED
#define __IN_ADDR_T_DEFINED
typedef uint32 in_addr_t;
#endif

#ifndef __SA_FAMILY_T_DEFINED
#define __SA_FAMILY_T_DEFINED
typedef uint32 sa_family_t;
#endif

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t    sin_family;
    in_port_t      sin_port;
    struct in_addr sin_addr;
    unsigned char  sin_zero[8];
};

#define INADDR_ANY       0x00000000
#define INADDR_BROADCAST 0xFFFFFFFF
#define INADDR_NONE      0xFFFFFFFF

/* IP Protocols */
#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

__END_DECLS

#endif /* __NETINET_IN_H */
