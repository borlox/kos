#include <bitop.h>
#include <ports.h>
#include <stdlib.h>
#include <kos/error.h>

#include "acpi.h"
#include "idt.h"
#include "kbc.h"
#include "keycode.h"
#include "keymap.h"
#include "tty.h"

#define CPOS(t) (t->x + t->y * TTY_SCREEN_X)

static char *names[NUM_TTYS] = {
	"tty0", "tty1",	"tty2",	"tty3",
	"tty4", "tty5", "tty6", "tty7"
};
static tty_t ttys[NUM_TTYS];
static tty_t *cur_tty;

static tty_t *kout_tty;

typedef struct
{
	const char *name;
	keymap_t    map;
} keymap_holder_t;
static keymap_holder_t *keymaps;
static int              nummaps;
static keymap_t         cur_map;

static word *vmem = (word*)0xB8000;

static struct {
	byte shift;
	byte ctrl;
	byte alt;
	byte altgr;
	byte capslock;
	byte numlock;
} modifiers;

/**
 *  scroll(tty, lines)
 */
static void scroll(tty_t *tty, int lines)
{
	dword offs = lines * TTY_SCREEN_X * 2;
	dword count = (TTY_SCREEN_Y - lines) * TTY_SCREEN_X * 2;
	dword ncount = lines * TTY_SCREEN_X * 2;

	if (tty == cur_tty) {
		memmove(((byte*)vmem), ((byte*)vmem) + offs, count);
		memset(((byte*)vmem) + count, 0, ncount);
	}

	memmove(((byte*)tty->outbuf), ((byte*)tty->outbuf) + offs, count);
	memset(((byte*)tty->outbuf) + count, 0, ncount);
	tty->y -= lines;
}

/**
 *  flush(tty)
 */
static inline void flush(tty_t *tty)
{
	if (tty == cur_tty) {
		memcpy(vmem, tty->outbuf, TTY_SCREEN_SIZE * 2);
	}
}

/**
 *  clear(tty)
 */
static inline void clear(tty_t *tty)
{
	memset(tty->outbuf, 0, TTY_SCREEN_SIZE * 2); // TTY_SCREEN_SIZE is in words
}

/**
 *  update_cursor(tty)
 */
static inline void update_cursor(tty_t *tty)
{
	if (tty == cur_tty) {
		word pos = CPOS(tty);

		outb(0x3D4, 15);
		outb(0x3D5, pos);
		outb(0x3D4, 14);
		outb(0x3D5, pos >> 8);
	}
}

/**
 *  putc(tty, c)
 */
static void putc(tty_t *tty, char c)
{
	switch (c) {
	case '\n':
		tty->x = 0;
		tty->y++;
		break;

	case '\r':
		tty->x = 0;
		break;

	case '\b':
		if (tty->x >= 1) {
			tty->x--;
			tty->outbuf[CPOS(tty)] = ' ' | (tty->status << 8);
			if (tty == cur_tty)
				vmem[CPOS(tty)] = ' ' | (tty->status << 8);
		}
		break;

	case '\t':
		do {
			putc(tty, ' ');
		} while (tty->x % 7);
		break;

	default:
		tty->outbuf[CPOS(tty)] = c | (tty->status << 8);
		if (tty == cur_tty)
			vmem[CPOS(tty)] = c | (tty->status << 8);
		tty->x++;
	}

	if (tty->x >= TTY_SCREEN_X) {
		tty->y++;
		tty->x = 0;
	}

	while (tty->y >= TTY_SCREEN_Y) {
		scroll(tty, 1);
	}

	update_cursor(tty);
}

/**
 *  puts(tty, str)
 */
static void puts(tty_t *tty, const char *str)
{
	while (*str)
		putc(tty, *str++);
}

/**
 *  putn(tty, num, base)
 */
static void putn(tty_t *tty, int num, int base, int pad, char pc)
{
	static char digits[] = "0123456789ABCDEFGHIJKLMOPQRSTUVWXYZ";

	char tmp[65];
	char *end = tmp + 64;
	int rem;

	if (base < 2 || base > 36)
		return;

	*end-- = 0;

	do {
		rem = num % base;
		num = num / base;
		*end-- = digits[rem];
		pad--;
	} while (num > 0);

	while (pad-- > 0) {
		putc(tty, pc);
	}

	while (*(++end)) {
		putc(tty, *end);
	}
}

/**
 *  can_answer_rq(tty, gotrq)
 *
 * Returns 1 if there is a request that can be answered
 */
static byte can_answer_rq(tty_t *tty, int gotrq)
{
	if (!gotrq && !tty->rqcount) {
		return 0;
	}

	if (tty->flags & TTY_RAW) {
		return (tty->incount >= tty->rqs[0]->buflen);
	}
	else {
		return (tty->eotcount > 0);
	}
}

