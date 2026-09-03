#include "tlsr825x.h"
#include "printf.h"

struct uart_tx_buf {
	volatile uint32_t len;
	volatile uint8_t data[32 - 4];
};

static struct uart_tx_buf uart_tx_buf;

void delay_micros(uint32_t us)
{
	int now = SYSTIM->TICK;
	int target = now + (us << 4);
	do {
		now = SYSTIM->TICK;
	} while (target - now > 0) ;
}

#define AREG_INITVAL(reg, val) ((reg & 0xff) | (val << 8))

void afe_initlist(const uint16_t *regvals)
{
	int saved;
	uint16_t rv;
	irq_save(saved);
	while ((rv = *regvals++) != 0) {
		AFE->ADDRVAL = rv;
		AFE->CMD = 0x60;
		while (AFE->CMD);
	};
	irq_restore(saved);
}

void afe_write(uint8_t reg, uint8_t val)
{
	int saved;
	irq_save(saved);
	AFE->ADDR = reg;
	AFE->VALUE = val;
	AFE->CMD = 0x60;
	while (AFE->CMD);
	irq_restore(saved);
}

uint8_t afe_read(uint8_t reg)
{
	int saved;
	uint8_t val;
	irq_save(saved);
	AFE->ADDR = reg;
	AFE->CMD = 0x40;
	while (AFE->CMD);
	val = AFE->VALUE;
	irq_restore(saved);
	return val;
}

int putchar(int c)
{
	while (!(UART->TXRX_STATUS & 1)); /* Wait for TX_DONE */
	uart_tx_buf.len = 1;
	uart_tx_buf.data[0] = c;
	DMA->TX_RDY |= (1 << 1); /* UART_TX channel */
	return c;
}

static void gpio_write(enum gpio_pin pin, unsigned value)
{
	volatile uint8_t *base = &GPIO->PA.OUT;
	unsigned ofs = pin & ~7;
	unsigned idx = pin & 7;
	uint8_t v = base[ofs];
	if (value) {
		v |= (1 << idx);
	} else {
		v &= ~(1 << idx);
	}
	base[ofs] = v;
}

static void gpio_config(enum gpio_pin pin, enum gpio_mode mode, enum gpio_pull pull, uint8_t altmode)
{
	unsigned bank = pin >> 3;
	unsigned idx = pin & 7;
	uint8_t mask = 1 << idx;
	unsigned altshft = idx << 1;
	uint16_t altmask = 3 << altshft;
	unsigned oen = 1;
	unsigned ie = 0;
	switch (mode) {
	case PIN_DISABLED: break;
	case PIN_INPUT:
		ie = 1;
		break;
	case PIN_OUTPUT:
		oen = 0;
		break;
	case PIN_INOUT:
		ie = 1;
		oen = 0;
		break;
	};
	if (oen) {
		GPIO->PORT[bank].OEN &= ~mask;
	} else {
		GPIO->PORT[bank].OEN |= mask;
	}
	if (bank == 1 || bank == 2) {
		int reg = bank == 1 ? 0xbd : 0xc0;
		uint8_t val = afe_read(reg);
		if (ie) {
			val |= mask;
		} else {
			val &= ~mask;
		}
		afe_write(reg, val);
	} else {
		if (ie) {
			GPIO->PORT[bank].IE |= mask;
		} else {
			GPIO->PORT[bank].IE &= ~mask;
		}
	}
	if (bank < 4) {
		uint16_t func = GPIO->MUX_FUNC[bank];
		func &= ~altmask;
		func |= (altmode << altshft) & altmask;
		GPIO->MUX_FUNC[bank] = func;

		uint32_t afe_pull_shift = pin & 3;
		uint8_t afe_pull_reg = 0x0e + (pin >> 2);
		uint8_t afe_pull_mask = 0x03 << afe_pull_shift;
		afe_write(afe_pull_reg, (afe_read(afe_pull_reg) & ~afe_pull_mask) | (pull << afe_pull_shift));
	}
}

