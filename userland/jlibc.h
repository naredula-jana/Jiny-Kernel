#define SYS_printf 1
#define SYS_nprintf 1
#define SYS_open 2
#define SYS_write 3
#define SYS_read 4
#define SYS_close 5
#define SYS_fadvise 6 
#define SYS_fdatasync 7 
#define SYS_exit 8 

#define __syscall_return(type, res) \
do { \
        if ((unsigned long)(res) >= (unsigned long)(-125)) { \
                errno = -(res); \
                res = -1; \
        } \
        return (type) (res); \
} while (0)

#define _syscall1(type,name,type1,arg1) \
type name(type1 arg1) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
        : "=a" (__res) \
        : "0" (SYS_##name),"b" ((long)(arg1))); \
__syscall_return(type,__res); \
}
#define _syscall2(type,name,type1,arg1,type2,arg2) \
type name(type1 arg1,type2 arg2) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
        : "=a" (__res) \
        : "0" (SYS_##name),"b" ((long)(arg1)),"c" ((long)(arg2))); \
__syscall_return(type,__res); \
}

#define _syscall3(type,name,type1,arg1,type2,arg2,type3,arg3) \
type name(type1 arg1,type2 arg2,type3 arg3) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
        : "=a" (__res) \
        : "0" (SYS_##name),"b" ((long)(arg1)),"c" ((long)(arg2)), \
                  "d" ((long)(arg3))); \
__syscall_return(type,__res); \
}

#define _new_syscall1(type,name,type1,arg1) \
type name(type1 arg1) \
{ \
long __res; \
__asm__ volatile ("syscall" \
        : "=a" (__res) \
        : "0" (SYS_##name),"D" ((long)(arg1))); \
__syscall_return(type,__res); \
}

#define _new_syscall(type,name,type1,arg1) \
type name(type1 arg1) \
{ \
long __res; \
__res=__syscall(arg1,0, 0, 0, 0, 0, (SYS_##name));\
__syscall_return(type,__res); \
}

#define __syscall0(id)                    __syscall(0, 0, 0, 0, 0, 0, id)
#define __syscall1(id,a0)                 __syscall(a0,0, 0, 0, 0, 0, id)
#define __syscall2(id,a0,a1)              __syscall(a0,a1,0, 0, 0, 0, id)
#define __syscall3(id,a0,a1,a2)           __syscall(a0,a1,a2,0, 0, 0, id)
#define __syscall4(id,a0,a1,a2,a3)         __syscall(a0,a1,a2,a3,0, 0, id)
#define __syscall5(id,a0,a1,a2,a3,a4)      __syscall(a0,a1,a2,a3,a4,0, id)
#define __syscall6(id,a0,a1,a2,a3,a4,a5)   __syscall(a0,a1,a2,a3,a4,a5,id)


/*_new_syscall1(int,printf,unsigned long,ptr) */
_new_syscall(int,printf,unsigned long,ptr)
_new_syscall(int,exit,unsigned long,ptr)
/*_syscall3(unsigned long,open,unsigned long,ptr,unsigned long , flag,unsigned long, flag2)
_syscall3(int,write,unsigned long,ptr,unsigned long,buff,unsigned long ,len)
_syscall3(int,read,unsigned long,ptr,unsigned long,buff,unsigned long ,len)
_syscall2(int,fadvise,unsigned long,ptr,unsigned long ,len)
_syscall1(int,close,unsigned long,ptr)
_syscall1(int,fdatasync,unsigned long,ptr)
*/