/**
 *  answer_rq(tty, rq)
 *
 * Answers the given request or the first request in the tty's queue.
 *
 * Note: This function does not check anything.
 *       Be sure you know what you're doing!
 */
static void answer_rq(tty_t *tty, fs_request_t *rq)
{
	byte remove_rq = 0;

	if (!rq) {
		rq = tty->rqs[0];
		remove_rq = 1;
	}

	byte len = 0, offs = 0;
	/* copy the data to the buffer */
	if (tty->flags & TTY_RAW) {
		memcpy(rq->buf, tty->inbuf, rq->buflen);
		len  = rq->buflen;
		offs = rq->buflen;
	}
	else {
		len  = strlen(tty->inbuf); // eot is marked by a \0
		strcpy(rq->buf, tty->inbuf);
		offs = len + 1;
	}

	/* update the inbuffer */
	//int num = tty->incount - offs;
	//if (num > 0) {
	//	memmove(tty->inbuf, tty->inbuf + offs, tty->incount - offs);
	//}
	memmove(tty->inbuf, tty->inbuf + offs, TTY_INBUF_SIZE - len);
	tty->incount -= offs;
	/* and finish the rq */
	rq->result = len;

	fs_finish_rq(rq);

	if (remove_rq) {
		memmove(tty->rqs, tty->rqs + 1, (tty->rqcount - 1) * sizeof(fs_request_t*));
		tty->rqcount--;
		tty->rqs = realloc(tty->rqs, tty->rqcount * sizeof(fs_request_t*));
	}

	if (!(tty->flags & TTY_RAW)) {
		tty->eotcount--;
	}
}


/**
 *  open(tty, rq)
 */
static void open(tty_t *tty, fs_request_t *rq)
{
	if (!tty->owner && !tty->opencount)
		tty->owner = rq->proc;

	if (tty->owner == rq->proc)
		tty->opencount++;

	fs_finish_rq(rq);
}

/**
 *  close(tty, rq)
 */
static void close(tty_t *tty, fs_request_t *rq)
{
	if (tty->owner == rq->proc)
		tty->opencount--;

	if (!tty->opencount)
		tty->owner = 0;

	fs_finish_rq(rq);
}

/**
 *  read(tty, rq)
 */
static void read(tty_t *tty, fs_request_t *rq)
{
	if (can_answer_rq(tty, 1)) { /* fullfill request */
		answer_rq(tty, rq);
	}
	else { /* store request */
		tty->rqs = realloc(tty->rqs, sizeof(fs_request_t*) * (++tty->rqcount));
		tty->rqs[tty->rqcount-1] = rq;
	}
}

/**
 *  write(tty, rq)
 */
static void write(tty_t *tty, fs_request_t *rq)
{
	int i=0;
	char *buffer = rq->buf;
	for (; i < rq->buflen; ++i) {
		putc(tty, buffer[i]);
	}

	rq->result = rq->buflen;
	fs_finish_rq(rq);
}

/**
 *  query(file, rq)
 */
static int query(fs_devfile_t *file, fs_request_t *rq)
{
	tty_t *tty = (tty_t*)file; // the file is just the first position in the tty_t struct

	switch (rq->type) {
	case RQ_OPEN:
		open(tty, rq);
		break;

	case RQ_CLOSE:
		close(tty, rq);
		break;

	case RQ_READ:
		read(tty, rq);
		break;

	case RQ_WRITE:
		write(tty, rq);
		break;

	default:
		fs_finish_rq(rq);
		return E_NOT_SUPPORTED;
	}

	return OK;
}

/**
 *  tty_dbg_info(tty)
 */
static void tty_dbg_info(tty_t *tty)
{
	putc(tty, '\n');
	puts(tty, "== TTY Debug Info ==\n");
	puts(tty, "   Id:        ");
	putc(tty, '0' + tty->id);
	putc(tty, '\n');

	puts(tty, "   Mode:      ");
	if (tty->flags & TTY_RAW)
		puts(tty, "raw\n");
	else
		puts(tty, "cbreak\n");

	puts(tty, "   Modifiers: ");
	if (modifiers.shift)
		puts(tty, "shift ");
	if (modifiers.alt)
		puts(tty, "alt ");
	if (modifiers.altgr)
		puts(tty, "altgr ");
	if (modifiers.ctrl)
		puts(tty, "ctrl ");
	putc(tty, '\n');

	puts(tty, "   Incount:   ");
	putc(tty, tty->incount + '0');
	putc(tty, '\n');

	puts(tty, "   RQs:       ");
	putc(tty, tty->rqcount + '0');
	putc(tty, '\n');

	puts(tty, "   EOTs:      ");
	putc(tty, tty->eotcount + '0');
	putc(tty, '\n');

}

