/****************************************************************************
 * drivers/wireless/bluetooth/bt_uart_cc2564.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/* CC2564 UART based Bluetooth driver */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/kmalloc.h>

#include "bt_uart.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* The arrays below do not contain firmware. Find the firmware at ti.com.
 * Convert .bts files to C arrays as described there and merge into these
 * arrays.
 */

static const uint8_t ble_firmware[] =
{
#warning Missing CC2564 ble firmware.
  0
};

static const uint8_t cc256x_firmware[] =
{
#warning Missing CC2564 bluetooth firmware
  0
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int cc2564_send(FAR const struct btuart_lowerhalf_s *lower,
                       FAR const uint8_t *buf, ssize_t count)
{
  FAR const uint8_t *ptr = buf;
  ssize_t nwritten;

  while (count > 0)
    {
      nwritten = lower->write(lower, ptr, count);
      count   -= nwritten;
      ptr     += nwritten;
    }

  return 0;
}

static void cc2564_recv(FAR const struct btuart_lowerhalf_s *lower,
                        FAR uint8_t *buf, ssize_t count)
{
  FAR uint8_t *ptr = buf;
  ssize_t nread;

  while (count > 0)
    {
      nread  = lower->read(lower, ptr, count);
      count -= nread;
      ptr   += nread;
    }
}

static int cc2564_load(FAR const struct btuart_lowerhalf_s *lower,
                       FAR const uint8_t *chipdata)
{
  uint8_t buffer[32];
  uint8_t h4_event = 0x04;
  uint8_t h4_cmd   = 0x01;
  uint32_t length  = 0;
  FAR const uint8_t *data;

  for (data = chipdata; *data++; data += length)
    {
      uint16_t opcode;
      opcode = ((uint16_t)(*(data + 1)) << 8) + *data;

      length = data[2] + sizeof(opcode) + sizeof(data[2]);

      cc2564_send(lower, &h4_cmd, 1);
      cc2564_send(lower, data, length);

      cc2564_recv(lower, buffer, 1);
      if (h4_event == buffer[0])
        {
          cc2564_recv(lower, &buffer[1], 2);
          cc2564_recv(lower, &buffer[3], buffer[2]);
        }
      else
        {
          wlerr("ERROR: Unknown data\n");
          return -EIO;
        }
    }

  return 0;
}

int load_cc2564_firmware(FAR const struct btuart_lowerhalf_s *lower)
{
  int ret;

  /* Check for missing firmware */

  if (sizeof(cc256x_firmware) < 10 || sizeof(ble_firmware) < 10)
    {
      return -EINVAL;
    }

  ret = cc2564_load(lower, cc256x_firmware);
  if (ret == 0)
    {
      ret = cc2564_load(lower, ble_firmware);
    }

  return ret;
}

/****************************************************************************
 * Name: btuart_create
 *
 *   Create the UART-based bluetooth device.
 *
 * Input Parameters:
 *   lower - an instance of the lower half driver interface
 *
 * Returned Value:
 *   Zero is returned on success; a negated errno value is returned on any
 *   failure.
 *
 ****************************************************************************/

int btuart_create(FAR const struct btuart_lowerhalf_s *lower,
                  FAR struct bt_driver_s **driver)
{
  FAR struct btuart_upperhalf_s *upper;
  int ret;

  wlinfo("lower %p\n", lower);

  if (lower == NULL)
    {
      wlerr("ERROR: btuart lower half is NULL\n");
      return -ENODEV;
    }

  /* Allocate a new instance of the upper half driver state structure */

  upper = (FAR struct btuart_upperhalf_s *)
    kmm_zalloc(sizeof(struct btuart_upperhalf_s));

  if (upper == NULL)
    {
      wlerr("ERROR: Failed to allocate upper-half state\n");
      return -ENOMEM;
    }

  /* Initialize the upper half driver state */

  upper->dev.head_reserve = H4_HEADER_SIZE;
  upper->dev.open         = btuart_open;
  upper->dev.send         = btuart_send;
  upper->dev.close        = btuart_close;
  upper->dev.ioctl        = btuart_ioctl;
  upper->lower            = lower;

  /* Load firmware */

  ret = load_cc2564_firmware(lower);
  if (ret < 0)
    {
      wlerr("ERROR: Firmware error\n");
      kmm_free(upper);
      return -EINVAL;
    }

  *driver = &upper->dev;
  return ret;
}