void mcu_init(void)
{
	MCU->RETENTION_DATA_END = 0x40;
	MCU->TAG_DATA_END = 0x40;

	/* bit7: HS  bit6:5 SRC  bit4:0 DIV
	 * 0x43 => source 2, div 3?
	 */
	SYSCTL->CLKSEL = 0x43;
	/* De-assert peripheral reset */
	SYSCTL->RST0 = 0;
	SYSCTL->RST1 = 0;
	SYSCTL->RST2 = 0;
	/* Enable peripheral clocks */
	SYSCTL->CLKEN0 = 0xff;
	SYSCTL->CLKEN1 = 0xff;
	SYSCTL->CLKEN2 = 0xff;

	static const uint16_t aregs[] = {
		/* bit2: ???
		 * bit5: FLD_CLK_48M_TO_DIG_EN
		 * bit6: FLD_CLK_24M_TO_SAR_EN
		 */
		AREG_INITVAL(AREG_CLK_SETTING, 0x64),
		/* USB areg 0x34: bit1: power on */
		AREG_INITVAL(0x34, 0x80),
		/* USB pull-up areg 0x0b: bit7: dp_pullup_res_3v */
		AREG_INITVAL(AREG_0b_PULL, 0x38),
		AREG_INITVAL(0x8c, 0x02),  /* unknown */
		/* Core voltage setting? */
		AREG_INITVAL(AREG_VOL_LDO_CTRL, 0xa2),
		/* Disable GPIO wakeup */
		AREG_INITVAL(AREG_WAKEUP_PA, 0),
		AREG_INITVAL(AREG_WAKEUP_PB, 0),
		AREG_INITVAL(AREG_WAKEUP_PC, 0),
		AREG_INITVAL(AREG_WAKEUP_PD, 0),
		0,
	};
	afe_initlist(aregs);

	DMA->ADDRHI0123 = 0x04040404;
	DMA->ADDRHI45_TA_A3 = 0x04040404;
	DMA->ADDRHI7 = 0x04;
	DMA->CHN_EN = 0;
	DMA->CHN_IRQ_MSK = 0;

#if 0
	SYSTIM->R750 = 0x1f40;
	if (SYSCTL->VER_ID == 1) {
		afe_write(AREG_XTAL_CTRL, 0x3c);
	} else {
		afe_write(AREG_XTAL_CTRL, 0x4c);
	}

	if (afe_read(0x7f) & 1) {
		/* not deep sleep return */
		SYSTIM->CTRL1 = 1;
		/* TODO cold_boot_init func */
	} else {
		/* TODO */
	}
	GPIO->WAKEUP_IRQ |= 0xc; /* core_wakeup_en | core_interrupt_en */

	// select 24M as the system clock source.
	// clock.c : rc_24m_cal
	afe_write(0xc8, 0x80);
	afe_write(0x30, afe_read(0x30) | BIT(7));
	afe_write(0xc7, 0x0e);
	afe_write(0xc7, 0x0f);
	while((afe_read(0xcf) & 0x80) == 0);
	afe_write(0x33, afe_read(0xcb));		//write 24m cap into manual register
	afe_write(0x30, afe_read(0x30) & (~BIT(7)));
	afe_write(0xc7, 0x0e);
#endif
}

#define LED PB5

