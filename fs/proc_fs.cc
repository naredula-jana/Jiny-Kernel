/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   fs/proc_fs.cc
*
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*/
#include "jdevice.h"
#include "file.hh"
extern "C" {
#include "vfs.h"
#include "mm.h"
#include "common.h"
enum {
	PROC_INVALID=0,
	PROC_STAT=1,
	PROC_MAP=2,
	PROC_MAIN_DIR=3,
	PROC_PID_DIR=4,
};
struct procfs_cmd{
	int pid;
	int cmd_type;
};

class proc_fs :public filesystem {
public:
	 int open(fs_inode *inode, int flags, int mode);
	 int lseek(struct file *file,  unsigned long offset, int whence);
	 long write(fs_inode *inode, uint64_t offset, unsigned char *buff, unsigned long len);
	 long read(fs_inode *inode, uint64_t offset,  unsigned char *buff, unsigned long len, int flags);
	 long readDir(fs_inode *inode, struct dirEntry *dir_ptr, unsigned long dir_max, int *offset);
	 int remove(fs_inode *inode);
	 int stat(fs_inode *inode, struct fileStat *stat);
	 int close(fs_inode *inodep);
	 int fdatasync(fs_inode *inodep);
	 int setattr(fs_inode *inode, uint64_t size);//TODO : currently used for truncate, later need to expand
	 int unmount();
	 void set_mount_pnt(unsigned char *mnt_pnt);
	 void print_stat();
};

static int get_cmd(unsigned char *filename, struct procfs_cmd *cmd){
	int i,j,k;
	unsigned char token[100];
	int first_token=1;

/* tokenise */
	k=0;
	j=0;
	cmd->cmd_type = PROC_INVALID;
	//ut_printf(" procfs cmd filename :%s:\n",filename);
	for (i=0; filename[i]!='\0'&& i<100;i++){
		if (filename[i]=='/' || filename[i+1]=='\0'){
			k++;
			if (filename[i+1]=='\0'){
				token[j]=filename[i];
				j++;
			}
			token[j]='\0';
	//		ut_printf("procfs token :%s:  k=%d\n",token,k);
			if (ut_strcmp(token,"proc")==0 && first_token==1){
				k=1;
				first_token=0;
				j=0;
				if (filename[i+1]=='\0'){
					cmd->cmd_type = PROC_MAIN_DIR;
					return JSUCCESS;
				}
				continue;
			}
			if (k==2){
				cmd->pid=ut_atoi(token, FORMAT_DECIMAL);
				if (filename[i+1]=='\0'){
					cmd->cmd_type = PROC_PID_DIR;
					return JSUCCESS;
				}
			}else if (k==3){
				if (ut_strcmp(token,"stat")==0){
					cmd->cmd_type = PROC_STAT;
					return JSUCCESS;
				}else if (ut_strcmp(token,"map")==0){
					cmd->cmd_type = PROC_MAP;
					return JSUCCESS;
				}
				return JFAIL;
			}
			j=0;
		}else{
			token[j]=filename[i];
			j++;
		}
	}
	return JFAIL;

}
int proc_fs::lseek(struct file *file, unsigned long offset, int whence) {

}
long proc_fs::write(fs_inode *inodep, uint64_t offset, unsigned char *buff, unsigned long len) {
	return 0;
}
extern int Jcmd_maps(char *arg1, char *arg2);
/*
 *        /proc/[pid]/stat
              Status information about the process.  This is used by ps(1).
              It is defined in /usr/src/linux/fs/proc/array.c.

              The fields, in order, with their proper scanf(3) format
              specifiers, are:

              pid %d      (1) The process ID.

              comm %s     (2) The filename of the executable, in
                          parentheses.  This is visible whether or not the
                          executable is swapped out.

              state %c    (3) One character from the string "RSDZTW" where R
                          is running, S is sleeping in an interruptible
                          wait, D is waiting in uninterruptible disk sleep,
                          Z is zombie, T is traced or stopped (on a signal),
                          and W is paging.

              ppid %d     (4) The PID of the parent.

              pgrp %d     (5) The process group ID of the process.

              session %d  (6) The session ID of the process.

              tty_nr %d   (7) The controlling terminal of the process.  (The
                          minor device number is contained in the
                          combination of bits 31 to 20 and 7 to 0; the major
                          device number is in bits 15 to 8.)

              tpgid %d    (8) The ID of the foreground process group of the
                          controlling terminal of the process.

              flags %u (%lu before Linux 2.6.22)
                          (9) The kernel flags word of the process.  For bit
                          meanings, see the PF_* defines in the Linux kernel
                          source file include/linux/sched.h.  Details depend
                          on the kernel version.

              minflt %lu  (10) The number of minor faults the process has
                          made which have not required loading a memory page
                          from disk.

              cminflt %lu (11) The number of minor faults that the process's
                          waited-for children have made.

              majflt %lu  (12) The number of major faults the process has
                          made which have required loading a memory page
                          from disk.

              cmajflt %lu (13) The number of major faults that the process's
                          waited-for children have made.

              utime %lu   (14) Amount of time that this process has been
                          scheduled in user mode, measured in clock ticks
                          (divide by sysconf(_SC_CLK_TCK)).  This includes
                          guest time, guest_time (time spent running a
                          virtual CPU, see below), so that applications that
                          are not aware of the guest time field do not lose
                          that time from their calculations.

              stime %lu   (15) Amount of time that this process has been
                          scheduled in kernel mode, measured in clock ticks
                          (divide by sysconf(_SC_CLK_TCK)).

              cutime %ld  (16) Amount of time that this process's waited-for
                          children have been scheduled in user mode,
                          measured in clock ticks (divide by
                          sysconf(_SC_CLK_TCK)).  (See also times(2).)  This
                          includes guest time, cguest_time (time spent
                          running a virtual CPU, see below).

              cstime %ld  (17) Amount of time that this process's waited-for
                          children have been scheduled in kernel mode,
                          measured in clock ticks (divide by
                          sysconf(_SC_CLK_TCK)).

              priority %ld
                          (18) (Explanation for Linux 2.6) For processes
                          running a real-time scheduling policy (policy
                          below; see sched_setscheduler(2)), this is the
                          negated scheduling priority, minus one; that is, a
                          number in the range -2 to -100, corresponding to
                          real-time priorities 1 to 99.  For processes
                          running under a non-real-time scheduling policy,
                          this is the raw nice value (setpriority(2)) as
                          represented in the kernel.  The kernel stores nice
                          values as numbers in the range 0 (high) to 39
                          (low), corresponding to the user-visible nice
                          range of -20 to 19.

                          Before Linux 2.6, this was a scaled value based on
                          the scheduler weighting given to this process.

              nice %ld    (19) The nice value (see setpriority(2)), a value
                          in the range 19 (low priority) to -20 (high
                          priority).

              num_threads %ld
                          (20) Number of threads in this process (since
                          Linux 2.6).  Before kernel 2.6, this field was
                          hard coded to 0 as a placeholder for an earlier
                          removed field.

              itrealvalue %ld
                          (21) The time in jiffies before the next SIGALRM
                          is sent to the process due to an interval timer.
                          Since kernel 2.6.17, this field is no longer
                          maintained, and is hard coded as 0.

              starttime %llu (was %lu before Linux 2.6)
                          (22) The time the process started after system
                          boot.  In kernels before Linux 2.6, this value was
                          expressed in jiffies.  Since Linux 2.6, the value
                          is expressed in clock ticks (divide by
                          sysconf(_SC_CLK_TCK)).

              vsize %lu   (23) Virtual memory size in bytes.

              rss %ld     (24) Resident Set Size: number of pages the
                          process has in real memory.  This is just the
                          pages which count toward text, data, or stack
                          space.  This does not include pages which have not
                          been demand-loaded in, or which are swapped out.

              rsslim %lu  (25) Current soft limit in bytes on the rss of the
                          process; see the description of RLIMIT_RSS in
                          getrlimit(2).

              startcode %lu
                          (26) The address above which program text can run.

              endcode %lu (27) The address below which program text can run.

              startstack %lu
                          (28) The address of the start (i.e., bottom) of
                          the stack.

              kstkesp %lu (29) The current value of ESP (stack pointer), as
                          found in the kernel stack page for the process.

              kstkeip %lu (30) The current EIP (instruction pointer).

              signal %lu  (31) The bitmap of pending signals, displayed as a
                          decimal number.  Obsolete, because it does not
                          provide information on real-time signals; use
                          /proc/[pid]/status instead.

              blocked %lu (32) The bitmap of blocked signals, displayed as a
                          decimal number.  Obsolete, because it does not
                          provide information on real-time signals; use
                          /proc/[pid]/status instead.

              sigignore %lu
                          (33) The bitmap of ignored signals, displayed as a
                          decimal number.  Obsolete, because it does not
                          provide information on real-time signals; use
                          /proc/[pid]/status instead.

              sigcatch %lu
                          (34) The bitmap of caught signals, displayed as a
                          decimal number.  Obsolete, because it does not
                          provide information on real-time signals; use
                          /proc/[pid]/status instead.

              wchan %lu   (35) This is the "channel" in which the process is
                          waiting.  It is the address of a location in the
                          kernel where the process is sleeping.  The
                          corresponding symbolic name can be found in
                          /proc/[pid]/wchan.

              nswap %lu   (36) Number of pages swapped (not maintained).

              cnswap %lu  (37) Cumulative nswap for child processes (not
                          maintained).

              exit_signal %d (since Linux 2.1.22)
                          (38) Signal to be sent to parent when we die.

              processor %d (since Linux 2.2.8)
                          (39) CPU number last executed on.

              rt_priority %u (since Linux 2.5.19; was %lu before Linux
              2.6.22)
                          (40) Real-time scheduling priority, a number in
                          the range 1 to 99 for processes scheduled under a
                          real-time policy, or 0, for non-real-time
                          processes (see sched_setscheduler(2)).

              policy %u (since Linux 2.5.19; was %lu before Linux 2.6.22)
                          (41) Scheduling policy (see
                          sched_setscheduler(2)).  Decode using the SCHED_*
                          constants in linux/sched.h.

              delayacct_blkio_ticks %llu (since Linux 2.6.18)
                          (42) Aggregated block I/O delays, measured in
                          clock ticks (centiseconds).

              guest_time %lu (since Linux 2.6.24)
                          (43) Guest time of the process (time spent running
                          a virtual CPU for a guest operating system),
                          measured in clock ticks (divide by
                          sysconf(_SC_CLK_TCK)).

              cguest_time %ld (since Linux 2.6.24)
                          (44) Guest time of the process's children,
                          measured in clock ticks (divide by
                          sysconf(_SC_CLK_TCK)).
 */
long proc_fs::read(fs_inode *inodep, uint64_t offset, unsigned char *buff, unsigned long len_arg, int flags) {
	struct procfs_cmd cmd;
	int len=0;
	struct task_struct *task;

	//ut_printf(" procfs read \n");
	if (get_cmd(inodep->filename,&cmd)== JSUCCESS){
		if (cmd.cmd_type == PROC_MAP){
			unsigned char pid[20];
			ut_snprintf(pid,20,"%x",cmd.pid);
			Jcmd_maps(pid,0);
			len = ut_strlen(pid);
		} else 	if (cmd.cmd_type == PROC_STAT){
			unsigned long flags;
			struct list_head *pos;
			int found=0;
			spin_lock_irqsave(&g_global_lock, flags);
			if (cmd.pid != 0) {
				list_for_each(pos, &g_task_queue.head) {
					task = list_entry(pos, struct task_struct, task_queue);
					if (task->task_id == cmd.pid) {
						found=1;
						break;
					}
				}
			} else {
				found=1;
				task = g_current_task;
			}
			if (found==1){
				unsigned char state='R';
				unsigned long rss_size = task->mm->stat_page_allocs-task->mm->stat_page_free;
				unsigned long rss_limit = 0xffffffff;
				unsigned long vsize = (task->mm->end_code-task->mm->start_code) + task->mm->brk_len + USERSTACK_LEN;
				len = ut_snprintf(buff,len_arg,"%d (%s) %c %d 0 0 0 0 0 0 0 0 0 0 0 0 0 16 0 1 0 0 ",task->task_id,task->name,state,task->parent_process_pid);
				/* starting from vsize */
				ut_snprintf(&buff[len],len_arg-len,"%d %d %d %d %d %d 0 0 0 0 0 0 0 0 0 0 0 0 0",vsize,rss_size,rss_limit,task->mm->start_code,task->mm->end_code,task->mm->stack_bottom);
			}
			spin_unlock_irqrestore(&g_global_lock, flags);
			len = ut_strlen(buff);
		}
	}
	inodep->fileStat.st_size = len;
	//ut_printf(" pid:%d len:%d \n",cmd.pid,len);
	if (offset > 0) return 0;
	return len;
}
long proc_fs::readDir(fs_inode *inodep, struct dirEntry *dir_p, unsigned long dir_max, int *offset) {
	struct procfs_cmd cmd;
	int total_len,len,i;
	unsigned char *p;

	//ut_printf(" porcfs readdir\n");
	total_len=0;

	if (get_cmd(inodep->filename,&cmd)== JSUCCESS){
		if (cmd.cmd_type == PROC_MAIN_DIR  ){
			unsigned long flags;
			struct list_head *pos;
			struct task_struct *task;

			p=(unsigned char *)dir_p;
			i=0;
			spin_lock_irqsave(&g_global_lock, flags);
			list_for_each(pos, &g_task_queue.head) {
				i++;
				if ( offset && (i-1)< *offset) continue;
				task = list_entry(pos, struct task_struct, task_queue);
				dir_p = (struct dirEntry *)p;
				dir_p->inode_no = task->task_id;
				ut_sprintf(dir_p->filename,"%d",task->task_id);
				len = 2 * (sizeof(unsigned long)) + sizeof(unsigned short) + ut_strlen((unsigned char *)dir_p->filename) + 2;
				dir_p->d_reclen = (len / 8) * 8;
				if ((dir_p->d_reclen) < len)
					dir_p->d_reclen = dir_p->d_reclen + 8;
				p = p + dir_p->d_reclen;
				total_len = total_len + dir_p->d_reclen;

				i++;
			}
			spin_unlock_irqrestore(&g_global_lock, flags);
			if (offset){
				*offset=i;
			}
		}else if ( cmd.cmd_type == PROC_PID_DIR){

		} else {
			return 0;
		}
	}
	return total_len;
}
int proc_fs::remove(fs_inode *inodep) {

}

int proc_fs::stat(fs_inode *inodep, struct fileStat *stat) {
	struct procfs_cmd cmd;
	struct fileStat fs;

	if (get_cmd(inodep->filename,&cmd)== JSUCCESS){
		fs.inode_no = 1;
		fs.type = REGULAR_FILE;
		fs.atime =0x0;
		fs.mtime =0x0;
		fs.mode =0x0;
		fs.blk_size =0x0;
		if (cmd.cmd_type == PROC_MAIN_DIR || cmd.cmd_type == PROC_PID_DIR ){
			fs.type = DIRECTORY_FILE;
		}
		ut_memcpy((unsigned char *)fs_get_stat(inodep,fs.type),(unsigned char *)&fs,sizeof(struct fileStat));
	}
	//ut_printf(" Inside procfs stat :%s: %d \n",inodep->filename,fs.type);
	return 0;
}
int proc_fs::open(fs_inode *inodep,int flags, int mode) {
	struct procfs_cmd cmd;

	if (get_cmd(inodep->filename,&cmd)== JSUCCESS){
		return JSUCCESS;
	}

	return JFAIL;
}
int proc_fs::close(fs_inode *inodep) {
	return 0;
}
int proc_fs::fdatasync(fs_inode *inodep) {
	return 0;
}
int proc_fs::setattr(fs_inode *inode, uint64_t size) { //TODO : currently used for truncate, later need to expand
	return 0;
}
int proc_fs::unmount() {
	return 0;
}
void proc_fs::print_stat(){

}
void proc_fs::set_mount_pnt(unsigned char *mnt_pnt){
/*TODO: nothing todo in procfs */
}
//struct filesystem proc_fs;
static class proc_fs *proc_fs_obj;
int init_procfs(unsigned long unused) {
	proc_fs_obj = jnew_obj(proc_fs);
	fs_registerFileSystem(proc_fs_obj,(unsigned char *)"proc","procfs");
	return 1;
}

}
