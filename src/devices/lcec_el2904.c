//
//    Copyright (C) 2018 Sascha Ittner <sascha.ittner@modusoft.de>
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
/// @brief Driver for Beckhoff EL2904 4-port safety output

#include "../lcec.h"

static void lcec_el2904_read(struct lcec_slave *slave, long period);
static void lcec_el2904_write(struct lcec_slave *slave, long period);
static int lcec_el2904_preinit(struct lcec_slave *slave);
static int lcec_el2904_init(int comp_id, struct lcec_slave *slave);

static lcec_typelist_t types[] = {
    {"EL2904", LCEC_BECKHOFF_VID, 0x0B583052, 0, lcec_el2904_preinit, lcec_el2904_init},
    {NULL},
};
ADD_TYPES(types);

typedef struct {
  hal_u32_t *fsoe_master_cmd;
  hal_u32_t *fsoe_master_crc;
  hal_u32_t *fsoe_master_connid;

  hal_u32_t *fsoe_slave_cmd;
  hal_u32_t *fsoe_slave_crc;
  hal_u32_t *fsoe_slave_connid;

  hal_bit_t *fsoe_out_0;
  hal_bit_t *fsoe_out_1;
  hal_bit_t *fsoe_out_2;
  hal_bit_t *fsoe_out_3;

  hal_bit_t *out_0;
  hal_bit_t *out_1;
  hal_bit_t *out_2;
  hal_bit_t *out_3;

  unsigned int fsoe_master_cmd_os;
  unsigned int fsoe_master_crc_os;
  unsigned int fsoe_master_connid_os;

  unsigned int fsoe_slave_cmd_os;
  unsigned int fsoe_slave_crc_os;
  unsigned int fsoe_slave_connid_os;

  unsigned int fsoe_out_0_os;
  unsigned int fsoe_out_0_bp;
  unsigned int fsoe_out_1_os;
  unsigned int fsoe_out_1_bp;
  unsigned int fsoe_out_2_os;
  unsigned int fsoe_out_2_bp;
  unsigned int fsoe_out_3_os;
  unsigned int fsoe_out_3_bp;

  unsigned int out_0_os;
  unsigned int out_0_bp;
  unsigned int out_1_os;
  unsigned int out_1_bp;
  unsigned int out_2_os;
  unsigned int out_2_bp;
  unsigned int out_3_os;
  unsigned int out_3_bp;

} lcec_el2904_data_t;

static const lcec_pindesc_t slave_pins[] = {{HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_master_cmd), "%s.%s.%s.fsoe-master-cmd"},
    {HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_master_crc), "%s.%s.%s.fsoe-master-crc"},
    {HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_master_connid), "%s.%s.%s.fsoe-master-connid"},
    {HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-slave-cmd"},
    {HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_slave_crc), "%s.%s.%s.fsoe-slave-crc"},
    {HAL_U32, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_slave_connid), "%s.%s.%s.fsoe-slave-connid"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_out_0), "%s.%s.%s.fsoe-out-0"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_out_1), "%s.%s.%s.fsoe-out-1"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_out_2), "%s.%s.%s.fsoe-out-2"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el2904_data_t, fsoe_out_3), "%s.%s.%s.fsoe-out-3"},
    {HAL_BIT, HAL_IN, offsetof(lcec_el2904_data_t, out_0), "%s.%s.%s.out-0"},
    {HAL_BIT, HAL_IN, offsetof(lcec_el2904_data_t, out_1), "%s.%s.%s.out-1"},
    {HAL_BIT, HAL_IN, offsetof(lcec_el2904_data_t, out_2), "%s.%s.%s.out-2"},
    {HAL_BIT, HAL_IN, offsetof(lcec_el2904_data_t, out_3), "%s.%s.%s.out-3"}, {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL}};

static const LCEC_CONF_FSOE_T fsoe_conf = {.slave_data_len = 1, .master_data_len = 1, .data_channels = 1};

static int lcec_el2904_preinit(struct lcec_slave *slave) {
  // set fsoe config
  slave->fsoeConf = &fsoe_conf;

  return 0;
}

