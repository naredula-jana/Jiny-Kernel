/*
  Copyright (c) 2009-2010 David Anderson.  All rights reserved.
 
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the example nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.
 
  THIS SOFTWARE IS PROVIDED BY David Anderson ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL David Anderson BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
*/
/*  simplereader.c
    This is an example of code reading dwarf .debug_info.
    It is kept as simple as possible to expose essential features.
    It does not do all possible error reporting or error handling.

    The --names 
    option adds some extra printing.

    To use, try
        make
        ./simplereader simplereader
*/
#include <sys/types.h> /* For open() */
#include <sys/stat.h>  /* For open() */
#include <fcntl.h>     /* For open() */
#include <stdlib.h>     /* For exit() */
#include <unistd.h>     /* For close() */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "dwarf.h"
#include "libdwarf.h"

struct srcfilesdata {
    char ** srcfiles;
    Dwarf_Signed srcfilescount;
    int srcfilesres;
};

static void read_cu_list(Dwarf_Debug dbg);
static void print_die_data(Dwarf_Debug dbg, Dwarf_Die print_me,int level,
   struct srcfilesdata *sf);
static void get_die_and_siblings(Dwarf_Debug dbg, Dwarf_Die in_die,int in_level,
   struct srcfilesdata *sf);
static void resetsrcfiles(Dwarf_Debug dbg,struct srcfilesdata *sf);
int count_data_types=0;
static int namesoptionon = 0;

int 
main(int argc, char **argv)
{

    Dwarf_Debug dbg = 0;
    int fd = -1;
    const char *filepath = "<stdin>";
    int res = DW_DLV_ERROR;
    Dwarf_Error error;
    Dwarf_Handler errhand = 0;
    Dwarf_Ptr errarg = 0;

    if(argc < 2) {
        fd = 0; /* stdin */
    } else {
        int i = 0;
        for(i = 1; i < (argc-1) ; ++i) {
            if(strcmp(argv[i],"--names") == 0) {
                namesoptionon=1;
            } else {
                printf("Unknown argument \"%s\" ignored\n",argv[i]);
            }
        }
        filepath = argv[i];
        fd = open(filepath,O_RDONLY);
    }
    if(argc > 2) {
    }
    if(fd < 0) {
        printf("Failure attempting to open \"%s\"\n",filepath);
    }
    res = dwarf_init(fd,DW_DLC_READ,errhand,errarg, &dbg,&error);
    if(res != DW_DLV_OK) {
        printf("Giving up, cannot do DWARF processing\n");
        exit(1);
    }

    read_cu_list(dbg);
    res = dwarf_finish(dbg,&error);
    if(res != DW_DLV_OK) {
        printf("dwarf_finish failed!\n");
    }
    close(fd);
    scan_db();
    return 0;
}

static void 
read_cu_list(Dwarf_Debug dbg)
{
    Dwarf_Unsigned cu_header_length = 0;
    Dwarf_Half version_stamp = 0;
    Dwarf_Unsigned abbrev_offset = 0;
    Dwarf_Half address_size = 0;
    Dwarf_Unsigned next_cu_header = 0;
    Dwarf_Error error;
    int cu_number = 0;

    for(;;++cu_number) {
        struct srcfilesdata sf;
        sf.srcfilesres = DW_DLV_ERROR;
        sf.srcfiles = 0;
        sf.srcfilescount = 0;
        Dwarf_Die no_die = 0;
        Dwarf_Die cu_die = 0;
        int res = DW_DLV_ERROR;
        res = dwarf_next_cu_header(dbg,&cu_header_length,
            &version_stamp, &abbrev_offset, &address_size,
            &next_cu_header, &error);
        if(res == DW_DLV_ERROR) {
            printf("Error in dwarf_next_cu_header\n");
            exit(1);
        }
        if(res == DW_DLV_NO_ENTRY) {
            /* Done. */
            return;
        }
        /* The CU will have a single sibling, a cu_die. */
        res = dwarf_siblingof(dbg,no_die,&cu_die,&error);
        if(res == DW_DLV_ERROR) {
            printf("Error in dwarf_siblingof on CU die \n");
            exit(1);
        }
        if(res == DW_DLV_NO_ENTRY) {
            /* Impossible case. */
            printf("no entry! in dwarf_siblingof on CU die \n");
            exit(1);
        }
        get_die_and_siblings(dbg,cu_die,0,&sf);
        dwarf_dealloc(dbg,cu_die,DW_DLA_DIE);
        resetsrcfiles(dbg,&sf);
       if (count_data_types > 0) {
    	 //  return;
       }
        printf(" Next lookp of task_struct offset:%x \n",abbrev_offset);
    }
}

