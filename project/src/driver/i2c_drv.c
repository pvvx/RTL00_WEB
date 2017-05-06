/*
 * i2c_drv.c
 *
 *  Created on: 02/05/2017.
 *      Author: pvvx
 */
#include "driver/i2c_drv.h"

#if CONFIG_I2C_EN

#include "pinmap.h"

typedef struct {
    unsigned char sda;
    unsigned char scl;
    unsigned char sel;
    unsigned char id;
} PinMapI2C;

#define I2C_SEL(idx, ps) ((idx<<4) | ps)

static const PinMapI2C PinMap_I2C[] = {
	// sda, scl, sel, id
	// I2C0
    {PD_4, PD_5, I2C_SEL(0, S0), I2C0},
    {PH_1, PH_0, I2C_SEL(0, S1), I2C0},
    {PC_8, PC_9, I2C_SEL(0, S2), I2C0},
    {PE_7, PE_6, I2C_SEL(0, S3), I2C0},
	// I2C1
    {PC_4, PC_5, I2C_SEL(1, S0), I2C1},
    {PH_3, PH_2, I2C_SEL(1, S1), I2C1},
    {PD_7, PD_6, I2C_SEL(1, S2), I2C1},
	// I2C2
    {PB_7, PB_6, I2C_SEL(2, S0), I2C2},
    {PE_1, PE_0, I2C_SEL(2, S1), I2C2},
    {PC_7, PC_6, I2C_SEL(2, S2), I2C2},
	// I2C3
    {PB_3, PB_2, I2C_SEL(3, S0), I2C3},
    {PE_3, PE_2, I2C_SEL(3, S1), I2C3},
    {PE_5, PE_4, I2C_SEL(3, S2), I2C3},
    {PD_9, PD_8, I2C_SEL(3, S3), I2C3},

    {0xff, 0xff, 0, 0}
};

static void * i2c_base_reg[4] = {
		I2C0_REG_BASE,
		I2C1_REG_BASE,
		I2C2_REG_BASE,
		I2C3_REG_BASE
};

#if 1
#define test_printf
#define i2c_dump_regs(p)
#else
#define test_printf rtl_printf
LOCAL int i2c_dump_regs(i2c_drv_t *pi2c)
{
	uint32 *ptr = pi2c->base_regs;
	test_printf("%s:", __func__);
	for(int i = 0; i < (0xA0>>2); i++) {
		if(!(i%4)) {
			test_printf("\n%08x:", &ptr[i]);
		}
		test_printf("\t%08x", ptr[i]);
	}
	test_printf("\n");
}
#endif

#define i2c_reg(r) *((volatile uint32 *)(pi2c->base_regs + r))

// flg =0 write, =1 Read
LOCAL int i2c_ready(i2c_drv_t *pi2c, unsigned char flg)
{
	test_printf("%s:\n", __func__);
	// Test enable i2c
	if(!(i2c_reg(REG_DW_I2C_IC_ENABLE) & BIT_IC_ENABLE)) {
		error_printf("I2C%d disable!\n", pi2c->idx);
		pi2c->status = DRV_I2C_IC_OFF;
		return DRV_I2C_IC_OFF;
	}
	// Wait Receive FIFO is empty &  Transmit FIFO Completely Empty
	int poll_count = DRV_I2C_POOL_TIMEOUT;
	do {
		if(i2c_reg(REG_DW_I2C_IC_RAW_INTR_STAT) & BIT_IC_RAW_INTR_STAT_TX_ABRT) {
			error_printf("I2C%d Abort!\n", pi2c->idx);
			// Clear abort status.
			(volatile)i2c_reg(REG_DW_I2C_IC_CLR_TX_ABRT);
			// Be sure that all interrupts flag are cleared.
//			(volatile)i2c_reg(REG_DW_I2C_IC_CLR_INTR);
			pi2c->status = DRV_I2C_ABORT;
			return DRV_I2C_ABORT;
		}
		if(flg) {
			// Receive FIFO ready ?
			if(i2c_reg(REG_DW_I2C_IC_STATUS) & (BIT_IC_STATUS_RFNE | BIT_IC_STATUS_RFF)) {
				//			pi2c->status = DRV_I2C_IC_ENABLE;
							return DRV_I2C_OK;
			}
		}
		else {
			// Transmit FIFO is not full ?
			if(i2c_reg(REG_DW_I2C_IC_STATUS) & BIT_IC_STATUS_TFNF) {
				//			pi2c->status = DRV_I2C_IC_ENABLE;
							return DRV_I2C_OK;
			}
		}
	} while(poll_count--);
	error_printf("I2C%d Timeout!\n", pi2c->idx);
	pi2c->status = DRV_I2C_TIMEOUT;
	return DRV_I2C_TIMEOUT;
}

