#ifndef KERNEL_H
#define KERNEL_H

#include <multiboot.h>

extern multiboot_info_t multiboot_info;

/* the following symbols are generated by the linker */
/* they are defined as extern-functions as only their address is important */
extern void kernel_phys_start(void);
extern void kernel_phys_end(void);
extern void kernel_start(void);
extern void kernel_end(void);
extern void kernel_size();

void panic(const char *fmt, ...);

#endif /*KERNEL_H*/
