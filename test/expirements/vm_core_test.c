
#define MAX_TEST_PAGES 20
unsigned long test_pages[MAX_TEST_PAGES];
static int start_polluting_memory=0;
static struct addr_list dirty_page_list;

int generate_vm_core_thread() {
	/*  create some memory that will be volatile */
	int i, ret;
	unsigned long start_addr, end_addr;

	test_pages[0] = mm_getFreePages(MEM_CLEAR, 5); /* create 1 big chunk of memory of size 32*4k */

	for (i = 0; i < MAX_TEST_PAGES; i++) { /* in to 4k chunks ,so that indvidual pages can be touched */
		test_pages[i] = test_pages[0] + PAGE_SIZE * i;
	}

	/* 1.clear the dirty bits in page table(exclude page cache) and start the memory_polluting_thread to pollute page */
	start_addr = test_pages[0];
	end_addr = test_pages[MAX_TEST_PAGES - 1] + PAGE_SIZE;

	ret = ar_scanPtes((unsigned long) start_addr, (unsigned long) end_addr, 0,
			&dirty_page_list); /* this function  clears  the dirty bits*/

	start_polluting_memory = 1;
	/* 2.copy relavent  pages to disk, this step should take atleast few seconds say 5 to 15 seconds. */

	while (start_polluting_memory == 1)
		;
	/* 3.check the pagetable for the volatile pages and print it. */

	dirty_page_list.total = 0;
	ret = ar_scanPtes((unsigned long) start_addr, (unsigned long) end_addr, 0,
			&dirty_page_list);
	for (i = 0; i < dirty_page_list.total; i++) {
		ut_printf("Page Dirted : addr :%x\n", dirty_page_list.addr[i]);
	}
	mm_putFreePages(test_pages[0], 5);
	SYS_sc_exit(1);
}

int memory_pollute_thread() {
	int i;
	/* 1. wait till we get signal from the generate_vm_core thread */
	while (start_polluting_memory == 0)
		;

	/* 2. start polluting memory */
	for (i = 0; i < MAX_TEST_PAGES; i = i + 3) {
		unsigned char *p = test_pages[i];
		*p = 1;
		ut_printf("Touching the page-no:%d with addr :%x\n",i, test_pages[i]);
	}
	start_polluting_memory = 0;
	SYS_sc_exit(1);
	return 1;

}
/* Output of the Test look like this:

Touching the page-no:0 with addr :7ff40000
Touching the page-no:3 with addr :7ff43000
Touching the page-no:6 with addr :7ff46000
Touching the page-no:9 with addr :7ff49000
Touching the page-no:12 with addr :7ff4c000
Touching the page-no:15 with addr :7ff4f000
Touching the page-no:18 with addr :7ff52000
SYSCALL(b :0 uip: 0) sys exit : status:1
freeing the mm :7fffa080 counter:7
Flushing entire page table : pages scan:20 last addr:7ff54000
Page Dirted : addr :7ff40000
Page Dirted : addr :7ff43000
Page Dirted : addr :7ff46000
Page Dirted : addr :7ff49000
Page Dirted : addr :7ff4c000
Page Dirted : addr :7ff4f000
Page Dirted : addr :7ff52000
SYSCALL(a :0 uip: 0) sys exit : status:1
freeing the mm :7fffa080 counter:6
 *
 */
