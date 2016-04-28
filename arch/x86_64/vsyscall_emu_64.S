/*
 * vsyscall_emu_64.S: Vsyscall emulation page
 *
 * Copyright (c) 2011 Andy Lutomirski
 *
 * Subject to the GNU General Public License, version 2
 */

#define ASM	1
#include <multiboot.h>

# __PAGE_ALIGNED_DATA
	.globl __vsyscall_page
	.balign 4096, 0xcc
	.type __vsyscall_page, @object
__vsyscall_page:

	mov $0x60, %rax  /* gettimeofday */
	syscall
	ret

	.balign 1024, 0xcc   /* time */
	mov $0xc9, %rax
	syscall
	ret

	.balign 1024, 0xcc  /* getcpu */
	mov $0x135, %rax
	syscall
	ret

	.balign 4096, 0xcc
	.size __vsyscall_page, 4096

/* vsyscall_page for the high priorty app */
	.globl __HP_vsyscall_page
	.balign 4096, 0xcc
	.type __HP_vsyscall_page, @object
__HP_vsyscall_page:

	movq $0x60 ,%rax /* gettimeofday */
	callq *0x3DD000
	ret

	.balign 1024, 0xcc   /* time */
	movq $0xc9 ,%rax
	callq HP_syscall
	ret

	.balign 1024, 0xcc  /* getcpu */
	movq $0x135 ,%rax
	callq HP_syscall
	ret
	.balign 4096, 0xcc
	.size __HP_vsyscall_page, 4096
