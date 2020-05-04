/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   kernel/module.c
 *   Author: Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
 */

extern "C" {
#include "common.h"
#include "interface.h"
#include "elf.h"
#include "symbol_table.h"
#include "mm.h"
#define MODULE_DEBUG 1
extern void * __HP_vsyscall_page;
static int complete_read(struct file *file, unsigned char *buf, int total_size);
typedef struct {
	struct file *file;
	unsigned char *code_start;
	unsigned long code_length;
} binary_source_t;
}
enum {
	SEC_TEXT = 0, SEC_DATA = 1, SEC_RODATA = 2, SEC_BSS = 3, SEC_TOTAL = 4
};
class module_t {
	int update_symboltable(Elf64_Sym *symb, int total_symb);
public:
	void sort_symbols();
	void rank_symbols_by_hits(int max_ranks);
	unsigned long _get_symbol_addr(unsigned char *name);
	unsigned char *_get_symbol_name(unsigned long addr);
	void free_module();
	char* do_relocation(struct file *file, Elf64_Sym *symb, int total_symbols, Elf64_Shdr *sec_rel,int sec_type);
	char *load_symbols_for_HP_app(binary_source_t *source, Elf64_Sym **arg_symb, Elf64_Shdr *sec_symb);
	char *update_symbols(binary_source_t *source, Elf64_Sym **arg_symb, Elf64_Shdr *sec_symb);
	unsigned char name[MAX_FILENAME]; /* module name and file name are same */
	int use_count; //TODO
	int type;

	struct {
		int sec_index;
		unsigned char *addr;
		unsigned long file_offset; /* offset in file */
		int length; /* length of section */
	} secs[SEC_TOTAL];

	symb_table_t *symbol_table;
	int symb_table_length;
	unsigned char *str_table;
	int str_table_length;
	//unsigned char *common_symbol_addr;

