/* KallistiOS ##version##

   dc/fs_dclsocket.h
   Copyright (C) 2007, 2008 Lawrence Sebald

*/

#ifndef __DC_FSDCLSOCKET_H
#define __DC_FSDCLSOCKET_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <dc/fs_dcload.h>

extern dbgio_handler_t dbgio_dcls;

/* Initialization */
void fs_dclsocket_init_console();
int fs_dclsocket_init();

int fs_dclsocket_shutdown();

__END_DECLS

#endif /* __DC_FSDCLSOCKET_H */
