#define SUCCESS 1
#define FAIL 0


#define MAX_KEY_VALUE 100
struct node{
	char key[MAX_KEY_VALUE];
	unsigned long value;
	struct node *left,*right;
};
class binary_tree {
	struct node *root_node;
	struct node *search_lastnode( char *key,int *cmp_value);
	struct node *malloc_node( char *key, unsigned long value);
	int free_node(struct node *node);

public:
	binary_tree(void){
		root_node=0;
	}
	int search( char *key,unsigned long *valuep);
	int insert( char *key,unsigned long value);
	int remove( char *key);

};