	int (*init_module)(unsigned char *arg1, unsigned char *arg2);
	int (*clean_module)(unsigned char *arg1, unsigned char *arg2);
	int (*highpriority_app_main)(int argc, unsigned char *argv);
	unsigned long mem_start,mem_end;
};
void module_t::rank_symbols_by_hits(int max_ranks) {
	unsigned long hits;
	int rank,i,j,k,max_symbols=0;

	for (i = 0; symbol_table[i].name != 0; i++) {/* reset all the temp variable, caluculate hit */
		hits=0;
		for (k = 0; k < MAX_CPUS; k++) {
			hits = hits + symbol_table[i].stats[k].hits;
		}
		symbol_table[i].total_hits = hits;
		symbol_table[i].hit_rank = 0;
		max_symbols++;
	}
	for (rank = 1; rank <max_ranks && rank<max_symbols; rank++) { /* sort top max_ranks */
		int max_hits =0;
		int max_i=-1;

		for (i = 0; symbol_table[i].name != 0; i++) {
			if (symbol_table[i].hit_rank != 0) continue;
			if (symbol_table[i].total_hits > max_hits){
				max_hits=symbol_table[i].total_hits;
				max_i =i;
			}
		}
		if (max_i > -1){
			symbol_table[max_i].hit_rank = rank;
		}else{
			break;
		}
	}
	return;
}
void module_t::sort_symbols() {
	int i, j;

	for (i = 0; symbol_table[i].name != 0; i++) {
		int smallest;
		smallest = i;
		for (j = i + 1; symbol_table[j].name != 0; j++) {
			if (symbol_table[j].address < symbol_table[smallest].address) {
				smallest = j;
			}
		}
		if (smallest != i) {
			symb_table_t temp;
			ut_memcpy((uint8_t *) &temp, (uint8_t *) &(symbol_table[smallest]), sizeof(symb_table_t));
			ut_memcpy((uint8_t *) &(symbol_table[smallest]), (uint8_t *) &(symbol_table[i]),
					sizeof(symb_table_t));
			ut_memcpy((uint8_t *) &(symbol_table[i]), (uint8_t *) &temp, sizeof(symb_table_t));
		}
	}
	symb_table_length = i;
}
unsigned char *module_t::_get_symbol_name(unsigned long addr) {
	int i;
	unsigned char *ret = 0;

	for (i = 0; symbol_table[i].name != 0; i++) {
		if (addr>=symbol_table[i].address && addr<=(symbol_table[i].address+symbol_table[i].len)){
			return symbol_table[i].name;
		}
	}

	return ret;
}
unsigned long module_t::_get_symbol_addr(unsigned char *name) {
	int i;
	unsigned long ret = ~(0x0);
	for (i = 0; symbol_table[i].name != 0; i++) {
		if (ut_strcmp(symbol_table[i].name, name) == 0 && (symbol_table[i].type != SYMBOL_TYPE_UNRESOLVED)) {
			ret = symbol_table[i].address;
			return ret;
		}
	}

	return ret;
}
char* module_t::do_relocation(struct file *file, Elf64_Sym *symb, int total_symbols, Elf64_Shdr *sec_rel,int sec_type) {
	const char *ret = 0;
	Elf64_Rela *org_reloc=0,*reloc = 0;
	int i;

	reloc = (Elf64_Rela *) vmalloc(sec_rel->sh_size, 0);
	org_reloc=reloc;
	if (reloc == 0) {
		ret = "getting memory to reloc fails";
		goto out;
	}
	fs_lseek(file, sec_rel->sh_offset, 0);
	complete_read(file, (unsigned char *) reloc, sec_rel->sh_size);

	for (i = 0; i < (sec_rel->sh_size / sec_rel->sh_entsize); i++, reloc++) {
		unsigned long addr = secs[sec_type].addr + (unsigned long) reloc->r_offset;
		if (addr > (secs[sec_type].addr + secs[sec_type].length)) {
			ret = " Fail during  relocation ";
			goto out;
		}
		int s_index = ELF64_R_SYM(reloc->r_info);
		int type = ELF64_R_TYPE(reloc->r_info);
		if (s_index > total_symbols) {
			ret = " symbol index is more then total symbols ";
			goto out;
		}
		unsigned long symbol_value = 0;
		if (ELF64_ST_BIND(symb[s_index].st_info) == STB_GLOBAL) {
			symbol_value = _get_symbol_addr(str_table + symb[s_index].st_name);
			if (symbol_value == ~(0x0)) {
				//	ut_printf(" reloca symb name: %s  ret:%x\n",module->str_table + symb[s_index].st_name,symbol_value);
				symbol_value = ut_get_symbol_addr(str_table + symb[s_index].st_name);
				if (type == R_X86_64_PC32 && symbol_value == 0x0) {
					ut_printf("Error: Relocation name :%s -> %x type:%x\n", str_table + symb[s_index].st_name,
							symbol_value, type);
					ret = "Error in relocation, cannot find externel symbol";
				}
			}
		} else if (ELF64_ST_BIND(symb[s_index].st_info) == STB_LOCAL) {
			if (ELF64_ST_TYPE(symb[s_index].st_info) == STT_SECTION) {
				if (symb[s_index].st_shndx == secs[SEC_RODATA].sec_index) {
					symbol_value = (unsigned long) secs[SEC_RODATA].addr + symb[s_index].st_value;
				} else if (symb[s_index].st_shndx == secs[SEC_BSS].sec_index) {
					symbol_value = (unsigned long) secs[SEC_BSS].addr + symb[s_index].st_value;
				} else if (symb[s_index].st_shndx == secs[SEC_DATA].sec_index) {
					symbol_value = (unsigned long) secs[SEC_DATA].addr + symb[s_index].st_value;
				} else if (symb[s_index].st_shndx == secs[SEC_TEXT].sec_index) {
					symbol_value = (unsigned long) secs[SEC_TEXT].addr + symb[s_index].st_value;
				} else {
					ut_printf(" ERROR in Relocating the sec index:%x i:%d\n", symb[s_index].st_shndx, i);
				}
			} else {
				ut_printf(" ERROR in Relocating the sec index:%x i:%d  type:%x \n", symb[s_index].st_shndx, i,
						ELF64_ST_TYPE(symb[s_index].st_info));
			}
		}
#ifdef MODULE_DEBUG
		ut_printf("Reloc:%x %d symbol :%s: value:%x type:%x  \n",reloc->r_offset,s_index, str_table + symb[s_index].st_name, symbol_value,type);
#endif

		if ((type == R_X86_64_PC32) || (type == R_X86_64_32) || (type == R_X86_64_32S)) {
			unsigned char *p = addr;
			int32_t *v = (int32_t *) p;

			if (type == R_X86_64_PC32) {
				*v = (int32_t) ((symbol_value) & 0xffffffff) - (int32_t) (addr & 0xffffffff) + reloc->r_addend;
			} else{
				*v = (int32_t) ((symbol_value) & 0xffffffff) + reloc->r_addend;
			}
#ifdef MODULE_DEBUG
			ut_printf("		1-New addr: %x  after int value:%x  addedend :%x(%d) \n",v,*v,reloc->r_addend);
#endif
		} else if ((type == R_X86_64_64) ) {
			unsigned char *p = addr;
			signed long *v = (signed long *) p;
			*v = (int32_t) ((symbol_value) & 0xffffffff) + reloc->r_addend;
#ifdef MODULE_DEBUG
			ut_printf("		2-New addr: %x  after int value:%x  addedend :%x(%d) \n",v,*v,reloc->r_addend);
#endif
		} else {
			ut_printf(" Error unhandled type :%x addr:%x \n", type, addr);
		}
	}

	out: vfree(org_reloc);
	return ret;
}
int module_t::update_symboltable(Elf64_Sym *symb, int total_symb) {
	int i, j;
	int ret = JSUCCESS;
	Elf64_Sym *tsemb;
	int global_symb = 0;
	int common_symb_length = 0; /* common symbol length */

	if (str_table == 0)
		return ret;

	tsemb = symb;
	for (i = 0; i < total_symb; i++, tsemb++) {
		if (ELF32_ST_BIND(tsemb->st_info) == STB_GLOBAL) {
			global_symb++;
		}
	}
	global_symb++;
	symbol_table = (symb_table_t *) mm_malloc((total_symb + 1) * sizeof(symb_table_t), 0);
	if (symbol_table == 0) {
		return JFAIL;
	}
	j = 0;
	tsemb = symb;
	for (i = 0; (i < total_symb); i++, tsemb++) {
#ifdef MODULE_DEBUG
		ut_printf(" %d: name:%d info:%x secton_ind:%d \n",i,tsemb->st_name,tsemb->st_info,tsemb->st_shndx);
#endif
		if ((ELF32_ST_BIND(tsemb->st_info) == STB_GLOBAL) || (ELF32_ST_BIND(tsemb->st_info) == STB_LOCAL)) {
			symbol_table[j].sec_index = tsemb->st_shndx;
			if (tsemb->st_shndx == SHN_UNDEF) { /* unresolved symbol */
				symbol_table[j].type = SYMBOL_TYPE_UNRESOLVED;
				symbol_table[j].address = 0;
			} else if (tsemb->st_shndx == secs[SEC_TEXT].sec_index) {
				symbol_table[j].address = tsemb->st_value + (uint64_t) secs[SEC_TEXT].addr;
				if (ELF32_ST_BIND(tsemb->st_info) == STB_LOCAL) {
					symbol_table[j].type = SYMBOL_LTEXT;
				} else {
					symbol_table[j].type = SYMBOL_GTEXT;
				}
			} else if (tsemb->st_shndx == secs[SEC_DATA].sec_index
					|| tsemb->st_shndx == secs[SEC_RODATA].sec_index
					|| tsemb->st_shndx == secs[SEC_BSS].sec_index) {
				int si = SEC_DATA;
				if (tsemb->st_shndx == secs[SEC_RODATA].sec_index)
					si = SEC_RODATA;
				if (tsemb->st_shndx == secs[SEC_BSS].sec_index)
					si = SEC_BSS;
				symbol_table[j].address = tsemb->st_value + (uint64_t) secs[si].addr;
				symbol_table[j].type = SYMBOL_DATA;
			} else if (tsemb->st_shndx == SHN_COMMON) { /* storage need to be allocated */
				symbol_table[j].address = mem_end;
				mem_end = mem_end + tsemb->st_size;
			} else if (tsemb->st_shndx == SHN_ABS) { /* storage need to be allocated */
				symbol_table[j].address = tsemb->st_value;
			} else {
				if ((ELF32_ST_BIND(tsemb->st_info) == STB_GLOBAL))
					ut_printf("ERROR NOT PROCESSED %d section index: %x  name:%s \n", i, tsemb->st_shndx,
							symbol_table[j].name);
			}
			symbol_table[j].name = str_table + tsemb->st_name;
			if (type == ET_EXEC) {
				symbol_table[j].address = tsemb->st_value;
			}

#ifdef MODULE_DEBUG
			ut_printf("		%d: symbol name:%s value:%x type:%x length:%x(%d)\n", j,
					symbol_table[j].name,
					symbol_table[j].address, symbol_table[j].type, tsemb->st_size );
#endif
			j++;
		}

		if (tsemb->st_shndx == secs[SEC_TEXT].sec_index) {
			unsigned char *name = str_table + tsemb->st_name;
			if (ut_strcmp(name, (uint8_t *) "init_module") == 0) {
				init_module = (int (*)(uint8_t *, uint8_t *))(tsemb->st_value + (uint64_t)secs[SEC_TEXT].addr);}else if ((ut_strcmp(name, (uint8_t *)"clean_module") == 0) && (highpriority_app_main==0)) {
							clean_module = (int (*)(uint8_t *,uint8_t *))(tsemb->st_value + (uint64_t)secs[SEC_TEXT].addr);
						} else if (ut_strcmp(name, (uint8_t *)"main") == 0) {
							highpriority_app_main = (int (*)(int,uint8_t *))(tsemb->st_value + (uint64_t)secs[SEC_TEXT].addr);
						}
			}
		}
	symbol_table[j].name = 0;
	symbol_table[j].address = 0;
#if 0
	if (common_symb_length > 0) {
		common_symbol_addr = (unsigned char *) mm_malloc(common_symb_length, MEM_CLEAR);

		for (i = 0; i < j; i++) {
			if (symbol_table[i].sec_index == SHN_COMMON) {
				symbol_table[i].address = symbol_table[i].address + (uint64_t) common_symbol_addr;
			}
		}
	}
#endif

	return ret;
}
char *module_t::update_symbols(binary_source_t *source, Elf64_Sym **arg_symb, Elf64_Shdr *sec_symb) {
	const char *ret = 0;
	Elf64_Sym *symb = 0;
	int file_ret;
	int total_symbols;

	symb = (Elf64_Sym *) vmalloc(sec_symb->sh_size, 0);
	if (symb == 0) {
		ret = "getting memory symbol table fails";
		goto out;
	}
	if (source->file != 0) {
		fs_lseek(source->file, sec_symb->sh_offset, 0);
		file_ret = complete_read(source->file, (unsigned char *) symb, sec_symb->sh_size);
		if (file_ret != sec_symb->sh_size) {
			ret = "getting symbol table from file fails";
			goto out;
		}
	} else {
		ut_memcpy((uint8_t *) symb, (uint8_t *) source->code_start + sec_symb->sh_offset, sec_symb->sh_size);
	}

	total_symbols = sec_symb->sh_size / sec_symb->sh_entsize;
	if (update_symboltable(symb, total_symbols) != JSUCCESS) {
		ret = "getting symbol table fails";
		goto out;
	}
	out: if (ret != 0) {
		vfree((uint64_t) symb);
		*arg_symb = 0;
	} else {
		*arg_symb = symb;
	}
	return (char *)ret;
}
extern unsigned char *syscalltable;

