#include "common.h"
#include "interface.h"
#include "elf.h"


// TODO : locking need to be taken care
enum{
	SYMBOL_TYPE_DATA=0x1,
	SYMBOL_TYPE_TEXT=0x100,
	SYMBOL_TYPE_UNRESOLVED=0x1000
};
typedef struct {
	unsigned char *name;
	unsigned long address;
	char type;
}mod_symb_table_t;

typedef struct {
	unsigned char name[MAX_FILENAME]; /* module name and file name are same */
	int use_count; //TODO

	unsigned char *code;
	unsigned long code_file_offset;
	int code_length;

	mod_symb_table_t *symbol_table;
	unsigned char *str_table;
}module_t;

#define MAX_MODULES 100
static module_t *g_modules[MAX_MODULES];
static int total_modules=0;


typedef struct {
 unsigned long mod_rodata_offset;
 unsigned long mod_data_offset;
 unsigned long mod_rodata_size;
 int mod_rodata_secindex;
 int mod_sec_text;
 int mod_sec_data;
} temp_area_t;

unsigned long ut_mod_get_symbol_addr(unsigned char *name);
static unsigned long _get_symbol_addr(module_t *modulep, unsigned char *name);
static int get_symboltable(struct file *file, Elf64_Sym *symb, int total_symb,
		module_t *module, temp_area_t *temp_area) {
	int i, j, retval;
	int ret = JSUCCESS;
	Elf64_Sym *tsemb;
	int global_symb = 0;

	if (module->str_table == 0)
		return ret;

	tsemb = symb;
	for (i = 0; i < total_symb; i++, tsemb++) {
		if (tsemb->st_info & STB_GLOBAL) {
			global_symb++;
		}
	}
	module->symbol_table = mm_malloc(
			(global_symb + 1) * sizeof(mod_symb_table_t), 0);
	j = 0;
	tsemb = symb;
	for (i = 0; i < total_symb; i++, tsemb++) {
#ifdef MODULE_DEBUG
		ut_printf(" %d: name:%d info:%x secton_ind:%d \n",i,tsemb->st_name,tsemb->st_info,tsemb->st_shndx);
#endif
		if (ELF32_ST_BIND(tsemb->st_info) == STB_GLOBAL) {
			if (tsemb->st_shndx == 0){ /* unresolved symbol */
				module->symbol_table[j].type = SYMBOL_TYPE_UNRESOLVED;
				module->symbol_table[j].address = 0;
			}else if (tsemb->st_shndx == temp_area->mod_sec_text){
				module->symbol_table[j].address = tsemb->st_value + module->code;
				module->symbol_table[j].type = SYMBOL_TYPE_TEXT;
			}else{
				module->symbol_table[j].address = tsemb->st_value + (module->code+temp_area->mod_data_offset-module->code_file_offset);
				module->symbol_table[j].type = SYMBOL_TYPE_DATA;
			}
			module->symbol_table[j].name = module->str_table + tsemb->st_name;
#ifdef MODULE_DEBUG
			ut_printf("%d: symbol name:%s value:%x \n", j,
					module->symbol_table[j].name,
					module->symbol_table[j].address);
#endif
			j++;
		}
	}
	module->symbol_table[j].name = 0;
	module->symbol_table[j].address = 0;

	return ret;
}
static unsigned char* text_relocation(struct file *file, Elf64_Shdr *sec_symb, Elf64_Shdr *sec_rel, module_t *module,temp_area_t *temp_area) {
	unsigned const char *ret = 0;
	Elf64_Sym *symb = 0;
	Elf64_Rela *reloc =0;
	int i;
	int total_symbols;

	symb = mm_malloc(sec_symb->sh_size, 0);
	fs_lseek(file, sec_symb->sh_offset, 0);
	fs_read(file, (unsigned char *) symb,
			sec_symb->sh_size );

	total_symbols = sec_symb->sh_size/sec_symb->sh_entsize;
	if (get_symboltable(file, symb, total_symbols, module, temp_area) != JSUCCESS) {
		ret = "getting symbol table fails";
		goto out;
	}

	reloc = mm_malloc(sec_rel->sh_size, 0);
	fs_lseek(file, sec_rel->sh_offset, 0);
	fs_read(file, (unsigned char *) reloc, sec_rel->sh_size );
	for (i=0; i<(sec_rel->sh_size/sec_rel->sh_entsize); i++,reloc++){
		unsigned addr = module->code + reloc->r_offset;
		if (addr > (module->code+module->code_length)){
			ret = " Fail during text relocation ";
			return ret;
		}
		int s_index = ELF64_R_SYM(reloc->r_info);
		int type = ELF64_R_TYPE(reloc->r_info);
		if (s_index > total_symbols){
			ret = " symbol index is more then total symbols ";
			return ret;
		}
		unsigned long symbol_value = 0;
		if (ELF64_ST_BIND(symb[s_index].st_info) == STB_GLOBAL) {
			symbol_value = _get_symbol_addr(module, module->str_table + symb[s_index].st_name);
			if (symbol_value == 0)
				symbol_value = ut_get_symbol_addr(module->str_table + symb[s_index].st_name);
		} else if (ELF64_ST_BIND(symb[s_index].st_info) == STB_LOCAL) {
			if (ELF64_ST_TYPE(symb[s_index].st_info) == STT_SECTION && (symb[s_index].st_shndx == temp_area->mod_rodata_secindex) ){
				symbol_value = module->code + temp_area->mod_rodata_offset - module->code_file_offset ;
			}else{
				symbol_value = symb[s_index].st_value + module->code;
			}
		}
#ifdef MODULE_DEBUG
		ut_printf(" %d symbol :%s: value:%x \n",s_index, module->str_table + symb[s_index].st_name, symbol_value);
#endif

		if ((type == R_X86_64_PC32) || (type == R_X86_64_32)){
			unsigned char *p = addr;
			int32_t *v = p;

			if (type == R_X86_64_PC32){
				*v = (int32_t)((symbol_value)&0xffffffff) - (int32_t)(addr&0xffffffff) +reloc->r_addend;
			}else{
				*v = (int32_t)((symbol_value)&0xffffffff)  + reloc->r_addend;
			}
#ifdef MODULE_DEBUG
			ut_printf("New addr: %x  after int value:%x  addedend :%x(%d) \n",v,*v,reloc->r_addend);
#endif
		}
	}

out:
    mm_free(symb);
	return ret;
}
static int free_module(module_t *modulep){
	mm_free(modulep->code);
	mm_free(modulep->str_table);
	mm_free(modulep->symbol_table);
	mm_free(modulep);
}

