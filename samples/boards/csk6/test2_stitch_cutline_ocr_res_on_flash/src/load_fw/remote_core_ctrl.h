#ifndef _REMOTE_CORE_CTRL_H_
#define _REMOTE_CORE_CTRL_H_
#include <in_common.h>

extern struct remoteproc *remote_core_create(unsigned int cpu_id);
extern int remote_core_loadFw(struct remoteproc *rproc);
extern int remote_core_config(struct remoteproc *rproc, void *data);
extern int remote_core_start(struct remoteproc *rproc);
extern int remote_core_stop(struct remoteproc *rproc);
extern int remote_core_shutdown(struct remoteproc *rproc);
extern int remote_core_remove(struct remoteproc *rproc);

#endif