extern "C" {
extern unsigned long HP_syscall;
}

char *module_t::load_symbols_for_HP_app(binary_source_t *source, Elf64_Sym **arg_symb, Elf64_Shdr *sec_symb) {
	const char *ret = 0;
	Elf64_Sym *symb = 0;
	int file_ret;
	int total_symbols;
	Elf64_Sym *tsemb;
	int i,j;

	symb = (Elf64_Sym *) vmalloc(sec_symb->sh_size, 0);
	if (symb == 0) {
		ret = "getting memory symbol table fails";
		goto out;
	}
	if (source->file != 0) {
		fs_lseek(source->file, sec_symb->sh_offset, 0);
		file_ret = complete_read(source->file, (unsigned char *) symb,
				sec_symb->sh_size);
		if (file_ret != sec_symb->sh_size) {
			ret = "getting symbol table from file fails";
			goto out;
		}
	}

	total_symbols = sec_symb->sh_size / sec_symb->sh_entsize;
	//symbol_table = (symb_table_t *) mm_malloc((total_symbols + 1) * sizeof(symb_table_t), 0);
	symbol_table = (symb_table_t *) vmalloc((total_symbols + 1) * sizeof(symb_table_t), 0);
	if (symbol_table == 0) {
		ret = "mem alloc failed";
		goto out;
	}
	tsemb=symb;
	j=0;

	for (i = 0; (i < total_symbols); i++, tsemb++) {
#ifdef MODULE_DEBUG
//		ut_printf(" %d: name:%s info:%x secton_ind:%d \n", i,tsemb->st_name, tsemb->st_info, tsemb->st_shndx);
#endif
		symbol_table[j].name = str_table + tsemb->st_name;
		symbol_table[j].address = tsemb->st_value;
		symbol_table[j].len = tsemb->st_size;

		if ((ut_strcmp(symbol_table[j].name, (uint8_t *) "main") == 0)) {
			highpriority_app_main = symbol_table[j].address;
			//ut_printf("		%d: symbol name:%s: value:%x type:%x length:%x(%d)\n", j, symbol_table[j].name, symbol_table[j].address, symbol_table[j].type, tsemb->st_size);
		}
		j++;
	}
	symbol_table[j].name =0;
	symbol_table[j].address =0;

	unsigned long *tmp_p;
	tmp_p=HIGHPRIORITY_APP_SYSCALLTABLE;
	*tmp_p = &HP_syscall; /* all syscalls of HP will routed through this point */


out:
	if (ret != 0) {
		vfree((uint64_t) symb);
		*arg_symb = 0;
	}else{
		*arg_symb = symb;
	}
	return ret;
}
void module_t::free_module() {
// TODO:  allocated from : 	mm_free(secs[SEC_TEXT].addr);
	//TODO need to free other regions in module
	mm_free(str_table);
	mm_free(symbol_table);

	mm_free(this);
}
extern "C" {
//#define MODULE_DEBUG 1
void Jcmd_reset_cpu_stat();
void Jcmd_lsmod(unsigned char *arg1, unsigned char *arg2);

#define MAX_MODULES 100
static module_t *g_modules[MAX_MODULES];
static int total_modules = 0;

static int remove_module(module_t *modulep);
unsigned long ut_mod_get_symbol_addr(unsigned char *name);
static int remove_module(module_t *modulep) {
	int i;

	for (i = 0; i < total_modules; i++) {
		if (modulep == g_modules[i]) {
			modulep->free_module();
			g_modules[i] = 0;
			total_modules--;
			ut_log(" REmoving the module \n");
			if (i < total_modules)
				g_modules[i] = g_modules[total_modules];
			return JSUCCESS;
		}
	}
	return JFAIL;
}
static int complete_read(struct file *file, unsigned char *buf, int total_size) {
	int total_read = 0;
	int retval = 1;

	while (retval > 0 && total_read != total_size) {
		retval = fs_read(file, (unsigned char *) buf + total_read, total_size - total_read);
		if (retval > 0) {
			total_read = total_read + retval;
		}
	}

	return total_read;
}

static void hp_main(module_t *modulep) { /* kernel thread */
	Jcmd_reset_cpu_stat();
	modulep->highpriority_app_main(0, 0);
	//Jcmd_lsmod("stat", 0);
	//remove_module(modulep);
	SYS_sc_exit(0);
}
static int launch_hp_task(module_t *modulep) {
	int ret;

	if (modulep == 0)
		return JFAIL;
	ret = sc_createKernelThread(hp_main, (void **) modulep, (uint8_t*) "hpriorty_task",CLONE_FS);
	//ret = sc_createKernelThread(modulep->highpriority_app_main, (void **) 0, (uint8_t*) "hpriorty_task",CLONE_FS);
	//ret = SYS_sc_clone(CLONE_VM | CLONE_KERNEL_THREAD | CLONE_FS, 0, 0, modulep->highpriority_app_main, 0,0);
	//ut_printf(" launched high priority taks: pid %d \n", ret);

	return JSUCCESS;
}
void Jcmd_insmod(unsigned char *filename, unsigned char *arg);

void Jcmd_insmod(unsigned char *filename, unsigned char *arg) {
	struct file *file = 0;
	struct elfhdr elf_ex;
	Elf64_Shdr *elf_shdata;
	int retval;
	int sect_size;
	const char *error = 0;
	int i;
	Elf64_Shdr *sec_data, *sec_rel_text, *sec_rel_data, *sec_rel_rodata, *sec_symb, *sec_str;
	unsigned char *sh_strtab = 0;
	unsigned char *strtab = 0;
	module_t *modulep = 0;
	unsigned long file_min_offset, file_max_offset;
	unsigned long flags;
	Elf64_Sym *symb = 0;
	binary_source_t source;
	int total_symbols = 0;

	source.code_start = 0;
	source.code_length = 0;

	sec_data = sec_rel_text = sec_rel_data = sec_rel_rodata = sec_symb = sec_str = 0;
	elf_shdata = 0;

	if (filename == 0) {
		error = "file is empty";
		goto out;
	}
	error = 0;
	file = (struct file *) fs_open(filename, 0, 0);
	if (file == 0) {
		error = "Fail to open the module file";
		goto out;
	} else { /* flush the old file contents */
		fs_fadvise(file->vinode, 0, 0, POSIX_FADV_DONTNEED);
	}
	source.file = file;
	fs_lseek(file, 0, 0);
	retval = fs_read(file, (unsigned char *) &elf_ex, sizeof(elf_ex));
	if (retval != sizeof(elf_ex)) {
		error = "incorrect elf format..";
		goto out;
	}

	if (elf_ex.e_type != ET_REL || !(elf_ex.e_machine == EM_X86_64)) {
		error = "ELF not a relocatable file or not a x86_64..";
		//throw error;
		goto out;

	}
	sect_size = sizeof(Elf64_Shdr) * elf_ex.e_shnum;
	elf_shdata = mm_malloc(sect_size, 0);
	if (elf_shdata == 0) {
		error = "malloc failed";
		goto out;
	}
	fs_lseek(file, (unsigned long) elf_ex.e_shoff, 0);

	retval = complete_read(file, (unsigned char *) elf_shdata, sect_size);
	if (retval != sect_size) {
		error = "failed to read the sections from file";
		goto out;
	}

	sh_strtab = mm_malloc(elf_shdata[elf_ex.e_shstrndx].sh_size, 0);
	fs_lseek(file, (unsigned long) elf_shdata[elf_ex.e_shstrndx].sh_offset, 0);
	retval = complete_read(file, (unsigned char *) sh_strtab, elf_shdata[elf_ex.e_shstrndx].sh_size);
	if (retval != elf_shdata[elf_ex.e_shstrndx].sh_size) {
		error = "failed to read the sections section symbol table";
		goto out;
	}
	modulep = mm_malloc(sizeof(module_t), MEM_CLEAR);

	modulep->type = elf_ex.e_type;
	file_min_offset = 0;
	file_max_offset = 0;
	for (i = 0; i < elf_ex.e_shnum; i++, elf_shdata++) {
		if ((elf_shdata->sh_type == SHT_PROGBITS) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".text") == 0) {
			modulep->secs[SEC_TEXT].file_offset = elf_shdata->sh_offset;
			modulep->secs[SEC_TEXT].length = elf_shdata->sh_size;
			modulep->secs[SEC_TEXT].sec_index = i;

			file_min_offset = elf_shdata->sh_offset;
			file_max_offset = file_min_offset + elf_shdata->sh_size;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_PROGBITS) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".data") == 0) {
			modulep->secs[SEC_DATA].file_offset = elf_shdata->sh_offset;
			modulep->secs[SEC_DATA].length = elf_shdata->sh_size;
			modulep->secs[SEC_DATA].sec_index = i;
			if ((file_max_offset) < (elf_shdata->sh_offset + elf_shdata->sh_size)) {
				file_max_offset = elf_shdata->sh_offset + elf_shdata->sh_size;
			}
			continue;
		}
		if ((elf_shdata->sh_type == SHT_RELA) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".rela.text") == 0) {
			sec_rel_text = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_RELA) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".rela.data") == 0) {
			sec_rel_data = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_RELA) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".rela.rodata") == 0) {
			sec_rel_rodata = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_STRTAB) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".strtab") == 0) {
			sec_str = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_SYMTAB) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".symtab") == 0) {
			sec_symb = elf_shdata;
			continue;
		}
		if (ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".rodata") == 0) {
			modulep->secs[SEC_RODATA].file_offset = elf_shdata->sh_offset;
			modulep->secs[SEC_RODATA].length = elf_shdata->sh_size;
			modulep->secs[SEC_RODATA].sec_index = i;

			if ((file_max_offset) < (elf_shdata->sh_offset + elf_shdata->sh_size)) {
				file_max_offset = elf_shdata->sh_offset + elf_shdata->sh_size;
			}
			continue;
		}
		if (ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".bss") == 0) {
			modulep->secs[SEC_BSS].file_offset = elf_shdata->sh_offset;
			modulep->secs[SEC_BSS].length = elf_shdata->sh_size;
			modulep->secs[SEC_BSS].sec_index = i;
			continue;
		}
	}
	if ((modulep->secs[SEC_TEXT].length == 0) || (sec_rel_text == 0) || (sec_symb == 0)) {
		ut_printf("  sec_rel:%x sec_symv:%x \n", sec_rel_text, sec_symb);
		error = "necessary section are missing";
		goto out;
	} else {
		ut_printf("  sec_data:%x sec_str:%x sec_symv \n", sec_data, sec_str, sec_symb);
	}

	modulep->str_table = mm_malloc(sec_str->sh_size, 0);
	if (modulep->str_table == 0) {
		error = "allocating str_table";
		goto out;
	}
	modulep->str_table_length = sec_str->sh_size;

	fs_lseek(file, (unsigned long) sec_str->sh_offset, 0);
	retval = complete_read(file, (unsigned char *) modulep->str_table, sec_str->sh_size);
	if (retval != sec_str->sh_size) {
		error = "str table reading";
		goto out;
	}

	modulep->secs[SEC_TEXT].length = file_max_offset; /* this will cover text+rodata+data */
	//modulep->secs[SEC_TEXT].addr = mm_malloc(modulep->secs[SEC_TEXT].length + modulep->secs[SEC_BSS].length, MEM_CLEAR);

	modulep->secs[SEC_TEXT].addr = HIGHPRIORITY_APP_START;
	modulep->mem_start = HIGHPRIORITY_APP_START;
	modulep->mem_end = HIGHPRIORITY_APP_START + modulep->secs[SEC_TEXT].length + modulep->secs[SEC_BSS].length;
	modulep->secs[SEC_TEXT].addr[0]=0;/* touching memory */

	if (modulep->secs[SEC_TEXT].addr == 0) {
		error = "allocating code";
		goto out;
	}
	if (modulep->secs[SEC_BSS].length > 0) {
		modulep->secs[SEC_BSS].addr = modulep->secs[SEC_TEXT].addr + modulep->secs[SEC_TEXT].length;
	}
	if (modulep->secs[SEC_RODATA].file_offset > 0)
		modulep->secs[SEC_RODATA].addr = modulep->secs[SEC_TEXT].addr + modulep->secs[SEC_RODATA].file_offset
				- modulep->secs[SEC_TEXT].file_offset;

	if (modulep->secs[SEC_DATA].file_offset > 0)
		modulep->secs[SEC_DATA].addr = modulep->secs[SEC_TEXT].addr + modulep->secs[SEC_DATA].file_offset
				- modulep->secs[SEC_TEXT].file_offset;

	fs_lseek(file, (unsigned long) modulep->secs[SEC_TEXT].file_offset, 0);
	retval = complete_read(file, (unsigned char *) modulep->secs[SEC_TEXT].addr, modulep->secs[SEC_TEXT].length);
	if (retval != modulep->secs[SEC_TEXT].length) {
		error = "code section reading";
		goto out;
	}

	error = modulep->update_symbols(&source, &symb, sec_symb);
	if (error != 0) {
		goto out;
	}
	total_symbols = sec_symb->sh_size / sec_symb->sh_entsize;

	error = modulep->do_relocation(file, symb, total_symbols, sec_rel_text, SEC_TEXT);
	if (error != 0) {
		goto out;
	}
