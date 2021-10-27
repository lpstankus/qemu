/*
 * Analog Device ADXL313
 *
 * Copyright (c) 2021 Lucas Stankus <lucas.p.stankus@gmail.com>
 *
 * This code is licensed under the GNU GPL v2.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 *
 * Note this is a bunch of hacks to test the corners of the adxl313 driver
 * that cared about. It is neither complete nor necessarily correct.
 */

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/ssi/ssi.h"
#include "migration/vmstate.h"
#include "qemu/module.h"
#include "ui/console.h"
#include "qom/object.h"

/* ADXL313 register definitions */
#define ADXL313_REG_DEVID0		0x00
#define ADXL313_REG_DEVID1		0x01
#define ADXL313_REG_PARTID		0x02
#define ADXL313_REG_OFS_AXIS_X		0x1e
#define ADXL313_REG_OFS_AXIS_Y		0x1f
#define ADXL313_REG_OFS_AXIS_Z		0x20
#define ADXL313_REG_BW_RATE		0x2c
#define ADXL313_REG_POWER_CTL		0x2d
#define ADXL313_REG_DATA_FORMAT		0x31
#define ADXL313_REG_DATA_AXIS_X0	0x32
#define ADXL313_REG_DATA_AXIS_X1	0x33
#define ADXL313_REG_DATA_AXIS_Y0	0x34
#define ADXL313_REG_DATA_AXIS_Y1	0x35
#define ADXL313_REG_DATA_AXIS_Z0	0x36
#define ADXL313_REG_DATA_AXIS_Z1	0x37

#define ADXL313_SPI_STANDBY	0
#define ADXL313_SPI_READ	1
#define ADXL313_SPI_MULREAD	2
#define ADXL313_SPI_WRITE	3

struct ADXL313State {
	SSIPeripheral ssidev;
	uint8_t address;
	uint8_t spi_mode;
	uint8_t multiple_read;
	uint8_t bw_rate;
	uint16_t accels[3];
	uint8_t offsets[3];
};

#define TYPE_ADXL313 "adxl313"
OBJECT_DECLARE_SIMPLE_TYPE(ADXL313State, ADXL313)

static uint32_t adxl313_read(SSIPeripheral *dev)
{
	ADXL313State *s = ADXL313(dev);

	switch (s->address) {
	case ADXL313_REG_DEVID0:
		return 0xad;
	case ADXL313_REG_DEVID1:
		return 0x1d;
	case ADXL313_REG_PARTID:
		return 0xcb;
	case ADXL313_REG_POWER_CTL:
		return 0x4b;
	case ADXL313_REG_DATA_FORMAT:
		return 0x0b;
	case ADXL313_REG_BW_RATE:
		return s->bw_rate;
	case ADXL313_REG_DATA_AXIS_X0:
		return (s->accels[0] + s->offsets[0]) & 0xff;
	case ADXL313_REG_DATA_AXIS_X1:
		return ((s->accels[0] + s->offsets[0]) >> 8) & 0xff;
	case ADXL313_REG_DATA_AXIS_Y0:
		return (s->accels[1] + s->offsets[1]) & 0xff;
	case ADXL313_REG_DATA_AXIS_Y1:
		return ((s->accels[1] + s->offsets[1]) >> 8) & 0xff;
	case ADXL313_REG_DATA_AXIS_Z0:
		return (s->accels[2] + s->offsets[2]) & 0xff;
	case ADXL313_REG_DATA_AXIS_Z1:
		return ((s->accels[2] + s->offsets[2]) >> 8) & 0xff;
	case ADXL313_REG_OFS_AXIS_X:
		return s->offsets[0];
	case ADXL313_REG_OFS_AXIS_Y:
		return s->offsets[1];
	case ADXL313_REG_OFS_AXIS_Z:
		return s->offsets[2];
	}

	return 0;
}

static void adxl313_write(SSIPeripheral *dev, uint8_t value)
{
	ADXL313State *s = ADXL313(dev);

	switch (s->address) {
	case ADXL313_REG_BW_RATE:
		s->bw_rate = value;
		break;
	case ADXL313_REG_OFS_AXIS_X:
		s->offsets[0] = value;
		break;
	case ADXL313_REG_OFS_AXIS_Y:
		s->offsets[1] = value;
		break;
	case ADXL313_REG_OFS_AXIS_Z:
		s->offsets[2] = value;
		break;
	}
}

static uint32_t adxl313_transfer(SSIPeripheral *dev, uint32_t value)
{
	ADXL313State *s = ADXL313(dev);
	uint32_t out = 0;

	switch (s->spi_mode) {
	case ADXL313_SPI_MULREAD:
		if (value == 0) {
			out = adxl313_read(dev);
			s->address += 1;
			break;
		}
		s->spi_mode = ADXL313_SPI_STANDBY;
		/* fallthrough */
	case ADXL313_SPI_STANDBY:
		s->address = value & 0x3F;
		switch (value >> 6) {
		case 0:
			s->spi_mode = ADXL313_SPI_WRITE;
			break;
		case 1:
			// Undefined behaviour for multiple writes
			break;
		case 2:
			s->spi_mode = ADXL313_SPI_READ;
			break;
		case 3:
			s->spi_mode = ADXL313_SPI_MULREAD;
			break;
		}
		break;
	case ADXL313_SPI_READ:
		out = adxl313_read(dev);
		s->spi_mode = ADXL313_SPI_STANDBY;
		break;
	case ADXL313_SPI_WRITE:
		adxl313_write(dev, value);
		s->spi_mode = ADXL313_SPI_STANDBY;
		break;
	}

	return out;
}

static const VMStateDescription vmstate_adxl313 = {
	.name = "adxl313",
	.version_id = 1,
	.minimum_version_id = 1,
	.fields = (VMStateField[]) {
		VMSTATE_SSI_PERIPHERAL(ssidev, ADXL313State),
		VMSTATE_UINT8(address, ADXL313State),
		VMSTATE_UINT8(spi_mode, ADXL313State),
		VMSTATE_UINT8(bw_rate, ADXL313State),
		VMSTATE_UINT16_ARRAY(accels, ADXL313State, 3),
		VMSTATE_UINT8_ARRAY(offsets, ADXL313State, 3),
		VMSTATE_END_OF_LIST()
	}
};

static void adxl313_realize(SSIPeripheral *d, Error **errp)
{
	ADXL313State *s = ADXL313(d);

	s->accels[0] = 200;
	s->accels[1] = 400;
	s->accels[2] = 800;
	s->offsets[0] = 0;
	s->offsets[1] = 0;
	s->offsets[2] = 0;

	vmstate_register(NULL, VMSTATE_INSTANCE_ID_ANY, &vmstate_adxl313, s);
}

static void adxl313_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);
	SSIPeripheralClass *k = SSI_PERIPHERAL_CLASS(klass);

	k->realize = adxl313_realize;
	k->transfer = adxl313_transfer;
	set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
}

static const TypeInfo adxl313_info = {
	.name          = TYPE_ADXL313,
	.parent        = TYPE_SSI_PERIPHERAL,
	.instance_size = sizeof(ADXL313State),
	.class_init    = adxl313_class_init,
};

static void adxl313_register_types(void)
{
	type_register_static(&adxl313_info);
}

type_init(adxl313_register_types)
