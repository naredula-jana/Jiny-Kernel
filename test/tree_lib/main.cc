using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tree.hh"

main(){
	binary_tree *bt = new binary_tree();
	int ret;
	unsigned long value;

	/* testing insert */
	ret = bt->insert((char *)"boy",111);
	printf("insert boy ret:%d  \n",ret);

	ret = bt->insert((char *)"apple",122);
	printf("insert apple ret:%d  \n",ret);

	ret = bt->insert((char *)"cat",133);
	printf("insert cat ret:%d  \n",ret);

	ret = bt->insert((char *)"apple",144);
	printf("insert apple ret:%d  \n",ret);

	/* testing serach */

	ret = bt->search((char *)"cat",&value);
	printf("search cat :%d\n",ret);

	ret = bt->search((char *)"apple",&value);
	printf("search apple :%d\n",ret);

	ret = bt->search((char *)"dog",&value);
	printf("search dog :%d\n",ret);



}