#if 1
	if (sec_rel_data != 0) {
		error = modulep->do_relocation(file, symb, total_symbols, sec_rel_data, SEC_DATA);
		if (error != 0) {
			goto out;
		}
	}
	if (sec_rel_rodata != 0) {
		error = modulep->do_relocation(file, symb, total_symbols, sec_rel_rodata, SEC_RODATA);
		if (error != 0) {
			goto out;
		}
	}
#endif

	spin_lock_irqsave(&g_global_lock, flags);
	if (error == 0 && (total_modules < MAX_MODULES)) {
		ut_strncpy(modulep->name, filename, MAX_FILENAME);
		g_modules[total_modules] = modulep;
		total_modules++;
	}
	spin_unlock_irqrestore(&g_global_lock, flags);
	if (modulep->highpriority_app_main == 0) {
		if ((modulep->init_module == 0 || modulep->clean_module == 0)) {
			error = "init_module or clean_module or main function not found";
			goto out;
		}
	}

	out: if (error != 0) {
		ut_printf("ERROR : %s\n", error);
	} else {
		modulep->sort_symbols();
		if (modulep->highpriority_app_main != 0) {
			launch_hp_task(modulep);
		} else {
			modulep->init_module(0, 0);
		}
		ut_printf(" Successfull loaded the module\n");
		return;
	}

	/* Free the allocated resources */
	if (file != 0) {
		fs_close(file);
	}
	mm_free(elf_shdata);
	mm_free(strtab);
	mm_free(sh_strtab);
	vfree(symb);
	modulep->free_module();

	return;
}
extern int Jcmd_maps(char *arg1, char *arg2);
void start_insexe(unsigned char *filename, unsigned char *arg);
unsigned long g_temp_hp_stack_len=0; /* TODO : remove later */
extern unsigned long fs_elf_check_prepare(struct file *file,unsigned char **argv, unsigned char **env,unsigned long *t_argc, unsigned long *t_argv,unsigned long  *stack_len, unsigned long *aux_addr,unsigned char **elf_interpreter, unsigned long *tmp_stackp);
extern int elf_initialize_userspace_stack(struct elfhdr elf_ex,unsigned long aux_addr,unsigned long tmp_stack, unsigned long stack_len,unsigned long load_addr,unsigned char *name);