void Jcmd_insmod(unsigned char *filename, unsigned char *arg);
void Jcmd_insmod(unsigned char *filename, unsigned char *arg){
	struct file *file=0;
	struct elfhdr elf_ex;
	Elf64_Shdr *elf_shdata;
	int retval ;
	int sect_size;
	const char *error=0;
	int i;
	Elf64_Shdr *sec_text,*sec_data,*sec_rel,*sec_symb,*sec_str;
	unsigned char *sh_strtab=0;
	unsigned char *strtab=0;
	module_t *modulep=0;
	unsigned long file_min_offset,file_max_offset;
	temp_area_t *temp_area;

	sec_text=sec_data=sec_rel=sec_symb=sec_str=0;
	elf_shdata =0;

	temp_area = mm_malloc(sizeof(temp_area_t),MEM_CLEAR);

	if (filename ==0){
		error = "file is empty";
		goto out;
	}
	error = 0;
	file = (struct file *)fs_open(filename, 0, 0);
	if (file == 0){
		error = "Fail to open the module file";
		goto out;
	}
	fs_lseek(file, 0, 0);
	retval = fs_read(file, (unsigned char *) &elf_ex, sizeof(elf_ex));
	if (retval != sizeof(elf_ex)) {
		error = "incorrect elf format";
		goto out;
	}

	if (elf_ex.e_type != ET_REL || !(elf_ex.e_machine == EM_X86_64)) {
		error = "ELF not a relocatable file or not a x86_64";
		goto out;
	}
	sect_size = sizeof(Elf64_Shdr) * elf_ex.e_shnum;
	elf_shdata = mm_malloc(sect_size, 0);
	if (elf_shdata==0){
		error = "malloc failed";
		goto out;
	}
	fs_lseek(file, (unsigned long) elf_ex.e_shoff, 0);
	retval = fs_read(file, (unsigned char *) elf_shdata, sect_size);
	if (retval != sect_size) {
		error = "failed to read the sections from file";
		goto out;
	}

	sh_strtab=mm_malloc(elf_shdata[elf_ex.e_shstrndx].sh_size, 0);
	fs_lseek(file, (unsigned long) elf_shdata[elf_ex.e_shstrndx].sh_offset, 0);
	retval = fs_read(file, (unsigned char *) sh_strtab, elf_shdata[elf_ex.e_shstrndx].sh_size);
	if (retval != elf_shdata[elf_ex.e_shstrndx].sh_size){
		error = "failed to read the sections section symbol table";
		goto out;
	}
	file_min_offset =0;
	file_max_offset=0;
	for (i=0; i<elf_ex.e_shnum;   i++,elf_shdata++){
		if ((elf_shdata->sh_type == SHT_PROGBITS) && ut_strcmp(sh_strtab+elf_shdata->sh_name,".text")==0){
			sec_text = elf_shdata;
			file_min_offset =  sec_text->sh_offset;
			file_max_offset = file_min_offset+sec_text->sh_size;
			temp_area->mod_sec_text = i;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_PROGBITS) && ut_strcmp(sh_strtab+elf_shdata->sh_name,".data")==0){
			sec_data = elf_shdata;
			temp_area->mod_data_offset = elf_shdata->sh_offset;
			temp_area->mod_sec_data = i;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_RELA) && ut_strcmp(sh_strtab+elf_shdata->sh_name,".rela.text")==0){
			sec_rel = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_STRTAB) && ut_strcmp(sh_strtab+elf_shdata->sh_name,".strtab")==0){
			sec_str = elf_shdata;
			continue;
		}
		if ((elf_shdata->sh_type == SHT_SYMTAB) && ut_strcmp(sh_strtab+elf_shdata->sh_name,".symtab")==0){
			sec_symb = elf_shdata;
			continue;
		}
		if (ut_strcmp(sh_strtab + elf_shdata->sh_name, ".rodata") == 0) {
			temp_area->mod_rodata_offset = elf_shdata->sh_offset;
			temp_area->mod_rodata_size = elf_shdata->sh_size;
			temp_area->mod_rodata_secindex = i;
			if ((file_max_offset) < (temp_area->mod_rodata_offset + temp_area->mod_rodata_size)) {
				file_max_offset = temp_area->mod_rodata_offset + temp_area->mod_rodata_size;
			}
			continue;
		}
	}
	if ((sec_text==0) || (sec_rel==0) || (sec_symb==0)){
		ut_printf(" sec_txt:%x sec_rel:%x sec_symv:%x \n",sec_text,sec_rel,sec_symb);
		error = "necessary section are missing";
		goto out;
	}else{
		ut_printf(" sec_txt:%x sec_data:%x sec_str:%x sec_symv \n",sec_text,sec_data,sec_str,sec_symb);
	}

	modulep=mm_malloc(sizeof(module_t),MEM_CLEAR);
	modulep->str_table = mm_malloc(sec_str->sh_size,0);
	fs_lseek(file, (unsigned long) sec_str->sh_offset, 0);
	retval = fs_read(file, (unsigned char *) modulep->str_table, sec_str->sh_size);
	if (retval != sec_str->sh_size){
		error = "str table reading";
		goto out;
	}

	modulep->code_length = file_max_offset - file_min_offset ;
	modulep->code=mm_malloc(modulep->code_length,0);
	modulep->code_file_offset = file_min_offset;

	fs_lseek(file, (unsigned long) file_min_offset, 0);
	retval = fs_read(file, (unsigned char *) modulep->code, modulep->code_length);
	if (retval != modulep->code_length){
		error = "code section reading";
		goto out;
	}

	error = text_relocation(file, sec_symb, sec_rel, modulep,temp_area);
	if ( error != 0){
		goto out;
	}

	if (error==0 && (total_modules < MAX_MODULES)) {
		ut_strncpy(modulep->name,filename,MAX_FILENAME);
		g_modules[total_modules] = modulep;
		total_modules++;
	}

