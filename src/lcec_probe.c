//
//    Copyright (C) 2025 Scott Laird <scott@sigkill.org>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

/// @file
/// @brief Probe all SDOs available on a device.
///
/// This is kind of terrible, and abuses Ethercat.  This is
/// effectively the first half of code behind `ethercat sdos`, and it
/// requires access to some of the EtherCAT internals that they don't
/// export in regular headers.
///
/// I'm not at all convinced that this is a good idea, but it's better
/// than walking 0x6000..0x67ff.

#include "lcec.h"
#include <sys/ioctl.h>

////////////////////////////
// Copied from ethercat/master/ioctl.h
#define EC_IOCTL_STRING_SIZE 64

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_position;

    // outputs
    uint16_t sdo_index;
    uint8_t max_subindex;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_sdo_t;

typedef struct ec_master {
    int fd;
    uint8_t *process_data;
    size_t process_data_size;

    ec_domain_t *first_domain;
    ec_slave_config_t *first_config;

    int last_err_64bit_ref_clk_queue;
    int last_err_64bit_ref_clk;
} ec_master_t;

#define EC_IOCTL_TYPE 0xa4
#define EC_IO(nr)           _IO(EC_IOCTL_TYPE, nr)
#define EC_IOR(nr, type)   _IOR(EC_IOCTL_TYPE, nr, type)
#define EC_IOW(nr, type)   _IOW(EC_IOCTL_TYPE, nr, type)
#define EC_IOWR(nr, type) _IOWR(EC_IOCTL_TYPE, nr, type)
#define EC_IOCTL_SLAVE_SDO            EC_IOWR(0x0c, ec_ioctl_slave_sdo_t)
////////////////////////////

lcec_sdolist_t *lcec_probe_device_sdos(struct lcec_slave *slave) {
  ec_ioctl_slave_sdo_t sdo;
  ec_master_t *master;
  ec_slave_info_t slave_info;
  lcec_sdolist_t *sdos;

  master = slave->master->master;

  if (ecrt_master_get_slave(master, slave->index, &slave_info)<0) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "ecrt_master_get_slave failed\n");
  }

  rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "probe says: slave info successful for %d\n", slave->index);

  sdos = hal_malloc(sizeof(lcec_sdolist_t));
  if (sdos==NULL) return NULL;

  sdos->count=slave_info.sdo_count;
  sdos->sdos = hal_malloc(sizeof(uint16_t)*sdos->count);

  rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "probe says: %d\n", sdos->count);
  if (sdos->sdos==NULL) return NULL;
	  
  for (int i = 0; i < slave_info.sdo_count; i++) {
    sdo.slave_position = slave->index;
    sdo.sdo_position = i;
    
    if (ioctl(master->fd, EC_IOCTL_SLAVE_SDO, &sdo)==0) {
      sdos->sdos[i] = sdo.sdo_index;
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "slave %s:%s   %d: 0x%04x -> \"%s\"\n", slave->name, slave->master->name, i, sdos->sdos[i], sdo.name);
    }
  }
  return sdos;
}
