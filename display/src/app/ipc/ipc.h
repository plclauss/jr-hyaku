#ifndef __IPC_H__
#define __IPC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

#define LINUX_INVAL_SOCKET_FD (-1)

int32_t ipcInitUNIXDomainSocket(void);
void ipcDeinitUNIXDomainSocket(const int32_t fd);

#ifdef __cplusplus
}
#endif

#endif  // __IPC_H__
