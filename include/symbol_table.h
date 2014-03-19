#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

/* symbol table related populated using the tools like nm ****/
//TODO the below structes are duplicate din util directory
#define MAX_SYMBOLLEN 40
#define MAX_FILELEN 50


enum {
  SYMBOL_GTEXT= 0x1, /* global text */
  SYMBOL_DATA = 0x2,
  SYMBOL_LTEXT = 0x3, /* local text */
  SYMBOL_CMD = 0x4,
  SYMBOL_CONF = 0x5,
  SYMBOL_TYPE_UNRESOLVED=0x6
};

typedef struct {
	unsigned char *name;
	unsigned long address;
	char type;
	char *file_lineno; /* currently not filled */
	int sec_index;
	unsigned char *subsystem_type;
	struct {
		unsigned long hits;
		unsigned long rip;
	}stats;
}symb_table_t;
extern unsigned long g_multiboot_mod1_addr;
extern unsigned long g_multiboot_mod1_len;

extern symb_table_t *g_symbol_table;
extern unsigned long g_total_symbols;
symb_table_t *module_load_kernel_symbols(unsigned char *start_addr, unsigned long mod_len); /* load kernel symbols from kernel file loaded as a module */


/********   DWARF related data , obtained using dwarflib
 *
 */


enum Dwarf_TAG_e {
    DW_TAG_array_type                        = 0x0001,
    DW_TAG_class_type                        = 0x0002,
    DW_TAG_entry_point                       = 0x0003,
    DW_TAG_enumeration_type                  = 0x0004,
    DW_TAG_formal_parameter                  = 0x0005,
    DW_TAG_imported_declaration              = 0x0008,
    DW_TAG_label                             = 0x000a,
    DW_TAG_lexical_block                     = 0x000b,
    DW_TAG_member                            = 0x000d,
    DW_TAG_pointer_type                      = 0x000f,
    DW_TAG_reference_type                    = 0x0010,
    DW_TAG_compile_unit                      = 0x0011,
    DW_TAG_string_type                       = 0x0012,
    DW_TAG_structure_type                    = 0x0013,
    DW_TAG_subroutine_type                   = 0x0015,
    DW_TAG_typedef                           = 0x0016,
    DW_TAG_union_type                        = 0x0017,
    DW_TAG_unspecified_parameters            = 0x0018,
    DW_TAG_variant                           = 0x0019,
    DW_TAG_common_block                      = 0x001a,
    DW_TAG_common_inclusion                  = 0x001b,
    DW_TAG_inheritance                       = 0x001c,
    DW_TAG_inlined_subroutine                = 0x001d,
    DW_TAG_module                            = 0x001e,
    DW_TAG_ptr_to_member_type                = 0x001f,
    DW_TAG_set_type                          = 0x0020,
    DW_TAG_subrange_type                     = 0x0021,
    DW_TAG_with_stmt                         = 0x0022,
    DW_TAG_access_declaration                = 0x0023,
    DW_TAG_base_type                         = 0x0024,
    DW_TAG_catch_block                       = 0x0025,
    DW_TAG_const_type                        = 0x0026,
     DW_TAG_constant                          = 0x0027,
     DW_TAG_enumerator                        = 0x0028,
     DW_TAG_file_type                         = 0x0029,
     DW_TAG_friend                            = 0x002a,
     DW_TAG_namelist                          = 0x002b,
     DW_TAG_namelist_item                     = 0x002c,
     DW_TAG_packed_type                       = 0x002d,
     DW_TAG_subprogram                        = 0x002e,
     DW_TAG_template_type_parameter           = 0x002f,
     DW_TAG_template_value_parameter          = 0x0030,
     DW_TAG_thrown_type                       = 0x0031,
     DW_TAG_try_block                         = 0x0032,
     DW_TAG_variant_part                      = 0x0033,
     DW_TAG_variable                          = 0x0034,
     DW_TAG_volatile_type                     = 0x0035,
     DW_TAG_dwarf_procedure                   = 0x0036,
     DW_TAG_restrict_type                     = 0x0037,
     DW_TAG_interface_type                    = 0x0038,
     DW_TAG_namespace                         = 0x0039,
     DW_TAG_imported_module                   = 0x003a,
     DW_TAG_unspecified_type                  = 0x003b,
     DW_TAG_partial_unit                      = 0x003c,
     DW_TAG_imported_unit                     = 0x003d,
     DW_TAG_mutable_type                      = 0x003e,
     DW_TAG_condition                         = 0x003f,
     DW_TAG_shared_type                       = 0x0040,
     DW_TAG_type_unit                         = 0x0041
};

#define MAX_NAME 100
struct dwarf_entry{
	unsigned char level;
	unsigned char name[MAX_NAME];
	int size;
	unsigned long offset; /* offset of this entry */
	unsigned long type_offset; /* offset of type */
	int member_location;
	int tag;
	char tmp_used;
	int type_index;
}dwarf_entry;

#endif
