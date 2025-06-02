#ifndef U_EMMC_H
#define U_EMMC_H

#include"u_ctypes.h"

int sd_init();
int sd_read_sector(unsigned int sector, unsigned char *buffer);
int sd_write_sector(unsigned int sector, const unsigned char *buffer);

#endif