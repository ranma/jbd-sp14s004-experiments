#ifndef _TLSR825X_
#define _TLSR825X_

#include <inttypes.h>

#define SYS_CLOCK	24000000  /* 24MHz */

#define READREG(r)	((volatile uint32_t)(*(uint32_t *)(r)))
#define WRITEREG(r,v)	(*(volatile uint32_t *)(r) = v)

#define ASSERT_EQ(a, b) do { \
	char foo[(a) == (b) ? 1 : -1]; \
} while (0)

#define BIT(x) (1 << (x))

struct dma_buf {
	volatile uint32_t len;
	volatile uint8_t data[];
};

struct dma_ch_regs {
	volatile uint16_t ADDRLO;
	volatile uint8_t  SIZE;
	volatile uint8_t  MODE;
};

struct dma_regs {
	union {
		struct {
			struct dma_ch_regs UART_RX;  /* 0xc00 */
			struct dma_ch_regs UART_TX;  /* 0xc04 */
			struct dma_ch_regs RF_RX;    /* 0xc08 */
			struct dma_ch_regs RF_TX;    /* 0xc0c */
			struct dma_ch_regs AES_DEC;  /* 0xc10 */
			struct dma_ch_regs AES_ENC;  /* 0xc14 */
			struct dma_ch_regs PWM_TX;   /* 0xc18 */
			struct dma_ch_regs CH7;      /* 0xc1c */
		};
		struct dma_ch_regs CHN[8];
	};
	volatile uint8_t CHN_EN;                     /* 0xc20 */
	volatile uint8_t CHN_IRQ_MSK;                /* 0xc21 */
	uint8_t res0xc22[2];                         /* 0xc22 */
	volatile uint16_t TX_RDY;                    /* 0xc24 */
	volatile uint16_t RX_RDY;                    /* 0xc26 */
	volatile uint8_t RX_RPTR;                    /* 0xc28 */
	volatile uint8_t RX_WPTR;                    /* 0xc29 */
	volatile uint8_t TX_RPTR;                    /* 0xc2a */
	volatile uint8_t TX_WPTR;                    /* 0xc2b */
	volatile uint16_t TX_FIFO;                   /* 0xc2c */
	uint8_t res0xc2e[0x12];                      /* 0xc2e */
	union {                                      /* 0xc40 */
		struct {
			volatile uint8_t UART_RX_ADDRHI;
			volatile uint8_t UART_TX_ADDRHI;
			volatile uint8_t RF_RX_ADDRHI;
			volatile uint8_t RX_TX_ADDRHI;
		};
		volatile uint32_t ADDRHI0123;
	};
	union {                                      /* 0xc44 */
		struct {
			volatile uint8_t AES_DEC_ADDRHI;
			volatile uint8_t AES_ENC_ADDRHI;
			volatile uint8_t ADDRHI_TA;
			volatile uint8_t ADDRHI_A3;
		};
		volatile uint32_t ADDRHI45_TA_A3;
	};
	volatile uint8_t ADDRHI7;                    /* 0xc48 */
};
#define DMA		((struct dma_regs *)    0x00800c00)

#define SYSTIM_CTRL0_IRQ_ENABLE         BIT(1)
#define SYSTIM_CTRL0_MANUAL_32K_WRITE   BIT(2)
#define SYSTIM_CTRL0_MANUAL_32K_MODE    BIT(3)
#define SYSTIM_CTRL0_TIMER_SS_EN        BIT(4)
#define SYSTIM_CTRL0_SUSPEND_BYPASS     BIT(5)
#define SYSTIM_CTRL0_32K_TICK           BIT(6) /* 1 at 32K timer tick */
#define SYSTIM_CTRL0_CAL_32K_EN         BIT(7)
#define SYSTIM_CTRL1_TICK_START         BIT(0)
#define SYSTIM_CTRL1_TICK_RUNNING       BIT(1) 
#define SYSTIM_CTRL1_MANUAL_32K_TRIGGER BIT(3) /* m_wr_32k_en */

struct systim_regs {
	volatile uint32_t TICK;         /* 0x740 */
	volatile uint32_t IRQ_TICK;     /* 0x744 */
	volatile uint32_t WAKEUP_TICK;  /* 0x748 */
	volatile uint8_t CTRL0;         /* 0x74c */
	uint8_t res0x74d[2];
	volatile uint8_t CTRL1;         /* 0x74f */
	volatile uint16_t R750;         /* 0x750 */
	uint8_t res0x752[2];
	volatile uint32_t TICK32K;      /* 0x754: manual 32k timer read/write buffer */
};
#define SYSTIM		((struct systim_regs *) 0x00800740)