/** following code handles the keyboard irq **/
static inline byte get_keycode(byte *brk)
{
	static byte e0 = 0; // set to 1 if the prev was 0xE0
	static byte e1 = 0; // set to 1 if the prev was 0xE1 or to 2 when the prev-prev was 0xE1
	static word e1_data = 0;

	byte data = kbc_getkey();

	if ((data & 0x80) &&
	    (e1 || (data != 0xE1)) &&
	    (e0 || (data != 0xE0)))
	{
		*brk = 1;
		data &= ~0x80;
	}
	else {
		*brk = 0;
	}

	if (e0) {
		e0 = 0;
		return kbc_scan_to_keycode(KBC_E0, data);
	}
	else if (e1 == 2) {
		e1_data |= ((word)data << 8);
		e1 = 0;
		return kbc_scan_to_keycode(KBC_E1, e1_data);
	}
	else if (e1 == 1) {
		e1_data = data;
		e1++;
	}
	else if (data == 0xE0) {
		e0 = 1;
	}
	else if (data == 0xE1) {
		e1 = 1;
	}
	else {
		return kbc_scan_to_keycode(KBC_NORMAL, data);
	}

	return 0;
}

static inline byte handle_modifiers(byte code, byte brk)
{
	switch (code) {
	case KEYC_SHIFT_LEFT:
	case KEYC_SHIFT_RIGHT:
		modifiers.shift = !brk;
		return 1;

	case KEYC_CTRL_LEFT:
	case KEYC_CTRL_RIGHT:
		modifiers.ctrl = !brk;
		return 1;

	case KEYC_ALT:
		modifiers.alt = !brk;
		return 1;

	case KEYC_ALTGR:
		modifiers.altgr = !brk;
		return 1;

	default:
		return 0;
	}
}

static inline byte handle_raw(byte code)
{
	if (modifiers.ctrl) {
		if (modifiers.shift) {
			if (code == KEYC_DEL) {
				kbc_reset_cpu();
			}
			else if (code == KEYC_END) {
				// weniger steuern, mehr alten und komplett entfernen!
				acpi_poweroff();
				puts(cur_tty, "\nCould not shutdown. Sorry.\n");
			}
		}

		if (code >= KEYC_F1 && code <= KEYC_F8) {
			tty_set_cur_term(code - KEYC_F1);
			return 1;
		}
	}

	if (code == KEYC_F12) {
		tty_dbg_info(cur_tty);
		return 1;
	}

	return 0;
}

static inline byte handle_cbreak_input(byte c)
{
	switch (c) {
	case EOT:
	case '\n':
		cur_tty->eotcount++;
		cur_tty->inbuf[cur_tty->incount++] = 0;
		return 1;

	case '\b':
		if (cur_tty->inbuf[cur_tty->incount-1] == 0)
			cur_tty->eotcount--;
		cur_tty->incount--;
		return 1;
	}

	return 0;
}

static void handle_input(byte code)
{
	if (!cur_map) {
		return;
	}

	byte c = 0;
	if (modifiers.shift || modifiers.capslock)
		c = cur_map[code].shift;
	else if (modifiers.altgr)
		c = cur_map[code].altgr;
	else if (modifiers.ctrl)
		c = cur_map[code].ctrl;
	else
		c = cur_map[code].normal;

	if (c == 0) {
		return;
	}

	if (cur_tty->incount == TTY_INBUF_SIZE)
		return;

	if ((cur_tty->flags & TTY_ECHO) && c != EOT)
		putc(cur_tty, c);

	if (!(cur_tty->flags & TTY_RAW)) {
		if (handle_cbreak_input(c))
			goto input_end;
	}
	else if (c == EOT) {
		return;
	}

	cur_tty->inbuf[cur_tty->incount++] = c;

input_end:
	while (can_answer_rq(cur_tty, 0)) {
		answer_rq(cur_tty, 0);
	}

}

/**
 *  tty_irq_handler(irq, esp)
 */
static void tty_irq_handler(int irq, dword *esp)
{
	byte brk = 0;
	byte keyc = get_keycode(&brk);

	if (!keyc)
		return;

	if (handle_modifiers(keyc, brk)) {
		return;
	}

	if (brk) {
		return; // no need for keydowns anymore
	}

	if (handle_raw(keyc)) {
		return;
	}

	handle_input(keyc);
}

/** some tty service functions **/

/**
 *  tty_set_cur_term(n)
 */
void tty_set_cur_term(byte n)
{
	if (n < NUM_TTYS) {
		cur_tty = &ttys[n];
		flush(cur_tty);
		update_cursor(cur_tty);
	}
}