static void
get_die_and_siblings(Dwarf_Debug dbg, Dwarf_Die in_die,int in_level,
   struct srcfilesdata *sf)
{
    int res = DW_DLV_ERROR;
    Dwarf_Die cur_die=in_die;
    Dwarf_Die child = 0;
    Dwarf_Error error;
   
    print_die_data(dbg,in_die,in_level,sf);

    for(;;) {
        Dwarf_Die sib_die = 0;
        res = dwarf_child(cur_die,&child,&error);
        if(res == DW_DLV_ERROR) {
            printf("Error in dwarf_child , level %d \n",in_level);
            exit(1);
        }
        if(res == DW_DLV_OK) {
            get_die_and_siblings(dbg,child,in_level+1,sf);
        }
        /* res == DW_DLV_NO_ENTRY */
        res = dwarf_siblingof(dbg,cur_die,&sib_die,&error);
        if(res == DW_DLV_ERROR) {
            printf("Error in dwarf_siblingof , level %d \n",in_level);
            exit(1);
        }
        if(res == DW_DLV_NO_ENTRY) {
            /* Done at this level. */
            break;
        }
        /* res == DW_DLV_OK */
        if(cur_die != in_die) {
            dwarf_dealloc(dbg,cur_die,DW_DLA_DIE);
        }
        cur_die = sib_die;
        print_die_data(dbg,cur_die,in_level,sf);
    }
    return;
}
static void
get_addr(Dwarf_Attribute attr,Dwarf_Addr *val)
{
    Dwarf_Error error = 0;
    int res;
    Dwarf_Addr uval = 0;
    res = dwarf_formaddr(attr,&uval,&error);
    if(res == DW_DLV_OK) {
        *val = uval;
        return;
    }
    return;
}
static void
get_number(Dwarf_Attribute attr,Dwarf_Unsigned *val)
{
    Dwarf_Error error = 0;
    int res;
    Dwarf_Signed sval = 0;
    Dwarf_Unsigned uval = 0;
    res = dwarf_formudata(attr,&uval,&error);
    if(res == DW_DLV_OK) {
        *val = uval;
        return;
    }
    res = dwarf_formsdata(attr,&sval,&error);
    if(res == DW_DLV_OK) {
        *val = sval;
        return;
    }
    return;
}
struct attributes{
	Dwarf_Off type_offset;
	Dwarf_Unsigned size;
	int member_location;
	char *filename;
}attributes;
static int
get_attributes(Dwarf_Debug dbg,Dwarf_Die die, int level,
    struct srcfilesdata *sf, struct attributes *attr)
{
    int res;
    Dwarf_Error error = 0;
    Dwarf_Attribute *attrbuf = 0;
    Dwarf_Addr lowpc = 0;
    Dwarf_Addr highpc = 0;
    Dwarf_Signed attrcount = 0;
    Dwarf_Unsigned i;
    Dwarf_Unsigned filenum = 0;
    Dwarf_Unsigned linenum = 0;
    char *filename = 0;
    int ret = 1;
    res = dwarf_attrlist(die,&attrbuf,&attrcount,&error);
    if(res != DW_DLV_OK) {
        return ret;
    }
    for(i = 0; i < attrcount ; ++i) {
        Dwarf_Half aform;
        res = dwarf_whatattr(attrbuf[i],&aform,&error);
        if(res == DW_DLV_OK) {
            if(aform == DW_AT_decl_file) {
                get_number(attrbuf[i],&filenum);
                if((filenum > 0) && (sf->srcfilescount > (filenum-1))) {
                    filename = sf->srcfiles[filenum-1];
                    attr->filename = filename;
                }
            }
            if(aform == DW_AT_declaration) {
             	Dwarf_Unsigned size=0;
                 get_number(attrbuf[i],&size);
                 ret =0;
             }
            if(aform == DW_AT_byte_size) {
            	attr->size=0;
                get_number(attrbuf[i],&attr->size);
            }
            if(aform == DW_AT_data_member_location) {
            	attr->member_location=0;
                get_number(attrbuf[i],&attr->member_location);
            }
            if(aform == DW_AT_type) {
            	attr->type_offset = 0;
            	dwarf_global_formref(attrbuf[i],&(attr->type_offset),&error);
            }
        }
        dwarf_dealloc(dbg,attrbuf[i],DW_DLA_ATTR);
    }

    dwarf_dealloc(dbg,attrbuf,DW_DLA_LIST);
    return ret;
}

