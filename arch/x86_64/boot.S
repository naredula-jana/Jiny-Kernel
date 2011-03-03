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
#ifndef __ELF__
	/* header_addr */
	.long	multiboot_header
	/* load_addr */
	.long	_start
	/* load_end_addr */
	.long	_edata
	/* bss_end_addr */
	.long	_end
	/* entry_addr */
	.long	multiboot_entry
#endif /* ! __ELF__ */

multiboot_entry:
	
#	jmp multiboot_entry
	/* Initialize the stack pointer.  */
	movl	$(stack + STACK_SIZE), %esp
#loop1:	jmp loop1
	/* Reset EFLAGS.  */
	pushl	$0
	popf

	/* save multiboot info pointer */
        movl %ebx, g_multiboot_info_ptr
        /* and multiboot magic value */
        movl %eax, g_multiboot_magic
	/*pushl   $0
	pushl   %ebx
	pushl   $0
	pushl   %eax */
#tloop:	jmp tloop

        lgdt boot_gdt_descr
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

        /* Enable paging to activate long mode */
        movl %cr0, %eax
        orl $CR0_PG, %eax
        orl $CR0_RW, %eax
        movl %eax, %cr0
        movl $kernel_jump_addr, %edx
        movl $(cmain), 0(%edx)
#loop4:	jmp loop4
	jmpl $KTEXT_SEG, $start64
	
/* Now enter the C main function...  */
.code64
start64:
        movq $(stack + STACK_SIZE -16), %rsp
        movq kernel_jump_addr, %rax
#loop3:	jmp loop3
        callq *%rax
	call	EXT_C(cmain)

	/* Halt.  */
	#pushl	$halt_message
#	call	EXT_C(printf)
.code32	
loop:	hlt
	jmp	loop

halt_message:
	.asciz	"Halted."

	/* Our stack area.  */
#.fill 1024, 8, 0
#.global stack
#stack:
	.comm	stack, STACK_SIZE
#.globl kernel_jump_addr
kernel_jump_addr:
        .quad 0x00000000000000
boot_gdt_descr:
        .word boot_gdt_end - boot_gdt_table
        .long boot_gdt_table

boot_gdt_table:
        .quad   0x0000000000000000  /* NULL descriptor */
        .quad   0x00af9a000000ffff  /* Code segment */
        .quad   0x008f92000000ffff  /* Data segment */
        .quad   0x00cf9a000000ffff  /* 32bit Code segment */
boot_gdt_end:

.org 0x1000
level4_pg_directory:
        /* default: 1023 entries */
        .quad   0x0000000000102007              /* -> level3_ident_pgt */
        .fill   511,8,0

.org 0x2000
/* Kernel does not "know" about 4-th level of page tables. */
level3_pg_table:
        .quad   0x0000000000103007
        .fill   511,8,0

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