void gpio_init(void)
{
	/* ??? */
	gpio_config(PC2, PIN_OUTPUT, PULL_NONE, 0);
	/* ??? */
	gpio_config(PA1, PIN_OUTPUT, PULL_NONE, 0);
	/* ??? */
	gpio_config(PD3, PIN_OUTPUT, PULL_NONE, 0);
	/* Maybe ALERTN? */
	gpio_config(PC4, PIN_INPUT, PULL_UP_1M, 0);
	/* LED */
	gpio_config(PB5, PIN_OUTPUT, PULL_NONE, 0);
	gpio_write(PB5, 1);
	/* Something todo with standby */
	gpio_config(PB6, PIN_OUTPUT, PULL_NONE, 0);
	gpio_write(PB6, 1);
	/* ??? */
	gpio_config(PB7, PIN_INPUT, PULL_NONE, 0);
	/* FET-related? */
	gpio_config(PD2, PIN_OUTPUT, PULL_NONE, 0);
	/* STX? */
	gpio_config(PD7, PIN_INOUT, PULL_NONE, 0);
	/* Power button */
	gpio_config(PC3, PIN_INPUT, PULL_NONE, 0);
	/* SDA */
	gpio_config(PC0, PIN_INPUT, PULL_UP_18K, 0);
	/* SCL */
	gpio_config(PC1, PIN_INPUT, PULL_UP_18K, 0);
	/* Should be ALERTN */
	gpio_config(PC4, PIN_INPUT, PULL_UP_1M, 0);
}

uint8_t crc8_arg(const uint8_t *data, int len, uint8_t previous_crc)
{
	uint32_t crc = previous_crc;
	int i, j;
	for (j = len; j; j--, data++) {
		crc ^= *data;
		for (i = 8; i; i--) {
			if (crc & 0x80) {
				crc <<= 1;
				crc ^= 0x07;
			} else {
				crc <<= 1;
			}
		}
	}
	return crc;
}

inline uint8_t crc8(const uint8_t *data, int len)
{
	return crc8_arg(data, len, 0);
}

static int addr = 0x56;

uint32_t i2c_hw_read_reg(uint8_t reg)
{
	uint32_t data = 0;
	uint8_t crc;

	I2C->ID = addr;
	I2C->ADR = reg;
	I2C->CTRL = I2C_CTRL_ID | I2C_CTRL_ADDR | I2C_CTRL_START;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	if (I2C->STATUS & I2C_STATUS_NAK) {
		puts("nak on addr|w or reg");
		goto err;
	}

	I2C->ID = addr | 1;
	I2C->CTRL = I2C_CTRL_ID | I2C_CTRL_START;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	if (I2C->STATUS & I2C_STATUS_NAK) {
		puts("nak on addr|r");
		goto err;
	}

	I2C->CTRL = I2C_CTRL_DI | I2C_CTRL_READ_ID;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	data = (int)I2C->DI << 8;

	I2C->CTRL = I2C_CTRL_DI | I2C_CTRL_READ_ID;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	data |= I2C->DI;

	I2C->CTRL = I2C_CTRL_DI | I2C_CTRL_READ_ID | I2C_CTRL_NAK | I2C_CTRL_STOP;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	crc = I2C->DI;

	return data | ((int)crc << 16);

err:
	I2C->CTRL = I2C_CTRL_STOP;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	return 0;
}

int i2c_hw_write_reg(uint8_t reg, uint16_t val)
{
	uint8_t data[4];
	uint8_t crc;

	data[0] = addr;
	data[1] = reg;
	data[2] = val >> 8;
	data[3] = val;
	crc = crc8_arg(data, sizeof(data), 0);

	I2C->ID = addr;
	I2C->ADR = reg;
	I2C->DO = val >> 8;
	I2C->DI = val;
	I2C->CTRL = I2C_CTRL_ID | I2C_CTRL_ADDR | I2C_CTRL_DO | I2C_CTRL_DI | I2C_CTRL_START;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	if (I2C->STATUS & I2C_STATUS_NAK) {
		puts("nak on addr|w or reg");
		goto err;
	}

	I2C->DI = crc;
	I2C->CTRL = I2C_CTRL_DI | I2C_CTRL_STOP;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	if (I2C->STATUS & I2C_STATUS_NAK) {
		puts("nak on crc");
		goto err;
	}
	return 0;

err:
	I2C->CTRL = I2C_CTRL_STOP;
	while (I2C->STATUS & I2C_STATUS_BUSY);
	return 1;
}

