#ifndef _MYDRM_API_H
#define _MYDRM_API_H

#include <linux/ioctl.h>

#define MYDRM_IOC_MAGIC 'Y'

#define MYDRM_GET_PCI_MMIO _IO(MYDRM_IOC_MAGIC, 1) 
#define MYDRM_GET_PCI_LFB  _IO(MYDRM_IOC_MAGIC, 2) 
#define MYDRM_GET_PCI_ROM  _IO(MYDRM_IOC_MAGIC, 3) 

#endif
