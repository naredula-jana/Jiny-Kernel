##  Performance improvements.

### Benchmark-1( Golang in Ring0):
 

**Test Description:** Comparision of performance between [golang14.2 application](https://github.com/naredula-jana/Jiny-Kernel/blob/master/test/golang_test/file.go) in  Ring-0 on Jiny platform versus default golang14.2 on linux x86-64 platform. In this comparision if the application is system call intesive then Ring-0 platform performs much better when compare to linux platform, the reason is system call need lot of cpu resources in switching from ring-3 to ring-0 and vice-versa. If the number of system calls are less then both run at the same speed. The test results shows that for the system call intensive app, golang app completes in 22sec versus 202 on default platform, means it is almost 10X improvent. 

**Golang14.2 in Ring-0:** [Golang Runtime system is modified](https://github.com/naredula-jana/Golang-Ring0/commit/f28f33636e253a59792495bc17727466ef819cf9) to run only in Ring-0 on a Jiny platform. Jiny kernel already supports any runtime systems like Java, Golang,..etc to run in ring-0. 

**Perf Test Results**


 <table border="1" style="width:100%">
  <tr>
    <td><b>Test# </b></td>
    <td><b> Description</b></td>
    <td><b>Throughput</b></td>
    <td><b>Comment</b></td>
  </tr>
  <tr>
    <td>1 </td>
    <td> default Golang14.2 app on linux platform  </td>
    <td> timetaken = 208 sec</td>
    <td>  </td> 
  </tr>
    <tr>
    <td>2 </td>
    <td>  golang14.2 app in  Ring-0 on Jiny platform </td>
    <td> timetaken = 22sec </td>
    <td> almost 10X improvement  </td> 
  </tr>
   
  </table>


Running Golang app in ring-0 on Jiny platform: (Test completed in 22 sec):

```

-->insexe /data/nfile -count=400000000

 Starting golang app :/data/nfile:  args :-count=400000000: 
 Successfull loaded the high priority app
------------------------------
FileApp:  SAMPLE File application count:  400000000
New Start Time:New New  Sunday, 26-Apr-20 11:19:44 UTC
total data count : 12000000000  write count:400000000
End Time: Sunday, 26-Apr-20 11:20:06 UTC

--------------
```

Running default Golang app  on Linux  platform: (Test completed in 208 sec):

```

root:/opt_src/Jiny-Kernel/test/golang_test# ./file -count=400000000
FileApp:  SAMPLE File application count:  400000000
New Start Time:New New  Sunday, 26-Apr-20 16:44:58 IST
total data count : 12000000000  write count:400000000
End Time: Sunday, 26-Apr-20 16:48:26 IST
root:/opt_src/Jiny-Kernel/test/golang_test# 

```

Profiling Data of Golang14.2 in Ring-0. Here both the Jiny kernel and Golang methods profiling data are as follows:

```
    2:t:84 hits:3253(3253:0) (rip=ffffffff80144e1a) idleTask_func -> ffffffff80144cdb (0) 
    3:t:84 hits: 209(209:0) (rip=ffffffff801412da) sc_after_syscall -> ffffffff80141245 (0) 
    4:t:116 hits: 182(182:0) (rip=ffffffff8012d3aa) io_delay -> ffffffff8012d395 (23) 
    5:t:72 hits: 118(118:0) (rip=ffffffff80112427) HP_syscall -> ffffffff80112427 (0) 
    6:t:84 hits: 114(114:0) (rip=ffffffff8015cd69) fs_fd_write -> ffffffff8015cd34 (0) 
    7:t:84 hits:  84(84:0) (rip=ffffffff8015b45b) fs_write -> ffffffff8015b40f (265) 
    8:t:84 hits:  83(83:0) (rip=ffffffff801757ca) ut_putchar_vga -> ffffffff8017570c (0) 
    9:t:84 hits:  73(73:0) (rip=ffffffff8015ce04) SYS_fs_write -> ffffffff8015cdd6 (51) 
   10:t:84 hits:  49(49:0) (rip=ffffffff801177a9) ar_check_valid_address -> ffffffff80117789 (39) 
   11:t:116 hits:  28(28:0) (rip=ffffffff80141244) check_signals -> ffffffff80141175 (0) 
   12:t:84 hits:   9(9:0) (rip=ffffffff80137462) net_bh -> ffffffff80137405 (262) 
   13:t:84 hits:   7(7:0) (rip=ffffffff801153df) pci_read -> ffffffff801152c4 (313) 
   14:t:116 hits:   4(4:0) (rip=ffffffff80137404) net_bh_recv -> ffffffff80137329 (0) 
   15:t:116 hits:   4(4:0) (rip=ffffffff801379e1) net_bh_send -> ffffffff801378f6 (0) 
   16:t:116 hits:   3(3:0) (rip=ffffffff80144c87) cpuspin_before_halt -> ffffffff80144b6d (0) 
   17:t:84 hits:   3(3:0) (rip=ffffffff80151b9b) vmalloc -> ffffffff80151a81 (0) 
   18:t:84 hits:   1(1:0) (rip=ffffffff8012d3be) udelay -> ffffffff8012d3ac (33) 
   19:t:116 hits:   1(1:0) (rip=ffffffff8012decc) read_ioapic_register -> ffffffff8012deb3 (39) 
1:  symbls count:2874
	 1: addr:400000 - 590770 
	10: addr:588440 - 58f770 
	 2: addr:4a8000 - 4fa174 
	11: addr:590770 - 5ba1e0 
    1:t: 0 hits: 649(649:0) (rip=0000000000433ff4) runtime.casgstatus -> 0000000000433f30 (382) 
    2:t: 0 hits: 395(395:0) (rip=00000000004390f3) runtime.reentersyscall -> 0000000000439000 (568) 
    3:t: 0 hits: 331(331:0) (rip=00000000004961c6) internal/poll.(*fdMutex).rwlock -> 0000000000496130 (358) 
    4:t: 0 hits: 264(264:0) (rip=0000000000439961) runtime.exitsyscallfast -> 00000000004398a0 (251) 
    5:t: 0 hits: 250(250:0) (rip=00000000004962a0) internal/poll.(*fdMutex).rwunlock -> 00000000004962a0 (282) 
    6:t: 0 hits: 221(221:0) (rip=0000000000496bac) internal/poll.(*FD).Write -> 0000000000496b30 (1055) 
    7:t: 0 hits: 175(175:0) (rip=000000000043bef9) runtime.wirep -> 000000000043beb0 (345) 
    8:t: 0 hits: 168(168:0) (rip=000000000043978d) runtime.exitsyscall -> 0000000000439620 (630) 
    9:t: 0 hits: 145(145:0) (rip=000000000049735c) os.(*File).Write -> 00000000004972e0 (814) 
   10:t: 0 hits:  84(84:0) (rip=000000000048c6b2) syscall.write -> 000000000048c610 (267) 
   11:t: 0 hits:  53(53:0) (rip=0000000000439266) runtime.entersyscall -> 0000000000439240 (48) 
   12:t: 0 hits:  52(52:0) (rip=00000000004399c2) runtime.exitsyscallfast_reacquired -> 00000000004399a0 (129) 
   13:t: 0 hits:  52(52:0) (rip=000000000049647e) internal/poll.(*FD).writeUnlock -> 0000000000496440 (90) 
   14:t: 0 hits:  41(41:0) (rip=0000000000438fb0) runtime.save -> 0000000000438fb0 (65) 
   15:t: 0 hits:  41(41:0) (rip=000000000048cb0b) syscall.Syscall -> 000000000048cab0 (97) 
   16:t: 0 hits:  41(41:0) (rip=00000000004a819e) main.main -> 00000000004a7ed0 (1147) 
   17:t: 0 hits:  36(36:0) (rip=00000000004965e0) internal/poll.(*pollDesc).prepare -> 00000000004965e0 (325) 
 No hits for rank :18 
```

### Benchmark-2( IPC Improvements): 

This benchmark measures InterProcess Communication(mutex,semphore,messages passing etc) in virtual environment. 

##### Problem Definition:
   During IPC, When cpu sleeps (blocking on "hlt" instruction) and getting awaken up by other cpu using IPI(Inter Processor Interrupt) within short period of time, this as performance implications for IPC and message passing workloads in virtual machines. The cost is more in vm when compare to metal because of vm exists in to hypervisor.
 
  
##### Test Environment and Results
    
  IPC Test: This is a Producer-consumer test using semaphores. Producer and consumer runs in two seperate threads. Producer is more cpu intesive when compare to consumer, this make consumer waits for producer at the end of each loop. producer wakes consumer once item is produced. In this way consumer get context switched at the end of every loop. This emulates  producer and consumer with unequal computation to process every item, this is a very common case. The [source code for test available here.] (https://github.com/naredula-jana/Jiny-Kernel/blob/master/test/expirements/sem_test4.c). If the amount of computation for producer and consumer is same then there will be minimum lock contention which will be very rare. 

 - **Hypervisor**: kvm/qemu-2.x
 - **host kernel version**:3.13.0-32
 - **command and arguments to run the test**:  "sem 800k 2000"  :  Here 800k represents the number of items produced and consumed, producer loops 2000 times in tight loop to emulate cpu consumption after every item, the more this value the more will be the consumer wait time.
     
     
 <table border="1" style="width:100%">
  <tr>
    <td><b>Test# </b></td>
    <td><b> Description</b></td>
    <td><b>Throughput</b></td>
    <td><b>Comment</b></td>
  </tr>
  <tr>
    <td>1 </td>
    <td> linux vm : kernel 3.0.x on 2 cpu,ubuntu-14  </td>
    <td> timetaken= 22 sec</td>
    <td> cpu consumption in host=150%,There are large number of IPI interrupts due to IPC test, close to 280k are generated inside the guestos </td> 
  </tr>
    <tr>
    <td>2 </td>
    <td> Jiny vm : on 2 cpu, version-2  </td>
    <td> timetaken= 9sec (140% over test-1)</td>
    <td> host cpu consumption is 200%(2 cpu's) noticed with top utility. No additional IPI interrupts are generated in the quest OS </td> 
  </tr>
    <tr>
    <td>3 </td>
    <td> linux on metal, with ubutuntu-14, more then 2 cpu  </td>
    <td> timetaken= 9.2sec (slightly less then Test-2) </td>
    <td>  </td> 
  </tr>
  <tr>
    <td>4 </td>
    <td> with ubuntu-14(linux vm - kernel 3.0.x) on 2 cpu, with cpu hogger with low priority in background </td>
    <td>Throughput: timetaken= 12sec (better then Test-1 by 83%) </td>
    <td> cpu hogger with low priority makes the cpu core  alive without going to sleep.</td> 
  </tr>
  </table>
  
          
##### **Summary of Tests:**
1. Test-1 versus Teset-2 : Jiny-vm as performed 140% better when compare to linux vm with same test setup, this is mainly interrupts are not generated because cpu avoids hitting "hlt" instruction when the thread is under wait for a short period.
2. Test-2 versus Test-3 :linux on baremetal is very close to that of jiny vm. 3% to 4% degredation is due to the IPI interrupts are generated on the metal when compare to jiny.
3. Test-1 versus Test-3 :linux kernel on the metal and on hypervisor(i,e test-1 and test-3) shows a gap of 140%. this is mainly the interrupts speed on hypervisor and hitting hlt instruction frequently. whenever "hlt" instruction is executed in guest os, control goes to kvm and get back to guest os upon wakeup, this transfer is costly when compare to small cpu spin in guest os. This is exactly implmented in jiny os to avoid hitting the "hlt" instruction. 

Issue-1: In IPC, if there is a contention in mutext or semop or other IPC, then one of the thread will be active and other thread goes to sleep. In this situation, It will be costly to wakeup a thread under sleep in virtual environment with linux  when compare to the metal or jiny os. 

##### Solution : Changes in Jiny kernel
1. In the producer and consumer test, threads will be running on seperate cpu's. when consumer thread goes to sleep due to the lock, and control goes to idle thread before hitting the "hlt" instruction, idle thread checks if there is any threads belonging to the cpu is waiting, if there is any threads under wait then sheduler spin the cpus or does a small house keeping tasks for a short duration(eg: 10ms or 1 tick) then it goes to sleep using "hlt" instruction, In this short duration if the consumer thread got woken up then it saves IPI(interrupts) generation and also exit/entry of the vcpu. In this way it saves around 800k interrupts for 800k loop in Test-2. Interrupts are costly in virtual environment when compare to metal. every interrupt causes exist and entry in the hypervisor. The "hlt" instruction casues context switch at host. 

##### When IPC based apps in linux vm can under perform due to Issue-1
1. linux as vm and with multicore(more then one cpu). The issue will not be present in single core, to minimum level on metal( shown it is only 3 to 4% as against 140%).
2. This issue will be more when the cpu's are idle relatively. example: only producer,consumer threads running on different cpu's.  If other app's are hogging the same vcpu as waiting thread then vcpu will not go to sleep and issue-1 will not popup.
3. The issue will popup if the critical section's in the app's are small and there is contention among the threads. If the critical section are large then waiter thread goes to a long sleep, in this case receving IPI interrupts is better when compare to tight cpu spin.

 Monitoring Issue-1: Due to this this, IPI interrupts will be generated in large number, this can be monitered using the number of [Rescheduling Interrupts](https://help.ubuntu.com/community/ReschedulingInterrupts) in /proc/interrupts, Rescheduling interrupts are implemented using Inter Processor Interrupt(IPI). 
 
#### Same Problem was solved differently:
Same Problem was solved by changing KVM hypervisor in this [paper](http://www.linux-kvm.org/images/a/ac/02x03-Davit_Matalack-KVM_Message_passing_Performance.pdf), published in kvm-forum-2105.  Difference between the two solutions are:

1. **Problem definition:** same in both. In this paper IPC workload is used to explain the problem, In other paper "message passing workload" is used to explain the problem.
2. **Solution to the problem:** It was fixed in guest kernel(Jiny kernel) in this paper, and the problem was solved completely. Whereas in other paper, it is fixed in hypervisor(kvm) , due to this the problem is solved partially(slide-29). 
3. Both solutions to the same problem can co-exist. since the problem is solved at two different layers(kernel and hypervisor). 


 