unsigned char g_conf_hp_filename[MAX_FILENAME]="";
unsigned char g_conf_hp_arg[MAX_FILENAME];
unsigned int g_conf_hp_max_procs=1;
void Jcmd_insexe(unsigned char *filename, unsigned char *arg) {

	ut_strncpy(g_conf_hp_filename, filename, MAX_FILENAME);
	if (arg != 0){
		ut_strncpy(g_conf_hp_arg, arg, MAX_FILENAME);
	}else{
		ut_snprintf(g_conf_hp_arg,MAX_FILENAME,"dummy");
	}

	//sc_createKernelThread(start_insexe, 0, (unsigned char *) "hp_starter", 0);
	start_insexe(g_conf_hp_filename,g_conf_hp_arg);
	sc_sleep(2000); /* sleep so that idle kernel thread will pickup and start start_insexe, */
	return;
}
static void start_insexe(unsigned char *unused_filename, unsigned char *unused_arg) {
	struct file *file = 0;
	struct elfhdr elf_ex;
	Elf64_Shdr *elf_shdata;
	Elf64_Phdr *elf_phdata,*eppnt;
	int retval;
	int sect_size,phdr_size;
	const char *error = 0;
	int i;
	Elf64_Shdr *sec_data, *sec_rel_text, *sec_rel_data, *sec_rel_rodata, *sec_symb, *sec_str;
	unsigned char *sh_strtab = 0;
	unsigned char *strtab = 0;
	module_t *modulep = 0;
	unsigned long file_min_offset, file_max_offset;
	unsigned long flags;
	Elf64_Sym *symb = 0;
	binary_source_t source;
	int total_symbols = 0;
	unsigned long elf_bss, bss_start, bss;
	unsigned char *filename=g_conf_hp_filename;
	unsigned char *application_arg=g_conf_hp_arg;
	ut_printf(" Starting golang app :%s:  args :%s: \n",filename,application_arg);

	source.code_start = 0;
	source.code_length = 0;

	sec_data = sec_rel_text = sec_rel_data = sec_rel_rodata = sec_symb = sec_str = 0;
	elf_shdata = 0;

	if (filename == 0) {
		error = "file is empty";
		goto out;
	}
	error = 0;
	file = (struct file *) fs_open(filename, 0, 0);
	filename[0]=0;
	if (file == 0) {
		error = "Fail to open the module file";
		goto out;
	} else { /* flush the old file contents */
		fs_fadvise(file->vinode, 0, 0, POSIX_FADV_DONTNEED);
	}
	source.file = file;
	fs_lseek(file, 0, 0);
	retval = fs_read(file, (unsigned char *) &elf_ex, sizeof(elf_ex));
	if (retval != sizeof(elf_ex)) {
		error = "incorrect elf format..";
		goto out;
	}

	if (elf_ex.e_type != ET_EXEC || !(elf_ex.e_machine == EM_X86_64)) {
		error = "ELF not a executable file or not a x86_64..";
		goto out;
	}
	/* read section headers */
	sect_size = sizeof(Elf64_Shdr) * elf_ex.e_shnum;
	elf_shdata = mm_malloc(sect_size, 0);
	if (elf_shdata == 0) {
		error = "malloc failed";
		goto out;
	}
	fs_lseek(file, (unsigned long) elf_ex.e_shoff, 0);

	retval = complete_read(file, (unsigned char *) elf_shdata, sect_size);
	if (retval != sect_size) {
		error = "failed to read the sections from file";
		goto out;
	}

	sh_strtab = mm_malloc(elf_shdata[elf_ex.e_shstrndx].sh_size, 0);
	fs_lseek(file, (unsigned long) elf_shdata[elf_ex.e_shstrndx].sh_offset, 0);
	retval = complete_read(file, (unsigned char *) sh_strtab, elf_shdata[elf_ex.e_shstrndx].sh_size);
	if (retval != elf_shdata[elf_ex.e_shstrndx].sh_size) {
		error = "failed to read the sections section symbol table";
		goto out;
	}
	/* end of section reading */

	modulep = mm_malloc(sizeof(module_t), MEM_CLEAR);
	modulep->type = elf_ex.e_type;
	file_min_offset = 0;
	file_max_offset = 0;
#if 1
	for (i = 0; i < elf_ex.e_shnum; i++, elf_shdata++) {
		if ((elf_shdata->sh_type == SHT_PROGBITS) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".text") == 0) {
			modulep->secs[SEC_TEXT].file_offset = elf_shdata->sh_offset;
			modulep->secs[SEC_TEXT].length = elf_shdata->sh_size;
			modulep->secs[SEC_TEXT].sec_index = i;

			file_min_offset = elf_shdata->sh_offset;
			file_max_offset = file_min_offset + elf_shdata->sh_size;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_PROGBITS) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".data") == 0) {
			modulep->secs[SEC_DATA].file_offset = elf_shdata->sh_offset;
			modulep->secs[SEC_DATA].length = elf_shdata->sh_size;
			modulep->secs[SEC_DATA].sec_index = i;
			if ((file_max_offset) < (elf_shdata->sh_offset + elf_shdata->sh_size)) {
				file_max_offset = elf_shdata->sh_offset + elf_shdata->sh_size;
			}
			continue;
		}
		if ((elf_shdata->sh_type == SHT_RELA) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".rela.text") == 0) {
			sec_rel_text = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_RELA) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".rela.data") == 0) {
			sec_rel_data = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_RELA) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".rela.rodata") == 0) {
			sec_rel_rodata = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_STRTAB) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".strtab") == 0) {
			sec_str = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_SYMTAB) && ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".symtab") == 0) {
			sec_symb = elf_shdata;
			continue;
		}
		if (ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".rodata") == 0) {
			modulep->secs[SEC_RODATA].file_offset = elf_shdata->sh_offset;
			modulep->secs[SEC_RODATA].length = elf_shdata->sh_size;
			modulep->secs[SEC_RODATA].sec_index = i;

			if ((file_max_offset) < (elf_shdata->sh_offset + elf_shdata->sh_size)) {
				file_max_offset = elf_shdata->sh_offset + elf_shdata->sh_size;
			}
			continue;
		}
		if (ut_strcmp(sh_strtab + elf_shdata->sh_name, (uint8_t *) ".bss") == 0) {
			modulep->secs[SEC_BSS].file_offset = elf_shdata->sh_offset;
			modulep->secs[SEC_BSS].length = elf_shdata->sh_size;
			modulep->secs[SEC_BSS].sec_index = i;
			continue;
		}
	}
