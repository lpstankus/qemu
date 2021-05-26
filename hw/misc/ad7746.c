/*
 * Analog Device AD7746 CDC
 *
 * Copyright (C) 2021 Lucas Stankus <lucas.p.stankus@gmail.com>
 *
 * Inspired by tmp105
 * Copyright (C) 2008 Nokia Corporation
 * Written by Andrzej Zaborowski <andrew@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/i2c/i2c.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/module.h"
#include "qom/object.h"

#define TYPE_AD7746 "ad7746"

/* Registers: */
/* r only */
#define AD7746_STATUS_REG       0x00
#define AD7746_CAP_DATA_H_REG   0x01
#define AD7746_CAP_DATA_M_REG   0x02
#define AD7746_CAP_DATA_L_REG   0x03
#define AD7746_VT_DATA_H_REG    0x04
#define AD7746_VT_DATA_M_REG    0x05
#define AD7746_VT_DATA_L_REG    0x06
/* rw */
#define AD7746_CAP_SETUP_REG    0x07
#define AD7746_VT_SETUP_REG     0x08
#define AD7746_EXC_SETUP_REG    0x09
#define AD7746_CONFIG_REG       0x0A
#define AD7746_CAP_DAC_A_REG    0x0B
#define AD7746_CAP_DAC_B_REG    0x0C
#define AD7746_CAP_OFFSET_H_REG 0x0D
#define AD7746_CAP_OFFSET_L_REG 0x0E
#define AD7746_CAP_GAIN_H_REG   0x0F
#define AD7746_CAP_GAIN_L_REG   0x10
#define AD7746_VOLT_GAIN_H_REG  0x11
#define AD7746_VOLT_GAIN_L_REG  0x12

typedef struct DeviceInfo {
    int model;
    const char *name;
} DeviceInfo;

static const DeviceInfo devices[] = {
    { TMP421_DEVICE_ID, "ad7746" },
    { TMP422_DEVICE_ID, "tmp422" },
    { TMP423_DEVICE_ID, "tmp423" },
};

struct AD7746State {
    /*< private >*/
    I2CSlave i2c;
    /*< public >*/

    uint8_t len;
    uint8_t buf[0x13];
    qemu_irq pin[4];

    uint8_t pointer;

    uint8_t status;
    uint32_t cap_data;
    uint32_t vt_data;

    uint8_t cap_setup;
    uint8_t vt_setup;
    uint8_t exc_setup;

    uint8_t config;

    uint8_t cap_dac[2];
    uint16_t cap_offset;
    uint16_t cap_gain;
    uint16_t volt_gain;
};

struct AD7746Class {
    I2CSlaveClass parent_class;
    DeviceInfo *dev;
};

static int ad7746_post_load(void *opaque, int version_id)
{
    return 0;
}

static const VMStateDescription vmstate_ad7150 = {
    .name = "AD7746",
    .version_id = 0,
    .minimum_version_id = 0,
    .post_load = ad7746_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(len, AD7746State),
        VMSTATE_UINT8_ARRAY(buf, AD7746State, 0x13),
        VMSTATE_UINT8(pointer, AD7746State),
        VMSTATE_I2C_SLAVE(i2c, AD7746State),
        VMSTATE_END_OF_LIST()
    },
};