out:  ;
	if (error != 0){
		ut_printf("ERROR : %s\n",error);
	}else{
		ut_printf(" Successfull in loading the file \n");
		return;
	}

/* Free the allocated resources */
	if (file != 0){
		fs_close(file);
	}
	mm_free(elf_shdata);
	mm_free(strtab);
	mm_free(sh_strtab);

	free_module(modulep);

	return;
}

void Jcmd_lsmod(unsigned char *arg1,unsigned char *arg2){
	module_t *modulep=0;
	int i,j;

	for (i=0; i<total_modules; i++ ){
		modulep=g_modules[i];
		ut_printf("%d: %s\n",i,modulep->name);
		for (j=0 ; modulep->symbol_table[j].name!=0; j++){
			ut_printf("		%d: %s -> %x \n", j, modulep->symbol_table[j].name, modulep->symbol_table[j].address);
		}
	}
	ut_printf(" Total modules: %d \n",total_modules);
}
void Jcmd_rmmod(unsigned char *filename, unsigned char *arg){
	int i;
	int ret=JFAIL;
	module_t *modulep=0;

	for (i=0; i<total_modules; i++ ){
		modulep=g_modules[i];
		if (ut_strcmp(modulep->name , filename)==0){
			free_module(modulep);

			g_modules[i]=0;
			total_modules--;
			if (i<total_modules)
				g_modules[i]=g_modules[total_modules];
			ret = JSUCCESS;
			goto last ;
		}
	}
last:
	return;
}
static unsigned long _get_symbol_addr(module_t *modulep, unsigned char *name){
	int i;
	unsigned long ret = 0;
	for (i = 0; modulep->symbol_table[i].name != 0; i++) {
		if (ut_strcmp(modulep->symbol_table[i].name, name) == 0) {
			ret = modulep->symbol_table[i].address;
			return ret;
		}
	}

	return ret;
}
unsigned long ut_mod_get_symbol_addr(unsigned char *name) {
	int i;
	module_t *modulep=0;
	unsigned long ret =0;

	for (i=0; i<total_modules; i++ ){
		modulep=g_modules[i];
		ret = _get_symbol_addr(modulep,name);
		if (ret != 0) goto last;
	}

last:
	return ret;
}
int ut_mod_symbol_execute(int type, char *name, char *argv1,char *argv2){
	int (*func)(char *argv1,char *argv2);
	int (*func1)();
	module_t *modulep=0;
	int i,j;
	int ret=0;

	ut_printf(" Trying to execute thefunction in a module \n");
	for (j=0; j<total_modules ; j++ ){
		modulep=g_modules[j];
		if (modulep ==0) {
			BUG();
		}
		for (i = 0; modulep->symbol_table[i].name != 0; i++) {
			if (ut_strcmp(modulep->symbol_table[i].name, name) == 0) {
				func1=(void *)modulep->symbol_table[i].address;
				ut_printf(" FOUND: before executing:%s: :%x\n",name,func1);
				func1();
				ut_printf(":FOUND: after executing:%s: \n",name);
				ret = 1;
				goto last;
			}
		}
	}
last:
	return ret;
}
