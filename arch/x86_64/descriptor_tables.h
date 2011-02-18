#ifndef __DESCRIPTOR_TABLES_H__
#define __DESCRIPTOR_TABLES_H__


#define GDT_SEL(desc) ((desc) << 3)
#define NULL_DESCR    0
#define KCODE_DESCR   1
#define KDATA_DESCR   2
#define UCODE_DESCR   3
#define UDATA_DESCR   4
#define KCODE32_DESCR 5
#define TSS_DESCR     6
#define LDT_DESCR     8

#define SEG_DPL_SHIFT  5
#define SEG_DPL_KERNEL 0
#define SEG_DPL_USER   3

#define SEG_ATTR_A    0x01
#define SEG_ATTR_R    0x02
#define SEG_ATTR_W    0x02
#define SEG_ATTR_C    0x04
#define SEG_ATTR_E    0x04
#define SEG_ATTR_CODE 0x08
#define SEG_ATTR_DATA 0x00
#define SEG_ATTR_S    0x10
#define SEG_ATTR_P    0x80

#define SEG_FLG_PRESENT 0x01
#define SEG_FLG_AVAIL   0x02
#define SEG_FLG_64BIT   0x04
#define SEG_FLG_OPSIZE  0x08
#define SEG_FLG_GRAN    0x10
#define SEG_TYPE_CODE  (SEG_ATTR_CODE | SEG_ATTR_S)
#define SEG_TYPE_DATA  (SEG_ATTR_W | SEG_ATTR_DATA | SEG_ATTR_S)
#define SEG_TYPE_TSS   (SEG_ATTR_A | SEG_ATTR_CODE)
#define SEG_TYPE_LDT   (SEG_ATTR_W | SEG_ATTR_DATA)
#define SEG_TYPE_TRAP  (SEG_ATTR_A | SEG_ATTR_R | SEG_ATTR_C | SEG_ATTR_CODE)
#define SEG_TYPE_INTR  (SEG_ATTR_R | SEG_ATTR_C | SEG_ATTR_CODE)

/* For low-level field accesses. */
#define CPU_SCHED_STAT_CPU_OFFT 0
#define CPU_SCHED_STAT_CURRENT_OFFT 0x8
#define CPU_SCHED_STAT_KSTACK_OFFT 0x10
#define CPU_SCHED_STAT_FLAGS_OFFT 0x18
#define CPU_SCHED_STAT_IRQCNT_OFFT 0x20
#define CPU_SCHED_STAT_PREEMPT_OFFT 0x28
#define CPU_SCHED_STAT_IRQLOCK_OFFT 0x30
#define CPU_SCHED_STAT_KERN_DS_OFFT 0x38
#define CPU_SCHED_STAT_USER_DS_OFFT 0x40
#define CPU_SCHED_STAT_USTACK_OFFT 0x48
#define CPU_SCHED_STAT_USER_ES_OFFT 0x50
#define CPU_SCHED_STAT_USER_FS_OFFT 0x58
#define CPU_SCHED_STAT_USER_GS_OFFT 0x60
#define CPU_SCHED_STAT_USER_WORKS_OFFT 0x68  /* It's a pointer ! */
#define CPU_SCHED_STAT_USER_PTD_OFFT  0x70

/* Offsets to parts of CPU exception stack frames. */
#define INT_STACK_FRAME_CS_OFFT 8

#define HW_INTERRUPT_CTX_RFLAGS_OFFT  0x10
#ifndef __ASM__
#include "common.h"
// Initialisation function is publicly accessible.
void init_descriptor_tables();
// This structure contains the value of one GDT entry.
// We use the attribute 'packed' to tell GCC not to change
// any of the alignment in the structure.
/*struct gdt_entry_struct
{
    u16int limit_low;           // The lower 16 bits of the limit.
    u16int base_low;            // The lower 16 bits of the base.
    u8int  base_middle;         // The next 8 bits of the base.
    u8int  access;              // Access flags, determine what ring this segment can be used in.
    u8int  granularity;
    u8int  base_high;           // The last 8 bits of the base.
} __attribute__((packed));*/

typedef struct gdt_entry_struct {
  unsigned seg_limit_low     :16;
  unsigned base_address_low  :24;
  unsigned flags_low         :8;
  unsigned seg_limit_high    :4;
  unsigned flags_high        :4;
  unsigned base_address_high :8;
} __attribute__ ((packed)) gdt_entry_t;


// This struct describes a GDT pointer. It points to the start of
// our array of GDT entries, and is in the format required by the
// lgdt instruction.
typedef struct gdt_ptr_struct {
  uint16_t limit;
  unsigned long base;
} __attribute__ ((packed)) gdt_ptr_t;

typedef struct idt_entry_struct {
  unsigned offset_low  :16;
  unsigned selector    :16;
  unsigned ist         :3;
  unsigned ignored0    :5;
  unsigned flags       :8;
  unsigned offset_midd :16;
  unsigned offset_high :32;
  unsigned ignored1    :32;
} __attribute__ ((packed)) idt_entry_t;

// A struct describing a pointer to an array of interrupt handlers.
// This is in a format suitable for giving to 'lidt'.
struct idt_ptr_struct
{
    uint16_t limit;
    addr_t base;                // The address of the first element in our idt_entry_t array.
} __attribute__((packed));

typedef struct idt_ptr_struct idt_ptr_t;
#endif
#endif


