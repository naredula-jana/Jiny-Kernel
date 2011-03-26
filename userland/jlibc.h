#define SYS_printf 1
#define SYS_open 2
#define SYS_write 3
#define SYS_read 4
#define SYS_close 5
#define SYS_fadvise 6 
#define SYS_fdatasync 7 

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
_syscall1(int,printf,unsigned long,ptr)
_syscall3(unsigned long,open,unsigned long,ptr,unsigned long , flag,unsigned long, flag2)
_syscall3(int,write,unsigned long,ptr,unsigned long,buff,unsigned long ,len)
_syscall3(int,read,unsigned long,ptr,unsigned long,buff,unsigned long ,len)
_syscall2(int,fadvise,unsigned long,ptr,unsigned long ,len)
_syscall1(int,close,unsigned long,ptr)
_syscall1(int,fdatasync,unsigned long,ptr)

