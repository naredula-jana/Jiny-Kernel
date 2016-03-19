#ifndef _ASM_X86_FUTEX_H
#define _ASM_X86_FUTEX_H

//#ifdef __KERNEL__

#if 0
#include <linux/futex.h>
#include <linux/uaccess.h>

#include <asm/asm.h>
#include <asm/errno.h>
#include <asm/processor.h>
#include <asm/smap.h>
#endif
#define u32 uint32_t
#define __stringify_1(x...)     #x
 #define __stringify(x...)       __stringify_1(x)

 #define b_replacement(num)      "664"#num
  #define e_replacement(num)      "665"#num

  #define alt_end_marker          "663"
  #define alt_slen                "662b-661b"
  #define alt_pad_len             alt_end_marker"b-662b"
  #define alt_total_slen          alt_end_marker"b-661b"
  #define alt_rlen(num)           e_replacement(num)"f-"b_replacement(num)"f"
#if 0
#define _ASM_EXTABLE(from,to)                                  \
          .pushsection "__ex_table","a" ;                         \
          .balign 8 ;                                             \
          .long (from) - . ;                                      \
          .long (to) - . ;                                        \
          .popsection
#endif

# define _ASM_EXTABLE(from,to)                                  \
          " .pushsection \"__ex_table\",\"a\"\n"                  \
          " .balign 8\n"                                          \
          " .long (" #from ") - .\n"                              \
          " .long (" #to ") - .\n"                                \
          " .popsection\n"



#define ALTINSTR_ENTRY(feature, num)                                          \
         " .long 661b - .\n"                             /* label           */ \
         " .long " b_replacement(num)"f - .\n"           /* new instruction */ \
         " .word " __stringify(feature) "\n"             /* feature bit     */ \
         " .byte " alt_total_slen "\n"                   /* source len      */ \
         " .byte " alt_rlen(num) "\n"                    /* replacement len */ \
         " .byte " alt_pad_len "\n"                      /* pad len */

 #define ALTINSTR_REPLACEMENT(newinstr, feature, num)    /* replacement */     \
         b_replacement(num)":\n\t" newinstr "\n" e_replacement(num) ":\n\t"


 #define __OLDINSTR(oldinstr, num)                                       \
          "661:\n\t" oldinstr "\n662:\n"                                  \
          ".skip -(((" alt_rlen(num) ")-(" alt_slen ")) > 0) * "          \
                  "((" alt_rlen(num) ")-(" alt_slen ")),0x90\n"

 #define OLDINSTR(oldinstr, num)                                         \
         __OLDINSTR(oldinstr, num)                                       \
         alt_end_marker ":\n"

#define ALTERNATIVE(oldinstr, newinstr, feature)                        \
         OLDINSTR(oldinstr, 1)                                           \
         ".pushsection .altinstructions,\"a\"\n"                         \
         ALTINSTR_ENTRY(feature, 1)                                      \
         ".popsection\n"                                                 \
         ".pushsection .altinstr_replacement, \"ax\"\n"                  \
         ALTINSTR_REPLACEMENT(newinstr, feature, 1)                      \
         ".popsection"
#define X86_FEATURE_SMAP        ( 9*32+20) /* Supervisor Mode Access Prevention */
#define __ASM_CLAC      .byte 0x0f,0x01,0xca
#define __ASM_STAC      .byte 0x0f,0x01,0xcb
#define ASM_CLAC \
	 ALTERNATIVE ("",__stringify(__ASM_CLAC), X86_FEATURE_SMAP)

#define ASM_STAC \
          ALTERNATIVE ("", __stringify(__ASM_STAC), X86_FEATURE_SMAP)

 #define FUTEX_OP_SET            0       /* *(int *)UADDR2 = OPARG; */
#define FUTEX_OP_ADD            1       /* *(int *)UADDR2 += OPARG; */
 #define FUTEX_OP_OR             2       /* *(int *)UADDR2 |= OPARG; */
 #define FUTEX_OP_ANDN           3       /* *(int *)UADDR2 &= ~OPARG; */
 #define FUTEX_OP_XOR            4       /* *(int *)UADDR2 ^= OPARG; */

 #define FUTEX_OP_OPARG_SHIFT    8       /* Use (1 << OPARG) instead of OPARG.  */

 #define FUTEX_OP_CMP_EQ         0       /* if (oldval == CMPARG) wake */
 #define FUTEX_OP_CMP_NE         1       /* if (oldval != CMPARG) wake */
 #define FUTEX_OP_CMP_LT         2       /* if (oldval < CMPARG) wake */
 #define FUTEX_OP_CMP_LE         3       /* if (oldval <= CMPARG) wake */
 #define FUTEX_OP_CMP_GT         4       /* if (oldval > CMPARG) wake */
 #define FUTEX_OP_CMP_GE         5       /* if (oldval >= CMPARG) wake */

