/* KallistiOS ##version##

   inet_pton.c
   Copyright (C) 2007 Lawrence Sebald

*/

#include <arpa/inet.h>
#include <errno.h>

int inet_pton(int af, const char *src, void *dst) {
    int parts[4] = { 0 };
    int count = 0;
    struct in_addr *addr = (struct in_addr *)dst;

    if(af != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    for(; *src && count < 4; ++src) {
        if(*src == '.') {
            ++count;
        }
        /* Unlike inet_aton(), inet_pton() only supports decimal parts */
        else if(*src >= '0' && *src <= '9') {
            parts[count] *= 10;
            parts[count] += *src - '0';
        }
        else {
            /* Invalid digit, and not a dot... bail */
            return 0;
        }
    }

    if(count != 3) {
        /* Not the right number of parts, bail */
        return 0;
    }

    /* Validate each part, note that unlike inet_aton(), inet_pton() only
       supports the standard xxx.xxx.xxx.xxx addresses. */
    if(parts[0] > 0xFF || parts[1] > 0xFF ||
       parts[2] > 0xFF || parts[3] > 0xFF)
        return 0;

    addr->s_addr = htonl(parts[0] << 24 | parts[1] << 16 |
                         parts[2] << 8 | parts[3]);

    return 1;
}