#define MCU_IRQMASK_TIMER0     0x00000001
#define MCU_IRQMASK_TIMER1     0x00000002
#define MCU_IRQMASK_TIMER2     0x00000004
#define MCU_IRQMASK_GPIO       0x00040000
#define MCU_IRQMASK_PMTIMER    0x00080000
#define MCU_IRQMASK_SYSTIMER   0x00100000
#define MCU_IRQMASK_ENABLE     0x01000000

struct mcu_regs {
	uint8_t res0x600[0x2];               /* 0x600 */
	/* 0x05: STOP
	 * 0x06: STALL
	 * 0x08: RUN
	 * 0x84: RESET + reload from NOR
	 * 0x88: RESET
	 */
	volatile uint8_t MODE;               /* 0x602 */
	uint8_t res0x603[0x9];               /* 0x603 */
	volatile uint8_t RETENTION_DATA_END; /* 0x60c */
	volatile uint8_t TAG_DATA_END;       /* 0x60d */
	uint8_t res0x60e[0x2];               /* 0x60e */
	uint8_t res0x610[0x10];              /* 0x610 */
	union {
		volatile uint32_t TMR_CTRLSTATUS; /* 0x620 */
		struct {
			volatile uint8_t TMR_CTRL0;  /* 0x620 */
			volatile uint8_t TMR_CTRL1;  /* 0x621 */
			volatile uint8_t TMR_CTRL2;  /* 0x622 */
			volatile uint8_t TMR_STATUS; /* 0x623 */
		};
	};
	volatile uint32_t TMR_CAPT0;         /* 0x624 */
	volatile uint32_t TMR_CAPT1;         /* 0x628 */
	volatile uint32_t TMR_CAPT2;         /* 0x62c */
	volatile uint32_t TMR_TICK0;         /* 0x630 */
	volatile uint32_t TMR_TICK1;         /* 0x634 */
	volatile uint32_t TMR_TICK2;         /* 0x638 */
	uint8_t res0x63c[0x4];               /* 0x63c */
	union {
		volatile uint32_t IRQMASK;   /* 0x640 */
		struct {
			uint8_t res0x640[3];       /* 0x640 */
			volatile uint8_t IRQMODE;  /* 0x643 */
		};
	};
	volatile uint32_t IRQPRI;            /* 0x644 */
	volatile uint32_t IRQSRC;            /* 0x648 */
	uint8_t res0x64c[0x14];              /* 0x64c */
	volatile uint8_t DIVMODE;            /* 0x660 */
	uint8_t res0x661[0x3];               /* 0x661 */
	volatile uint32_t DIVARG1;           /* 0x664 */
	volatile uint32_t DIVARG2;           /* 0x668 */
	uint8_t res0x66c[0x14];              /* 0x66c */
	volatile uint32_t REGS[16];          /* 0x680; PC (R15) at 0x6bc */
	volatile uint32_t CPSR;              /* 0x6c0 */
	volatile uint32_t UNKNOWN1;          /* 0x6c4 */
	volatile uint32_t PREV_PC;           /* 0x6c8 */
	volatile uint32_t ALUVAL;            /* 0x6cc; Or value written to register */
	volatile uint32_t LITVAL;            /* 0x6d0; Or general memory read result? */
};
#define MCU		((struct mcu_regs *)    0x00800600)

struct gpio_port_regs {
	union {
		struct {
			volatile uint8_t IN;  /* Input (read pin) */
			volatile uint8_t IE;  /* Input enable (1 == input on) */
			volatile uint8_t OEN; /* Output Enable Negative (0 == output on) */
			volatile uint8_t OUT; /* Output (write pin) */
		};
		volatile uint32_t SETTING1;
	};
	union {
		struct {
			volatile uint8_t POL;         /* Polarity for interrupt */
			volatile uint8_t DS;          /* Drive strength */
			volatile uint8_t ACT_AS_GPIO; /* 1 == GPIO, 0 == ALTFN */
			volatile uint8_t IRQ_EN;      /* Irq enable */
		};
		volatile uint32_t SETTING2;
	};
};

