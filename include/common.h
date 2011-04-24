#ifndef __COMMON_H__
#define __COMMON_H__
#include "spinlock.h"
#include "atomic.h"
#include "list.h"
#include "isr.h"
#include "types.h"
#include "multiboot.h"
#define NULL ((void *) 0)
#define POW2(n) (1 << (n))

#define __sti() __asm__ __volatile__ ("sti": : :"memory")
#define __cli() __asm__ __volatile__ ("cli": : :"memory")

#ifdef ARCH_X86_64
#define warn_if_not_ulong(x) do { unsigned long foo; (void) (&(x) == &foo); } while (0)
#define __save_flags(x)         do { warn_if_not_ulong(x); __asm__ __volatile__("# save_flags \n\t pushfq ; popq %q0":"=g" (x): /* no input */ :"memory"); } while (0)
#define __restore_flags(x)      __asm__ __volatile__("# restore_flags \n\t pushq %0 ; popfq": /* no output */ :"g" (x):"memory", "cc")
#else
#define __save_flags(x) \
	__asm__ __volatile__("pushfl ; popl %0":"=g" (x): /* no input */ :"memory")
#define __restore_flags(x) \
										   __asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"g" (x):"memory")
#endif

#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define restore_flags(x) __restore_flags(x)
extern int g_syscall_debug;
extern unsigned long g_debug_level;
#define SYS_DEBUG(x...) do { \
	if (g_syscall_debug==1 && g_serial_output==1)	{ut_printf("SYSCALL "); ut_printf(x);} \
} while (0) 

																		      //#define DEBUG_ENABLE 1
#ifdef DEBUG_ENABLE 
#define DEBUG(x...) do { \
	if (g_serial_output==1 && g_debug_level==1)	ut_printf(x); \
} while (0) 
#else
#define DEBUG(x...) do { \
} while (0) 
#endif


#define MAX_SYMBOLLEN 40
#define TYPE_TEXT 0
#define TYPE_DATA 1
typedef struct {
	addr_t address;
	char type;
	char name[MAX_SYMBOLLEN];
}symb_table_t ;
extern symb_table_t *g_symbol_table;
extern unsigned long g_total_symbols;

typedef struct registers
{
	addr_t ds;                  // Data segment selector
	addr_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
	addr_t int_no, err_code;    // Interrupt number and error code (if applicable)
	addr_t eip, cs, eflags, useresp, ss; // Pushed by the processor automatically.
} registers_t;


typedef struct {
	unsigned long tv_sec;
	unsigned long tv_nsec;
}time_t;

#define BUG() do { unsigned long stack_var; ut_printf(" Kernel BUG  file : %s : line :%d \n",__FILE__,__LINE__);  \
	cli(); ut_showTrace(&stack_var);while(1) ; } while(0)
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)

// Enables registration of callbacks for interrupts or IRQs.
// For IRQs, to ease confusion, use the #defines above as the
// first parameter.
typedef void (*isr_t)();
extern int g_serial_output;
extern spinlock_t g_inode_lock;
#define printk ut_printf
unsigned char kb_getchar();
void register_interrupt_handler(uint8_t n, isr_t handler);
int serial_write( char *buf , int len);
void *kmalloc (long size, int flags);
void kfree (const void *objp);
extern addr_t g_jiffies;
#endif
