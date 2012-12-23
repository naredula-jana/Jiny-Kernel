#ifndef __COMMON_H__
#define __COMMON_H__

#define VIRTIO 1
#include "types.h"
#include "atomic.h"
#include "list.h"
#include "isr.h"
#include "descriptor_tables.h"
#include "multiboot.h"
#include "task.h"
#include "spinlock.h"


#ifndef NULL
#define NULL ((void *) 0)
#endif
#define POW2(n) (1 << (n))


extern int g_conf_syscall_debug;
extern int g_conf_debug_level;
#define SYSCALL_DEBUG(x...) do { \
	if (g_conf_syscall_debug==1)	{ut_printf("SYSCALL(%x :%d uip: %x) ",g_current_task->pid,getcpuid(),g_cpu_state[getcpuid()].user_ip); ut_printf(x);} \
} while (0) 

 //#define DEBUG_ENABLE 1
#ifdef DEBUG_ENABLE 
#define DEBUG(x...) do { \
	if ( g_conf_debug_level==1)	ut_printf(x); \
} while (0) 
#else
#define DEBUG(x...) do { \
} while (0) 
#endif

extern int brk_pnt;
#define BRK while(brk_pnt==0)


#define MAX_SYMBOLLEN 40
#define MAX_FILELEN 50
#define SYMBOL_TEXT 0
#define SYMBOL_DATA 1
#define SYMBOL_CMD 10
#define SYMBOL_CONF 12
typedef struct {
	addr_t address;
	char type;
	unsigned char name[MAX_SYMBOLLEN];
	char file_lineno[MAX_FILELEN];
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

#define unlikely(x)     (x) /* TODO */
#define BUG() do { unsigned long stack_var; ut_printf(" Kernel BUG  file : %s : line :%d \n",__FILE__,__LINE__);  \
	cli(); ut_showTrace(&stack_var);while(1) ; } while(0)
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)

#define ASSERT(x)                                              \
do {                                                           \
        if (!(x)) {                                                \
                printk("ASSERTION FAILED: %s at %s:%d.\n",             \
                           # x ,                                           \
                           __FILE__,                                       \
                           __LINE__);                                      \
        BUG();                                                 \
        }                                                          \
} while(0)

// Enables registration of callbacks for interrupts or IRQs.
// For IRQs, to ease confusion, use the #defines above as the
// first parameter.
typedef int (*isr_t)();
extern int g_serial_output;
extern spinlock_t g_inode_lock;
#define printk ut_printf
unsigned char kb_getchar();
void register_interrupt_handler(uint8_t n, isr_t handler);
int serial_write( char *buf , int len);
void *kmalloc (long size, int flags);
void kfree (const void *objp);
extern addr_t g_jiffies;

#define MAX_DMESG_LOG 30000
#include "interface.h"

#endif