static int lcec_el2904_init(int comp_id, struct lcec_slave *slave) {
  lcec_master_t *master = slave->master;
  lcec_el2904_data_t *hal_data;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el2904_read;
  slave->proc_write = lcec_el2904_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el2904_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el2904_data_t));
  slave->hal_data = hal_data;

  // initialize POD entries
  lcec_pdo_init(slave, 0x7000, 0x01, &hal_data->fsoe_master_cmd_os, NULL);
  lcec_pdo_init(slave, 0x7001, 0x01, &hal_data->fsoe_out_0_os, &hal_data->fsoe_out_0_bp);
  lcec_pdo_init(slave, 0x7001, 0x02, &hal_data->fsoe_out_1_os, &hal_data->fsoe_out_1_bp);
  lcec_pdo_init(slave, 0x7001, 0x03, &hal_data->fsoe_out_2_os, &hal_data->fsoe_out_2_bp);
  lcec_pdo_init(slave, 0x7001, 0x04, &hal_data->fsoe_out_3_os, &hal_data->fsoe_out_3_bp);
  lcec_pdo_init(slave, 0x7000, 0x02, &hal_data->fsoe_master_crc_os, NULL);
  lcec_pdo_init(slave, 0x7000, 0x03, &hal_data->fsoe_master_connid_os, NULL);
  lcec_pdo_init(slave, 0x7010, 0x01, &hal_data->out_0_os, &hal_data->out_0_bp);
  lcec_pdo_init(slave, 0x7010, 0x02, &hal_data->out_1_os, &hal_data->out_1_bp);
  lcec_pdo_init(slave, 0x7010, 0x03, &hal_data->out_2_os, &hal_data->out_2_bp);
  lcec_pdo_init(slave, 0x7010, 0x04, &hal_data->out_3_os, &hal_data->out_3_bp);
  lcec_pdo_init(slave, 0x6000, 0x01, &hal_data->fsoe_slave_cmd_os, NULL);
  lcec_pdo_init(slave, 0x6000, 0x03, &hal_data->fsoe_slave_crc_os, NULL);
  lcec_pdo_init(slave, 0x6000, 0x04, &hal_data->fsoe_slave_connid_os, NULL);

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  return 0;
}

static void lcec_el2904_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el2904_data_t *hal_data = (lcec_el2904_data_t *)slave->hal_data;
  uint8_t *pd = master->process_data;

  copy_fsoe_data(slave, hal_data->fsoe_slave_cmd_os, hal_data->fsoe_master_cmd_os);

  *(hal_data->fsoe_slave_cmd) = EC_READ_U8(&pd[hal_data->fsoe_slave_cmd_os]);
  *(hal_data->fsoe_slave_crc) = EC_READ_U16(&pd[hal_data->fsoe_slave_crc_os]);
  *(hal_data->fsoe_slave_connid) = EC_READ_U16(&pd[hal_data->fsoe_slave_connid_os]);

  *(hal_data->fsoe_master_cmd) = EC_READ_U8(&pd[hal_data->fsoe_master_cmd_os]);
  *(hal_data->fsoe_master_crc) = EC_READ_U16(&pd[hal_data->fsoe_master_crc_os]);
  *(hal_data->fsoe_master_connid) = EC_READ_U16(&pd[hal_data->fsoe_master_connid_os]);

  *(hal_data->fsoe_out_0) = EC_READ_BIT(&pd[hal_data->fsoe_out_0_os], hal_data->fsoe_out_0_bp);
  *(hal_data->fsoe_out_1) = EC_READ_BIT(&pd[hal_data->fsoe_out_1_os], hal_data->fsoe_out_1_bp);
  *(hal_data->fsoe_out_2) = EC_READ_BIT(&pd[hal_data->fsoe_out_2_os], hal_data->fsoe_out_2_bp);
  *(hal_data->fsoe_out_3) = EC_READ_BIT(&pd[hal_data->fsoe_out_3_os], hal_data->fsoe_out_3_bp);
}

static void lcec_el2904_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el2904_data_t *hal_data = (lcec_el2904_data_t *)slave->hal_data;
  uint8_t *pd = master->process_data;

  EC_WRITE_BIT(&pd[hal_data->out_0_os], hal_data->out_0_bp, *(hal_data->out_0));
  EC_WRITE_BIT(&pd[hal_data->out_1_os], hal_data->out_1_bp, *(hal_data->out_1));
  EC_WRITE_BIT(&pd[hal_data->out_2_os], hal_data->out_2_bp, *(hal_data->out_2));
  EC_WRITE_BIT(&pd[hal_data->out_3_os], hal_data->out_3_bp, *(hal_data->out_3));
}
