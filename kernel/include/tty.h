#ifndef TTY_H
#define TTY_H

#include <stdarg.h>
#include <types.h>
#include "keymap.h"
#include "pm.h"
#include "fs/fs.h"
#include "util/list.h"

#define NUM_TTYS 8

#define TTY_INBUF_SIZE 256
#define TTY_SCREEN_X    80
#define TTY_SCREEN_Y    25
#define TTY_SCREEN_SIZE (TTY_SCREEN_X * TTY_SCREEN_Y)

#define TTY_ECHO 0x01 // 00000001
#define TTY_RAW  0x02 // 00000010

typedef struct tty
{
	int   id;

	inode_t inode;

	char  inbuf[TTY_INBUF_SIZE];
	dword incount;
	dword eotcount;

	word  outbuf[TTY_SCREEN_SIZE];

	byte  x, y;
	byte  status;

	byte  flags;

	list_t *requests;

	int    opencount;
	proc_t *owner;
} tty_t;

#define kout_id (NUM_TTYS-1)

void init_kout(void);
void kout_puts(const char *str);
void kout_putn(int num, int base);
void kout_aprintf(const char *fmt, va_list args);
void kout_printf(const char *fmt, ...);
byte kout_set_status(byte status);
void kout_select(void);
void kout_clear(void);

void init_tty(void);

void tty_set_cur_term(byte n);
byte tty_get_cur_term(void);

void tty_register_keymap(const char *name, keymap_t map);
int  tty_select_keymap(const char *name);

void tty_puts(const char *str);
void tty_putn(int num, int base);

#endif /*TTY_H*/
