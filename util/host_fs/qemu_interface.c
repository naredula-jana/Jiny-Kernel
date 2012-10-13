#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/kvm.h>
int vm_fd=0;
#if 0
struct kvm_irq_level {
	union {
		unsigned int irq;     /* GSI */
		int status;  /* not used for KVM_IRQ_LEVEL */
	};
	unsigned int level;           /* 0 or 1 */
};
#endif
int send_vm_irq() {
	int ret;
	if (vm_fd == 0)
		return 0;
	struct kvm_irq_level irq_level;

	irq_level.irq= 11;
	irq_level.level = 1;
//#define KVM_IRQ_LINE 0xc008ae67
	ret = ioctl(vm_fd, KVM_IRQ_LINE, &irq_level);
	printf(" first ioctl ret: %d \n", ret);
	irq_level.irq= 11;
	irq_level.level = 0;
	ret = ioctl(vm_fd, KVM_IRQ_LINE, &irq_level);
	printf(" second ioctl ret: %d \n", ret);
}
int recv_qemu_msg(int sock)
{
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr *cmptr;
	struct cmsghdr *cmsg;
	size_t len;
	size_t msg_size = sizeof(int);
	//char control[CMSG_SPACE(msg_size)];
	char control[1024],data[1024];
	long posn = 0;
	int added,pos,*posp;
	int i,fd;

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);
	msg.msg_flags = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	iov[0].iov_base = data;
	iov[0].iov_len = sizeof(data)-1;
	len = recvmsg(sock, &msg, 0);
printf("RECVED from QEMU \n");
	added=0;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {

		if (cmsg->cmsg_len != CMSG_LEN(sizeof(int)) ||
				cmsg->cmsg_level != SOL_SOCKET ||
				cmsg->cmsg_type != SCM_RIGHTS)
		{
			fd = *((int *)CMSG_DATA(cmsg));
			printf("DELETING the fd :%d \n",fd);
			continue;
		}
		posp=&data;
		pos=*posp;
		fd = *((int *)CMSG_DATA(cmsg));
		vm_fd=dup(fd);
		printf("Got vmfd from the QEMU :%d \n",vm_fd);
		break;
	}
}
int create_listening_socket(char *path) {

    struct sockaddr_un local;
    int len, conn_socket;

    if ((conn_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, path);
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(conn_socket, (struct sockaddr *)&local, len) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(conn_socket, 5) == -1) {
        perror("listen");
        exit(1);
    }

    return conn_socket;

}
void qemu_thread(void *arg)
{
    struct sockaddr_un remote;
    socklen_t t = sizeof(remote);

	int server_socket=create_listening_socket("/tmp/ivshmem_socket_qemu");
	while(1)
	{
	    int vm_sock = accept(server_socket, (struct sockaddr *)&remote, &t);
		recv_qemu_msg(vm_sock);
	}
}
/****************************************/

