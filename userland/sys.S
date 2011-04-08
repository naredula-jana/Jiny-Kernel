
.global __syscall
.type __syscall STT_FUNC

__syscall:
        movslq 8(%rsp), %rax /* setup rax to syscall number */
        movq %rcx, %r10
        syscall
        ret