struct gpio_regs {
	union {
		struct {
			struct gpio_port_regs PA; /* 0x580 */
			struct gpio_port_regs PB; /* 0x588; AREG for IE/DS */
			struct gpio_port_regs PC; /* 0x590; AREG for IE/DS */
			struct gpio_port_regs PD; /* 0x598 */
			struct gpio_port_regs PE; /* 0x5a0 */
		};
		struct gpio_port_regs PORT[5];
	};
	volatile uint16_t MUX_FUNC[4];   /* 0x5a8 */
	uint8_t res0x5b0[5];
	volatile uint8_t WAKEUP_IRQ;     /* 0x5b5 */
	volatile uint8_t I2C_SPI_OUT_EN; /* 0x5b6 */
	volatile uint8_t I2C_SPI_EN;     /* 0x5b7 */
	volatile uint8_t RISC0_EN[5];    /* 0x5b8; m0 */
	uint8_t res0x5bd[3];
	volatile uint8_t RISC1_EN[5];    /* 0x5c0; m1 */
	uint8_t res0x5c5[3];
	volatile uint8_t RISC2_EN[5];    /* 0x5c8; m2 */
	uint8_t res0x5cd[3];
	uint8_t res0x5d0[0x10];
	volatile uint8_t IRQ_STATUS[5];  /* 0x5e0; which GPIO triggered irq */
};
#define GPIO		((struct gpio_regs *)    0x00800580)

struct i2c_adr_regs {
	volatile uint8_t HADR;  /* 0x0e0 */
};
#define I2C_ADR		((struct i2c_adr_regs *) 0x008000e0)

struct afe_regs {
	union {
		struct {
			volatile uint8_t ADDR;    /* 0xb8 */
			volatile uint8_t VALUE;   /* 0xb9 */
		} __attribute__((packed));
		volatile uint16_t ADDRVAL;
	} __attribute__((packed));
	volatile uint8_t CMD;                     /* 0xba */
};
#define AFE		((struct afe_regs *)    0x008000b8)

struct uart_regs {
	union {
		volatile uint8_t BUF[4];
		volatile uint32_t BUF32;
	};
	volatile uint16_t CLK_DIV;    /* 0x94 */
	volatile uint8_t CTRL0;       /* 0x96 */
	volatile uint8_t CTRL1;       /* 0x97 */
	volatile uint8_t CTRL2;       /* 0x98 */
	volatile uint8_t CTRL3;       /* 0x99 */
	volatile uint16_t RX_TIMEOUT; /* 0x9a */
	volatile uint8_t BUF_CNT;     /* 0x9c */
	volatile uint8_t STATUS;      /* 0x9d */
	volatile uint8_t TXRX_STATUS; /* 0x9e */
	volatile uint8_t STATE;       /* 0x9f */
};
#define UART		((struct uart_regs *)   0x00800090)

struct sysctl_regs {
	uint8_t res0x40[0x20];
	volatile uint8_t RST0;        /* 0x60 */
	volatile uint8_t RST1;        /* 0x61 */
	volatile uint8_t RST2;        /* 0x62 */
	volatile uint8_t CLKEN0;      /* 0x63 */
	volatile uint8_t CLKEN1;      /* 0x64 */
	volatile uint8_t CLKEN2;      /* 0x65 */
	volatile uint8_t CLKSEL;      /* 0x66 */
	volatile uint8_t I2S_STEP;    /* 0x67 */
	volatile uint8_t I2S_MOD;     /* 0x68 */
	uint8_t res0x69[3];
	volatile uint8_t DMIC_STEP;   /* 0x6c */
	volatile uint8_t DMIC_MOD;    /* 0x6d */
	/* bit0: wakeup from i2c host
	 * bit1: wakeup from SPI host
	 * bit2: wakeup from USB
	 * bit3: wakeup from gpio
	 * bit4: wakeup from i2c sync if
	 * bit5: gpio remote wakeup
	 * bit6: if set, USB will generate resume signal
	 * bit7: sleep wakeup reset system enable
	 */
	volatile uint8_t WAKEUP_EN;   /* 0x6e */
	volatile uint8_t PWDN_CTRL;   /* 0x6f */
	volatile uint8_t FHS_SEL;     /* 0x70 */
	uint8_t res0x71;
	volatile uint8_t WD_STATUS;   /* 0x72 */
	uint8_t res0x73[0x05];
	union {
		volatile uint32_t MCU_WAKEUP_MASK;         /* 0x78 */
		struct {
			volatile uint8_t MCU_WAKEUP_MASK0; /* 0x78 */
			volatile uint8_t MCU_WAKEUP_MASK1; /* 0x79 */
			volatile uint8_t MCU_WAKEUP_MASK2; /* 0x7a */
			volatile uint8_t CLK_DIV_7816;    /* 0x7b */
		};
	};
	uint8_t res0x7c;
	volatile uint8_t VER_ID;      /* 0x7d */
	volatile uint16_t PROD_ID;    /* 0x7e */
};
#define SYSCTL		((struct sysctl_regs *) 0x00800040)

