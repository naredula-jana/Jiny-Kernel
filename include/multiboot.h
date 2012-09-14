#ifndef __MULTIBOOT_H__
#define __MULTIBOOT_H__
/* Macros.  */
/* The magic number for the Multiboot header.  */
#define MULTIBOOT_HEADER_MAGIC		0x1BADB002

/* The flags for the Multiboot header.  */
//#ifdef __ELF__
//# define MULTIBOOT_HEADER_FLAGS		0x00000003
//#else
# define MULTIBOOT_HEADER_FLAGS		0x00010003
//#endif

/* The magic number passed by a Multiboot-compliant boot loader.  */
#define MULTIBOOT_BOOTLOADER_MAGIC	0x2BADB002
#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)
/* The size of our stack (16KB).  */
/*#define TASK_SIZE 4*PAGE_SIZE */
#define TASK_SIZE 4*(0x1000)
#ifdef SMP
#define MAX_CPUS 5
#else
#define MAX_CPUS 1
#endif
#define MAX_IRQS 256

/* C symbol format. HAVE_ASM_USCORE is defined by configure.  */
#ifdef HAVE_ASM_USCORE
# define EXT_C(sym)			_ ## sym
#else
# define EXT_C(sym)			sym
#endif

#ifndef ASM
/* Do not include here in boot.S.  */

/* Types.  */

/* The Multiboot header.  */
#if 0
typedef struct multiboot_header
{
  unsigned long magic;
  unsigned long flags;
  unsigned long checksum;
  unsigned long header_addr;
  unsigned long load_addr;
  unsigned long load_end_addr;
  unsigned long bss_end_addr;
  unsigned long entry_addr;
} multiboot_header_t;

/* The symbol table for a.out.  */
typedef struct aout_symbol_table
{
  unsigned long tabsize;
  unsigned long strsize;
  unsigned long addr;
  unsigned long reserved;
} aout_symbol_table_t;

/* The section header table for ELF.  */
typedef struct elf_section_header_table
{
  unsigned long num;
  unsigned long size;
  unsigned long addr;
  unsigned long shndx;
} elf_section_header_table_t;
#endif


#if 0
+-------------------+
0       | flags             |    (required)
+-------------------+
4       | mem_lower         |    (present if flags[0] is set)
8       | mem_upper         |    (present if flags[0] is set)
+-------------------+
12      | boot_device       |    (present if flags[1] is set)
+-------------------+
16      | cmdline           |    (present if flags[2] is set)
+-------------------+
20      | mods_count        |    (present if flags[3] is set)
24      | mods_addr         |    (present if flags[3] is set)
+-------------------+
28 - 40 | syms              |    (present if flags[4] or
|                   |                flags[5] is set)
+-------------------+
44      | mmap_length       |    (present if flags[6] is set)
48      | mmap_addr         |    (present if flags[6] is set)
+-------------------+
52      | drives_length     |    (present if flags[7] is set)
56      | drives_addr       |    (present if flags[7] is set)
+-------------------+
60      | config_table      |    (present if flags[8] is set)
+-------------------+
64      | boot_loader_name  |    (present if flags[9] is set)
+-------------------+
68      | apm_table         |    (present if flags[10] is set)
+-------------------+
72      | vbe_control_info  |    (present if flags[11] is set)
76      | vbe_mode_info     |
80      | vbe_mode          |
82      | vbe_interface_seg |
84      | vbe_interface_off |
86      | vbe_interface_len |
+-------------------+

#endif

typedef struct multiboot_info {
  uint32_t flags;
  uint32_t mem_lower;     /* amount of lower memory 0M - 640M */
  uint32_t mem_upper;     /* amount of upper memory 1G - NG */
  uint32_t boot_device;   /* indicates BIOS disk device */
  uint32_t cmdline;       /* physical address of the passed kernel comand line */
  uint32_t mods_count;    /* number of loaded modules */
  uint32_t mods_addr;     /* physical address of first module structure */
  uint32_t syms[4];
  uint32_t mmap_length;   /* size of memory map buffer */
  uint32_t mmap_addr;     /* physical address of memory map buffer */
} __attribute__ ((packed)) multiboot_info_t;

typedef struct memory_map
{
  uint32_t size;
  uint32_t base_addr_low;
  uint32_t base_addr_high;
  uint32_t length_low;
  uint32_t length_high;
#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
  uint32_t type;
} memory_map_t;
typedef struct multiboot_module {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t string;
  uint32_t reserved;
} __attribute__ ((packed)) multiboot_mod_t;

#endif /* ! ASM */
#endif 
