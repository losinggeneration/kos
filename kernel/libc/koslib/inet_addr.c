/* KallistiOS ##version##

   inet_addr.c
   Copyright (C) 2006, 2007 Lawrence Sebald

*/

#include <arpa/inet.h>

in_addr_t inet_addr(const char *cp) {
    struct in_addr addr;

    /* inet_aton() returns 0 on failure, 1 on success */
    if(inet_aton(cp, &addr)) {
        return addr.s_addr;
    }

    return INADDR_NONE;
}
