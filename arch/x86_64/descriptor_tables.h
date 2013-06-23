#ifndef __DESCRIPTOR_TABLES_H__
#define __DESCRIPTOR_TABLES_H__

#define CPU_STATE_KERNEL_STACK_OFFSET 0x0  /* see the struct definition at the end of file */
#define CPU_STATE_KERNEL_DS_OFFSET 0x8
#define CPU_STATE_USER_STACK_OFFSET 0x10
#define CPU_STATE_USER_DS_OFFSET 0x18
#define CPU_STATE_USER_ES_OFFSET 0x20
#define CPU_STATE_USER_FS_OFFSET 0x28
#define CPU_STATE_USER_GS_OFFSET 0x30
#define CPU_STATE_USER_FS_BASE 0x38
#define CPU_STATE_USER_IP 0x40
#define CPU_STATE_CPUID 0x48   // currently hardcoded in getcpuid , before changing this , change in getcpuid also
#define CPU_STATE_SYSCALLID 0x50
//#define CPU_STATE_USER_PTD_OFFSET 0x40

#define IRET_OFFSET_RIP 0x0
#define IRET_OFFSET_CS 0x8
#define IRET_OFFSET_RFLAGS 0x10
#define IRET_OFFSET_RSP 0x18
#define IRET_OFFSET_SS 0x20

#define IRET_OFFSET_SIZE 0x28


#define GDT_SEL(desc) ((desc) << 3)
#define NULL_DESCR    0
#define KCODE_DESCR   1
#define KDATA_DESCR   2
#define UCODE_DESCR   3
#define UDATA_DESCR   4
#define KCODE32_DESCR 5
#define TSS_DESCR     6
#define FS_UDATA_DESCR     7  /* TODO : this is for ARCH_SET_FS implementation , need to reimplement using LDT */
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



/* Offsets to parts of CPU exception stack frames. */
#define INT_STACK_FRAME_CS_OFFT 8

#define HW_INTERRUPT_CTX_RFLAGS_OFFT  0x10
#ifndef __ASM__
#include "common.h"

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

#define TSS_USED_ISTS     1
#define TSS_BASIC_SIZE    104
#define TSS_DEFAULT_LIMIT (TSS_BASIC_SIZE - 1)
#define TSS_NUM_ISTS      7
typedef struct tss {
  uint32_t ignored0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t ignored1;
  uint64_t ists[TSS_NUM_ISTS];
  uint64_t ignored2;
  uint16_t ignored3;
  uint16_t iomap_base;
  uint8_t  iomap[];
} __attribute__ ((packed)) tss_t;

struct tss_descr {
  struct gdt_entry_struct seg_low;
  struct {
    uint32_t base_rest;
    uint32_t ignored;
  } seg_high __attribute__ ((packed));
} __attribute__ ((packed));
typedef struct tss_descr tss_descr_t;

struct md_cpu_state { /* NOTE: do not alter the attributes, there location matter */
        unsigned long kernel_stack;
        unsigned long kernel_ds;
        unsigned long user_stack;
        unsigned long user_ds,user_es,user_fs,user_gs,user_fs_base;
        unsigned long user_ip; /* Stats and debugging pupose */
        unsigned long cpu_id;
        unsigned long syscall_id;
};
//extern struct cpu_state g_cpu_state[];

#endif
#endif