int _i2c_break(i2c_drv_t *pi2c)
{
	test_printf("%s\n", __func__);
//	(volatile)i2c_reg(REG_DW_I2C_IC_CLR_INTR);
	// ABORT operation
	int poll_count = DRV_I2C_POOL_TIMEOUT;
	i2c_reg(REG_DW_I2C_IC_ENABLE) |= 2;
	// Wait until controller is disabled.
	while(i2c_reg(REG_DW_I2C_IC_ENABLE) & 2) {
		if(poll_count-- <= 0) {
			error_printf("Error abort i2c%d\n", pi2c->idx);
			pi2c->status = DRV_I2C_TIMEOUT;
			return DRV_I2C_TIMEOUT;
		};
	};
	pi2c->status = DRV_I2C_OFF;
	// All interrupts flag are cleared.
	(volatile)i2c_reg(REG_DW_I2C_IC_CLR_INTR);
	return DRV_I2C_OK;
}

/* (!) вызывать после _i2c_init */
int _i2c_set_speed(i2c_drv_t *pi2c, uint32 clk_hz)
{
	test_printf("%s:\n", __func__);
	if(clk_hz < 10000 || clk_hz > HalGetCpuClk()/16) {
		error_printf("I2C%d Error clk!\n", pi2c->idx);
		return DRV_I2C_ERR;
	}
    uint32 tmp;
	uint32 CpuClkTmp = HalGetCpuClk()/clk_hz;
	switch(pi2c->mode) {
	case I2C_SS_MODE:
		// Standard Speed Clock SCL High Count
		tmp = (CpuClkTmp * I2C_SS_MIN_SCL_HTIME)/(I2C_SS_MIN_SCL_HTIME + I2C_SS_MIN_SCL_LTIME);
		i2c_reg(REG_DW_I2C_IC_SS_SCL_HCNT) = BIT_CTRL_IC_SS_SCL_HCNT(tmp);
		// Standard Speed Clock SCL Low Count
		tmp = (CpuClkTmp * I2C_SS_MIN_SCL_LTIME)/(I2C_SS_MIN_SCL_HTIME + I2C_SS_MIN_SCL_LTIME);
		i2c_reg(REG_DW_I2C_IC_SS_SCL_LCNT) = BIT_CTRL_IC_SS_SCL_LCNT(tmp);
		break;
	case I2C_HS_MODE:
		// Standard Speed Clock SCL High Count
		i2c_reg(REG_DW_I2C_IC_SS_SCL_HCNT) = BIT_CTRL_IC_SS_SCL_HCNT(400);
		// Standard Speed Clock SCL Low Count
		i2c_reg(REG_DW_I2C_IC_SS_SCL_LCNT) = BIT_CTRL_IC_SS_SCL_LCNT(470);
		// Fast Speed Clock SCL High Count
		i2c_reg(REG_DW_I2C_IC_FS_SCL_HCNT) = BIT_CTRL_IC_FS_SCL_HCNT(85);
		// Fast Speed I2C Clock SCL Low Count
		i2c_reg(REG_DW_I2C_IC_FS_SCL_LCNT) = BIT_CTRL_IC_FS_SCL_LCNT(105);
		// High Speed I2C Clock SCL High Count
		tmp = (CpuClkTmp * I2C_HS_MIN_SCL_HTIME_100)/(I2C_HS_MIN_SCL_HTIME_100 + I2C_HS_MIN_SCL_LTIME_100);
	    if (tmp > 8) tmp -= 3;
		i2c_reg(REG_DW_I2C_IC_HS_SCL_HCNT) = BIT_CTRL_IC_HS_SCL_HCNT(tmp);
		// High Speed I2C Clock SCL Low Count
		tmp = (CpuClkTmp * I2C_HS_MIN_SCL_LTIME_100)/(I2C_HS_MIN_SCL_HTIME_100 + I2C_HS_MIN_SCL_LTIME_100);
	    if (tmp > 6) tmp -= 6;
		i2c_reg(REG_DW_I2C_IC_HS_SCL_LCNT) = BIT_CTRL_IC_FS_SCL_LCNT(tmp);
		break;
//	case I2C_FS_MODE:
	default:
		pi2c->mode = I2C_FS_MODE;
		// Fast Speed Clock SCL High Count
		tmp = (CpuClkTmp * I2C_FS_MIN_SCL_HTIME)/(I2C_FS_MIN_SCL_HTIME + I2C_FS_MIN_SCL_LTIME);
        if (tmp > 4) tmp -= 4;// this part is according to the fine-tune result
		i2c_reg(REG_DW_I2C_IC_FS_SCL_HCNT) = BIT_CTRL_IC_FS_SCL_HCNT(tmp);
		// Fast Speed I2C Clock SCL Low Count
		tmp = (CpuClkTmp * I2C_FS_MIN_SCL_LTIME)/(I2C_FS_MIN_SCL_HTIME + I2C_FS_MIN_SCL_LTIME);
        if (tmp > 3) tmp -= 3; // this part is according to the fine-tune result
		i2c_reg(REG_DW_I2C_IC_FS_SCL_LCNT) = BIT_CTRL_IC_FS_SCL_LCNT(tmp);
	}
	return DRV_I2C_OK;
}

