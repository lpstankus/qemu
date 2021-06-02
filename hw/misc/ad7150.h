/*
 * Texas Instruments TMP105 Temperature Sensor
 *
 * Browse the data sheet:
 *
 *    http://www.ti.com/lit/gpn/tmp105
 *
 * Copyright (C) 2012 Alex Horn <alex.horn@cs.ox.ac.uk>
 * Copyright (C) 2008-2012 Andrzej Zaborowski <balrogg@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later. See the COPYING file in the top-level directory.
 */
#ifndef QEMU_AD7150_H
#define QEMU_AD7150_H

#include "hw/irq.h"
#include "hw/i2c/i2c.h"
//#include "hw/misc/tmp105_regs.h"
#include "qom/object.h"

//#define TYPE_TMP105 "tmp105"
#define TYPE_AD7150 "ad7150"
OBJECT_DECLARE_SIMPLE_TYPE(AD7150State, AD7150)

/**
 * TMP105State:
 * @config: Bits 5 and 6 (value 32 and 64) determine the precision of the
 * temperature. See Table 8 in the data sheet.
 *
 * @see_also: http://www.ti.com/lit/gpn/tmp105
 */
struct AD7150State {
    /*< private >*/
    I2CSlave i2c;
    /*< public >*/

    uint8_t len;
    uint8_t buf[2];
    //qemu_irq pin;
    qemu_irq *pin;

    uint8_t pointer;
    uint8_t config;
    int16_t temperature;
    //int16_t limit[2];
    //int faults;
    //uint8_t alarm;
    int64_t cap[2];
    uint64_t cap_avg[2];
    uint64_t sensitivity_or_thresh_high[2]; //pode ser que o tipo e tamanho seja outro
    uint64_t timeout_or_thresh_low[2]; //pode ser que o tipo seja outro
    bool pwrdwn;
    bool out[2]; //pode ser que o tamanho seja outro
    /*
     * The TMP105 initially looks for a temperature rising above T_high;
     * once this is detected, the condition it looks for next is the
     * temperature falling below T_low. This flag is false when initially
     * looking for T_high, true when looking for T_low.
     */
    //bool detect_falling;
};

#endif