#endif
	/* load program table */
	phdr_size = sizeof(Elf64_Phdr) * elf_ex.e_phentsize;
	elf_phdata = mm_malloc(phdr_size, 0);
	if (elf_phdata == 0) {
		error = "malloc failed";
		goto out;
	}
	fs_lseek(file, (unsigned long) elf_ex.e_phoff, 0);

	retval = complete_read(file, (unsigned char *) elf_phdata, phdr_size);
	if (retval != phdr_size) {
		error = "failed to read the programheaders from file";
		goto out;
	}

	/* end of load program table */
	for (i = 0, eppnt = elf_phdata; i < elf_ex.e_phnum; i++, eppnt++) /* mmap all loadable program headers */
	{
		if (eppnt->p_type != PT_LOAD) {
			continue;
		}
		if (eppnt->p_filesz > 0) {
			unsigned long addr;
			unsigned long start_addr = ELF_PAGESTART(eppnt->p_vaddr);
			unsigned long end_addr = eppnt->p_filesz
					+ ELF_PAGEOFFSET(eppnt->p_vaddr);
			addr = vm_mmap(file, start_addr, end_addr, eppnt->p_flags, 0,
					(eppnt->p_offset - ELF_PAGEOFFSET(eppnt->p_vaddr)), "hp_text");
			if (addr == 0)
				error = 0;
		}else{
			while(1);
		}
		elf_bss = eppnt->p_vaddr + eppnt->p_filesz;
		//	padzero(elf_bss);

		/* TODO :  bss start address in not at the PAGE_ALIGN or ELF_MIN_ALIGN , need to club this partial page with the data */
		//	len = ELF_PAGESTART(eppnt->p_filesz + eppnt->p_vaddr + ELF_MIN_ALIGN - 1);
		bss_start = eppnt->p_filesz + eppnt->p_vaddr;
		bss = eppnt->p_memsz + eppnt->p_vaddr;
		//ut_log(" bss start :%x end:%x memsz:%x elf_bss:%x \n",bss_start, bss,eppnt->p_memsz,elf_bss);
		if (bss > bss_start) {
			vm_setupBrk(bss_start, bss - bss_start);
		}
	}
	vm_mmap(0, HIGHPRIORITY_APP_SYSCALLTABLE, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, 0,"hp_syscalltable");
	modulep->str_table = mm_malloc(sec_str->sh_size, 0);
	if (modulep->str_table == 0) {
		error = "allocating str_table";
		goto out;
	}
	modulep->str_table_length = sec_str->sh_size;

	fs_lseek(file, (unsigned long) sec_str->sh_offset, 0);
	retval = complete_read(file, (unsigned char *) modulep->str_table, sec_str->sh_size);
	if (retval != sec_str->sh_size) {
		error = "str table reading";
		goto out;
	}

	modulep->secs[SEC_TEXT].length = file_max_offset; /* this will cover text+rodata+data */
	//modulep->secs[SEC_TEXT].addr = mm_malloc(modulep->secs[SEC_TEXT].length + modulep->secs[SEC_BSS].length, MEM_CLEAR);

	modulep->secs[SEC_TEXT].addr = 0x400000;
	modulep->mem_start = 0x400000;
	modulep->mem_end = 0x400000 + modulep->secs[SEC_TEXT].length + modulep->secs[SEC_BSS].length;
	//ut_printf(" addr: %x - %x\n",modulep->secs[SEC_TEXT].addr,modulep->mem_end );
	i = (unsigned char )modulep->secs[SEC_TEXT].addr[0];/* reading memory */

	if (modulep->secs[SEC_TEXT].addr == 0) {
		error = "allocating code";
		goto out;
	}
	if (modulep->secs[SEC_BSS].length > 0) {
		modulep->secs[SEC_BSS].addr = modulep->secs[SEC_TEXT].addr + modulep->secs[SEC_TEXT].length;
	}
	if (modulep->secs[SEC_RODATA].file_offset > 0)
		modulep->secs[SEC_RODATA].addr = modulep->secs[SEC_TEXT].addr + modulep->secs[SEC_RODATA].file_offset
				- modulep->secs[SEC_TEXT].file_offset;

	if (modulep->secs[SEC_DATA].file_offset > 0)
		modulep->secs[SEC_DATA].addr = modulep->secs[SEC_TEXT].addr + modulep->secs[SEC_DATA].file_offset
				- modulep->secs[SEC_TEXT].file_offset;