LOCAL int i2c_disable(i2c_drv_t *pi2c)
{
	test_printf("%s:\n", __func__);
	pi2c->status = DRV_I2C_IC_OFF;
	if(i2c_reg(REG_DW_I2C_IC_ENABLE_STATUS) & BIT_IC_ENABLE_STATUS_IC_EN) {
		test_printf("I2C%d Already disable!\n", pi2c->idx);
		return DRV_I2C_OK;
	}
	// Disable controller.
	int poll_count = DRV_I2C_POOL_TIMEOUT;
	i2c_reg(REG_DW_I2C_IC_ENABLE) &= ~BIT_IC_ENABLE;
	// Wait until controller is disabled.
	while(i2c_reg(REG_DW_I2C_IC_ENABLE_STATUS) & BIT_IC_ENABLE_STATUS_IC_EN) {
		if(poll_count-- <= 0) {
			error_printf("I2C%d Error disable!\n", pi2c->idx);
			pi2c->status = DRV_I2C_TIMEOUT;
			return DRV_I2C_TIMEOUT;
		};
	};
	return DRV_I2C_OK;
}

LOCAL int i2c_enable(i2c_drv_t *pi2c)
{
	test_printf("%s:\n", __func__);
	int poll_count = DRV_I2C_POOL_TIMEOUT;
	if(!(i2c_reg(REG_DW_I2C_IC_ENABLE_STATUS) & BIT_IC_ENABLE_STATUS_IC_EN)) {
		// Enable controller.
		i2c_reg(REG_DW_I2C_IC_ENABLE) = BIT_IC_ENABLE;
		// Wait until controller is enabled
		while(!(i2c_reg(REG_DW_I2C_IC_ENABLE_STATUS) & BIT_IC_ENABLE_STATUS_IC_EN)) {
			if(poll_count-- <= 0) {
				error_printf("I2C%d Error enable\n", pi2c->idx);
				pi2c->status = DRV_I2C_TIMEOUT;
				return DRV_I2C_TIMEOUT;
			};
		};
	};
	// Be sure that all interrupts flag are cleared.
	(volatile)i2c_reg(REG_DW_I2C_IC_CLR_INTR);
	pi2c->status = DRV_I2C_IC_ENABLE;
	return DRV_I2C_OK;
}