#define __futex_atomic_op1(insn, ret, oldval, uaddr, oparg)	\
	asm volatile("\t" ASM_STAC "\n"				\
		     "1:\t" insn "\n"				\
		     "2:\t" ASM_CLAC "\n"			\
		     "\t.section .fixup,\"ax\"\n"		\
		     "3:\tmov\t%3, %1\n"			\
		     "\tjmp\t2b\n"				\
		     "\t.previous\n"				\
		     _ASM_EXTABLE(1b, 3b)			\
		     : "=r" (oldval), "=r" (ret), "+m" (*uaddr)	\
		     : "i" (-EFAULT), "0" (oparg), "1" (0))

#define __futex_atomic_op2(insn, ret, oldval, uaddr, oparg)	\
	asm volatile("\t" ASM_STAC "\n"				\
		     "1:\tmovl	%2, %0\n"			\
		     "\tmovl\t%0, %3\n"				\
		     "\t" insn "\n"				\
		     "2:\t" LOCK_PREFIX "cmpxchgl %3, %2\n"	\
		     "\tjnz\t1b\n"				\
		     "3:\t" ASM_CLAC "\n"			\
		     "\t.section .fixup,\"ax\"\n"		\
		     "4:\tmov\t%5, %1\n"			\
		     "\tjmp\t3b\n"				\
		     "\t.previous\n"				\
		     _ASM_EXTABLE(1b, 4b)			\
		     _ASM_EXTABLE(2b, 4b)			\
		     : "=&a" (oldval), "=&r" (ret),		\
		       "+m" (*uaddr), "=&r" (tem)		\
		     : "r" (oparg), "i" (-EFAULT), "1" (0))

static inline int futex_atomic_op_inuser(int encoded_op, u32  *uaddr)
{
	int op = (encoded_op >> 28) & 7;
	int cmp = (encoded_op >> 24) & 15;
	int oparg = (encoded_op << 8) >> 20;
	int cmparg = (encoded_op << 20) >> 20;
	int oldval = 0, ret, tem;

	if (encoded_op & (FUTEX_OP_OPARG_SHIFT << 28))
		oparg = 1 << oparg;

//	if (!access_ok(VERIFY_WRITE, uaddr, sizeof(u32)))
//		return -EFAULT;

	//TODO: pagefault_disable();

	switch (op) {
	case FUTEX_OP_SET:
		__futex_atomic_op1("xchgl %0, %2", ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_ADD:
		__futex_atomic_op1(LOCK_PREFIX "xaddl %0, %2", ret, oldval,
				   uaddr, oparg);
		break;
	case FUTEX_OP_OR:
		__futex_atomic_op2("orl %4, %3", ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_ANDN:
		__futex_atomic_op2("andl %4, %3", ret, oldval, uaddr, ~oparg);
		break;
	case FUTEX_OP_XOR:
		__futex_atomic_op2("xorl %4, %3", ret, oldval, uaddr, oparg);
		break;
	default:
		ret = -ENOSYS;
	}

	//TODO: pagefault_enable();

	if (!ret) {
		switch (cmp) {
		case FUTEX_OP_CMP_EQ:
			ret = (oldval == cmparg);
			break;
		case FUTEX_OP_CMP_NE:
			ret = (oldval != cmparg);
			break;
		case FUTEX_OP_CMP_LT:
			ret = (oldval < cmparg);
			break;
		case FUTEX_OP_CMP_GE:
			ret = (oldval >= cmparg);
			break;
		case FUTEX_OP_CMP_LE:
			ret = (oldval <= cmparg);
			break;
		case FUTEX_OP_CMP_GT:
			ret = (oldval > cmparg);
			break;
		default:
			ret = -ENOSYS;
		}
	}
	return ret;
}
/*
static inline int futex_atomic_cmpxchg_inatomic(u32 *uval, u32  *uaddr,
						u32 oldval, u32 newval)
{
	return user_atomic_cmpxchg_inatomic(uval, uaddr, oldval, newval);
}
*/
//#endif
#endif /* _ASM_X86_FUTEX_H */
