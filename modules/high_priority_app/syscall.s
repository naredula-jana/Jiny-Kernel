

jiny_syscalltable:
        /* default: 512 entries */
        .quad   0x0
        .fill   511,8,0
.global my_write
.global my_syscall
my_write:

   mov $0x1,%eax
   CALL *jiny_syscalltable(,%rax,8)
   retq