// IC On & Enable CLK
LOCAL void _i2c_ic_on(i2c_drv_t *pi2c)
{
	test_printf("%s:\n", __func__);
	uint32 tmp = 1 << (pi2c->idx << 1);
	HAL_PERI_ON_WRITE32(REG_PESOC_PERI_CLK_CTRL1,
			HAL_PERI_ON_READ32(REG_PESOC_PERI_CLK_CTRL1) | tmp);
	HAL_PERI_ON_WRITE32(REG_PESOC_PERI_CLK_CTRL1,
			HAL_PERI_ON_READ32(REG_PESOC_PERI_CLK_CTRL1) | (tmp << 1));
	tmp = BIT_PERI_I2C0_EN << pi2c->idx;
	HAL_PERI_ON_WRITE32(REG_SOC_PERI_FUNC0_EN,
			HAL_PERI_ON_READ32(REG_SOC_PERI_FUNC0_EN) & (~tmp));
	HAL_PERI_ON_WRITE32(REG_SOC_PERI_FUNC0_EN,
			HAL_PERI_ON_READ32(REG_SOC_PERI_FUNC0_EN) | tmp);

	tmp = HAL_READ32(PERI_ON_BASE, REG_PESOC_CLK_SEL);
	tmp &= (~(BIT_PESOC_PERI_SCLK_SEL(3)));
	HAL_WRITE32(PERI_ON_BASE, REG_PESOC_CLK_SEL, tmp);

	HalPinCtrlRtl8195A(I2C0 + pi2c->idx, pi2c->io_sel, 1);
}

// IC Off & Disable CLK
void _i2c_ic_off(i2c_drv_t *pi2c)
{
	test_printf("%s:\n", __func__);
	if(pi2c->status) {
		// mask all interrupts
		i2c_reg(REG_DW_I2C_IC_INTR_MASK) = 0;
		// Disable (Abort I2C Controller)
		_i2c_break(pi2c);
		i2c_disable(pi2c);
		uint32 mask = BIT_PERI_I2C0_EN << pi2c->idx;
		HAL_PERI_ON_WRITE32(REG_SOC_PERI_FUNC0_EN,
				HAL_PERI_ON_READ32(REG_SOC_PERI_FUNC0_EN) | mask);
		HalPinCtrlRtl8195A(I2C0 + pi2c->idx, pi2c->io_sel, 0);
#ifdef CONFIG_SOC_PS_MODULE
		REG_POWER_STATE i2cPwrState;
		// To register a new peripheral device power state
		i2cPwrState.FuncIdx = I2C0 + pi2c->idx;
		i2cPwrState.PwrState = INACT;
		RegPowerState(i2cPwrState);
#endif
		pi2c->status = DRV_I2C_OFF;
	}
}

/* (!) вызывать до _i2c_init, если параметрв драйвера не заданы в i2c_drv_t */
int _i2c_setup(i2c_drv_t *pi2c, PinName sda, PinName scl, unsigned char mode)
{
	test_printf("%s:\n", __func__);
	if(mode < DRV_I2C_SS_MODE || mode > DRV_I2C_HS_MODE) {
	    error_printf("I2C Error mode!\n");
	    return DRV_I2C_ERR;
	}
	// Pins -> index
	PinMapI2C *p =  PinMap_I2C;
	while(p->sda != 0xFF) {
		if(p->sda == sda && p->scl == scl) {
			pi2c->io_sel = RTL_GET_PERI_SEL(p->sel);
			pi2c->idx = RTL_GET_PERI_IDX(p->sel);
		    pi2c->mode = mode;
			return DRV_I2C_OK;
		}
		p++;
	}
    error_printf("I2C Error pins!\n");
    return DRV_I2C_ERR;
}

