#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tree.hh"

/* TODO-1: currently it return memory only memory, it can be from secondary storage or particualar area of memory */
struct node *binary_tree::malloc_node( char *key, unsigned long value){
	struct node *nodep=(struct node *)malloc(sizeof(struct node ));

	strcpy(nodep->key,(const char *)key);
	nodep->value = value;
	nodep->left=0;
	nodep->right=0;
	return nodep;
}
/* TODO-1:  same as malloc_node*/
int binary_tree::free_node(struct node *node){
	if (node != 0){
		free(node);
		return SUCCESS;
	}
	return FAIL;
}


struct node *binary_tree::search_lastnode( char *key,int *cmp_value){
	struct node *next_node;
	struct node *curr_node =root_node;

	while (curr_node != 0){
		*cmp_value= strcmp((const char *)key,(const char *)curr_node->key);
		if (*cmp_value < 0){
			next_node = curr_node->left;
		}else if (*cmp_value > 0){
			next_node = curr_node->right;
		}else {
			return curr_node;
		}
		if (next_node == 0){
			return curr_node;
		}
		curr_node = next_node;
	}
	return 0;
}

int binary_tree::search( char *key, unsigned long *valuep){
	struct node *curr_node;
	int cmp_value;

	curr_node = search_lastnode(key,&cmp_value);
	if (curr_node!=0 && cmp_value==0){
		return SUCCESS;
	}
	return FAIL;
}

int binary_tree::insert( char *key , unsigned long value){
	struct node *curr_node,*new_node;
	int cmp_value;

	new_node = malloc_node(key, value);
	if (new_node == 0) {
		return -2;
	}
	if (root_node != 0) {
		curr_node = search_lastnode(key, &cmp_value);
		if (curr_node == 0 || cmp_value==0) {
			free_node(new_node);
			return -1;
		}
	}else{
		root_node = new_node;
		return SUCCESS;
	}
	if (cmp_value < 0) {
		curr_node->left = new_node;
	} else {
		curr_node->right = new_node;
	}

	return SUCCESS;
}

int binary_tree::remove( char *key){
	struct node *curr_node;
	int cmp_value;

	curr_node = search_lastnode(key,&cmp_value);
	if (curr_node!=0 && cmp_value==0){
		/* TODO : need to remove the curr_node */
	}
	return FAIL;
}