#if 1
	error = modulep->load_symbols_for_HP_app(&source, &symb, sec_symb);
	if (error != 0) {
		goto out;
	}
#endif
	modulep->highpriority_app_main = elf_ex.e_entry ;
	//ut_printf("New HP starting address :%x \n",modulep->highpriority_app_main);
	spin_lock_irqsave(&g_global_lock, flags);
	if (error == 0 && (total_modules < MAX_MODULES)) {
		ut_strncpy(modulep->name, filename, MAX_FILENAME);
		g_modules[total_modules] = modulep;
		total_modules++;
	}
	spin_unlock_irqrestore(&g_global_lock, flags);

	if (modulep->highpriority_app_main == 0) {
		if ((modulep->init_module == 0 || modulep->clean_module == 0)) {
			error = "init_module or clean_module or main function not found";
			goto out;
		}
	}

out:
	if (error != 0) {
		ut_printf("ERROR : %s\n", error);
	} else {
		modulep->sort_symbols();  /* TODO: make it optional ,this is resource intensive */
		if (modulep->highpriority_app_main != 0) {
			unsigned char *temp_addr;

			vm_mmap(0, USER_SYSCALL_PAGE, 0x1000, PROT_READ | PROT_EXEC |PROT_WRITE, MAP_ANONYMOUS, 0,"hp_fst_syscal");
			ut_memcpy((unsigned char *)USER_SYSCALL_PAGE,(unsigned char *)&__HP_vsyscall_page,0x1000);

			temp_addr =(unsigned char *) modulep->highpriority_app_main;
			i= (int)temp_addr[0]; /*TODO: remove later  touch the mmeory just to help debugging */
			//launch_hp_task(modulep);
			if (1){ /* prepare the stack */
					unsigned long argv[4],env[4];
					unsigned long t_argc, t_argv;
					unsigned long stack_len, tmp_stack, tmp_stack_top, tmp_aux;
					unsigned char *elf_interp = 0;
					unsigned char env_str[100];

					argv[0]="highpriorityapp"; /* zero arguments */
					if (argv ==0){
						argv[1]=0;
					}else{
						argv[1]=application_arg;
						argv[2]=0;
					}
					ut_snprintf(&env_str[0],100,"GOMAXPROCS=%d",g_conf_hp_max_procs);
					env[0]=env_str;
					env[1]=0;
					tmp_stack_top = fs_elf_check_prepare(file, (unsigned char **)argv, (unsigned char **)env, &t_argc, &t_argv, &stack_len, &tmp_aux, &elf_interp, &tmp_stack);
					elf_initialize_userspace_stack(elf_ex, tmp_aux,tmp_stack_top, stack_len, ELF_PAGESTART(eppnt->p_vaddr),"hp_userstack");
					g_temp_hp_stack_len = stack_len;
					//ut_printf(" stack: %x len:%d \n",tmp_stack_top,g_temp_hp_stack_len);
					SYS_sc_clone(CLONE_VM | CLONE_KERNEL_THREAD| CLONE_HP_THREAD | CLONE_FS, tmp_stack_top, 0, modulep->highpriority_app_main, 0,0);
			}
			ut_printf(" Successfull loaded the high priority app\n------------------------------\n");
			Jcmd_reset_cpu_stat();
			//Jcmd_maps(0,0);
		}
		return;
	}

	/* Free the allocated resources */
	if (file != 0) {
		fs_close(file);
	}
	mm_free(elf_shdata);
	mm_free(strtab);
	mm_free(sh_strtab);
	vfree(symb);
	modulep->free_module();

	return;
}
#define MAX_FUNC_HITS 1000
struct func_debug {
	unsigned long addr;
	int hits;
};
static int func_hits_count=0;
struct func_debug func_hits[MAX_FUNC_HITS];
static int stat_cpu_rip_unknown_hit = 0;
long g_conf_func_debug  __attribute__ ((section ("confdata")))=982;
static unsigned long stat_unknown_ip=0;

void Jcmd_lsmod(unsigned char *arg1, unsigned char *arg2) {
	module_t *modulep = 0;
	int i, j,k;
	int option = 0;
	unsigned char *buf;
	int bsize = 1000;
	int total_hits = 0;
	int cpu;

	buf = mm_malloc(bsize+10, 0);
	if (arg1 != 0 && ut_strcmp(arg1, (uint8_t *) "all") == 0) {
		option = 1;
	} else if (arg1 != 0 && ut_strcmp(arg1, (uint8_t *) "stat") == 0) {
		option = 2;
	}else if (arg1 != 0 && ut_strcmp(arg1, (uint8_t *) "clear") == 0) {
		option = 3;
	}
	cpu=-1;
	if (arg2 != 0){
		cpu=ut_atoi(arg2,FORMAT_DECIMAL);
	}
	ut_snprintf(buf, bsize, " Stats for cpu: %d \n",cpu);
	fs_fd_write(1, buf, ut_strlen(buf));
	for (i = 0; i < total_modules; i++) {
		modulep = g_modules[i];
		modulep->use_count++;
		ut_printf("%d: %s symbls count:%d\n", i, modulep->name, modulep->symb_table_length);
		for (j = 0; j < SEC_TOTAL; j++) {
			ut_snprintf(buf, bsize, "	%2d: addr:%5x - %5x \n", modulep->secs[j].sec_index, modulep->secs[j].addr,
					modulep->secs[j].addr + modulep->secs[j].length);
			fs_fd_write(1, buf, ut_strlen(buf));
		}
		if (option == 2) {
			int max_ranks=20;

			modulep->rank_symbols_by_hits(max_ranks);

			for (k=1; k<max_ranks; k++) {
				for (j = 0; modulep->symbol_table[j].name != 0; j++){
					if (modulep->symbol_table[j].hit_rank == k){
						break;
					}
				}
				if (modulep->symbol_table[j].name == 0){
					ut_printf(" No hits for rank :%d \n",k);
					continue;
				}

				ut_snprintf(buf, bsize, "  %3d:t:%2d hits:%4d(%d:%d:%d:%d:%d) (rip=%p) %s -> %p (%d) \n", k,
						modulep->symbol_table[j].type, modulep->symbol_table[j].total_hits,modulep->symbol_table[j].stats[0].hits,modulep->symbol_table[j].stats[1].hits,modulep->symbol_table[j].stats[2].hits,modulep->symbol_table[j].stats[3].hits,modulep->symbol_table[j].stats[4].hits,
						modulep->symbol_table[j].stats[0].rip, modulep->symbol_table[j].name, modulep->symbol_table[j].address,
						modulep->symbol_table[j].len);
				fs_fd_write(1, buf, ut_strlen(buf));
				//total_hits = total_hits + hits;
			}
		}else if (option == 1){
			for (j = 0; modulep->symbol_table[j].name != 0; j++){
				ut_snprintf(buf, bsize, "  %3d:t:%2d s_idx:%2d hits:%4d (rip=%p) %s -> %p (%d) \n", j,
									modulep->symbol_table[j].type, modulep->symbol_table[j].sec_index, modulep->symbol_table[j].total_hits,
									modulep->symbol_table[j].stats[0].rip, modulep->symbol_table[j].name, modulep->symbol_table[j].address,
									modulep->symbol_table[j].len);
				fs_fd_write(1, buf, ut_strlen(buf));
			}
		}else if (option == 3){
			for (j = 0; modulep->symbol_table[j].name != 0; j++){
				for (k=0; k<MAX_CPUS; k++){
					modulep->symbol_table[j].stats[k].hits =0;
				}
			}
		}
		modulep->use_count--;
	}
	for (j=0; j<func_hits_count; j++){
		ut_snprintf(buf, bsize, " addr: %p  hits:%d\n",func_hits[j].addr,func_hits[j].hits);
		fs_fd_write(1, buf, ut_strlen(buf));
	}
	ut_snprintf(buf, bsize, " Total modules: %d total Hits:%d  unknownhits:%d unown ip:%p \n", total_modules, total_hits,
			stat_cpu_rip_unknown_hit,stat_unknown_ip);
	fs_fd_write(1, buf, ut_strlen(buf));

	mm_free(buf);
	return;
}

