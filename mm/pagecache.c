#include "mm.h"
#include "vfs.h"
#include "list.h"
#include "../util/host_fs/filecache_schema.h"
page_struct_t *pagecache_map;
unsigned char *pc_startaddr;

static LIST_HEAD(free_list);
static LIST_HEAD(active_list);
static LIST_HEAD(inactive_list);


/*********************** local function *************************/
static int page_init(page_struct_t *p)
{
	p->next=p->prev=NULL;
	p->count.counter=0;
	p->flags=0;
	p->inode=NULL;
	p->offset=0;
	INIT_LIST_HEAD(&(p->lru_list));
	INIT_LIST_HEAD(&(p->list));
	return 1;
}
/***************************** API function **********************/

int pc_init(unsigned char *start_addr,unsigned long len)
{
	int total_pages,reserved_size;
	page_struct_t *p;
	int i;

	pc_startaddr=(unsigned char *)start_addr;
	total_pages=len/PC_PAGESIZE;
	reserved_size=sizeof(fileCache_t)+sizeof(page_struct_t)*total_pages;
	pagecache_map=(page_struct_t *)(start_addr + sizeof(fileCache_t));
	ut_memset(pagecache_map, 0, sizeof(page_struct_t)*total_pages);

	for( i=0; i<total_pages; i++)
	{
		p=pagecache_map+i;
		atomic_set(&p->count, 0);
		if ( i < (reserved_size/PC_PAGESIZE))
		{
			p->flags =  (1 << PG_reserved);
		}else
		{
			list_add(&p->lru_list,&free_list);
		}

	}
	ut_printf(" startaddr: %x totalpages:%d reserved size:%d \n",pc_startaddr,total_pages,reserved_size);	
	return 1;
}
struct page *pc_getInodePage(struct inode *inode,unsigned long offset)
{
        struct list_head *p;
	struct page *page;
	int i;
        unsigned long page_offset=(offset/PC_PAGESIZE)*PC_PAGESIZE ;

	i=0;
        list_for_each(p, &(inode->page_list)) {
                page=list_entry(p, struct page, list);
		i++;
		ut_printf("get page address: %x %d \n",page,i);
                if (page->offset == page_offset)
                {
                        return page;
                }
        } 
        return NULL;

}
int pc_insertInodePage(struct inode *inode,struct page *page)
{
        struct list_head *p;
	struct page *tmp_page;
	int ret=0;

	if (page->offset > inode->length) return ret;
        list_for_each(p, &(inode->page_list))
	 {
                tmp_page=list_entry(p, struct page, list);
		ut_printf("insert page address: %x \n",tmp_page);
                if (page->offset < tmp_page->offset)
                {
			inode->nrpages++;
			page->inode=inode;
                        list_add(&page->list, &tmp_page->list); 
			ret=1;
			goto last;
                } 
        } 
	if (ret == 0)
	{
		inode->nrpages++;
                list_add_tail(&page->list, &inode->page_list); 
		ret=1;
	}
last:
        return ret;

}

int pc_putFreePage(struct page *page) 
{
	list_add_tail(&page->lru_list, &free_list); 
	return 1;
}

page_struct_t *pc_getFreePage()
{
	struct list_head *node;
	page_struct_t *p;
	int pn=0;

	node=free_list.next;
	if (node == &free_list) return 0;

	list_del(node); /* delete the node from free list */
        p=list_entry(node, struct page, lru_list);
	page_init(p);
	return p;
}

