#include <kos/msg.h>
#include <kos/syscalln.h>
#include <sys/types.h>
#include "helper.h"

int open_std_files(void)
{
	return SYSCALL0(SC_OPEN_STD);
}

void yield(void)
{
	SYSCALL0(SC_YIELD);
}

void sleep(uint32_t msec)
{
	SYSCALL1(SC_SLEEP, msec);
}

int send(int target, msg_t *msg)
{
	return SYSCALL2(SC_SEND, target, (int32_t)msg);
}

int recv(msg_t *buf, int block)
{
	return SYSCALL2(SC_RECV, (int32_t)buf, block);
}

pid_t waitpid(pid_t pid, int *status, int options)
{
	return SYSCALL3(SC_WAITPID, pid, (int32_t)status, options);
}

pid_t execute(const char *path, const char *cmdline)
{
	STR_PARAM(path_p, path);
	STR_PARAM(cmd_p, cmdline);

	return SYSCALL2(SC_EXECUTE, path_p, cmd_p);
}

char *getcmdline()
{
	return (char*)SYSCALL0(SC_GETCMDLINE);
}
