#ifndef _TLSR825X_
#define _TLSR825X_

#include <inttypes.h>

#define SYS_CLOCK	24000000  /* 24MHz */

#define READREG(r)	((volatile uint32_t)(*(uint32_t *)(r)))
#define WRITEREG(r,v)	(*(volatile uint32_t *)(r) = v)

#define ASSERT_EQ(a, b) do { \
	char foo[(a) == (b) ? 1 : -1] \
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

struct systim_regs {
	volatile uint32_t TICK;
	volatile uint32_t TRIG;
	uint8_t res0x748[4];
	volatile uint8_t CTRL0;
	uint8_t res0x74d[2];
	volatile uint8_t CTRL1;
	volatile uint16_t R750;
	uint8_t res0x752[2];
};
#define SYSTIM		((struct systim_regs *) 0x00800740)

struct mcu_regs {
	uint8_t res0x600[0x2];               /* 0x600 */
	/* 0x05: STOP
	 * 0x06: STALL
	 * 0x08: GO alt
	 * 0x84: GO
	 * 0x88: RESET
	 */
	volatile uint8_t MODE;               /* 0x602 */
	uint8_t res0x603[0x9];               /* 0x603 */
	volatile uint8_t RETENTION_DATA_END; /* 0x60c */
	volatile uint8_t TAG_DATA_END;       /* 0x60d */
	uint8_t res0x60e[0x2];               /* 0x60e */
	uint8_t res0x610[0x30];              /* 0x610 */
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
			volatile uint8_t IN;
			volatile uint8_t IE;
			volatile uint8_t OEN;
			volatile uint8_t OUT;
		};
		volatile uint32_t SETTING1;
	};
	union {
		struct {
			volatile uint8_t POL;
			volatile uint8_t DS;
			volatile uint8_t ACT_AS_GPIO;
			volatile uint8_t IRQ_EN;
		};
		volatile uint32_t SETTING2;
	};
};

struct gpio_regs {
	union {
		struct {
			struct gpio_port_regs PA;
			struct gpio_port_regs PB; /* AFE for IE/DS */
			struct gpio_port_regs PC; /* AFE for IE/DS */
			struct gpio_port_regs PD;
			struct gpio_port_regs PE;
		};
		struct gpio_port_regs PORT[5];
	};
	volatile uint16_t MUX_FUNC[4];
	uint8_t res0x5b0[5];
	volatile uint8_t WAKEUP_IRQ; /* 0x5b5 */
	volatile uint8_t I2C_SPI_OUT_EN; /* 0x5b6 */
	volatile uint8_t I2C_SPI_EN; /* 0x5b7 */
	volatile uint8_t RISC0_EN[5]; /* 0x5b8 */
	uint8_t res0x5bd[3];
	volatile uint8_t RISC1_EN[5]; /* 0x5c0 */
	uint8_t res0x5c5[3];
	volatile uint8_t RISC2_EN[5]; /* 0x5c8 */
};
#define GPIO		((struct gpio_regs *)    0x00800580)

struct i2c_adr_regs {
	volatile uint8_t HADR;  /* 0x0e0 */
};
#define I2C_ADR		((struct i2c_adr_regs *) 0x008000e0)

struct afe_regs {
	union {
		struct {
			volatile uint8_t ADDR;
			volatile uint8_t VALUE;
		};
		volatile uint16_t ADDRVAL;
	} __attribute__((packed));
	volatile uint8_t CMD;
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
	volatile uint8_t WAKEUP_EN;   /* 0x6e */
	volatile uint8_t PWDN_CTRL;   /* 0x6f */
	volatile uint8_t FHS_SEL;     /* 0x70 */
	uint8_t res0x71[0x07];
	volatile uint8_t MCU_WAKEUP_MASK; /* 0x78 */
	uint8_t res0x79[2];
	volatile uint8_t CLK_DIV_7816;    /* 0x7b */
	uint8_t res0x7c;
	volatile uint8_t VER_ID;      /* 0x7d */
	volatile uint8_t PROD_ID;     /* 0x7e */
};
#define SYSCTL		((struct sysctl_regs *) 0x00800040)

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
	AREG_XTAL_CTRL = 0x01,
	AREG_VOL_LDO_CTRL = 0x02,
	AREG_PLL_BG = 0x06,
	AREG_0b_PULL = 0x0b, /* bit7: dp_pullup_res_3v */
	AREG_WAKEUP_PA = 0x27,
	AREG_WAKEUP_PB = 0x28,
	AREG_WAKEUP_PC = 0x29,
	AREG_WAKEUP_PD = 0x2a,
	AREG_CLK_SETTING = 0x82,
	AREG_XO_SETTING = 0x8a,
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
