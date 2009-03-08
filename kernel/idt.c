#include <bitop.h>
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "kernel.h"
#include <ports.h>
#include "regs.h"
#include "syscall.h"

idt_entry_t   idt[IDT_SIZE];
irq_handler_t irq_handlers[16] = {0};

byte idt_in_irq_handler;

/* int.s */
extern void isr_null_handler(void);

extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_9(void);
extern void isr_stub_10(void);
extern void isr_stub_11(void);
extern void isr_stub_12(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_15(void);
extern void isr_stub_16(void);
extern void isr_stub_17(void);
extern void isr_stub_18(void);
extern void isr_stub_19(void);

extern void irq_stub_0(void);
extern void irq_stub_1(void);
extern void irq_stub_2(void);
extern void irq_stub_3(void);
extern void irq_stub_4(void);
extern void irq_stub_5(void);
extern void irq_stub_6(void);
extern void irq_stub_7(void);
extern void irq_stub_8(void);
extern void irq_stub_9(void);
extern void irq_stub_10(void);
extern void irq_stub_11(void);
extern void irq_stub_12(void);
extern void irq_stub_13(void);
extern void irq_stub_14(void);
extern void irq_stub_15(void);

extern void syscall_stub(void);

static const char *fault_msg[] = {
	"Devide by 0",
	"Debug exception",
	"NMI",
	"INT3",
	"INTO",
	"BOUND exception",
	"Invalid opcode",
	"No coprocessor",
	"Double fault",
	"Coprocessor segment overrun",
	"Bad TSS",
	"Segment not present",
	"Stack fault",
	"General protection fault",
	"Page fault",
	"??",
	"Coprocessor error",
	"Alignment check",
	"??",	"??", "??", "??", "??",
	"??",	"??", "??", "??", "??",
	"??", "??", "??", "??",
};

static void idt_handle_exception(dword *esp)
{
	regs_t *regs = (regs_t*)*esp;
	con_set_color(0x4F);
	con_printf("kOS triggered an exception.\n");

	con_printf("Exception: #%02d (%s) @ %06x:%010x\n",
	           regs->intr, fault_msg[regs->intr], regs->cs, regs->eip);
//	con_printf("ss:esp = %06x:%010x error code: %010x\n",
//	           regs->u_ss, regs->u_esp, regs->errc);
//	con_printf("eax: %010x ebx: %010x ecx: %010x edx: %010x\n",
//	           regs->eax, regs->ebx, regs->ecx, regs->edx);
//	con_printf("ebp: %010x esp: %010x esi: %010x edi: %010x\n",
//	           regs->ebp, regs->esp, regs->esi, regs->edi);
//	con_printf("eflags: %010x ds: %06x es: %06x fs: %06x gs: %06x\n",
//	            regs->eflags, regs->ds, regs->es, regs->fs, regs->gs);

	con_putc('\n');

	panic("Kernel Exception\n");
}

static void idt_handle_irq(dword *esp)
{
	regs_t *regs = (regs_t*)*esp;
	dword irq = regs->intr - IRQ_BASE;

	/*con_printf("IRQ: %d\n", irq);*/

	//pm_restore(esp);

	if (irq == 7 || irq == 15) {
		byte pic = (irq < 8) ? PIC1 : PIC2;
		outb(pic + 3, 0x03);
		if ((inb(pic) & 0x80) != 0) {
			goto irq_handeled;
		}
	}

	if (irq_handlers[irq])
		irq_handlers[irq](irq, esp);

	//pm_pick(esp);

irq_handeled:
	if (irq >= 8)
		outb(PIC2_CMD, PIC_EOI);
	outb(PIC1_CMD, PIC_EOI);
}

/* this function is called from int.s */
dword idt_handle_int(dword esp)
{
	regs_t *regs = (regs_t*)esp;

	idt_in_irq_handler = 1;

	pm_restore(&esp);

	if (regs->intr < IRQ_BASE) {
		idt_handle_exception(&esp);
	}
	else if (regs->intr < SYSCALL) {
		idt_handle_irq(&esp);
	}
	else {
		syscall(&esp);
	}

	pm_pick(&esp);

	idt_in_irq_handler = 0;

	return esp;
}

byte idt_set_irq_handler(byte irq, irq_handler_t handler)
{
	if (irq > 15)
		return 0;

	if (irq_handlers[irq] != 0)
		return 0;

	irq_handlers[irq] = handler;
	return 1;
}

byte idt_clr_irq_handler(byte irq)
{
	if (irq > 15)
		return 0;

	irq_handlers[irq] = 0;
	return 1;
}

void init_idt(void)
{
	int i = 0;

	for (; i < IDT_SIZE; ++i) {
		idt_set_gate(i, GDT_SEL_CODE, isr_null_handler, 0, IDT_INTERRUPT_GATE);
	}

	idt_in_irq_handler = 0;

	idt_set_gate( 0, GDT_SEL_CODE, isr_stub_0,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 1, GDT_SEL_CODE, isr_stub_1,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 2, GDT_SEL_CODE, isr_stub_2,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 3, GDT_SEL_CODE, isr_stub_3,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 4, GDT_SEL_CODE, isr_stub_4,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 5, GDT_SEL_CODE, isr_stub_5,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 6, GDT_SEL_CODE, isr_stub_6,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 7, GDT_SEL_CODE, isr_stub_7,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 8, GDT_SEL_CODE, isr_stub_8,  0, IDT_INTERRUPT_GATE);
	idt_set_gate( 9, GDT_SEL_CODE, isr_stub_9,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(10, GDT_SEL_CODE, isr_stub_10, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(11, GDT_SEL_CODE, isr_stub_11, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(12, GDT_SEL_CODE, isr_stub_12, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(13, GDT_SEL_CODE, isr_stub_13, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(14, GDT_SEL_CODE, isr_stub_14, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(15, GDT_SEL_CODE, isr_stub_15, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(16, GDT_SEL_CODE, isr_stub_16, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(17, GDT_SEL_CODE, isr_stub_17, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(18, GDT_SEL_CODE, isr_stub_18, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(19, GDT_SEL_CODE, isr_stub_19, 0, IDT_INTERRUPT_GATE);

	idt_set_gate(IRQ_BASE +  0, GDT_SEL_CODE, irq_stub_0,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  1, GDT_SEL_CODE, irq_stub_1,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  2, GDT_SEL_CODE, irq_stub_2,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  3, GDT_SEL_CODE, irq_stub_3,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  4, GDT_SEL_CODE, irq_stub_4,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  5, GDT_SEL_CODE, irq_stub_5,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  6, GDT_SEL_CODE, irq_stub_6,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  7, GDT_SEL_CODE, irq_stub_7,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  8, GDT_SEL_CODE, irq_stub_8,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE +  9, GDT_SEL_CODE, irq_stub_9,  0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE + 10, GDT_SEL_CODE, irq_stub_10, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE + 11, GDT_SEL_CODE, irq_stub_11, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE + 12, GDT_SEL_CODE, irq_stub_12, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE + 13, GDT_SEL_CODE, irq_stub_13, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE + 14, GDT_SEL_CODE, irq_stub_14, 0, IDT_INTERRUPT_GATE);
	idt_set_gate(IRQ_BASE + 15, GDT_SEL_CODE, irq_stub_15, 0, IDT_INTERRUPT_GATE);

	idt_set_gate(SYSCALL, GDT_SEL_CODE, syscall_stub, 3, IDT_INTERRUPT_GATE);

	/* PIC */
	/* start initialization */
	outb_wait(PIC1_CMD, ICW1_INIT + ICW1_ICW4);
	outb_wait(PIC2_CMD, ICW1_INIT + ICW1_ICW4);
  /* define PIC vectors */
	outb_wait(PIC1_DATA, IRQ_BASE);
	outb_wait(PIC2_DATA, IRQ_BASE + 8);
	/* continue initialization */
	outb_wait(PIC1_DATA, 4);
	outb_wait(PIC2_DATA, 2);
	/* mode: 8086 */
  outb_wait(PIC1_DATA, ICW4_8086);
  outb_wait(PIC2_DATA, ICW4_8086);
	/* nothing masked */
  outb_wait(PIC1_DATA, 0x00);
  outb_wait(PIC2_DATA, 0x00);

	struct {
		word  size;
		dword base;
	} __attribute__((packed)) idt_ptr = {
		.size = IDT_SIZE * 8 - 1,
		.base = (dword)idt,
	};

	asm volatile("lidt %0" : : "m"(idt_ptr));

	disable_intr();
}

void idt_set_gate(int intr, word selector, void *handler,
                  byte dpl, byte type)
{
	idt[intr].base_low  = bmask((dword)handler, BMASK_WORD);
	idt[intr].selector  = selector;
	idt[intr].zero      = 0x00;
	idt[intr].type      = IDT_GATE_PRESENT | (bmask(dpl, BMASK_2BIT) << 5) |
	                      bmask(type,BMASK_4BIT);
	idt[intr].base_high = bmask(((dword)handler >> 16), BMASK_WORD);
}
