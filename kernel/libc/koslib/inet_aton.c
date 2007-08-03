/* KallistiOS ##version##

   inet_aton.c
   Copyright (C) 2007 Lawrence Sebald

*/

#include <arpa/inet.h>

int inet_aton(const char *cp, struct in_addr *pin) {
    int parts[4] = { 0 };
    int count = 0;
    int base = 0;
    char tmp;

    for(; *cp && count < 4; ++cp) {
        if(*cp == '.') {
            ++count;
            base = 0;
        }
        else if(base == 0) {
            /* Determine which base this part is in */
            if(*cp == '0') {
                tmp = *++cp;

                if(tmp == '.') {
                    base = 0;
                    parts[count++] = 0;
                }
                else if(tmp == '\0') {
                    base = 0;
                    parts[count] = 0;
                    --cp;
                }
                else if(tmp != 'x' && tmp != 'X') {
                    /* Octal, handle the character just read too. */
                    base = 8;
                    parts[count] = *cp - '0';
                }
                else {
                    /* Hexadecimal */
                    base = 16;
                }
            }
            else if(*cp > '0' && *cp <= '9') {
                /* Decimal, handle the digit */
                base = 10;
                parts[count] = *cp - '0';
            }
            else {
                /* Non-number starting character... bail out. */
                return 0;
            }
        }
        else if(base == 10 && *cp >= '0' && *cp <= '9') {
            parts[count] *= 10;
            parts[count] += *cp - '0';
        }
        else if(base == 8 && *cp >= '0' && *cp <= '7') {
            parts[count] <<= 3;
            parts[count] += *cp - '0';
        }
        else if(base == 16) {
            parts[count] <<= 4;

            if(*cp >= '0' && *cp <= '9') {
                parts[count] += *cp - '0';
            }
            else if(*cp >= 'A' && *cp <= 'F') {
                parts[count] += *cp - 'A' + 10;
            }
            else if(*cp >= 'a' && *cp <= 'f') {
                parts[count] += *cp - 'a' + 10;
            }
            else {
                /* Invalid hex digit */
                return 0;
            }
        }
        else {
            /* Invalid digit, and not a dot... bail */
            return 0;
        }
    }

    if(count == 4) {
        /* Too many dots, bail out */
        return 0;
    }

    /* Validate each part */
    if(count == 0) {
        /* Easiest case, store our computed part, and it's done */
        pin->s_addr = htonl(parts[0]);
    }
    else if(count == 1) {
        if(parts[0] > 0xFF || parts[1] > 0xFFFFFF)
            return 0;

        pin->s_addr = htonl(parts[0] << 24 | parts[1]);
    }
    else if(count == 2) {
        if(parts[0] > 0xFF || parts[1] > 0xFF || parts[2] > 0xFFFF)
            return 0;

        pin->s_addr = htonl(parts[0] << 24 | parts[1] << 16 | parts[2]);
    }
    else {
        if(parts[0] > 0xFF || parts[1] > 0xFF ||
           parts[2] > 0xFF || parts[3] > 0xFF)
            return 0;

        pin->s_addr = htonl(parts[0] << 24 | parts[1] << 16 |
                            parts[2] << 8 | parts[3]);
    }

    return 1;
}
