/* boot.S - bootstrap the kernel */

#define ASM	1
#include <multiboot.h>


#define MSR_EFER 0xc0000080		/* extended feature register */
#define _EFER_LME 8  /* Long mode enable */
#define CR0_PG 0x80000000 /* Paging */	
#define CR0_RW 0x10000 /* write protect, enables accedental writes by supervisor mode */	
/*extern unsigned long  multiboot_info_ptr;
extern unsigned long  multiboot_magic;
*/
#define BOOT_SHIFT 0x40000000

.set KTEXT_SEG,   8
.set KDATA_SEG,   16

	.text
	.code32

	.globl	start, _start
start:
_start:
	jmp	multiboot_entry

	/* Align 32 bits boundary.  */
	.align	4
	
	/* Multiboot header.  */
multiboot_header:
	/* magic */
	.long	MULTIBOOT_HEADER_MAGIC
	/* flags */
	.long	MULTIBOOT_HEADER_FLAGS
	/* checksum */
	.long	-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
/*#ifndef __ELF__ */
	/* header_addr */
	.long	0x100004
	/* load_addr */
	.long	0x100024

	.long   0
	.long   0
	/* load_end_addr */
//	.long	0x20d730
	/* bss_end_addr */
//	.long	0x42fe68
	/* entry_addr */
	.long	0x100024
/* #endif  ! __ELF__ */ 

multiboot_entry:
	
#	jmp 0x100024
	/* Initialize the stack pointer.  */
	movl	$(0x4000+TASK_SIZE), %esp
#loop1:	jmp loop1
	/* Reset EFLAGS.  */
	pushl	$0
	popf

	/* save multiboot info pointer */
 #       movl %ebx, g_multiboot_info_ptr-BOOT_SHIFT
        /* and multiboot magic value */
 #       movl %eax, g_multiboot_magic-BOOT_SHIFT
	/*pushl   $0
	pushl   %ebx
	pushl   $0
	pushl   %eax */
tloop:	
#	jmp tloop

#	lgdt boot_gdt_descr
	lgdt 0x100900
t1:	
#	jmp t1
        movl $KDATA_SEG, %eax
        movl %eax, %ds
        movl %eax, %es
        movl %eax, %fs
        movl %eax, %gs
	    movl %eax, %ss
        /*
         * Prepare for entering 64bits mode
         */

        /* Enable PAE mode and PGE */
        xorl    %eax, %eax
        btsl    $5, %eax
        btsl    $7, %eax
	/* enable osfxsr  for 128 bit media instructions */
        btsl    $9, %eax
        movl    %eax, %cr4

        /* Setup early boot stage 4 level pagetables */
        movl    $0x101000, %eax
        movl    %eax, %cr3
        /* Setup EFER (Extended Feature Enable Register) */
        movl    $MSR_EFER, %ecx
        rdmsr
        /* Fool rdmsr and reset %eax to avoid dependences */
        xorl    %eax, %eax
        /* Enable Long Mode */
        btsl    $_EFER_LME, %eax
	wrmsr

#loop4:	jmp loop4
        /* Enable paging to activate long mode */
        movl %cr0, %eax
        orl $CR0_PG, %eax
	orl $CR0_RW, %eax
	movl %eax, %cr0
    /*    movl $kernel_jump_addr-BOOT_SHIFT, %edx
        movl $(cmain), 0(%edx)*/
	jmpl $KTEXT_SEG, $start64
	
/* Now enter the C main function...  */
.code64
start64:
        movq $(g_idle_stack) + TASK_SIZE*2 -16, %rax
        and $0xffffffffffffc000 ,%rax
        sub $16 ,%rax
        movq %rax, %rsp

        movq $(cmain), %rax
   //     movq kernel_jump_addr, %rax
        callq *%rax

	/* Halt.  */
	#pushl	$halt_message
#	call	EXT_C(printf)
.code32	
loop:	hlt
	jmp	loop

halt_message:
	.asciz	"Halted."


#.globl kernel_jump_addr
kernel_jump_addr:
        .quad 0x00000000000000

.org 0x900
boot_gdt_descr:
        .word boot_gdt_end - boot_gdt_table
#        .long boot_gdt_table
        .long 0x100930

.org 0x930
boot_gdt_table:
        .quad   0x0000000000000000  /* NULL descriptor */
        .quad   0x00af9a000000ffff  /* Code segment */
        .quad   0x008f92000000ffff  /* Data segment */
        .quad   0x00cf9a000000ffff  /* 32bit Code segment */
boot_gdt_end:

.org 0x1000
level4_pg_directory:
        /* default: 512 entries */
        .quad   0x0000000000102007              /* -> level3_ident_pgt */
        .fill   510,8,0
        .quad   0x0000000000102007              /* -> level3_ident_pgt */



.org 0x2000
/* Kernel does not "know" about 4-th level of page tables. */
level3_pg_table:
        .quad   0x0000000000103007 /* TODO : currently used only while initializing */
        .quad   0x0000000000103007 /* TODO : currently based on harcoded of kernel offset it is located in 2nd location*/
        .fill   508,8,0
        .quad   0x0000000000103007 /* TODO : currently used only while initializing */
        .quad   0x0000000000103007 /* TODO : currently based on harcoded of kernel offset it is located in 2nd location*/



.org 0x3000
level2_pg_table:
        /* 40MB for bootup.     */
        .quad   0x0000000000000283 /* made the code Readonly by changing from 283 -> 281 */
        .quad   0x0000000000200183
        .quad   0x0000000000400183
        .quad   0x0000000000600183
        .quad   0x0000000000800183
        .quad   0x0000000000A00183
        .quad   0x0000000000C00183
        .quad   0x0000000000E00183
        .quad   0x0000000001000183
        .quad   0x0000000001200183
        .quad   0x0000000001400183
        .quad   0x0000000001600183
        .quad   0x0000000001800183
        .quad   0x0000000001A00183
        .quad   0x0000000001C00183
        .quad   0x0000000001E00183
        .quad   0x0000000002000183
        .quad   0x0000000002200183
        .quad   0x0000000002400183
        .quad   0x0000000002600183
        /* Temporary mappings for the super early allocator in arch/x86_64/mm/init.c */
/*        .globl temp_boot_pmds */
temp_boot_pmds:
        .fill   492,8,0

.org 0x4000  /* temp stacks  */
temp_stack:
        .fill   TASK_SIZE*(2),1,0
#stack:

#	.comm	g_idle_stack, TASK_SIZE*(MAX_CPUS+1)
