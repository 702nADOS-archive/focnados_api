/*
 * \brief  Utilities fo thread creation on seL4
 * \author Norman Feske
 * \date   2015-05-12
 *
 * This file is used by both the core-specific implementation of the Thread API
 * and the platform-thread implementation for managing threads outside of core.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__THREAD_SEL4_H_
#define _CORE__INCLUDE__THREAD_SEL4_H_

/* Genode includes */
#include <base/printf.h>

/* base-internal includes */
#include <internal/assert.h>

/* core includes */
#include <map_local.h>
#include <kernel_object.h>

namespace Genode {

	struct Thread_info
	{
		unsigned tcb_sel = 0;
		unsigned ep_sel  = 0;

		addr_t ipc_buffer_phys = 0;

		inline void write_thread_info_to_ipc_buffer(unsigned pd_ep_sel);

		Thread_info() { }

		inline void init(addr_t const utcb_virt_addr);
	};

	/**
	 * Set register values for the instruction pointer and stack pointer and
	 * start the seL4 thread
	 */
	static inline void start_sel4_thread(unsigned tcb_sel, addr_t ip, addr_t sp);
};


void Genode::Thread_info::init(addr_t const utcb_virt_addr)
{
	Platform &platform = *platform_specific();
	Range_allocator &phys_alloc = *platform.ram_alloc();

	/* create IPC buffer of one page */
	size_t const ipc_buffer_size_log2 = get_page_size_log2();
	Untyped_address ipc_buffer =
		Untyped_memory::alloc_log2(phys_alloc, ipc_buffer_size_log2);

	ipc_buffer_phys = ipc_buffer.phys();

	Untyped_memory::convert_to_page_frames(ipc_buffer.phys(), 1);

	/* allocate TCB within core's CNode */
	tcb_sel = platform.alloc_core_sel();
	Kernel_object::create<Kernel_object::Tcb>(phys_alloc,
	                                          platform.core_cnode().sel(),
	                                          tcb_sel);

	/* allocate synchronous endpoint within core's CNode */
	ep_sel = platform.alloc_core_sel();
	Kernel_object::create<Kernel_object::Endpoint>(phys_alloc,
	                                               platform.core_cnode().sel(),
	                                               ep_sel);
	/* assign IPC buffer to thread */
	{
		/* determine page frame selector of the allocated IPC buffer */
		unsigned ipc_buffer_sel = Untyped_memory::frame_sel(ipc_buffer.phys());

		int const ret = seL4_TCB_SetIPCBuffer(tcb_sel, utcb_virt_addr,
		                                      ipc_buffer_sel);
		ASSERT(ret == 0);
	}

	/* set scheduling priority */
	enum { PRIORITY_MAX = 0xff };
	seL4_TCB_SetPriority(tcb_sel, PRIORITY_MAX);
}


void Genode::start_sel4_thread(unsigned tcb_sel, addr_t ip, addr_t sp)
{
	/* set register values for the instruction pointer and stack pointer */
	seL4_UserContext regs;
	Genode::memset(&regs, 0, sizeof(regs));
	size_t const num_regs = sizeof(regs)/sizeof(seL4_Word);

	regs.eip = ip;
	regs.esp = sp;
	regs.gs  = IPCBUF_GDT_SELECTOR;

	int const ret = seL4_TCB_WriteRegisters(tcb_sel, false, 0, num_regs, &regs);
	ASSERT(ret == 0);

	seL4_TCB_Resume(tcb_sel);
}

#endif /* _CORE__INCLUDE__THREAD_SEL4_H_ */