/**
 *  tty_get_cur_term()
 */
byte tty_get_cur_term()
{
	return cur_tty->id;
}

/**
 *  tty_register_keymap(name, map)
 */
void tty_register_keymap(const char *name, keymap_t map)
{
	keymaps = realloc(keymaps, (++nummaps) * sizeof(keymap_holder_t));
	keymaps[nummaps-1].name = name;
	keymaps[nummaps-1].map  = map;

	if (!cur_map)
		cur_map = map;
}

/**
 *  tty_select_keymap(name)
 */
int tty_select_keymap(const char *name)
{
	int i=0;
	for (; i < nummaps; ++i) {
		if (strcmp(keymaps[i].name, name) == 0) {
			cur_map = keymaps[i].map;
			return OK;
		}
	}
	return E_NOT_FOUND;
}

/**
 *  tty_puts(str)
 *
 * Prints the given string on the current terminal
 */
void tty_puts(const char *str)
{
	puts(cur_tty, str);
}

/**
 *  tty_putn(num, base)
 *
 * Prints the given number on the current terminal
 */
void tty_putn(int num, int base)
{
	putn(cur_tty, num, base, 0, ' ');
}

/**
 *  init_tty()
 */
void init_tty(void)
{
	int i=0;
	for (; i < NUM_TTYS - 1; ++i) { // spare the kout_tty
		tty_t *tty = &ttys[i];

		tty->id = i;
		tty->file.path  = names[i];
		tty->file.query = query;

		tty->incount = 0;
		tty->status  = 0x07;
		tty->flags   = TTY_ECHO;

		tty->rqcount = 0;

		clear(tty);

		fs_create_dev(&tty->file);
	}

	// anything else for tty7 is done in init_kout
	fs_create_dev(&kout_tty->file);

	modifiers.shift = 0;
	modifiers.ctrl  = 0;
	modifiers.alt   = 0;
	modifiers.altgr = 0;

	nummaps = 0;
	cur_map = NULL;

	idt_set_irq_handler(1, tty_irq_handler);

	tty_set_cur_term(0);
}

/** kout **/

void init_kout(void)
{
	kout_tty = &ttys[kout_id];

	kout_tty->id = kout_id;
	kout_tty->file.path  = names[kout_id];
	kout_tty->file.query = query;

	kout_tty->incount = 0;
	kout_tty->status  = 0x07;
	kout_tty->flags   = TTY_ECHO;

	kout_tty->rqcount = 0;

	clear(kout_tty);

	cur_tty = kout_tty;
}

void kout_puts(const char *str)
{
	puts(kout_tty, str);
}

void kout_putn(int num, int base)
{
	putn(kout_tty, num, base, 0, ' ');
}

void kout_aprintf(const char *fmt, int **args)
{
	long long val = 0;
	int pad;
	char padc;

	while (*fmt) {
		if (*fmt == '%') {
			fmt++;

			pad = 0;
			if (*fmt == '0') {
				padc = '0';
				fmt++;
			}
			else {
				padc = ' ';
			}

			while (*fmt >= '0' && *fmt <= '9') {
				pad = pad * 10 + *fmt++ - '0';
			}

			if (*fmt == 'd' || *fmt == 'u') {
				val = *(*args)++;
				if (val < 0) {
					putc(kout_tty, '-');
					pad--;
					val = -val;
				}
			}
			else if (*fmt == 'i' || *fmt == 'o' || *fmt == 'p' || *fmt == 'x') {
				val = *(*args)++;
				val = val  & 0xffffffff;
			}


			switch (*fmt) {
			case 'c':
				putc(kout_tty, *(*args)++);
				break;

			case 'd':
			case 'i':
			case 'u':
				putn(kout_tty, val, 10, pad, padc);
				break;

			case 'o':
				putn(kout_tty, val, 8, pad, padc);
				break;

			case 'p':
			case 'x':
				putn(kout_tty, val, 16, pad, padc);
				break;

			case 's':
				puts(kout_tty, (char*)*(*args)++);
				break;

			case '%':
				putc(kout_tty, '%');
				break;

			default:
				putc(kout_tty, '%');
				putc(kout_tty, *fmt);
				break;
			}
			fmt++;
		}
		else {
			putc(kout_tty, *fmt++);
		}
	}
}

void kout_printf(const char *fmt, ...)
{
	int *args = ((int*)&fmt) + 1;
	kout_aprintf(fmt, &args);
}

byte kout_set_status(byte status)
{
	byte old = kout_tty->status;
	kout_tty->status = status;
	return old;
}

void kout_select(void)
{
	tty_set_cur_term(kout_id);
}