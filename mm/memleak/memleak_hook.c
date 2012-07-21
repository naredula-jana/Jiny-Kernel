
#define MAX_EARLY_ALLOCS 10240
static struct {
	unsigned char *ptr;
	int size;
	int type;
} early_alloc_log[MAX_EARLY_ALLOCS];
static int early_alloc_count = 0;

enum {
	TOOL_DISABLED = 0, TOOL_EARLY_START = 1, TOOL_ACTIVE = 2
};
static int early_alloc_state = TOOL_EARLY_START;
static void (*p_memleak_alloc)(const void *ptr, int size, int type, void *cachep);
static void (*p_memleak_free)(const void *ptr, void *cachep);
static void (*p_memleak_update)(const void *ptr, unsigned long type);
void memleakHook_update(unsigned char *ptr, unsigned long type) {
	if (early_alloc_state == TOOL_DISABLED)
		return;
	if (early_alloc_state == TOOL_ACTIVE) {
		p_memleak_update(ptr,  type);
		return;
	}
}

void memleakHook_alloc(unsigned char *ptr, int size, int type, void *cachep) {
	if (early_alloc_state == TOOL_DISABLED)
		return;
	if (early_alloc_state == TOOL_ACTIVE) {
		p_memleak_alloc(ptr, size, type, cachep);
		return;
	}

	if (early_alloc_count >= MAX_EARLY_ALLOCS) {
		early_alloc_state = TOOL_DISABLED;
	//	BUG();
		goto last;
	}

	early_alloc_log[early_alloc_count].ptr = ptr;
	early_alloc_log[early_alloc_count].size = size;
	early_alloc_log[early_alloc_count].type = type;
	if (size == 0) {
	//	BUG();
	}
	early_alloc_count++;

	last: return;
}
void memleakHook_free(unsigned char *ptr, void *cachep) {
	if (early_alloc_state == TOOL_DISABLED)
		return;
	if (early_alloc_state == TOOL_ACTIVE) {
		p_memleak_free(ptr, cachep);
		return;
	}
	if (early_alloc_count == 0)
		return; /* This is workaround for the way free pages are created */
	memleakHook_alloc(ptr, 0, 0, 0);
	return;
}

int memleakHook_copyFromEarlylog(void (*palloc)(const void *ptr, int size, int type, void *cachep),
		void (*pfree)(const void *ptr, void *cachep),void (*pupdate)(const void *ptr, unsigned long type)) {
	int i;

	if (early_alloc_state == TOOL_DISABLED)
		return 0;
	p_memleak_alloc = palloc;
	p_memleak_free = pfree;
	p_memleak_update = pupdate;
	for (i = 0; i < early_alloc_count; i++) {
		if (early_alloc_log[i].size == 0) {
			p_memleak_free(early_alloc_log[i].ptr, 0);
		} else {
			p_memleak_alloc(early_alloc_log[i].ptr, early_alloc_log[i].size, early_alloc_log[i].type, 0);
		}
		early_alloc_log[i].ptr = 0;
		early_alloc_log[i].size = 0;
		early_alloc_log[i].type = 0;
	}
	early_alloc_state = TOOL_ACTIVE;
	return 1;
}
int memleakHook_disable(){
	early_alloc_state = TOOL_DISABLED;
	p_memleak_alloc = 0;
	p_memleak_free =0 ;
	return 1;
}
