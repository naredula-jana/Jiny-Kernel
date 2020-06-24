##  Golang in Ring-0 Performance improvements.

### Performance Improvement-1:

 **Golang-14.2 in Ring-0:** [Golang in Ring-0 means running Golang application in ring-0.](https://github.com/naredula-jana/Golang-Ring0) Here Golang application run on a [Jiny kernel](https://github.com/naredula-jana/Jiny-Kernel). Jiny kernel supports runtime systems like Java, Golang,..etc to run in ring-0. More details are available [here:](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/GolangAppInRing0.pdf). Below are the performance tests of golang applications on two platforms namely  Jiny and Linux. On Jiny platform golang application runs in ring-0 only. On linux platfom the application runs in ring-3 and ring-0, here it switches from ring-3 to ring-0 on entry of system call and vice versa on exit, this switching slows down the application. Below Test results shows the application running on Jiny is 2X to 10X faster when compare to linux platform.

 The degree of performance improvement depends on the following factors: 
 1.  **Percentage of IO calls:** The amount of io calls used inside the applications, the io calls are file,network read-write. Most of the IO calls like file,network leads to system calls. Test-1 clearly demonstrates the amount the performance gain on Jiny when compare to Linux platform.
 2.  **Percentage of channel messages across the cpu-cores:** The amount of channel messages across the go-routines sitting on different cores.
 

**Performance Tests:**
1.   **Test-1 (IO intensive) :**  The test results shows that for the IO intensive [application](https://github.com/naredula-jana/Jiny-Kernel/blob/master/test/golang_test/file.go) performs better on Jiny due to system call intesive. golang app completes in 22sec on Jiny platform versus 202sec on default linux platform, means it is almost 10X improvement. 
2.   **Test-2: (IO + channel Intensive)** The test results shows that for the IO  and channel intensive [application](https://github.com/naredula-jana/Jiny-Kernel/blob/master/test/golang_test/server.go) performs better on Jiny, the reason is system calls from IO aswell as futex from channel messages across the cores. If the number of system calls are less then both run at similar speed. The test results shows that golang application completes in 90sec on Jiny platform versus 180sec on default linux platform, means it is almost 2X improvement. 
2.   **Test-3( channel intensive):** The test results shows that for the channel intensive [application](https://github.com/naredula-jana/Jiny-Kernel/blob/master/test/golang_test/serverchannel.go) performs better on Jiny, the reason is system calls from IO aswell as futex from channel messages across the cores. If the number of system calls are less then both run at similar speed. The test results shows that golang application completes in 60sec on Jiny platform versus 90sec on default linux platform, means it is almost 1.5X improvement. 

**Test-1 Results:**

 <table border="1" style="width:100%">
  <tr>
    <td><b>Test Number </b></td>
    <td><b> Throughput on Jiny-Ring-0 </b></td>
    <td><b>Throughput on Linux platform</b></td>
    <td><b>Comments</b></td>
  </tr>
  <tr>
    <td>Test-1 </td>
    <td> timetaken = 22sec   </td>
    <td> timetaken = 208 sec</td>
    <td>  almost 10X improvement. Reason: large amount of write system calls due to IO. On linux platform system call is a cpu intesive because of switching from ring-3 to ring-0 and vice versa. On Jiny platform system call is light weight function call and runs only in ring-0. </td> 
  </tr>
    <tr>
    <td>Test-2 </td>
    <td> timetaken = 96sec </td>
    <td> timetaken = 180 sec </td>
    <td> almost 2X improvement. Reasons:a) large amount of futex system calls generated due to send/recv function from  channels between go-routines . b)  write system calls due to IO. </td> 
  </tr>
      <tr>
    <td>Test-3 </td>
    <td> timetaken = 60sec </td>
    <td> timetaken = 90sec </td>
    <td> almost 2X improvement. Reasons:a) large amount of futex system calls generated due to send/recv function from  channels between go-routines  </td> 
  </tr>
  </table>

  

**Test-1 on both the platforms:**

```

Running On Jiny Platform: (Test completed in 22 sec):

-->insexe /data/nfile -count=400000000

 Starting golang app :/data/nfile:  args :-count=400000000: 
 Successfull loaded the high priority app
------------------------------
FileApp:  SAMPLE File application count:  400000000
New Start Time:New New  Sunday, 26-Apr-20 11:19:44 UTC
total data count : 12000000000  write count:400000000
End Time: Sunday, 26-Apr-20 11:20:06 UTC

--------------

Running on Linux Platform: (Test completed in 208 sec):

root:/opt_src/Jiny-Kernel/test/golang_test# ./file -count=400000000
FileApp:  SAMPLE File application count:  400000000
New Start Time:New New  Sunday, 26-Apr-20 16:44:58 IST
total data count : 12000000000  write count:400000000
End Time: Sunday, 26-Apr-20 16:48:26 IST
root:/opt_src/Jiny-Kernel/test/golang_test# 
```


**Test-1 Profiling Data:**

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

**Test-2 Running on both platform:**

```

Running  on Jiny platform:

-->insexe /data/server -count=3000000

 Starting golang app :/data/server:  args :-count=3000000: 
 Successfull loaded the high priority app
------------------------------
SERVER:  ver=1.1 SAMPLE application with files and channels:  3000000
SERVER:   Arguments to application  : [highpriorityapp -count=3000000]
-->SERVER: Final total:  3387232619202732032

--------------

Running  on Linux  platform: (Test completed in 208 sec):

root@nvnr:/opt_src/Jiny-Kernel/test/golang_test#  export GOMAXPROCS=2
root@nvnr:/opt_src/Jiny-Kernel/test/golang_test# date; ./server -count=3000000 ; date
Sun May  3 17:12:52 IST 2020
SERVER:  ver=1.1 SAMPLE application with files and channels:  3000000
SERVER:   Arguments to application  : [./server -count=3000000]
SERVER: Final total:  3387232619202732032
Sun May  3 17:15:44 IST 2020

```

**Test-2 Profiling Data:**

```
System calls stats on Jiny:
[11(17):11: f](105882801)cpu-5:   1 (426431/      0)  3ffd8020:10 hp_go_thread_1 sleeptick(998347) cpu:65535 :/: count:1 status:WAIT-timer: 1000000 stickcpu:ffff
     0: calls:103804240(per-sys:0        app:0       ) :SYS_fs_write
     1: calls:18      (per-sys:0        app:0       ) :SYS_vm_mmap
     2: calls:114     (per-sys:0        app:0       ) :SYS_rt_sigaction_PART
     3: calls:8       (per-sys:0        app:0       ) :SYS_rt_sigprocmask_PART
     4: calls:503116  (per-sys:0        app:0       ) :SYS_sched_yield
     5: calls:219060  (per-sys:0        app:0       ) :SYS_nanosleep
     6: calls:3       (per-sys:0        app:0       ) :SYS_sc_clone
     7: calls:1       (per-sys:0        app:0       ) :SYS_uname
     8: calls:3       (per-sys:0        app:0       ) :SYS_fs_fcntl
     9: calls:7       (per-sys:0        app:0       ) :SYS_gettimeofday
     10: calls:2       (per-sys:0        app:0       ) :SYS_sigalt_stack_PART
     11: calls:1       (per-sys:0        app:0       ) :SYS_arch_prctl
     12: calls:1       (per-sys:0        app:0       ) :SYS_gettid
     13: calls:1356068 (per-sys:0        app:0       ) :SYS_futex
     14: calls:1       (per-sys:0        app:0       ) :SYS_sched_getaffinity_PART
     15: calls:103     (per-sys:0        app:0       ) :SYS_epoll_ctl
     16: calls:52      (per-sys:0        app:0       ) :SYS_fs_openat
     17: calls:1       (per-sys:0        app:0       ) :SYS_readlinkat_PART
     18: calls:1       (per-sys:0        app:0       ) :SYS_epoll_create1_PART
     19: calls:1       (per-sys:0        app:0       ) :SYS_pipe2_PART
[12(18):11:11](13719)cpu-1:   1 ( 9089/      0)  3ffd8020:10 hp_go_thread_2 sleeptick( 12) cpu:65535 :/: count:1 status:WAIT-futex: 50 stickcpu:ffff
     0: calls:1       (per-sys:0        app:0       ) :SYS_rt_sigprocmask_PART
     1: calls:4591    (per-sys:0        app:0       ) :SYS_nanosleep
     2: calls:4592    (per-sys:0        app:0       ) :SYS_gettimeofday
     3: calls:2       (per-sys:0        app:0       ) :SYS_sigalt_stack_PART
     4: calls:1       (per-sys:0        app:0       ) :SYS_arch_prctl
     5: calls:2       (per-sys:0        app:0       ) :SYS_gettid
     6: calls:4530    (per-sys:0        app:0       ) :SYS_epoll_pwait_PART
[13(19):11:11](94294308)cpu-2:   1 (373374/    434)  3ffd8020:10 hp_go_thread_3 sleeptick(  8) cpu:65535 :/: count:1 status:WAIT-futex: 50 stickcpu:ffff
     0: calls:92393459(per-sys:0        app:0       ) :SYS_fs_write
     1: calls:1       (per-sys:0        app:0       ) :SYS_vm_mmap
     2: calls:1       (per-sys:0        app:0       ) :SYS_rt_sigprocmask_PART
     3: calls:470214  (per-sys:0        app:0       ) :SYS_sched_yield
     4: calls:204618  (per-sys:0        app:0       ) :SYS_nanosleep
     5: calls:2       (per-sys:0        app:0       ) :SYS_sigalt_stack_PART
     6: calls:1       (per-sys:0        app:0       ) :SYS_arch_prctl
     7: calls:2       (per-sys:0        app:0       ) :SYS_gettid
     8: calls:1226010 (per-sys:0        app:0       ) :SYS_futex
[14(20):11:11](105970963)cpu-3:   1 (421786/      0)  3ffd8020:10 hp_go_thread_4 sleeptick(  0) cpu:65535 :/: count:1 status:WAIT-timer: 1000000 stickcpu:ffff
     0: calls:103802337(per-sys:0        app:0       ) :SYS_fs_write
     1: calls:1       (per-sys:0        app:0       ) :SYS_rt_sigprocmask_PART
     2: calls:534602  (per-sys:0        app:0       ) :SYS_sched_yield
     3: calls:242563  (per-sys:0        app:0       ) :SYS_nanosleep
     4: calls:1       (per-sys:0        app:0       ) :SYS_getpid
     5: calls:2       (per-sys:0        app:0       ) :SYS_sigalt_stack_PART
     6: calls:1       (per-sys:0        app:0       ) :SYS_arch_prctl
     7: calls:2       (per-sys:0        app:0       ) :SYS_gettid
     8: calls:1391453 (per-sys:0        app:0       ) :SYS_futex
     9: calls:1       (per-sys:0        app:0       ) :SYS_tgkill_PART
     
  Summary of system calls stats on Jiny:
   write and futex calls are large in number. 
     1) Reasons for large amount of write calls: application as a lot of write calls.
     2) Reasons for large amount of futex calls: lot of channel communication between go-routines and due to GOMAXPROCS is greater then 1. If GOMAXPROCS=1 then  futex calls will be minimum, due to multi core futex calls are used to wake up the other thread. 
     
 ----------------------------------------------------    
  function call stat on Jiny:
     1:t:84 hits:31866(10654:8429) (rip=ffffffff80144dad) idleTask_func -> ffffffff80144c6e (0) 
    2:t:84 hits:5069(0:0) (rip=0000000000000000) sc_schedule -> ffffffff80141636 (515) 
    3:t:116 hits:4647(0:67) (rip=0000000000000000) cpuspin_before_halt -> ffffffff80144b00 (0) 
    4:t:84 hits:2291(0:27) (rip=0000000000000000) net_bh -> ffffffff80137411 (262) 
    5:t:84 hits:  87(0:0) (rip=0000000000000000) sc_after_syscall -> ffffffff80141251 (628) 
    6:t:72 hits:  50(0:0) (rip=0000000000000000) HP_syscall -> ffffffff80112427 (0) 
    7:t:84 hits:  38(0:0) (rip=0000000000000000) _ZN10wait_queue13wait_internalEmP10spinlock_t -> ffffffff801368b2 (0) 
    8:t:84 hits:  38(0:0) (rip=0000000000000000) fs_write -> ffffffff8015b3a7 (265) 
    9:t:84 hits:  35(0:0) (rip=0000000000000000) fs_fd_write -> ffffffff8015cccc (0) 
   10:t:84 hits:  34(0:0) (rip=0000000000000000) SYS_fs_write -> ffffffff8015cd6e (51) 
   11:t:84 hits:  26(0:0) (rip=0000000000000000) _ZN10wait_queue6wakeupEv -> ffffffff8013675e (340) 
   12:t:84 hits:  23(0:0) (rip=0000000000000000) ar_check_valid_address -> ffffffff80117789 (39) 
   13:t:116 hits:  17(0:0) (rip=0000000000000000) find_futex -> ffffffff80134d86 (147) 
   14:t:115 hits:  12(0:0) (rip=0000000000000000) spin -> ffffffff80134602 (0) 
   15:t:84 hits:   4(0:0) (rip=0000000000000000) _ZN14serial_jdriver14dr_serialWriteEPci -> ffffffff80176e44 (0) 
   16:t:116 hits:   3(0:0) (rip=0000000000000000) arch_spinlock_unlock -> ffffffff801347dd (0) 
   17:t:116 hits:   2(0:0) (rip=0000000000000000) get_percpu_ns -> ffffffff801127ba (629) 
   18:t:84 hits:   2(0:0) (rip=0000000000000000) apic_send_ipi_vector -> ffffffff8012ed97 (0) 
   19:t:116 hits:   2(0:0) (rip=0000000000000000) arch_spinlock_lock -> ffffffff8013455a (361) 
1:  symbls count:2819
    1:t: 0 hits:3816(0:0) (rip=0000000000000000) main.spin -> 000000000049fe30 (35) 
    2:t: 0 hits: 936(0:0) (rip=0000000000000000) runtime.casgstatus -> 0000000000434090 (382) 
    3:t: 0 hits: 438(0:0) (rip=0000000000000000) runtime.lock -> 0000000000409ff0 (412) 
    4:t: 0 hits: 341(0:0) (rip=0000000000000000) runtime.unlock -> 000000000040a190 (193) 
    5:t: 0 hits: 289(0:0) (rip=0000000000000000) internal/poll.(*fdMutex).rwlock -> 000000000048e090 (358) 
    6:t: 0 hits: 233(0:0) (rip=0000000000000000) runtime.runqput -> 000000000043e010 (249) 
    7:t: 0 hits: 205(0:0) (rip=0000000000000000) runtime.chanrecv -> 0000000000405980 (1716) 
    8:t: 0 hits: 195(0:0) (rip=0000000000000000) runtime.runqget -> 000000000043e310 (168) 
    9:t: 0 hits: 177(0:0) (rip=0000000000000000) main.process -> 000000000049fe60 (359) 
   10:t: 0 hits: 149(0:0) (rip=0000000000000000) runtime.gopark -> 0000000000432500 (309) 
   11:t: 0 hits: 141(0:0) (rip=0000000000000000) runtime.chansend -> 0000000000404e60 (1557) 
   12:t: 0 hits: 135(0:0) (rip=0000000000000000) runtime.reentersyscall -> 0000000000439160 (568) 
   13:t: 0 hits: 116(0:0) (rip=0000000000000000) runtime.ready -> 00000000004337e0 (704) 
   14:t: 0 hits: 111(0:0) (rip=0000000000000000) internal/poll.(*fdMutex).rwunlock -> 000000000048e200 (282) 
   15:t: 0 hits: 106(0:0) (rip=0000000000000000) runtime.runqgrab -> 000000000043e3c0 (335) 
   16:t: 0 hits: 105(0:0) (rip=0000000000000000) runtime.exitsyscallfast -> 0000000000439a00 (251) 
   17:t: 0 hits:  93(0:0) (rip=0000000000000000) internal/poll.(*FD).Write -> 000000000048ea90 (1055) 
   18:t: 0 hits:  92(0:0) (rip=0000000000000000) runtime.schedule -> 0000000000437d70 (1316) 
   19:t: 0 hits:  90(0:0) (rip=0000000000000000) runtime.releaseSudog -> 0000000000432a40 (904) 
 Total modules: 2 total Hits:0  unknownhits:0 unown ip:0000000000000000 
 
 -------------------------------------------------------------------------------
 Summary of system call from Linux platform:
 
 root@nvnr:/home/nvnr# ps -ax | grep server
11761 pts/1    SLl+   0:14 ./server -count=3000000
11766 pts/2    S+     0:00 grep --color=auto server
root@nvnr:/home/nvnr# strace -c -f -p 11761
 ^Cstrace: Process 11761 detached
strace: Process 11762 detached
strace: Process 11763 detached
strace: Process 11764 detached
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 37.41    3.493554         608      5749           nanosleep
 32.19    3.006519           2   1627229           write
 29.70    2.773982          87     31960       922 futex
  0.70    0.065674          53      1238           epoll_pwait
  0.00    0.000042          42         1           restart_syscall
------ ----------- ----------- --------- --------- ----------------
100.00    9.339771               1666177       922 total
 
```
#test-3

```
-->insexe /data/serverchannel -count=3000000

 Starting golang app :/data/serverchannel:  args :-count=3000000: 
 Successfull loaded the high priority app
------------------------------
SERVERCHANNEL:  ver=1.1 SAMPLE application with files and channels:  3000000
SERVERCHANNEL:   Arguments to application  : [highpriorityapp -count=3000000]
SERVERCHANNEL Start Time:  Monday, 04-May-20 20:09:04 UTC
-->SERVERCHANNEL End Time:  Monday, 04-May-20 20:09:58 UTC
SERVERCHANNEL: Final total:  3387232619202732032

------------------------
on Linux platform
root@nvnr:/opt_src/Jiny-Kernel/test/golang_test# date; ./serverchannel -count=3000000 ; date
Tue May  5 01:31:43 IST 2020
SERVERCHANNEL:  ver=1.1 SAMPLE application with files and channels:  3000000
SERVERCHANNEL:   Arguments to application  : [./serverchannel -count=3000000]
SERVERCHANNEL Start Time:  Tuesday, 05-May-20 01:31:43 IST
SERVERCHANNEL End Time:  Tuesday, 05-May-20 01:33:10 IST
SERVERCHANNEL: Final total:  3387232619202732032

```
Test-3 Profile Data:

```
[11(17):11: f](3121047)cpu-2:   1 (583760/      0)  3ffd8020:10 hp_go_thread_1 sleeptick( 42) cpu:65535 :/: count:1 status:WAIT-Futex: 50 stickcpu:ffff
     0: calls:1       (per-sys:0        app:0       ) :SYS_fs_read
     1: calls:3       (per-sys:0        app:0       ) :SYS_fs_write
     2: calls:1       (per-sys:0        app:0       ) :SYS_fs_close
     3: calls:18      (per-sys:0        app:0       ) :SYS_vm_mmap
     4: calls:114     (per-sys:0        app:0       ) :SYS_rt_sigaction_PART
     5: calls:6       (per-sys:0        app:0       ) :SYS_rt_sigprocmask_PART
     6: calls:783872  (per-sys:0        app:0       ) :SYS_sched_yield
     7: calls:386979  (per-sys:0        app:0       ) :SYS_nanosleep
     8: calls:2       (per-sys:0        app:0       ) :SYS_sc_clone
     9: calls:1       (per-sys:0        app:0       ) :SYS_uname
     10: calls:3       (per-sys:0        app:0       ) :SYS_fs_fcntl
     11: calls:9       (per-sys:0        app:0       ) :SYS_gettimeofday
     12: calls:2       (per-sys:0        app:0       ) :SYS_sigalt_stack_PART
     13: calls:1       (per-sys:0        app:0       ) :SYS_arch_prctl
     14: calls:1       (per-sys:0        app:0       ) :SYS_gettid
     15: calls:1950030 (per-sys:0        app:0       ) :SYS_futex
     16: calls:1       (per-sys:0        app:0       ) :SYS_sched_getaffinity_PART
     17: calls:2       (per-sys:0        app:0       ) :SYS_fs_openat
     18: calls:1       (per-sys:0        app:0       ) :SYS_readlinkat_PART
[12(18):11:11](12725)cpu-3:   1 ( 6469/      0)  3ffd8020:10 hp_go_thread_2 sleeptick( 43) cpu:65535 :/: count:1 status:WAIT-Futex: 50 stickcpu:ffff
     0: calls:1       (per-sys:0        app:0       ) :SYS_rt_sigprocmask_PART
     1: calls:6356    (per-sys:0        app:0       ) :SYS_nanosleep
     2: calls:1       (per-sys:0        app:0       ) :SYS_getpid
     3: calls:6359    (per-sys:0        app:0       ) :SYS_gettimeofday
     4: calls:2       (per-sys:0        app:0       ) :SYS_sigalt_stack_PART
     5: calls:1       (per-sys:0        app:0       ) :SYS_arch_prctl
     6: calls:2       (per-sys:0        app:0       ) :SYS_gettid
     7: calls:2       (per-sys:0        app:0       ) :SYS_futex
     8: calls:1       (per-sys:0        app:0       ) :SYS_tgkill_PART
[13(19):11:11](2589775)cpu-4:   1 (495746/      1)  3ffd8020:10 hp_go_thread_3 sleeptick(993553) cpu:65535 :/: count:1 status:WAIT-timer: 1000000 stickcpu:ffff
     0: calls:2       (per-sys:0        app:0       ) :SYS_fs_write
     1: calls:1       (per-sys:0        app:0       ) :SYS_vm_mmap
     2: calls:3       (per-sys:0        app:0       ) :SYS_rt_sigprocmask_PART
     3: calls:642179  (per-sys:0        app:0       ) :SYS_sched_yield
     4: calls:305848  (per-sys:0        app:0       ) :SYS_nanosleep
     5: calls:1       (per-sys:0        app:0       ) :SYS_sc_clone
     6: calls:2       (per-sys:0        app:0       ) :SYS_gettimeofday
     7: calls:2       (per-sys:0        app:0       ) :SYS_sigalt_stack_PART
     8: calls:1       (per-sys:0        app:0       ) :SYS_arch_prctl
     9: calls:2       (per-sys:0        app:0       ) :SYS_gettid
     10: calls:1641734 (per-sys:0        app:0       ) :SYS_futex
[14(20):11:13](2266016)cpu-1:   1 (435057/      0)  3ffd8020:10 hp_go_thread_4 sleeptick(  0) cpu:65535 :/: count:1 status:WAIT-timer: 1000000 stickcpu:ffff
     0: calls:32      (per-sys:0        app:0       ) :SYS_fs_write
     1: calls:1       (per-sys:0        app:0       ) :SYS_rt_sigprocmask_PART
     2: calls:546299  (per-sys:0        app:0       ) :SYS_sched_yield
     3: calls:274867  (per-sys:0        app:0       ) :SYS_nanosleep
     4: calls:1       (per-sys:0        app:0       ) :SYS_getpid
     5: calls:2       (per-sys:0        app:0       ) :SYS_sigalt_stack_PART
     6: calls:1       (per-sys:0        app:0       ) :SYS_arch_prctl
     7: calls:2       (per-sys:0        app:0       ) :SYS_gettid
     8: calls:1444810 (per-sys:0        app:0       ) :SYS_futex
     9: calls:1       (per-sys:0        app:0       ) :SYS_tgkill_PART
-->lsmod stat

 Stats for cpu: -1 
0: kernel symbls count:2025
	 0: addr:    0 -     0 
	 0: addr:    0 -     0 
	 0: addr:    0 -     0 
	 0: addr:    0 -     0 
    1:t:84 hits:42643(13954:6161:5684:10702:6142) (rip=ffffffff80144e91) idleTask_func -> ffffffff80144d52 (0) 
    2:t:116 hits:4079(0:1433:1146:319:1181) (rip=0000000000000000) cpuspin_before_halt -> ffffffff80144be4 (0) 
    3:t:84 hits:2949(0:2888:32:0:29) (rip=0000000000000000) sc_schedule -> ffffffff8014171a (515) 
    4:t:84 hits:1895(0:681:520:143:551) (rip=0000000000000000) net_bh -> ffffffff801374f5 (262) 
    5:t:84 hits:  64(0:0:33:0:31) (rip=0000000000000000) _ZN10wait_queue13wait_internalEmP10spinlock_t -> ffffffff80136996 (0) 
    6:t:84 hits:  57(0:0:29:0:28) (rip=0000000000000000) _ZN10wait_queue6wakeupEv -> ffffffff80136842 (340) 
    7:t:115 hits:  18(0:0:11:0:7) (rip=0000000000000000) spin -> ffffffff80134602 (0) 
    8:t:116 hits:   9(0:0:2:0:7) (rip=0000000000000000) get_percpu_ns -> ffffffff801127ba (629) 
    9:t:116 hits:   4(0:0:3:0:1) (rip=0000000000000000) arch_spinlock_unlock -> ffffffff801347dd (0) 
   10:t:84 hits:   4(0:0:0:0:4) (rip=0000000000000000) _ZN14serial_jdriver14dr_serialWriteEPci -> ffffffff80176fc4 (0) 
   11:t:84 hits:   3(0:0:2:0:1) (rip=0000000000000000) ut_get_systemtime_ns -> ffffffff80112a69 (0) 
   12:t:116 hits:   3(0:0:2:0:1) (rip=0000000000000000) arch_spinlock_lock -> ffffffff8013455a (361) 
   13:t:116 hits:   3(0:0:3:0:0) (rip=0000000000000000) find_futex -> ffffffff80134d86 (257) 
   14:t:116 hits:   3(0:0:0:0:3) (rip=0000000000000000) wakeup_cpus -> ffffffff80135895 (0) 
   15:t:72 hits:   2(0:0:1:0:1) (rip=0000000000000000) HP_syscall -> ffffffff80112427 (0) 
   16:t:84 hits:   2(0:0:1:0:1) (rip=0000000000000000) ar_read_tsc -> ffffffff8011279b (0) 
   17:t:84 hits:   2(0:0:2:0:0) (rip=0000000000000000) SYS_futex -> ffffffff801350bf (0) 
   18:t:84 hits:   2(0:0:1:0:1) (rip=0000000000000000) sc_after_syscall -> ffffffff80141335 (628) 
   19:t:84 hits:   1(0:0:0:0:1) (rip=0000000000000000) SYS_nanosleep -> ffffffff8013bd47 (355) 
1:  symbls count:2874
	 1: addr:400000 - 590770 
	10: addr:588440 - 58f770 
	 2: addr:4a8000 - 4fa0d5 
	11: addr:590770 - 5ba1c0 
    1:t: 0 hits:2711(0:0:1427:0:1284) (rip=0000000000000000) main.spin -> 00000000004a7c60 (35) 
    2:t: 0 hits: 586(0:0:317:0:269) (rip=0000000000000000) runtime.casgstatus -> 0000000000434090 (382) 
    3:t: 0 hits: 546(0:0:301:0:245) (rip=0000000000000000) runtime.unlock -> 000000000040a190 (193) 
    4:t: 0 hits: 498(0:0:262:0:236) (rip=0000000000000000) runtime.lock -> 0000000000409ff0 (412) 
    5:t: 0 hits: 273(0:0:147:0:126) (rip=0000000000000000) runtime.chanrecv -> 0000000000405980 (1716) 
    6:t: 0 hits: 241(0:0:130:0:111) (rip=0000000000000000) runtime.runqput -> 000000000043e010 (249) 
    7:t: 0 hits: 234(0:0:120:0:114) (rip=0000000000000000) runtime.runqget -> 000000000043e310 (168) 
    8:t: 0 hits: 160(0:0:82:0:78) (rip=0000000000000000) runtime.chansend -> 0000000000404e60 (1557) 
    9:t: 0 hits: 134(0:0:78:0:56) (rip=0000000000000000) runtime.releaseSudog -> 0000000000432a40 (904) 
   10:t: 0 hits: 114(0:0:63:0:51) (rip=0000000000000000) runtime.ready -> 00000000004337e0 (704) 
   11:t: 0 hits: 104(0:0:59:0:45) (rip=0000000000000000) runtime.gopark -> 0000000000432500 (309) 
   12:t: 0 hits: 102(0:0:55:0:47) (rip=0000000000000000) main.process -> 00000000004a7c90 (185) 
   13:t: 0 hits: 101(0:0:56:0:45) (rip=0000000000000000) runtime.schedule -> 0000000000437d70 (1316) 
   14:t: 0 hits:  98(0:0:62:0:36) (rip=0000000000000000) runtime.memmove -> 000000000045d860 (1723) 
   15:t: 0 hits:  93(0:0:54:0:39) (rip=0000000000000000) runtime.runqgrab -> 000000000043e3c0 (335) 
   16:t: 0 hits:  84(0:0:53:0:31) (rip=0000000000000000) runtime.acquireSudog -> 00000000004326b0 (907) 
   17:t: 0 hits:  81(0:0:30:0:51) (rip=0000000000000000) runtime.(*waitq).dequeue -> 0000000000406280 (252) 
   18:t: 0 hits:  75(0:0:44:0:31) (rip=0000000000000000) runtime.execute -> 00000000004369b0 (358) 
   19:t: 0 hits:  70(0:0:35:0:35) (rip=0000000000000000) runtime.chansend1 -> 0000000000404e20 (63) 
 Total modules: 2 total Hits:0  unknownhits:0 unown ip:0000000000000000 
```


