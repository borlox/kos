#include <kos/syscalln.h>
#include <kos/syscall.h>

#include "syscall_helper.h"

void kos_exit(void)
{
	SYSCALL0(SC_EXIT);
}
