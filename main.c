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

void areg_initlist(const uint16_t *regvals)
{
	int saved;
	uint16_t rv;
	irq_save(saved);
	while ((rv = *regvals++) != 0) {
		AFE->ADDRVAL = rv;
		AFE->CMD = 0x60;
		while (AFE->CMD & 1);
	};
	irq_restore(saved);
}

void areg_write(uint8_t reg, uint8_t val)
{
	int saved;
	irq_save(saved);
	AFE->ADDR = reg;
	AFE->VALUE = val;
	AFE->CMD = 0x60;
	while (AFE->CMD & 1);
	AFE->CMD = 0;
	irq_restore(saved);
}

uint8_t areg_read(uint8_t reg)
{
	int saved;
	uint8_t val;
	irq_save(saved);
	AFE->ADDR = reg;
	AFE->CMD = 0x40;
	while (AFE->CMD & 1);
	val = AFE->VALUE;
	AFE->CMD = 0;
	irq_restore(saved);
	return val;
}

void mspi_start_cmd(uint8_t cmd)
{
	MSPI->CTRL = MSPI_CTRL_CS;  /* CS high */
	delay_micros(1);
	MSPI->CTRL = 0;  /* CS low */
	MSPI->DATA = cmd; /* writing DATA triggers a transfer cycle */
	while (MSPI->CTRL & MSPI_CTRL_BUSY);
}

uint32_t mspi_jedec_id(void)
{
	uint32_t res = 0;
	mspi_start_cmd(0x9f);  /* Read JEDEC ID */
	MSPI->CTRL = 0x0a;  /* auto read mode */
	for (int i = 0; i < 4; i++) {
		while (MSPI->CTRL & MSPI_CTRL_BUSY);
		res <<= 8;
		res |= MSPI->DATA;
	}
	MSPI->CTRL = MSPI_CTRL_CS;  /* CS high */
	return res;
}

static int read_32k_timer(void)
{
	SYSTIM->CTRL1 |= SYSTIM_CTRL1_MANUAL_32K_TRIGGER;
	asm volatile (".rept 16\ntnop\n.endr\n");
	while (SYSTIM->CTRL1 & SYSTIM_CTRL1_MANUAL_32K_TRIGGER);
	return SYSTIM->TICK32K;
}

static void write_32k_timer(int val)
{
	uint8_t ctrl0 = SYSTIM->CTRL0;
	SYSTIM->CTRL0 = SYSTIM_CTRL0_SUSPEND_BYPASS | SYSTIM_CTRL0_MANUAL_32K_MODE | SYSTIM_CTRL0_MANUAL_32K_WRITE;
	SYSTIM->TICK32K = val;
	SYSTIM->CTRL1 |= SYSTIM_CTRL1_MANUAL_32K_TRIGGER;
	asm volatile (".rept 16\ntnop\n.endr\n");
	while (SYSTIM->CTRL1 & SYSTIM_CTRL1_MANUAL_32K_TRIGGER);
	SYSTIM->CTRL0 = 0;  /* errata? */
	SYSTIM->CTRL0 = ctrl0;
	SYSTIM->CTRL1 = 1;
	SYSTIM->IRQ_TICK |= 0;  /* errata? */
}

int read_sync_32k_timer(void)
{
	/* repeatedly read 32k timer value until we synced to a tick */
	int val1 = 0;
	int val2 = 2;
	uint8_t ctrl0 = SYSTIM->CTRL0;
	SYSTIM->CTRL0 = SYSTIM_CTRL0_SUSPEND_BYPASS | SYSTIM_CTRL0_MANUAL_32K_MODE;
	while ((val1 ^ val2) != 1) {
		val2 = val1;
		val1 = read_32k_timer();
	}
	SYSTIM->CTRL0 = 0;  /* errata? */
	SYSTIM->CTRL0 = ctrl0;
	SYSTIM->CTRL1 = 1;
	SYSTIM->IRQ_TICK |= 0;  /* errata? */
	return val1;
}

extern void bitbang_char(int ch, int gpio_mask, volatile uint8_t *gpio_addr);

