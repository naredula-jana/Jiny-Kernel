


struct hlist_head {
        struct hlist_node *first;
};

struct hlist_node {
       struct hlist_node *next, **pprev;
};
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
 {
         h->next = NULL;
         h->pprev = NULL;
 }
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
 {
         struct hlist_node *first = h->first;
         n->next = first;
         if (first)
                 first->pprev = &n->next;
         h->first = n;
         n->pprev = &h->first;
 }
static inline int hlist_empty(const struct hlist_head *h)
 {
         return !h->first;
 }
static inline void __hlist_del(struct hlist_node *n)
 {
         struct hlist_node *next = n->next;
         struct hlist_node **pprev = n->pprev;
         *pprev = next;
         if (next)
                 next->pprev = pprev;
 }

#ifdef CONFIG_ILLEGAL_POINTER_VALUE
# define POISON_POINTER_DELTA _AC(CONFIG_ILLEGAL_POINTER_VALUE, UL)
#else
# define POISON_POINTER_DELTA 0
#endif

/*
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define LIST_POISON1  ((void *) 0x00100100 + POISON_POINTER_DELTA)
#define LIST_POISON2  ((void *) 0x00200200 + POISON_POINTER_DELTA)

static inline void hlist_del(struct hlist_node *n)
 {
         __hlist_del(n);
         n->next = LIST_POISON1;
         n->pprev = LIST_POISON2;
 }

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

/**
 * hlist_for_each_entry_from - iterate over a hlist continuing from current point
 * @tpos:       the type * to use as a loop cursor.
 * @pos:        the &struct hlist_node to use as a loop cursor.
 * @member:     the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_from(tpos, pos, member)                     \
        for (; pos &&                                                    \
                ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
             pos = pos->next)

#define hlist_for_each_entry_safe(tpos, pos, n, head, member)            \
         for (pos = (head)->first;                                        \
              pos && ({ n = pos->next; 1; }) &&                           \
                 ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
              pos = n)


#define hlist_for_each_entry(tpos, pos, head, member)                    \
         for (pos = (head)->first;                                        \
              pos &&                                                      \
                 ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
              pos = pos->next)




#define max(x, y) ({                            \
          typeof(x) _max1 = (x);                  \
          typeof(y) _max2 = (y);                  \
          (void) (&_max1 == &_max2);              \
          _max1 > _max2 ? _max1 : _max2; })

#define min(x, y) ({                            \
          typeof(x) _min1 = (x);                  \
          typeof(y) _min2 = (y);                  \
          (void) (&_min1 == &_min2);              \
          _min1 < _min2 ? _min1 : _min2; })

extern int memleakHook_disable();
extern int memleak_serious_bug;
#define unlikely(x)     (x) /* TODO */
#define MM_BUG_ON(condition,y) do { if (unlikely((condition)!=0)) {printf("BUGON: %d\n",y);  memleakHook_disable(); memleak_serious_bug=1; goto out;}} while(0)