/* (!) Использует заполненную структуру i2c_drv_t */
int _i2c_init(i2c_drv_t *pi2c)
{
	test_printf("%s:\n", __func__);
    // Set base address regs i2c
    pi2c->base_regs = i2c_base_reg[pi2c->idx];
    // IC On & Enable CLK
    if(pi2c->status == DRV_I2C_OFF) _i2c_ic_on(pi2c);
	// mask all interrupts
	i2c_reg(REG_DW_I2C_IC_INTR_MASK) =  0;
	// disable i2c
	if(i2c_disable(pi2c)) return pi2c->status;
	// Set Control Register:
	// bit0: master enabled,
	// bit1..2: fast mode (400 kbit/s), ...
	// bit2: Slave Addressing Mode 7-bit
	// bit4: Master Addressing Mode 7-bit
	// bit5: Restart disable
	// bit6: Slave Mode Disable
	// bit7: STOP_DET_IFADDRESSED
	// bit8: TX_EMPTY_CTRL
	// bit9: RX_FIFO_FULL_HLD_CTRL
	// Set MASTER_MODE
	i2c_reg(REG_DW_I2C_IC_CON) =
			BIT_CTRL_IC_CON_MASTER_MODE(1)
		|	BIT_IC_CON_SPEED(pi2c->mode)
		|	BIT_CTRL_IC_CON_IC_10BITADDR_SLAVE(0)
		|	BIT_CTRL_IC_CON_IC_10BITADDR_MASTER(0)
		| 	BIT_CTRL_IC_CON_IC_RESTART_EN(1)
		|	BIT_CTRL_IC_CON_IC_SLAVE_DISABLE(1);
	// Master Target Address
	//	i2c_reg(REG_DW_I2C_IC_TAR) = 0x40;
	// Slave Address
	//	i2c_reg(REG_DW_I2C_IC_SAR) = 0x55;
	// High Speed Master ID (00001xxx) bit0..2
//	i2c_reg(REG_DW_I2C_IC_HS_MADDR) = BIT_CTRL_IC_HS_MADDR(0x4);
	// Standard Speed Clock SCL High Count (100kHz)
	i2c_reg(REG_DW_I2C_IC_SS_SCL_HCNT) = BIT_CTRL_IC_SS_SCL_HCNT(400);
	// Standard Speed Clock SCL Low Count (100kHz)
	i2c_reg(REG_DW_I2C_IC_SS_SCL_LCNT) = BIT_CTRL_IC_SS_SCL_LCNT(470);
	// Fast Speed Clock SCL High Count (400kHz)
	i2c_reg(REG_DW_I2C_IC_FS_SCL_HCNT) = BIT_CTRL_IC_FS_SCL_HCNT(80);
	// Fast Speed I2C Clock SCL Low Count (400kHz)
	i2c_reg(REG_DW_I2C_IC_FS_SCL_LCNT) = BIT_CTRL_IC_FS_SCL_LCNT(100);
	// High Speed I2C Clock SCL High Count (1MHz)
	i2c_reg(REG_DW_I2C_IC_HS_SCL_HCNT) = BIT_CTRL_IC_HS_SCL_HCNT(30);
	// High Speed I2C Clock SCL Low Count (1MHz)
	i2c_reg(REG_DW_I2C_IC_HS_SCL_LCNT) = BIT_CTRL_IC_FS_SCL_LCNT(40);
	// SDA Hold (IC_CLK period, when I2C controller acts as a transmitter/receiver)
	i2c_reg(REG_DW_I2C_IC_SDA_HOLD) = BIT_CTRL_IC_SDA_HOLD(10);
	// General Call Ack
	i2c_reg(REG_DW_I2C_IC_ACK_GENERAL_CALL) = BIT_CTRL_IC_ACK_GENERAL_CALL(1);
	// Receive FIFO Threshold Level
	//	i2c_reg(REG_DW_I2C_IC_RX_TL) = 0x0;
	// Transmit FIFO Threshold Level
	//	i2c_reg(REG_DW_I2C_IC_TX_TL) = 0x0;
	// Transmit Abort Source
	//	i2c_reg(REG_DW_I2C_IC_TX_ABRT_SOURCE) = 0x0;
	// DMA Transmit Data Level Register
	//	i2c_reg(REG_DW_I2C_IC_DMA_TDLR) = 0x09;

#ifdef CONFIG_SOC_PS_MODULE
        REG_POWER_STATE i2cPwrState;
        // To register a new peripheral device power state
        i2cPwrState.FuncIdx = I2C0 + pi2c->idx;
        i2cPwrState.PwrState = ACT;
        RegPowerState(i2cPwrState);
#endif
	i2c_dump_regs(pi2c);
//	pi2c->status = DRV_I2C_IC_OFF;
    return DRV_I2C_OK;
}

