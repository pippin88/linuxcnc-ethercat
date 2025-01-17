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
/// @brief Driver for Beckhoff AX5200 Servo controllers

#include "lcec_ax5200.h"

#include "../lcec.h"

static int lcec_ax5200_init(int comp_id, struct lcec_slave *slave);

static lcec_modparam_desc_t lcec_ax5200_modparams[] = {
    {"enableFB2", LCEC_AX5_PARAM_ENABLE_FB2, MODPARAM_TYPE_BIT},
    {"enableDiag", LCEC_AX5_PARAM_ENABLE_DIAG, MODPARAM_TYPE_BIT},
    {NULL},
};

static lcec_typelist_t types[] = {
    {"AX5203", LCEC_BECKHOFF_VID, 0x14536012, 0, lcec_ax5200_preinit, lcec_ax5200_init, lcec_ax5200_modparams},
    {"AX5206", LCEC_BECKHOFF_VID, 0x14566012, 0, lcec_ax5200_preinit, lcec_ax5200_init, lcec_ax5200_modparams},
    {NULL},
};
ADD_TYPES(types);

typedef struct {
  lcec_syncs_t syncs;
  lcec_class_ax5_chan_t chans[LCEC_AX5200_CHANS];
} lcec_ax5200_data_t;

static const LCEC_CONF_FSOE_T fsoe_conf = {
    .slave_data_len = 2,
    .master_data_len = 2,
    .data_channels = 2,
};

static void lcec_ax5200_read(struct lcec_slave *slave, long period);
static void lcec_ax5200_write(struct lcec_slave *slave, long period);

/*static*/ int lcec_ax5200_preinit(struct lcec_slave *slave) {
  // check if already initialized
  if (slave->fsoeConf != NULL) {
    return 0;
  }

  // set FSOE conf (this will be used by the corresponding AX5805
  slave->fsoeConf = &fsoe_conf;

  return 0;
}

static int lcec_ax5200_init(int comp_id, struct lcec_slave *slave) {
  lcec_master_t *master = slave->master;
  lcec_ax5200_data_t *hal_data;
  int i;
  lcec_class_ax5_chan_t *chan;
  int err;
  char pfx[HAL_NAME_LEN];

  // initialize callbacks
  slave->proc_read = lcec_ax5200_read;
  slave->proc_write = lcec_ax5200_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_ax5200_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_ax5200_data_t));
  slave->hal_data = hal_data;

  // initialize pins
  for (i = 0; i < LCEC_AX5200_CHANS; i++) {
    chan = &hal_data->chans[i];

    // init subclasses
    rtapi_snprintf(pfx, HAL_NAME_LEN, "ch%d.", i);
    if ((err = lcec_class_ax5_init(slave, chan, i, pfx)) != 0) {
      return err;
    }
  }

  // initialize sync info
  lcec_syncs_init(&hal_data->syncs);
  lcec_syncs_add_sync(&hal_data->syncs, EC_DIR_OUTPUT, EC_WD_DEFAULT);
  lcec_syncs_add_sync(&hal_data->syncs, EC_DIR_INPUT, EC_WD_DEFAULT);

  lcec_syncs_add_sync(&hal_data->syncs, EC_DIR_OUTPUT, EC_WD_DEFAULT);
  lcec_syncs_add_pdo_info(&hal_data->syncs, 0x0018);
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0086, 0x01, 16);  // control-word
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0018, 0x01, 32);  // velo-command

  lcec_syncs_add_pdo_info(&hal_data->syncs, 0x1018);
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0086, 0x02, 16);  // control-word
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0018, 0x02, 32);  // velo-command

  lcec_syncs_add_sync(&hal_data->syncs, EC_DIR_INPUT, EC_WD_DEFAULT);
  lcec_syncs_add_pdo_info(&hal_data->syncs, 0x0010);
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0087, 0x01, 16);  // status word
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0033, 0x01, 32);  // position feedback
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0054, 0x01, 16);  // torque feedback

  if (hal_data->chans[0].fb2_enabled) {
    lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0035, 0x01, 32);  // position feedback 2
  }
  if (hal_data->chans[0].diag_enabled) {
    lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0186, 0x01, 32);  // diagnostic number
  }

  lcec_syncs_add_pdo_info(&hal_data->syncs, 0x1010);
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0087, 0x02, 16);  // status word
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0033, 0x02, 32);  // position feedback
  lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0054, 0x02, 16);  // torque feedback

  if (hal_data->chans[1].fb2_enabled) {
    lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0035, 0x02, 32);  // position feedback 2
  }
  if (hal_data->chans[1].diag_enabled) {
    lcec_syncs_add_pdo_entry(&hal_data->syncs, 0x0186, 0x02, 32);  // diagnostic number
  }
  slave->sync_info = &hal_data->syncs.syncs[0];

  return 0;
}

static void lcec_ax5200_read(struct lcec_slave *slave, long period) {
  lcec_ax5200_data_t *hal_data = (lcec_ax5200_data_t *)slave->hal_data;
  int i;
  lcec_class_ax5_chan_t *chan;

  // check inputs
  for (i = 0; i < LCEC_AX5200_CHANS; i++) {
    chan = &hal_data->chans[i];
    lcec_class_ax5_read(slave, chan);
  }
}

static void lcec_ax5200_write(struct lcec_slave *slave, long period) {
  lcec_ax5200_data_t *hal_data = (lcec_ax5200_data_t *)slave->hal_data;
  int i;
  lcec_class_ax5_chan_t *chan;

  // write outputs
  for (i = 0; i < LCEC_AX5200_CHANS; i++) {
    chan = &hal_data->chans[i];
    lcec_class_ax5_write(slave, chan);
  }
}
