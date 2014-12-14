/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   drivers/scsi.cc
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
#if 0
#include "file.hh"
#include "network.hh"
extern "C" {
#include "common.h"
#include "pci.h"
#include "interface.h"

#include "virtio.h"
#include "virtio_ring.h"
#include "virtio_pci.h"
#include "net/virtio_net.h"
#include "mach_dep.h"
extern int p9_initFs(void *p);
extern void print_vq(struct virtqueue *_vq);
extern int init_tarfs(jdriver *driver_arg);
}
#include "jdevice.h"
#endif

#if 0
/* SCSI command request, followed by CDB and data-out */
  typedef struct {
      uint8_t lun[8];              /* Logical Unit Number */
      uint64_t tag;                /* Command identifier */
      uint8_t task_attr;           /* Task attribute */
      uint8_t prio;
      uint8_t crn;
  }VirtIOSCSICmdReq;

  /* Response, followed by sense data and data-in */
  typedef struct {
      uint32_t sense_len;          /* Sense data length */
      uint32_t resid;              /* Residual bytes in data buffer */
      uint16_t status_qualifier;   /* Status qualifier */
      uint8_t status;              /* Command completion status */
      uint8_t response;            /* Response values */
 }VirtIOSCSICmdResp;


 /* Task Management Request */
  typedef struct {
      uint32_t type;
      uint32_t subtype;
      uint8_t lun[8];
      uint64_t tag;
  }VirtIOSCSICtrlTMFReq;

  typedef struct {
      uint8_t response;
  }VirtIOSCSICtrlTMFResp;
#endif