void Jcmd_reset_cpu_stat() {
	module_t *modulep = 0;
	int i, j,cpu;

	for (i = 0; i < total_modules; i++) {
		modulep = g_modules[i];
		modulep->use_count++;

		for (j = 0; modulep->symbol_table[j].name != 0; j++) {
			for (cpu = 0; cpu < MAX_CPUS; cpu++) {
				modulep->symbol_table[j].stats[cpu].hits = 0;
				modulep->symbol_table[j].stats[cpu].rip = 0;
			}
		}
		modulep->use_count--;
	}
	stat_cpu_rip_unknown_hit = 0;
	func_hits_count=0;
}

void Jcmd_rmmod(unsigned char *filename, unsigned char *arg) {
	int i;
	int ret = JFAIL;
	module_t *modulep = 0;

	for (i = 0; i < total_modules; i++) {
		modulep = g_modules[i];
		if (ut_strcmp(modulep->name, filename) == 0) {
			ret = remove_module(modulep);
			goto last;
		}
	}
	last: return;
}

unsigned char *ut_mod_get_symbol_name(unsigned long addr) {
	int i;
	module_t *modulep = 0;
	unsigned char *ret = 0;

	for (i = 1; i < total_modules; i++) {
		modulep = g_modules[i];
		ret = modulep->_get_symbol_name(addr);
	}

	return ret;
}

unsigned long ut_mod_get_symbol_addr(unsigned char *name) {
	int i;
	module_t *modulep = 0;
	unsigned long ret = 0;

	for (i = 0; i < total_modules; i++) {
		modulep = g_modules[i];
		ret = modulep->_get_symbol_addr(name);
		if (ret != ~(0x0)) {
			goto last;
		} else {
			ret = 0x0;
		}
	}

	last:

	return ret;
}
int ut_mod_symbol_execute(int type, char *name, char *argv1, char *argv2) {
	int (*func1)();
	module_t *modulep = 0;
	int i, j;
	int ret = 0;

//	ut_printf(" Trying to execute the function in a module \n");
	for (j = 1; j < total_modules; j++) { /* only for add on modules , not for kernel*/
		modulep = g_modules[j];
		if (modulep == 0) {
			BUG();
		}
		for (i = 0; modulep->symbol_table[i].name != 0; i++) {
			if ((modulep->symbol_table[i].type == SYMBOL_GTEXT || modulep->symbol_table[i].type == SYMBOL_CMD)
					&& ut_strcmp((uint8_t *) modulep->symbol_table[i].name, (uint8_t *) name) == 0) {
				func1 = (void *) modulep->symbol_table[i].address;
				ut_printf("FOUND: BEFORE executing:%s: :%x\n", name, func1);
				func1();
				ut_printf("FOUND: AFTER executing:%s: \n", name);
				return ret;
			}
		}
	}
 return ret;
}

int perf_stat_rip_hit(unsigned long rip) {
	int ret = JFAIL;
	int addr_found=0;
	int j;
	int curr=0;
	unsigned long cur_max=0;
	module_t *modulep = 0;
	int cpu=getcpuid();

	for (j = 0; j < total_modules; j++) {
		int min = 0;
		int max;

		if (j==0){
			min=35; /* TODO: initial symbols in kernel as low values, that collides with HP app */
		}
		modulep = g_modules[j];
		max = modulep->symb_table_length - 4;
		if (max < 0)
			max = 0;
		curr = max / 2;
		cur_max = modulep->symbol_table[max].address;
		if (rip < modulep->symbol_table[min].address || rip > modulep->symbol_table[max].address)
			continue;
		while (min < (max - 1)) { /* binary search */
			if (rip >= modulep->symbol_table[curr].address) {
				min = curr;
			} else {
				max = curr;
			}
			curr = (max + min) / 2;
		}
		curr = min;

		if ((rip >= modulep->symbol_table[curr].address) && (rip <= modulep->symbol_table[curr + 1].address)) {
			modulep->symbol_table[curr].stats[cpu].hits++;
			modulep->symbol_table[curr].stats[cpu].rip = rip;

			if (g_conf_func_debug == curr){
				int f;
				for (f=0; f<func_hits_count; f++){
					if (func_hits[f].addr == rip){
						func_hits[f].hits++;
						addr_found=1;
						break;
					}
				}
				if (addr_found==0 && func_hits_count<MAX_FUNC_HITS){
					func_hits[func_hits_count].addr = rip;
					func_hits[func_hits_count].hits = 1;
					func_hits_count++;
				}
			}
		}
		ret = JSUCCESS;
		goto last;
	}
	stat_cpu_rip_unknown_hit++;
	stat_unknown_ip=rip;
last:
	return ret;
}
static module_t kernel_module;
int init_kernel_module(symb_table_t *symbol_table, int total_symbols){
	ut_memset((unsigned char *)&kernel_module, 0, sizeof(kernel_module));
	total_modules=1;
	g_modules[0]=&kernel_module;
	ut_strcpy((unsigned char *)&kernel_module.name[0],(unsigned char *)"kernel");
	kernel_module.symbol_table = symbol_table;
	kernel_module.symb_table_length = total_symbols;
	return JSUCCESS;
}
}