static uint8_t ad7746_read(AD7746State *s)
{
    s->len = 0;

    /*
     * Device has autoincrement and there's no easy way of knowing what how much
     * data will be loaded, so we pass everything to s->buf
     */
    switch(s->pointer) {
    case AD7746_STATUS_REG:
        s->buf[s->len++] = s->status;
        /* Fallthrough */
    case AD7746_CAP_DATA_H_REG:
        s->buf[s->len++] = (s->cap_data >> 0) & 0xFF;
        /* Fallthrough */
    case AD7745_CAP_DATA_M_REG:
        s->buf[s->len++] = (s->cap_data >> 8) & 0xFF;
        /* Fallthrough */
    case AD7746_CAP_DATA_L_REG:
        s->buf[s->len++] = (s->cap_data >> 16) & 0xFF;
        /* Fallthrough */
    case AD7746_VT_DATA_H_REG:
        s->buf[s->len++] = (s->vt_data >> 0) & 0xFF;
        /* Fallthrough */
    case AD7746_VT_DATA_M_REG:
        s->buf[s->len++] = (s->vt_data >> 8) & 0xFF;
        /* Fallthrough */
    case AD7746_VT_DATA_L_REG:
        s->buf[s->len++] = (s->vt_data >> 16) & 0xFF;
        /* Fallthrough */
    case AD7746_CAP_SETUP_REG:
        s->buf[s->len++] = s->cap_setup;
        /* Fallthrough */
    case AD7746_VT_SETUP_REG:
        s->buf[s->len++] = s->vt_setup;
        /* Fallthrough */
    case AD7746_EXC_SETUP_REG:
        s->buf[s->len++] = s->exc_setup;
        /* Fallthrough */
    case AD7746_CONFIG_REG:
        s->buf[s->len++] = s->config;
        /* Fallthrough */
    case AD7746_CAP_DAC_A_REG:
        s->buf[s->len++] = s->cap_dac[0];
        /* Fallthrough */
    case AD7746_CAP_DAC_B_REG:
        s->buf[s->len++] = s->cap_dac[1];
        /* Fallthrough */
    case AD7746_CAP_OFFSET_H_REG:
        s->buf[s->len++] = (s->cap_offset >> 0) & 0xFF;
        /* Fallthrough */
    case AD7746_CAP_OFFSET_L_REG:
        s->buf[s->len++] = (s->cap_offset >> 8) & 0xFF;
        /* Fallthrough */
    case AD7746_CAP_GAIN_H_REG:
        s->buf[s->len++] = (s->cap_gain >> 0) & 0xFF;
        /* Fallthrough */
    case AD7746_CAP_GAIN_L_REG:
        s->buf[s->len++] = (s->cap_gain >> 8) & 0xFF;
        /* Fallthrough */
    case AD7746_VOLT_GAIN_H_REG:
        s->buf[s->len++] = (s->volt_gain >> 0) & 0xFF;
        /* Fallthrough */
    case AD7746_VOLT_GAIN_L_REG:
        s->buf[s->len++] = (s->volt_gain >> 8) & 0xFF;
    default:
        break;
    }
}

static void ad7150_write(AD7150State *s)
{
}

static uint8_t ad7746_rx(I2CSlave *i2c)
{
    AD7746State *s = AD7746(i2c);

    if (s->len < 4) {
        return s->buf[s->len++];
    } else {
        return 0xff;
    }
}

static int ad7746_tx(I2CSlave *i2c, uint8_t data)
{
    AD7746State *s = AD7746(i2c);

    if (s->len == 0) {
        s->pointer = data;
        s->len++;
    } else {
        if (s->len < 3) {
            s->buf[s->len - 1] = data;
        }
        ad7746_write(s);
        s->len++;
    }

    return 0;
}

static int ad7746_event(I2CSlave *i2c, enum i2c_event event)
{
    AD7746State *s = AD7746(i2c);

    if (event == I2C_START_RECV) {
        ad7746_read(s);
    }

    s->len = 0;
    return 0;
}

static void ad7746_reset(I2CSlave *i2c)
{
    AD7746State *s = AD7746(i2c);

    s->status = 7;
    s->cap_data = 0;
    s->vt_data = 0;
    s->cap_setup = 0;
    s->vt_setup = 0;
    s->exc_setup = 3;
    s->config = 0xA0;
    s->cap_dac[0] = 0;
    s->cap_dac[1] = 0;
    s->cap_offset = 0x8000;

    s->pointer = 0;
}

static void tmp105_realize(DeviceState *dev, Error **errp)
{
    I2CSlave *i2c = I2C_SLAVE(dev);
    AD7746State *s = AD7746(i2c);

    // TODO: IO pins

    ad7746_reset(&s->i2c);
}

static void ad7746_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->realize = ad7746_realize;
    k->event = ad7746_event;
    k->recv = ad7746_rx;
    k->send = ad7746_tx;
    dc->vmsd = &vmstate_ad7746;
}

static const TypeInfo ad7746_info = {
    .name          = TYPE_AD7746,
    .parent        = TYPE_I2C_SLAVE,
    .instance_size = sizeof(AD7746State),
    .instance_init = ad7746_initfn,
    .class_init    = ad7746_class_init,
};

static void ad7746_register_types(void)
{
    type_register_static(&ad7746_info);
}

type_init(ad7746_register_types)