static void
print_comp_dir(Dwarf_Debug dbg,Dwarf_Die die,int level, struct srcfilesdata *sf)
{
    int res;
    Dwarf_Error error = 0;
    Dwarf_Attribute *attrbuf = 0;
    Dwarf_Signed attrcount = 0;
    Dwarf_Unsigned i;
    res = dwarf_attrlist(die,&attrbuf,&attrcount,&error);
    if(res != DW_DLV_OK) {
        return;
    }
    sf->srcfilesres = dwarf_srcfiles(die,&sf->srcfiles,&sf->srcfilescount, 
        &error);
    for(i = 0; i < attrcount ; ++i) {
        Dwarf_Half aform;
        res = dwarf_whatattr(attrbuf[i],&aform,&error);
        if(res == DW_DLV_OK) {
            if(aform == DW_AT_comp_dir) {
                char *name = 0;
                res = dwarf_formstring(attrbuf[i],&name,&error);
                if(res == DW_DLV_OK) {
                    printf(    "<%3d> compilation directory : \"%s\"\n",
                        level,name);
                }
            }
            if(aform == DW_AT_stmt_list) {
                /* Offset of stmt list for this CU in .debug_line */
            }
        }
        dwarf_dealloc(dbg,attrbuf[i],DW_DLA_ATTR);
    }
    dwarf_dealloc(dbg,attrbuf,DW_DLA_LIST);
}

static void
resetsrcfiles(Dwarf_Debug dbg,struct srcfilesdata *sf)
{
    Dwarf_Signed sri = 0;
    for (sri = 0; sri < sf->srcfilescount; ++sri) {
        dwarf_dealloc(dbg, sf->srcfiles[sri], DW_DLA_STRING);
    }
    dwarf_dealloc(dbg, sf->srcfiles, DW_DLA_LIST);
    sf->srcfilesres = DW_DLV_ERROR;
    sf->srcfiles = 0;
    sf->srcfilescount = 0;
}
Dwarf_Half curr_top_tag=0;

#define MAX_NAME 100
#define MAX_ENTRIES 20000
struct db_entry{
	unsigned char level;
	unsigned char name[MAX_NAME];
	int size;
	unsigned long offset; /* offset of this entry */
	unsigned long type_offset; /* offset of type */
	int member_location;
	int tag;
	char tmp_used;
	int type_index;
}db_entry;
struct db_entry db_entries[MAX_ENTRIES];
struct db_entry final_entries[MAX_ENTRIES];
int db_count=0;
int final_count=0;

static void
print_die_data(Dwarf_Debug dbg, Dwarf_Die print_me,int level,
    struct srcfilesdata *sf)
{
    char *name = 0;
    Dwarf_Error error = 0;
    Dwarf_Half tag = 0;
    const char *tagname = 0;
    int localname = 0;
    struct attributes attr;

    int res = dwarf_diename(print_me,&name,&error);

    if(res == DW_DLV_ERROR) {
        printf("Error in dwarf_diename , level %d \n",level);
        exit(1);
    }
    if(res == DW_DLV_NO_ENTRY) {
        name = "<no DW_AT_name attr>";
        localname = 1;
    }
    res = dwarf_tag(print_me,&tag,&error);
    if(res != DW_DLV_OK) {
        printf("Error in dwarf_tag , level %d \n",level);
        exit(1);
    }
    res = dwarf_get_TAG_name(tag,&tagname);
    if(res != DW_DLV_OK) {
        printf("Error in dwarf_get_TAG_name , level %d \n",level);
        exit(1);
    }
	if (level == 1) {
		if (tag == DW_TAG_structure_type || tag == DW_TAG_base_type ||(tag==DW_TAG_array_type)|| (tag == DW_TAG_pointer_type) || (tag == DW_TAG_typedef) || (tag==DW_TAG_variable)) {
			curr_top_tag = DW_TAG_structure_type;
		} else {
			curr_top_tag = 0;
		}
	}