#define MSPI_CTRL_CS        BIT(0)
#define MSPI_CTRL_SDO       BIT(1)
#define MSPI_CTRL_CONT      BIT(2)
#define MSPI_CTRL_RD        BIT(3)
#define MSPI_CTRL_BUSY      BIT(4)
struct mspi_regs {
	volatile uint8_t DATA;        /* 0x0c */
	volatile uint8_t CTRL;        /* 0x0d */
	uint8_t res;                  /* 0x0e */
	volatile uint8_t MODE;        /* 0x0f */
};
#define MSPI		((struct mspi_regs *) 0x0080000c)

struct spi_regs {
	volatile uint8_t DAT;         /* 0x08 */
	volatile uint8_t CT;          /* 0x09 */
	volatile uint8_t SP;          /* 0x0a */
	volatile uint8_t MODE;        /* 0x0b */
};
#define SPI		((struct spi_regs *) 0x00800008)

#define I2C_STATUS_BUSY         BIT(0)
#define I2C_STATUS_NAK          BIT(2)

#define I2C_CTRL_ID             BIT(0)
#define I2C_CTRL_ADDR           BIT(1)
#define I2C_CTRL_DO             BIT(2)
#define I2C_CTRL_DI             BIT(3)
#define I2C_CTRL_START          BIT(4)
#define I2C_CTRL_STOP           BIT(5)
#define I2C_CTRL_READ_ID        BIT(6)
#define I2C_CTRL_NAK            BIT(7)

struct i2c_regs {
	volatile uint8_t SPD;         /* 0x00 */
	volatile uint8_t ID;          /* 0x01 */
	volatile uint8_t STATUS;      /* 0x02 */
	volatile uint8_t MODE;        /* 0x03 */
	volatile uint8_t ADR;         /* 0x04 */
	volatile uint8_t DO;          /* 0x05 */
	volatile uint8_t DI;          /* 0x06 */
	volatile uint8_t CTRL;        /* 0x07 */
};
#define I2C		((struct i2c_regs *) 0x00800000)

enum areg {
	/* 3.3V domain regs 0x00..0x7f */
	AREG_XTAL_CTRL = 0x01,
	AREG_VOL_LDO_CTRL = 0x02,
	/* deepsleep with sram: write 5 to bit[0:2] */
	AREG_02_UNKN = 0x02, /* sleep-related */
	/* suspend (turn off LL and Modem): write 0x48 */
	AREG_04_UNKN = 0x04, /* sleep-related */
	/* bit0: power down 32kHz RC-OSC
	 * bit1: power down 32kHz XTAL
	 * bit2: power down 24MHz RC-OSC
	 * bit3: power down 24MHz XTAL
	 * bit4: power down power logic, VBUS_LDO and DCDC
	 * bit5: power down DCDC
	 * bit6: power down VBUS_LDO
	 * bit7: power down baseband PLL */
	AREG_05_PWDN = 0x05,
	AREG_PLL_BG = 0x06, /* bit4: 1 -> power down pll */
	AREG_06_PWDN = 0x06, /* 0xff */
	/* bit0: power down SPD LDO
	 * bit1: power down main digital LDO
	 * bit2: power down retention LDO
	 * bit3: power down low-current comparator
	 * bit4: power down temp sensor */
	AREG_07_PWDN = 0x07,
	AREG_0b_PULL = 0x0b,     /* bit7: dp_pullup_res_3v bit5:4: LPCOMP scaling select */
	AREG_0c_FLASH_DCDC = 0x0c, /* 0xc4: vdd_f 1.8V; 0xc6: vdd_f 1.9V; 0xc7: vdd_f 1.95V (max) (bit[0:2]: vdd_f)  */
	AREG_0d_LPCOMP = 0x0d,   /* LPCOMP input + reference select */
	AREG_0e_PA_PULL = 0x0e,  /* pull-up/down control for PA[0:3] */
	AREG_0f_PA_PULL = 0x0f,  /* pull-up/down cotronl for PA[4:7] */
	AREG_10_PB_PULL = 0x10,
	AREG_11_PB_PULL = 0x11,
	AREG_12_PC_PULL = 0x12,
	AREG_13_PC_PULL = 0x13,
	AREG_14_PD_PULL = 0x14,
	AREG_15_PD_PULL = 0x15,
	AREG_1f_DCDC_RDLY1 = 0x1f, /* how long to wait for DCDC ready (16kHz count; default 0x40 -> 4ms) in deep-sleep wakeup */
	/* 0x20: another delay? */
	AREG_PA_POL = 0x21,      /* GPIOA polarity for wakeup */
	AREG_PB_POL = 0x22,      /* GPIOB polarity for wakeup */
	AREG_PC_POL = 0x23,      /* GPIOC polarity for wakeup */
	AREG_PD_POL = 0x24,      /* GPIOD polarity for wakeup */
	AREG_26_WAKEUP_EN = 0x26,   /* bit7: low-power comparator; bit6: 32kHz timer; bit5: usb core; bit4: gpio */
	AREG_WAKEUP_PA = 0x27,
	AREG_WAKEUP_PB = 0x28,
	AREG_WAKEUP_PC = 0x29,
	AREG_WAKEUP_PD = 0x2a,
	/* 0x2b, 0x2c -> sleep related */
	AREG_2B_UNKN = 0x2b, /* 0x5e: suspend, 0xde: deepsleep */
	AREG_2C_UNKN = 0x2c, /* 0x9e: suspend, 0xde: deep w/o sram, 0x5e: deep with sram */
	/* bit0: baseband power (see baseband_reset)
	 * bit1: USB power
	 * bit2: Audio power?
	 */
	AREG_34_PWR = 0x34,

