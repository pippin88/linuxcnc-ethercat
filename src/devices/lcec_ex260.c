//
// Created by chad on 2021-05-28.
//

/// @file
/// @brief Driver for SMC EX260 Valve controllers

#include "lcec.h"

static int lcec_ex260_init(int comp_id, struct lcec_slave *slave);

static lcec_typelist_t types[] = {
    {"EX260-SEC1", LCEC_SMC_VID, 0x01000001, 0, NULL, lcec_ex260_init, NULL, 4},
    {"EX260-SEC2", LCEC_SMC_VID, 0x01000002, 0, NULL, lcec_ex260_init, NULL, 4},
    {"EX260-SEC3", LCEC_SMC_VID, 0x01000003, 0, NULL, lcec_ex260_init, NULL, 2},
    {"EX260-SEC4", LCEC_SMC_VID, 0x01000004, 0, NULL, lcec_ex260_init, NULL, 2},
    {NULL},
};

ADD_TYPES(types);

typedef struct {
  hal_bit_t *sol_1a;
  hal_bit_t *sol_1b;
  hal_bit_t *sol_2a;
  hal_bit_t *sol_2b;
  hal_bit_t *sol_3a;
  hal_bit_t *sol_3b;
  hal_bit_t *sol_4a;
  hal_bit_t *sol_4b;
  unsigned int pdo_os;
  unsigned int pdo_bp;
} lcec_ex260_pin_t;

static const lcec_pindesc_t slave_pins[] = {
    {HAL_BIT, HAL_IN, offsetof(lcec_ex260_pin_t, sol_1a), "%s.%s.%s.sol-%d-1a"},
    {HAL_BIT, HAL_IN, offsetof(lcec_ex260_pin_t, sol_1b), "%s.%s.%s.sol-%d-1b"},
    {HAL_BIT, HAL_IN, offsetof(lcec_ex260_pin_t, sol_2a), "%s.%s.%s.sol-%d-2a"},
    {HAL_BIT, HAL_IN, offsetof(lcec_ex260_pin_t, sol_2b), "%s.%s.%s.sol-%d-2b"},
    {HAL_BIT, HAL_IN, offsetof(lcec_ex260_pin_t, sol_3a), "%s.%s.%s.sol-%d-3a"},
    {HAL_BIT, HAL_IN, offsetof(lcec_ex260_pin_t, sol_3b), "%s.%s.%s.sol-%d-3b"},
    {HAL_BIT, HAL_IN, offsetof(lcec_ex260_pin_t, sol_4a), "%s.%s.%s.sol-%d-4a"},
    {HAL_BIT, HAL_IN, offsetof(lcec_ex260_pin_t, sol_4b), "%s.%s.%s.sol-%d-4b"},
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

static void lcec_ex260_write(struct lcec_slave *slave, long period);

static int lcec_ex260_init(int comp_id, struct lcec_slave *slave) {
  lcec_master_t *master = slave->master;
  lcec_ex260_pin_t *hal_data;
  lcec_ex260_pin_t *pin;
  int i;
  int err;

  // initialize callbacks
  slave->proc_write = lcec_ex260_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_ex260_pin_t) * slave->flags)) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_ex260_pin_t) * slave->flags);
  slave->hal_data = hal_data;

  // initialize pins
  for (i = 0, pin = hal_data; i < slave->flags; i++, pin++) {
    // initialize PDO entry
    lcec_pdo_init(slave, 0x3101, 0x01 + i, &pin->pdo_os, &pin->pdo_bp);

    if ((err = lcec_pin_newf_list(pin, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  return 0;
}

static void lcec_ex260_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_ex260_pin_t *hal_data = (lcec_ex260_pin_t *)slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_ex260_pin_t *pin;
  int i, s;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  // set outputs
  for (i = 0, pin = hal_data; i < slave->flags; i++, pin++) {
    s = *(pin->sol_1a);
    s |= *(pin->sol_1b) << 1;
    s |= *(pin->sol_2a) << 2;
    s |= *(pin->sol_2b) << 3;
    s |= *(pin->sol_3a) << 4;
    s |= *(pin->sol_3b) << 5;
    s |= *(pin->sol_4a) << 6;
    s |= *(pin->sol_4b) << 7;
    EC_WRITE_U8(&pd[pin->pdo_os], s);
  }
}
