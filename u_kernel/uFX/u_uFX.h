// uX is a API for graphic & sound & paralel processing

// Warnings about Compute Modules
// Compute modules must not contain any EL0/EL1 SuperRH request or jump to unrelated el0/el1 area
// Kernel only gives them permission to access video memory

// Note: uX commands are not thread safe. They must be hadled by SuperRH Call

#ifndef U_UX_H
#define U_UX_H

#include <u_kernel/util/u_ctypes.h>

#define uX_VERSION "uX100"

#endif