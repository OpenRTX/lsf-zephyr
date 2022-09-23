#ifndef __PLIOT_APP_
#define __PLIOT_APP_

#include <w_pilot_param_module.h>
int INIT_ENGINE(IPLT_INST *ifr_inst);
int UINIT_ENGINE(IPLT_INST ifr_inst);
int SET_PARMA_ENGINE(IPLT_INST ifr_inst);
int CSK_test_function(IPLT_INST ifr_inst);

#endif //__PLIOT_APP_