void uart_init(void)
{
	// UART_RX
	gpio_config(PA0, PIN_INPUT, PULL_UP_18K, 2 /* ALT_UART_RX */);
	// UART_TX
	gpio_config(PB1, PIN_INPUT, PULL_UP_18K, 1 /* ALT_UART_TX */);

	/* 16000000 / (9+1) / (13+1) => 114285 (115200) */
	int clk_div = 9;
	int bwpc = 13;
	UART->CTRL0 = bwpc | (1 << 5) /* TX dma en */;
	UART->CLK_DIV = clk_div | 0x8000;
	UART->RX_TIMEOUT = ((bwpc + 1) * 12) | 0x100;
	UART->CTRL1 = 0; /* 8n1 */

	DMA->UART_TX.ADDRLO = (uint16_t)(int)&uart_tx_buf;
	DMA->UART_TX.SIZE = 0xff;
	DMA->CHN_EN |= (1 << 1); /* UART_TX channel */
	uart_tx_buf.len = 4;
	uart_tx_buf.data[0] = 'H';
	uart_tx_buf.data[1] = 'i';
	uart_tx_buf.data[2] = '!';
	uart_tx_buf.data[2] = '\n';
	DMA->TX_RDY |= (1 << 1); /* UART_TX channel */
}

void i2c_init(void)
{
	/* SDA */
	gpio_config(PC0, PIN_INPUT, PULL_UP_18K, 0);
	/* SCL */
	gpio_config(PC1, PIN_INPUT, PULL_UP_18K, 0);

	GPIO->PC.ACT_AS_GPIO &= ~3;

	I2C->SPD = (16000000 / (4 * 100000)) - 1;
	I2C->ID = 0x56;
	I2C->MODE |= (1 << 1); /* i2c master enable */
}

static volatile char timer_flag;

void irq_handler(void)
{
	//puts("Hello IRQ!\n");
	SYSTIM->TRIG += 1000000 << 4;
	MCU->IRQSRC = 0x00100000; /* Ack system timer irq */
	timer_flag = 1;
}

int main(void)
{
	/* Early turn-on of PB6 to keep the lights on */
	GPIO->PB.OEN &= ~(1 << 6) & 0xff;
	GPIO->PB.OUT |= 1 << 6;
	/* Early turn-on of blue indicator LED */
	GPIO->PB.OEN &= ~(1 << 5) & 0xff;
	GPIO->PB.OUT |= 1 << 5;
	uint8_t ledon = GPIO->PB.OUT;
	uint8_t ledoff = ledon & ~(1 << 5);
	for (int i = 0; i < 10; i++) {
		GPIO->PB.OUT = ledon;
		for (int j = 0; j < 100000; j++) GPIO->PB.OUT;
		GPIO->PB.OUT = ledoff;
		for (int j = 0; j < 100000; j++) GPIO->PB.OUT;
	}
	mcu_init();
	uart_init();
	gpio_init();
	i2c_init();

	SYSTIM->TRIG = SYSTIM->TICK + 1000;
	MCU->IRQMASK = 0x01100000; /* Enable system timer irq */

	uint8_t r = 0;
	while (1) {
		if (timer_flag) {
			timer_flag = 0;

			printf("%08x Hello world!\n", (int)SYSTIM->TICK);
			i2c_hw_write_reg(0x16, 0x3714);
			delay_micros(10000);
			/* Wiggle PA1 / WKUP */
			GPIO->PA.OUT &= ~(1 << 1);
			delay_micros(50000);
			GPIO->PA.OUT |= 1 << 1;
			delay_micros(50000);
			GPIO->PA.OUT &= ~(1 << 1);
			delay_micros(50000);
			i2c_hw_write_reg(0x16, 0x3714);
			uint32_t v = i2c_hw_read_reg(r);
			printf("R%02x = %08x\n", r, (int)v);
			r++;
		}
	};
}
