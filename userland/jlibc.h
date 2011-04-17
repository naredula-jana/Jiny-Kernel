#define SYS_printf 1
#define SYS_open 2
#define SYS_write 3
#define SYS_read 4
#define SYS_close 5
#define SYS_fadvise 6 
#define SYS_fdatasync 7 
#define SYS_lseek 8 
#define SYS_exit 9
#define SYS_execve 10
#define SYS_fork 11
#define SYS_kill 12
#define SYS_clone 13
#define SYS_mmap 14

#define __syscall_return(type, res) \
do { \
        if ((unsigned long)(res) >= (unsigned long)(-125)) { \
                errno = -(res); \
                res = -1; \
        } \
        return (type) (res); \
} while (0)


#define _new_syscall1(type,name,type1,arg1) \
type name(type1 arg1) \
{ \
long __res; \
__asm__ volatile ("syscall" \
        : "=a" (__res) \
        : "0" (SYS_##name),"D" ((long)(arg1))); \
__syscall_return(type,__res); \
}

#define _syscall0(type,name) \
type name() \
{ \
long __res; \
__res=__syscall(0,0, 0, 0, 0, 0, (SYS_##name));\
__syscall_return(type,__res); \
}
#define _syscall1(type,name,type1,arg1) \
type name(type1 arg1) \
{ \
long __res; \
__res=__syscall(arg1,0, 0, 0, 0, 0, (SYS_##name));\
__syscall_return(type,__res); \
}

#define _syscall2(type,name,type1,arg1,type2,arg2) \
type name(type1 arg1,type2 arg2) \
{ \
long __res; \
__res=__syscall(arg1,arg2, 0, 0, 0, 0, (SYS_##name));\
__syscall_return(type,__res); \
}

#define _syscall3(type,name,type1,arg1,type2,arg2,type3,arg3) \
type name(type1 arg1,type2 arg2,type3 arg3) \
{ \
long __res; \
__res=__syscall(arg1,arg2, arg3, 0, 0, 0, (SYS_##name));\
__syscall_return(type,__res); \
}
#define _syscall4(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4) \
type name(type1 arg1,type2 arg2,type3 arg3,type4 arg4) \
{ \
long __res; \
__res=__syscall(arg1,arg2, arg3, arg4, 0, 0, (SYS_##name));\
__syscall_return(type,__res); \
}
#define _syscall6(type,name,type1,arg1,type2,arg2,type3,arg3,type4,arg4,type5,arg5,type6,arg6) \
type name(type1 arg1,type2 arg2,type3 arg3,type4 arg4,type5 arg5 ,type6 arg6) \
{ \
long __res; \
__res=__syscall(arg1,arg2, arg3, arg4,arg5,arg6, (SYS_##name));\
__syscall_return(type,__res); \
}

_syscall1(int,printf,unsigned long,ptr)
_syscall1(int,exit,unsigned long,status)
_syscall3(unsigned long,open,unsigned long,ptr,unsigned long , flag,unsigned long, flag2)
_syscall3(int,write,unsigned long,ptr,unsigned long,buff,unsigned long ,len)
_syscall3(int,read,unsigned long,ptr,unsigned long,buff,unsigned long ,len)
_syscall2(int,fadvise,unsigned long,ptr,unsigned long ,len)
_syscall1(int,close,unsigned long,ptr)
_syscall1(int,fdatasync,unsigned long,ptr)
_syscall3(int,execve,unsigned long,name,unsigned long,argv,unsigned long ,env)
_syscall0(int,fork)
_syscall2(int,kill,unsigned long,pid,unsigned long ,signal)
_syscall4(int,clone,unsigned long,func_ptr,unsigned long ,stack,int ,clone_flags,unsigned long ,args)
_syscall6(int,mmap,unsigned long,addr,unsigned long,length,unsigned long ,proto,unsigned long, flag,unsigned long, fd,unsigned long ,offset)