	/* bit0: woke from low-power comp
	 * bit1: woke from 32kHz timer
	 * bit2: woke from USB core
	 * bit3: woke from GPIO
	 * bit4: cal_done_32k
	 * bit5: cal_done_24m
	 * bit6: wd_status
	 * bit7: dcdc_rdy
	 */
	AREG_44_STATUS = 0x44,
	/* 0x00 / 0b00000000: suspend
	 * 0x61 / 0b01100001: deepsleep retain 8KiB
	 * 0x43 / 0b01000011: deepsleep retain 16KiB
	 * 0x07 / 0b00000111: deepsleep retain 32KiB
	 */
	AREG_7E_SLEEP_MODE = 0x7e,
	AREG_7F_SLEEP_MODE = 0x7f,

	/* 1.8V domain regs 0x80..0xff */
	AREG_82_CLK_SETTING = 0x82, /* bit6: 24M_TO_SAR_EN; bit5: 48M_TO_DIG_EN */
	AREG_88_LPCOMP_OUT = 0x88,  /* bit6: lpcomp output */
	AREG_XO_SETTING = 0x8a,
	AREG_BD_GPIOB_IE = 0xbd,
	AREG_BF_GPIOB_DS = 0xbf,
	AREG_C0_GPIOC_IE = 0xc0,
	AREG_C2_GPIOC_DS = 0xc2,
};

#define irq_save(saved) do { \
	int temp; \
	asm volatile ( \
		"/* irq_save */\n" \
		"tmov  %[tmp], #0x80\n" \
		"tmrcs %[out]\n" \
		"tor   %[tmp], %[out]\n" \
		"tmcsr %[tmp]\n" \
		: [out] "=l" (saved), [tmp] "=l" (temp) \
		: : "cc", "memory" \
	); \
} while (0)

static inline void irq_restore(int saved) {
	asm volatile ("tmcsr %0 /* irq_restore */\n" : : "r" (saved));
}

enum gpio_pin {
	PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7,
	PB0, PB1, PB2, PB3, PB4, PB5, PB6, PB7,
	PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7,
	PD0, PD1, PD2, PD3, PD4, PD5, PD6, PD7,
	PE0, PE1, PE2, PE3, PE4, PE5, PE6, PE7,
};

enum gpio_mode {
	PIN_DISABLED,
	PIN_INPUT,
	PIN_OUTPUT,
	PIN_INOUT,
};

enum gpio_pull {
	PULL_NONE,
	PULL_UP_1M,
	PULL_DOWN_160K,
	PULL_UP_18K,
};

#endif /* _TLSR825X_ */