void bitbang_putchar(int c)
{
	bitbang_char(c | 0x100, (1 << 1), &GPIO->PB.OUT);
}

static int early_putchar;

int putchar(int c)
{
	if (early_putchar) {
		bitbang_putchar(c);
		return c;
	}
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

static void gpio_wakeup(enum gpio_pin pin, int enable, int level)
{
	unsigned bank = pin >> 3;
	unsigned idx = pin & 7;
	uint8_t mask = 1 << idx;

	int reg = AREG_WAKEUP_PA + bank;
	uint8_t val = areg_read(reg);
	if (enable) {
		val |= mask;
	} else {
		val &= ~mask;
	}
	areg_write(reg, val);

	reg = AREG_PA_POL + bank;
	val = areg_read(reg);
	if (level) {
		val |= mask;
	} else {
		val &= ~mask;
	}
	areg_write(reg, val);
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
		GPIO->PORT[bank].OEN |= mask;
	} else {
		GPIO->PORT[bank].OEN &= ~mask;
	}
	if (bank == 1 || bank == 2) {
		int reg = bank == 1 ? 0xbd : 0xc0;
		uint8_t val = areg_read(reg);
		if (ie) {
			val |= mask;
		} else {
			val &= ~mask;
		}
		areg_write(reg, val);
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

		uint32_t areg_pull_shift = pin & 3;
		uint8_t areg_pull_reg = 0x0e + (pin >> 2);
		uint8_t areg_pull_mask = 0x03 << areg_pull_shift;
		areg_write(areg_pull_reg, (areg_read(areg_pull_reg) & ~areg_pull_mask) | (pull << areg_pull_shift));
	}
}

static uint8_t areg_dump[256];
static int boot_cause;
static const char* boot_cause_str[] = {
	"cold",
	"warm",
	"watchdog",
};

