	.code 16
	.section .vectors,"ax"

	.extern main
	.extern irq_handler

	.global	__start
	.global	__reset
	.global	__irq
	.global __divsi3
	.global __modsi3
	.global __udivsi3
	.global __umodsi3
	.global bitbang_char

	.org 0
__start:
	tj	__reset

	.org 8
	.ascii	"TNLK"
	.word	0x00880400 /* FIXME: Fixed for retention size 0x4000 */

	.org 0x10
	tj	__irq

	.org 24
	.word end

	.org 0x20
__data:
	.word 0x00850000 /* IRQ stack top, assumes 64KiB RAM */
	.word 0x0084fe00 /* SVC stack top, assumes 64KiB RAM */

__irq:
	tpush	{lr}
	tpush	{r0-r7}
	tmrss	r0
	tmov	r1, r8
	tmov	r2, r9
	tmov	r3, r10
	tmov	r4, r11
	tmov	r5, r12
	tpush	{r0-r5}

	tjl	irq_handler

	tpop	{r0-r5}
	tmov	r12, r5
	tmov	r11, r4
	tmov	r10, r3
	tmov	r9, r2
	tmov	r8, r1
	tmssr	r0
	tpop	{r0-r7}
	treti	{r15}

__reset:
	tmov	r0, #0x20
	tloadr	r1, [r0]     @ IRQ stack top
	tmov	r2, #0x12    @ IRQ Mode
	tmcsr	r2
	tmov	sp, r1

	tloadr	r1, [r0, #4] @ SVC stack top
	tmov	r2, #0x13    @ SVC Mode
	tmcsr	r2
	tmov	sp, r1

	tjl	main
	tj	.

__udivsi3:
	tmrcs	r2           /* save current flags */
	tmov	r3, #0x80
	tor	r3, r2
	tmcsr	r3           /* disable interrupts */
	tloadr	r3, .LDIVREG /* load constant 0x800660 */
	tstorer	r0, [r3, #4] /* 0x664 = r0 */
	tstorer	r1, [r3, #8] /* 0x668 = r1 */
	tmov	r0, #0       /* trigger unsigned division */
.LDIVTRIG:
	tstorerb	r0, [r3] /* 0x660 = 0 */
.LDIVWAIT:
	tloadrb	r0, [r3]
	tcmp	r0, #0
	tjne	.LDIVWAIT
	tloadr	r0, [r3, #4] /* result in 0x664 */
	tmcsr	r2           /* restore interrupts */
	tjex	lr

__divsi3:
	tmrcs	r2           /* save current flags */
	tmov	r3, #0x80
	tor	r3, r2
	tmcsr	r3           /* disable interrupts */
	tloadr	r3, .LDIVREG /* load constant 0x800660 */
	tstorer	r0, [r3, #4] /* 0x664 = r0 */
	tstorer	r1, [r3, #8] /* 0x668 = r1 */
	tmov	r0, #1       /* trigger signed division */
	tj .LDIVTRIG

__umodsi3:
	tmrcs	r2           /* save current flags */
	tmov	r3, #0x80
	tor	r3, r2
	tmcsr	r3           /* disable interrupts */
	tloadr	r3, .LDIVREG /* load constant 0x800660 */
	tstorer	r0, [r3, #4] /* 0x664 = r0 */
	tstorer	r1, [r3, #8] /* 0x668 = r1 */
	tmov	r0, #2       /* trigger unsigned modulo */
.LMODTRIG:
	tstorerb	r0, [r3] /* 0x660 = 0 */
.LMODWAIT:
	tloadrb	r0, [r3]
	tcmp	r0, #0
	tjne	.LMODWAIT
	tloadr	r0, [r3, #8] /* result in 0x668 */
	tmcsr	r2           /* restore interrupts */
	tjex	lr

__modsi3:
	tmrcs	r2           /* save current flags */
	tmov	r3, #0x80
	tor	r3, r2
	tmcsr	r3           /* disable interrupts */
	tloadr	r3, .LDIVREG /* load constant 0x800660 */
	tstorer	r0, [r3, #4] /* 0x664 = r0 */
	tstorer	r1, [r3, #8] /* 0x668 = r1 */
	tmov	r0, #3       /* trigger signed modulo */
	tj .LMODTRIG

	.align	4
.LDIVREG:
	.word(0x800660)

	/* r0: char (incl. stop bit), r1: gpio_mask, r2: gpio_addr */
bitbang_char:
	tpush	{r4-r5}
	tloadrb r4, [r2, #0] /* load gpio data */
	tbclr	r4, r1       /* zero bit */
	tstorerb r4, [r2, #0] /* start bit (0) */

	/* bit write loop */
	tmov	r3, #10      /* bit count */
2:
	/* bit delay loop */
	tmov	r5, #62      /* ~115200 baud @ 16MHz (still needs better calibration) */
1:
	tsub	r5, #1
	tjne	1b  /* bit delay loop */

	tmov	r5, #1
	tand	r5, r0 /* extract bit */
	tneg	r5, r5 /* 1 -> 0xff */
	tand	r5, r1 /* 0 -> 0, 0xff -> mask */
	tor	r5, r4
	tstorerb r5, [r2, #0]

	tshftr	r0, #1
	tsub	r3, #1
	tjne	2b  /* bit write loop */

	tpop	{r4-r5}
	tjex	lr