int _i2c_write(i2c_drv_t *pi2c, uint32 address, const char *data, int length, int stop)
{
	test_printf("%s: [%d]%d\n", __func__, length, stop);
	uint8_t *d = (uint8_t *)data;
	// Write slave address to TAR.
	// bit12: = 1 - 10-bit addressing mode when acting as a master
	// bit11: = 1 - Special Command Enable
	// bit10: = 1 - Special Command Type START BYTE
	i2c_reg(REG_DW_I2C_IC_TAR) = address;
	// Enable controller
	if(i2c_enable(pi2c)) return pi2c->status;
	while (length--) {
		// Transmit FIFO is not full
		if(i2c_ready(pi2c, 0))	return pi2c->status; // BIT_IC_STATUS_TFNF
		// Fill IC_DATA_CMD[7:0] with the data.
		// Send stop after last byte ?
		if(length == 0 && stop)	i2c_reg(REG_DW_I2C_IC_DATA_CMD) = *d | BIT_IC_DATA_CMD_STOP;
		else i2c_reg(REG_DW_I2C_IC_DATA_CMD) = *d;
		d++;
	}
	// Disable controller.
	if(stop) {
		if(i2c_disable(pi2c)) return pi2c->status;
	}
	return DRV_I2C_OK;
}

int _i2c_read(i2c_drv_t *pi2c, uint32 address, const char *data, int length, int stop)
{
	test_printf("%s: [%d]%d\n", __func__, length, stop);
	uint8_t *d = (uint8_t *)data;
	int len = length;
	// Write slave address to TAR.
	// bit12: = 1 - 10-bit addressing mode when acting as a master
	// bit11: = 1 - Special Command Enable
	// bit10: = 1 - Special Command Type START BYTE
	i2c_reg(REG_DW_I2C_IC_TAR) = address;
	// Enable controller.
	if(i2c_enable(pi2c)) return pi2c->status;
	while (len--) {
		// Transmit FIFO is not full
		if(i2c_ready(pi2c, 0))	return pi2c->status; // BIT_IC_STATUS_TFE
		// Send stop after last byte ?
		if (len == 0 && stop) {
			// bit10: = 1 - Restart Bit Control
			// bit9: = 1 - Stop Bit Control
			// bit8: = 1 - Command read / = 0 Command write
			i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD | BIT_IC_DATA_CMD_STOP;
		} else {
			//  Read command -IC_DATA_CMD[8] = 1.
			i2c_reg(REG_DW_I2C_IC_DATA_CMD) = BIT_IC_DATA_CMD_CMD;
		}
		// Receive FIFO ready?
		if(i2c_reg(REG_DW_I2C_IC_STATUS) & (BIT_IC_STATUS_RFNE | BIT_IC_STATUS_RFF)) {
			// IC_DATA_CMD[7:0] contains received data.
			if(length)	{
				*d++ = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
				length--;
			}
			else (volatile) i2c_reg(REG_DW_I2C_IC_DATA_CMD);
		};
	}
	while(length) {
		// Receive FIFO ready?
		if(i2c_ready(pi2c, 1))	return pi2c->status; // BIT_IC_STATUS_TFE
		// IC_DATA_CMD[7:0] contains received data.
		*d++ = i2c_reg(REG_DW_I2C_IC_DATA_CMD);
		length--;
	};
	// Disable controller.
	if(stop) {
		if(i2c_disable(pi2c)) return pi2c->status;
	}
	return DRV_I2C_OK;
}

#endif // CONFIG_I2C_EN