void mcu_init(void)
{
	MCU->RETENTION_DATA_END = 0x40;
	MCU->TAG_DATA_END = 0x40;

	/* De-assert peripheral reset */
	SYSCTL->RST0 = 0;
	SYSCTL->RST1 = 0;
	SYSCTL->RST2 = 0;
	/* Enable peripheral clocks */
	SYSCTL->CLKEN0 = 0xff;
	SYSCTL->CLKEN1 = 0xff;
	SYSCTL->CLKEN2 = 0xff;

	/* save original areg values for testing */
	for (int i = 0; i < 0x100; i++) {
		areg_dump[i] = areg_read(i);
	}

	static const uint16_t aregs[] = {
		/* bit2: ???
		 * bit5: FLD_CLK_48M_TO_DIG_EN
		 * bit6: FLD_CLK_24M_TO_SAR_EN
		 */
		AREG_INITVAL(AREG_82_CLK_SETTING, 0x64),
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
	areg_initlist(aregs);

	/* bit7: HS  bit6:5 SRC  bit4:0 DIV
	 * 0x43 => source 2, div 3?
	 * SRC 0: RC24M
	 * SRC 1: FHS MUX output
	 * SRC 2: FHS MUX + divider
	 * SRC 3: Doubler + 2/3 divider -> 32MHz for 24MHz XTAL
	 */
	SYSCTL->CLKSEL = 0x43; /* 16MHz from 24 MHz XTAL -> doubler -> div 3 */

	DMA->ADDRHI0123 = 0x04040404;
	DMA->ADDRHI45_TA_A3 = 0x04040404;
	DMA->ADDRHI7 = 0x04;
	DMA->CHN_EN = 0;
	DMA->CHN_IRQ_MSK = 0;

	SYSTIM->CTRL0 = 0;
	SYSTIM->CTRL0 = SYSTIM_CTRL0_CAL_32K_EN | SYSTIM_CTRL0_TIMER_SS_EN | SYSTIM_CTRL0_IRQ_ENABLE;
	SYSTIM->CTRL1 = 1;
	SYSTIM->R750 = 0x1f40; /* 8000; related to 32k timer; 16MHz / 32KHz * 16; used in sleep time calculation */
	ASSERT_EQ((int)(&SYSCTL->MCU_WAKEUP_MASK), 0x00800078);
	ASSERT_EQ((int)(&SYSCTL->VER_ID), 0x0080007d);
	ASSERT_EQ((int)(&MCU->IRQMASK), 0x00800640);
	if (SYSCTL->VER_ID == 1) {
		areg_write(AREG_XTAL_CTRL, 0x3c);
	} else {
		areg_write(AREG_XTAL_CTRL, 0x4c);
	}

	if (areg_read(0x7f) & 1) {
		boot_cause = 0;
		/* not deep sleep return */
		SYSTIM->CTRL1 = 1;  /* enable system timer */
		/* TODO cold_boot_init func */
	} else {
		boot_cause = 1;
		/* TODO */
	}
	if (SYSCTL->WD_STATUS & 1) {
		boot_cause = 2;
		SYSCTL->WD_STATUS = 1;
	}
	GPIO->WAKEUP_IRQ |= 0xc; /* core_wakeup_en | core_interrupt_en */

#if 0
	// select 24M as the system clock source.
	// clock.c : rc_24m_cal
	areg_write(0xc8, 0x80);
	areg_write(0x30, areg_read(0x30) | BIT(7));
	areg_write(0xc7, 0x0e);
	areg_write(0xc7, 0x0f);
	while((areg_read(0xcf) & 0x80) == 0);
	areg_write(0x33, areg_read(0xcb));		//write 24m cap into manual register
	areg_write(0x30, areg_read(0x30) & (~BIT(7)));
	areg_write(0xc7, 0x0e);
#endif
}

#define LED PB5

void gpio_init(void)
{
	/* Well-understood GPIO */
	/* exernal watchdog */
	gpio_config(PD3, PIN_OUTPUT, PULL_NONE, 0);
	/* LED */
	gpio_config(PB5, PIN_OUTPUT, PULL_UP_18K, 0);
	gpio_write(PB5, 1);  /* turn on LED */
	/* Power button */
	gpio_config(PC3, PIN_INPUT, PULL_UP_1M, 0);
	gpio_wakeup(PC3, 1, 0);
	GPIO->PC.ACT_AS_GPIO |= 1 << 3;
	GPIO->PC.POL |= 1 << 3;    /* active low */
	GPIO->PC.IRQ_EN |= 1 << 3;
	/* SDA */
	gpio_config(PC0, PIN_INPUT, PULL_UP_18K, 0);
	/* SCL */
	gpio_config(PC1, PIN_INPUT, PULL_UP_18K, 0);
	/* ALERTN */
	gpio_config(PD4, PIN_INPUT, PULL_UP_18K, 0);
	gpio_wakeup(PD4, 1, 0);
	GPIO->PD.ACT_AS_GPIO |= 1 << 4;
	GPIO->PD.POL |= 1 << 4;    /* active low */
	GPIO->PD.IRQ_EN |= 1 << 4;

	/* PB4: Probably unconnected, PWM4 used as timer */

	/* Triggers power down if 0 (with some delay) */
	gpio_config(PB6, PIN_OUTPUT, PULL_UP_18K, 0);
	gpio_write(PB6, 1);
	/* Unconnected ??? */
	gpio_config(PC2, PIN_OUTPUT, PULL_NONE, 0);
	/* AZ3714 wakeup (pulse high to wake) */
	gpio_config(PA1, PIN_OUTPUT, PULL_NONE, 0);
	/* Unconnected ??? */
	gpio_config(PB7, PIN_INPUT, PULL_NONE, 0);
	/* STX write (open-drain) */
	gpio_config(PD2, PIN_OUTPUT, PULL_NONE, 0);
	/* STX read */
	gpio_config(PD7, PIN_INOUT, PULL_NONE, 0);
	/* Charger detection? */
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

	return data;

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
	GPIO->PB.ACT_AS_GPIO &= ~(1 << 1);
	early_putchar = 0;

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
	/* clock stretching is used when accessing OZ3714 registers
	 * that trigger an A/D conversion. */
	I2C->MODE |= (1 << 1)  /* i2c master enable */
		   | (1 << 3); /* clock stretch enable */
}

static volatile char timer_flag;
static volatile char pmtimer_flag;
static volatile char gpio_flag;
static volatile char gpio_pd4_flag;

void irq_handler(void)
{
	uint32_t src = MCU->IRQSRC;
	//puts("Hello IRQ!\n");
	if (src & MCU_IRQMASK_SYSTIMER) {
		SYSTIM->IRQ_TICK += 1000000 << 4;
		SYSTIM->WAKEUP_TICK = SYSTIM->IRQ_TICK;
		timer_flag = 1;
	}
	if (src & MCU_IRQMASK_PMTIMER) {
		pmtimer_flag = 1;
	}
	if (src & MCU_IRQMASK_GPIO) {
		gpio_flag = 1;
		if (GPIO->IRQ_STATUS[3] & (1 << 4)) {
			gpio_pd4_flag = 1;
		}
	}
	MCU->IRQSRC = src; /* Ack irq */
}

void oz3714_wake(void)
{
	delay_micros(10000);
	/* Wiggle PA1 / WKUP */
	GPIO->PA.OUT &= ~(1 << 1);
	delay_micros(50000);
	GPIO->PA.OUT |= 1 << 1;
	delay_micros(50000);
	GPIO->PA.OUT &= ~(1 << 1);
	delay_micros(50000);
}

void oz3714_init(void)
{
	oz3714_wake();
	// CADCRTL
	i2c_hw_write_reg(0x38, 0);
	// IER1
	i2c_hw_write_reg(0x02, 0xf);
	// IER2
	i2c_hw_write_reg(0x02, 0xf);
	// MAPR: Unlock config registers 0x05..0x0e
	i2c_hw_write_reg(0x0f, 0x3714);
	// UVPR
	i2c_hw_write_reg(0x05, 0xc000);
	// UVSHUTDOWN (disable undervolt shutdown)
	i2c_hw_write_reg(0x06, 0xc000);
	// OVPR (4.575V)
	i2c_hw_write_reg(0x07, 0xc0ff);
	// DOC1PR (14 cells)
	i2c_hw_write_reg(0x08, 0x7fff);
	// COCPR
	i2c_hw_write_reg(0x09, 0x03ff);
	// DOC2PR
	i2c_hw_write_reg(0x0a, 0x8000);
	// PWMEFETC (disable EFETC function)
	i2c_hw_write_reg(0x0b, 0);
	// STLCFG (minimal settling time for ADC channels)
	i2c_hw_write_reg(0x0d, 0);
	// SCANCTL (auto-scan disabled)
	i2c_hw_write_reg(0x0e, 0);
	// Lock config registers
	i2c_hw_write_reg(0x0f, 0);
	// CHGCTRL (enable discharge and charge)
	i2c_hw_write_reg(0x10, 3);
	// CRRTSEL (120uA THM sensor current)
	i2c_hw_write_reg(0x11, 2);
	// CBCTRL (balancing disabled)
	i2c_hw_write_reg(0x15, 0);
	// CADCRTL
	i2c_hw_write_reg(0x38, 0x009b);
}

int get_balance_mask(int *cvs, int len)
{
	int candidate_mask = ~(-1 << len);
	int result = 0;
	while (candidate_mask) {
		int minv = 0;
		int maxv = 0;
		int maxi = 0;
		for (int i = 0; i < len; i++) {
			if (!(candidate_mask & (1 << i)))
				continue;
			if (cvs[i] < minv && cvs[i] > 1000) {
				minv = cvs[i];
			}
			if (cvs[i] <= maxv)
				continue;
			maxi = i;
			maxv = cvs[i];
		}
		if (maxv - minv < 20) {
			break;
		}
		if (maxv < 3200) {
			break;
		}
		if (maxi == 0) {
			candidate_mask &= ~3;
		} else {
			candidate_mask &= ~(7 << (maxi-1));
		}
		result |= 1 << maxi;
	}
	return result;
}

void oz3714_dump(void)
{
	int hwid = i2c_hw_read_reg(0);
	int alertnr1 = i2c_hw_read_reg(1);
	int ier1 = i2c_hw_read_reg(2);
	int alertnr2 = i2c_hw_read_reg(3);
	int ier2 = i2c_hw_read_reg(4);
	int status = i2c_hw_read_reg(0x12);
	int pa = GPIO->PA.IN;
	int pb = GPIO->PB.IN;
	int pc = GPIO->PC.IN;
	int pd = GPIO->PD.IN;

#if 1
	/* ACK alerts */
	i2c_hw_write_reg(1, alertnr1);
	i2c_hw_write_reg(3, alertnr2);
	/* Enable final_cadc alert */
	i2c_hw_write_reg(2, 0x0001);
#endif
	int alertnr1_2 = i2c_hw_read_reg(1);

	int pa2 = GPIO->PA.IN;
	int pb2 = GPIO->PB.IN;
	int pc2 = GPIO->PC.IN;
	int pd2 = GPIO->PD.IN;
	printf("PA:%02x->%02x PB:%02x->%02x PC:%02x->%02x PD:%02x->%02x HWID:%04x STATUS:%04x\n",
		pa, pa2, pb, pb2, pc, pc2, pd, pd2, hwid, status);
	printf("ALRT1:%04x->%04x IER1:%04x ALRT2:%04x IER2:%04x\n",
		alertnr1, alertnr1_2, ier1, alertnr2, ier2);

	int cxcn = i2c_hw_read_reg(0x1d);
	int maxcellnum = cxcn & 31;
	int mincellnum = (cxcn >> 8) & 31;
	int maxcellmv = (i2c_hw_read_reg(0x1e) * 625) / 1000;
	int mincellmv = (i2c_hw_read_reg(0x1f) * 625) / 1000;
	printf("CXCN:%04x CMAX:%d:%5dmV CMIN:%d:%5dmV\n",
		cxcn, maxcellnum, maxcellmv, mincellnum, mincellmv);

	int cdata = (i2c_hw_read_reg(0x39) * 78125) / 10000;
	int cchr = i2c_hw_read_reg(0x3a);
	int cclr = i2c_hw_read_reg(0x3b);
	int ccr = cclr | (cchr << 16);
	printf("VSHNT:%duV CC:%d\n", cdata, ccr);

	int intmp = i2c_hw_read_reg(0x40) * 3125 / 10000 / 4;
	int vccv = i2c_hw_read_reg(0x5d) * 3125 / 1000;
	int v5av = i2c_hw_read_reg(0x5e) * 3125 / 10000;
	int vmcuv = i2c_hw_read_reg(0x5f) * 125 / 100 / 4;
	printf("INTMP:%dmV VCC:%dmV V5A:%dmV VMCU:%dmV\n",
		intmp, vccv, v5av, vmcuv);

	int cvs[14];
	for (int i = 1; i <= 14; i++) {
		int cv = i2c_hw_read_reg(0x40+i) * 625 / 4000;
		cvs[i-1] = cv;
		printf("C%2d:%5dmV ", i, cv);
		if (i % 4 == 0) {
			printf("\n");
		}
	}
	printf("\n");

	int balance_mask = get_balance_mask(cvs, 14);
	i2c_hw_write_reg(0x15, balance_mask);
	int fcbsel = i2c_hw_read_reg(0x13);
	printf("BAL:%04x FCBSEL:%04x\n", balance_mask, fcbsel);
}

int stall_wakeup_by_timer0(int ticks)
{
	int tenter = SYSTIM->TICK;
	MCU->TMR_TICK0 = 0;
	MCU->TMR_CAPT0 = ticks;
	MCU->TMR_CTRL0 = (MCU->TMR_CTRL0 & ~7) | 1;
	MCU->TMR_STATUS = 1;
	int wakeup_mask = SYSCTL->MCU_WAKEUP_MASK & ~0xffffff;
	SYSCTL->MCU_WAKEUP_MASK = wakeup_mask | MCU_IRQMASK_TIMER0;
	SYSCTL->PWDN_CTRL = 0x80; /* stall cpu (wait for interrupt) */
	asm volatile (".rept 2\ntnop\n.endr\n");
	int texit = SYSTIM->TICK;
	return texit - tenter;
}

static int skipped_sleep;

void sleep_start(void)
{
	uint8_t areg_34_saved = areg_read(0x34);
	uint8_t areg_82_saved = areg_read(AREG_82_CLK_SETTING);
	areg_write(0x34, 0x87); /* Power down baseband/USB/audio? */

	MSPI->CTRL = 0;    /* CS low */
	MSPI->DATA = 0xb9; /* NOR flash suspend mode */
	while (MSPI->CTRL & MSPI_CTRL_BUSY);
	MSPI->CTRL = 1;    /* CS high */

	GPIO->PE.IE = 0;        /* Disable MSPI flash GPIO inputs */

	/* bit0: ?
	 * bit1: ?
	 * bit2: ?
	 * bit3: DOUBLER_POWER_DOWN
	 * bit4: 48M_TO_RX_EN
	 * bit5: 48M_TO_DIG_EN
	 * bit6: 24M_TO_SAR_EN
	 * bit7: 48M_TO_CAL_DIG_MAN_EN
	 */
	areg_write(AREG_82_CLK_SETTING, 0x0c);

	/* TODO: possible errata requiring writing "tnop; tnop" (0x06c006c0) into ram location? */
	if ((areg_read(AREG_44_STATUS) & 0xf) == 0) {
		/* All wakeup sources are still quiet, go ahead. */
		SYSCTL->PWDN_CTRL = 0x81; /* suspend mcu */
		asm volatile (".rept 16\ntnop\n.endr\n");
	} else {
		skipped_sleep++;
	}

	areg_write(AREG_82_CLK_SETTING, areg_82_saved);

	GPIO->PE.IE = 0xf;      /* Enable MSPI flash GPIO inputs */

	MSPI->CTRL = 0;    /* CS low */
	MSPI->DATA = 0xab; /* NOR flash exit suspend */
	while (MSPI->CTRL & MSPI_CTRL_BUSY);
	MSPI->CTRL = 1;    /* CS high */

	areg_write(0x34, areg_34_saved); /* Power on baseband/USB/audio? */
}

void deep_sleep(void)
{
	uint8_t irqmode_saved = MCU->IRQMODE;
	MCU->IRQMODE = 0;    /* Disable IRQs */

	uint8_t clksel_saved = SYSCTL->CLKSEL;
	SYSCTL->CLKSEL = 0;  /* Select 24M RC OSC */

	uint8_t systim_ctrl0 = SYSTIM->CTRL0;
	SYSTIM->CTRL0 = SYSTIM_CTRL0_SUSPEND_BYPASS;

	/* SYSCTL->WAKEUP_EN already defaults to 0x1f (I2C, SPI, USB, GPIO) */
#if 0
	areg_write(AREG_26_WAKEUP_EN, 0x50);  /* 32kHz timer, GPIO */
#else
	areg_write(AREG_26_WAKEUP_EN, 0x40);  /* 32kHz timer */
#endif

	/* From code path for deep sleep with SRAM retention */
	areg_write(2, (areg_read(2) & ~7) | 5);
	areg_write(AREG_7E_SLEEP_MODE, 0x07); /* DEEPSLEEP_MODE_RET_SRAM_LOW32K */
	areg_write(0x2b, 0xde);
	areg_write(0x2c, 0x5e);
	areg_write(7, (areg_read(7) & ~7) | 1); /* Power down SPD LDO, keep on retention LDO and main digital LDO */

	areg_write(AREG_7F_SLEEP_MODE, 0);    /* 0: deep sleep; 1: suspend */

	GPIO->PB.OUT |= (1 << 5);
	sleep_start();  /* trigger sleep */
	GPIO->PB.OUT &= ~(1 << 5);

	SYSTIM->CTRL0 = 0;  /* errata? */
	SYSTIM->CTRL0 = systim_ctrl0;
	SYSTIM->CTRL1 = 1;
	SYSTIM->IRQ_TICK = SYSTIM->TICK + 1000;

	SYSCTL->CLKSEL = clksel_saved;
	MCU->IRQMODE = irqmode_saved;
}

int main(void)
{
	/* UART_TX as GPIO */
	gpio_config(PB1, PIN_OUTPUT, PULL_NONE, 0);
	GPIO->PB.ACT_AS_GPIO |= (1 << 1);
	early_putchar = 1;
	/* Early turn-on of PB6 to keep the lights on */
	GPIO->PB.OEN &= ~(1 << 6) & 0xff;
	GPIO->PB.OUT |= 1 << 6;
	GPIO->PB.ACT_AS_GPIO |= 1 << 6;
	printf("Early hello world!\n");
	/* Triggers power down if 0 (with some delay) */
	gpio_config(PB6, PIN_OUTPUT, PULL_NONE, 0);
	gpio_write(PB6, 1);
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
	oz3714_init();

	SYSTIM->IRQ_TICK = SYSTIM->TICK + 1000;
	SYSTIM->WAKEUP_TICK = SYSTIM->TICK + 100;
	MCU->IRQMASK = MCU_IRQMASK_ENABLE
	             | MCU_IRQMASK_GPIO
	             | MCU_IRQMASK_PMTIMER
	             | MCU_IRQMASK_SYSTIMER;
	int tick32k = read_sync_32k_timer();
	int wake32k = tick32k + 64000;
	write_32k_timer(wake32k);

	printf("mcu id %04x, boot cause: %s\n",
	       (int)SYSCTL->PROD_ID, boot_cause_str[boot_cause]);
	for (int i = 0; i < sizeof(areg_dump); i++) {
		if ((i & 15) == 0) {
			printf("%02x:", i);
		}
		printf(" %02x", (int)areg_dump[i]);
		if ((i & 15) == 15) {
			printf("\n");
		}
	}

	int pd2_flag = 0;
	int ctr = 0;
	skipped_sleep = 0;
	while (1) {
		if (timer_flag) {
			ctr++;
			timer_flag = 0;
			GPIO->PD.OUT ^= (1 << 3); /* Toggle PD3 to prevent watchdog reset */

			printf("\n%08x %08x Hello world!\n", (int)SYSTIM->TICK, read_sync_32k_timer());
			int wakesrc = areg_read(0x44);
			areg_write(0x44, wakesrc); /* ack/clear */
			int wakesrc2 = areg_read(0x44);
			int wakeen = areg_read(0x26);
			printf("wkupsrc: %02x->%02x wken: %02x\n", wakesrc, wakesrc2, wakeen);
			printf("jedec id: %08x\n", (int)mspi_jedec_id());
			oz3714_dump();
			if (pd2_flag) {
				GPIO->PD.OUT |= (1 << 2);
				pd2_flag = 0;
			} else {
				GPIO->PD.OUT &= ~(1 << 2);
				pd2_flag = 1;
			}
		}
		if (gpio_flag) {
			if (gpio_pd4_flag) {
				printf("GPIO PD4 IRQ\n");
				gpio_pd4_flag = 0;
			} else {
				printf("other GPIO IRQ\n");
			}
			gpio_flag = 0;
		}
		if (pmtimer_flag) {
			printf("pmtimer IRQ (ctr:%d)\n", ctr);
			pmtimer_flag = 0;
			wake32k += 64000;
			write_32k_timer(wake32k);
			if (ctr > 4) {
#if 0
				printf("stall (msk:%08x): ", (int)SYSCTL->MCU_WAKEUP_MASK);
				printf("%d ticks\n", stall_wakeup_by_timer0(8000000));
#else
				areg_write(0x44, 0xf); /* ack/clear */
				int tstart = SYSTIM->TICK;
				deep_sleep();
				int tend = SYSTIM->TICK;
				int wake_cause = areg_read(0x44) & 0xf;
				delay_micros(10);
				printf("Slept %d ticks (skipped:%d; cause:%1x)\n", tend - tstart, skipped_sleep, wake_cause);
#endif
			}
		}
	};
}
