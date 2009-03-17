#include <kos/syscalln.h>
#include <kos/syscall.h>

#include "syscall_helper.h"

void kos_exit(int status)
{
	SYSCALL1(SC_EXIT, status);
}
