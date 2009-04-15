#include <bitop.h>
#include <string.h>
#include <kos/error.h>
#include "ipc.h"
#include "syscall.h"
#include "mm/util.h"
#include "mm/virt.h"

// FIXME: does not handle user space!

static int send(proc_t *proc, msg_t *msg)
{
	/* check args */
	if (proc->status == PS_SLOT_FREE)
		return E_INVALID_ARG;

	if (!msg)
		return E_INVALID_ARG;

	if (proc->msg_count == PROC_MSG_BUFFER_SIZE) {
		return E_TRY_AGAIN;
	}

	/* copy the message to the targets buffer */
	memcpy(proc->msg_head, msg, sizeof(msg_t));
	proc->msg_head++;
	proc->msg_count++;

	if (proc->msg_head == &proc->msg_buffer[PROC_MSG_BUFFER_SIZE])
		proc->msg_head = proc->msg_buffer;

	return OK;
}

static int receive(proc_t *proc, msg_t *msg)
{
	/* check args */
	if (proc->status == PS_SLOT_FREE)
		return E_INVALID_ARG;

	if (!msg)
		return E_INVALID_ARG;

	if (proc->msg_count == 0)
		return E_TRY_AGAIN;

	/* copy the message from the procbuffer to the user */
	memcpy(msg, proc->msg_tail, sizeof(msg_t));
	proc->msg_tail++;
	proc->msg_count--;

	if (proc->msg_tail == &proc->msg_buffer[PROC_MSG_BUFFER_SIZE])
		proc->msg_tail = proc->msg_buffer;

	return OK;
}

/**
 *  ipc_send(from, to, msg)
 *
 * Sends a message from a process to another.
 * This function may unblock the 'to'-proc, when it
 * was blocked for receiving a not yet exisiting message.
 */
int ipc_send(proc_t *from, proc_t *to, msg_t *msg)
{
	msg->sender = from->pid;
	byte status = send(to, msg);

	// if the target was blocked by waiting for a message wake it up
	if (pm_is_blocked_for(to, BR_RECEIVING)) {
		receive(to, to->msg_wait_buffer);
		km_free_addr(to->msg_wait_buffer, sizeof(msg_t));
		to->msg_wait_buffer = (void*)0;
		pm_unblock(to);
	}

	return status;
}

/**
 *  ipc_receive(proc, msg, block)
 *
 * Tries to receive a message for a process.
 * If there is no message yet the process is blocked (when 'block' is true)
 * or E_TRY_AGAIN is returnd.
 */
int ipc_receive(proc_t *proc, msg_t *msg, byte block)
{
	int status = receive(proc, msg);

	// if there's no message to receive and we
	//   should block, disable the process
	if (status == E_TRY_AGAIN && block) {
		proc->msg_wait_buffer = msg;
		pm_block(proc, BR_RECEIVING);
		return OK;
	}
	else if (status == OK) {
		km_free_addr(msg, sizeof(msg_t));
	}

	return status;
}

dword sys_send(dword calln, dword target, dword msgptr, dword arg2)
{
	proc_t *proc = pm_get_proc(target);
	msg_t  *msg  = vm_user_to_kernel(cur_proc->pagedir, (vaddr_t)msgptr,
	                                 sizeof(msg_t));

	dword result = ipc_send(cur_proc, proc, msg);

	km_free_addr(msg, sizeof(msg_t));

	return result;
}

dword sys_receive(dword calln, dword msgptr, dword block, dword arg2)
{
	msg_t *msg = vm_user_to_kernel(cur_proc->pagedir, (vaddr_t)msgptr,
	                               sizeof(msg_t));

	// receive has to free the address
	return ipc_receive(cur_proc, msg, block);
}

void init_ipc(void)
{
	syscall_register(SC_SEND, sys_send);
	syscall_register(SC_RECEIVE, sys_receive);
}