	if (curr_top_tag != 0) {
		Dwarf_Off offset = 0;
		dwarf_dieoffset(print_me, &offset, &error);
		if (level > 1) {
			printf("    ");
		}
		count_data_types++;
		attr.size =0;
		attr.type_offset =0;
		attr.filename = 0;
		attr.member_location = 0;
		if (get_attributes(dbg, print_me, level, sf, &attr)==1){
			printf("<%d> tag: %d %s  name: \"%s\"  offset:%x ", level, tag, tagname,
					name, offset);
			if (attr.type_offset != 0){
				printf(" type offset: %x ",attr.type_offset);
			}
			if (attr.size != 0){
				printf("size : %d ",attr.size);
			}
			if (attr.filename != 0){
				printf(" filename: %s ",attr.filename);
			}
			printf("\n");
			db_entries[db_count].offset = offset;
			db_entries[db_count].type_offset = attr.type_offset;
			db_entries[db_count].tag = tag;
			db_entries[db_count].level = level;
			db_entries[db_count].size = attr.size;
			db_entries[db_count].member_location = attr.member_location;

			strncpy(db_entries[db_count].name,name,MAX_NAME-1);
			db_count++;
		}else{
		//	printf("Decleartion <%d> tag: %d %s  name: \"%s\"  offset:%x \n", level, tag, tagname,
		//						name, offset);
		}

	}
	if (!localname) {
		dwarf_dealloc(dbg, name, DW_DLA_STRING);
	}
}
int check_no_duplicate(unsigned char *name){
	int i;
	for (i=0; i<final_count; i++){
		if (strcmp(final_entries[i].name,name)==0){
			return 0;
		}
	}
	return 1;
}
int touch_offsets(Dwarf_Off type_offset, int recursive_level){
	int i,k;

	if (recursive_level>10) return 0;
	for (i=0; i<db_count; i++){
		if (db_entries[i].offset == type_offset){
			if (db_entries[i].tmp_used == 1) return 1;
			db_entries[i].tmp_used =1;
			for (k=i+1; k<db_count; k++){
				if (db_entries[k].level == 1){
					break;
				}
				db_entries[k].tmp_used = 1;
				touch_offsets(db_entries[k].type_offset, recursive_level+1);
			}
			touch_offsets(db_entries[i].type_offset, recursive_level+1);
			return 1;
		}
	}
	return 0;
}
int scan_db(){
	int i,j;
    const char *tagname = 0;
    int var_count;

	/* untouch all entries */
	for (i=0; i<db_count; i++){
		db_entries[i].tmp_used =0;
	}
	/* copy variables and touch the types */
	for (i=0; i<db_count; i++){
		if ((db_entries[i].tag == DW_TAG_variable) && check_no_duplicate(db_entries[i].name)==1){
			memcpy(&final_entries[final_count],&db_entries[i],sizeof(struct db_entry));
			touch_offsets(final_entries[final_count].type_offset,0);
			final_count++;
		}
	}
	var_count=final_count;
	/* copy types and subtypes */
	for (i=0; i<db_count; i++){
		if ((db_entries[i].tag != DW_TAG_variable) && (db_entries[i].tmp_used == 1)){
			memcpy(&final_entries[final_count],&db_entries[i],sizeof(struct db_entry));
			final_count++;
		}
	}

/* create index istead of using offset values for types*/
	for (i=0; i<final_count; i++){
		final_entries[i].type_index  = -1;
		final_entries[i].tmp_used = 0;

		if (final_entries[i].tag == DW_TAG_variable ){
			final_entries[i].tmp_used = -1;
			final_entries[i].type_index  = 0;
		}
	}
	for (i=0; i<final_count; i++){
		Dwarf_Off type_offset;
		if (final_entries[i].tag == DW_TAG_variable ) continue;
		type_offset = final_entries[i].offset;
		for (j=0; j<final_count; j++){

			if (final_entries[j].type_offset == type_offset){
				final_entries[j].type_index = i;
				final_entries[i].tmp_used++;
			}
		}
	}

	/* print the final entries */
	printf(" Db count :%d Final_count:%d varcount:%d \n",db_count,final_count,var_count);
	for (i=0; i<final_count; i++){
		tagname=0;
	    dwarf_get_TAG_name(final_entries[i].tag,&tagname);
		printf("final-%d <%d> tag: %d %s  name: \"%s\"  offset:%x type_offset:%x len:%d locatoin:%d used:%d type_index:%d\n",i,final_entries[i].level, final_entries[i].tag, tagname,
				final_entries[i].name, final_entries[i].offset, final_entries[i].type_offset,final_entries[i].size,final_entries[i].member_location,final_entries[i].tmp_used,final_entries[i].type_index);
	}
	write_to_file("./dwarf_datatypes");
}
void write_to_file(char *filename) {
    int  wfd;

    wfd = open(filename, O_WRONLY | O_CREAT);
    if ( wfd < 0) {
        printf(" NO FILE EXISTS \n");
        return 0;
    }

    write(wfd, &final_entries[0], final_count * sizeof(struct db_entry));
    close(wfd);
}

