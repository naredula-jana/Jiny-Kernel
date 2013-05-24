#include <stdio.h>

main()
{
	               char *newargv[] = { NULL, "hello", "world", NULL };
				                  char *newenviron[] = { NULL };
 execve("./y", newargv, newenviron);
}
