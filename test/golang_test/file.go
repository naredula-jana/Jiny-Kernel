package main

import "fmt"
import "os"
import "time"
//import "io/ioutil"
/*
ON LINUX:
janardhana.reddy@megh-none-393124:~$ ./go_file
Start Time:Thursday, 28-Apr-16 10:38:11 IST
total data count : 2700000000  write count:90000000
End Time:Thursday, 28-Apr-16 10:38:32 IST

total time taken on linux on metal : 21 sec
------------------------------------------------------

ON JINY as Normal APP
/ $ data/go_file
Start Time:Thursday, 28-Apr-16 04:54:21 UTC
total data count : 2700000000  write count:90000000
End Time:Thursday, 28-Apr-16 04:54:42 UTC

total time taken as Normal APP on Jiny : 21 sec
------------------------------------------------------
ON JINY as Highpriority APP
-->insexe data/file
 Successfull loaded the high priority app
-->
Start Time:Thursday, 28-Apr-16 04:57:14 UTC
total data count : 2700000000  write count:90000000
End Time:Thursday, 28-Apr-16 04:57:22 UTC

total time taken as High Priority(HP mode) APP on Jiny : 8 sec
-------------------------------------------------------
*/

/*
performance stats for golang,golang runtime system(LVM) and jiny kernel at the same place.
 The sysmbols of golang app and LVM are kept sepearate when compare to jiny.
In HP mode stats: (jiny kernel stats)
    2:t:84 hits:  34(34:0) (rip=ffffffff801425a3) fs_write -> ffffffff8014254d (263) 
    3:t:84 hits:  34(34:0) (rip=ffffffff801436ce) fs_fd_write -> ffffffff80143635 (0) 
    4:t:84 hits:  24(24:0) (rip=ffffffff801436d3) SYS_fs_write -> ffffffff801436d3 (51) 
    5:t:84 hits:  10(10:0) (rip=ffffffff801164c1) ar_check_valid_address -> ffffffff801164bd (39) 
    6:t:84 hits:   9(9:0) (rip=ffffffff80123694) net_bh -> ffffffff8012363b (290) 
    7:t:116 hits:   8(8:0) (rip=ffffffff8012d634) cpuspin_before_halt -> ffffffff8012d597 (0) 
    8:t:72 hits:   7(7:0) (rip=ffffffff80111429) HP_syscall -> ffffffff80111427 (0) 
    9:t:84 hits:   6(6:0) (rip=ffffffff8013a9a4) vmalloc -> ffffffff8013a88a (0) 
   10:t:116 hits:   4(4:0) (rip=ffffffff80123639) net_bh_recv -> ffffffff8012355f (0) 
   11:t:84 hits:   4(4:0) (rip=ffffffff8013097f) _ZN8module_t12sort_symbolsEv -> ffffffff801308d8 (513) 
   12:t:116 hits:   3(3:0) (rip=ffffffff80123b4d) net_bh_send -> ffffffff80123b4d (0) 
   13:t:84 hits:   1(1:0) (rip=ffffffff8012db2a) ut_memcpy -> ffffffff8012da80 (262) 
   14:t:84 hits:   1(1:0) (rip=ffffffff8014ed0a) _ZN12virtio_queue6notifyEv -> ffffffff8014ecc8 (68) 
   15:t:84 hits:   1(4:0) (rip=ffffffff80158437) _ZN14serial_jdriver14dr_serialWriteEPci -> ffffffff8015830e (0) 

1: golang app runtime system perf stats:
    1:t: 0 hits: 340(340:0) (rip=0000000000482899) runtime/internal/atomic.Cas -> 0000000000482890 (0) 
    2:t: 0 hits: 126(126:0) (rip=000000000042be00) runtime.casgstatus -> 000000000042bdd0 (0) 
    3:t: 0 hits: 118(118:0) (rip=000000000042fff3) runtime.reentersyscall -> 000000000042fd30 (0) 
    4:t: 0 hits:  95(95:0) (rip=00000000004829eb) runtime/internal/atomic.Store -> 00000000004829e0 (0) 
    5:t: 0 hits:  79(79:0) (rip=00000000004305d2) runtime.exitsyscall -> 0000000000430550 (0) 
    6:t: 0 hits:  69(69:0) (rip=000000000046af8f) os.(*File).write -> 000000000046aea0 (0) 
    7:t: 0 hits:  65(65:0) (rip=0000000000430891) runtime.exitsyscallfast -> 0000000000430800 (0) 
    8:t: 0 hits:  54(54:0) (rip=00000000004a9920) syscall.Write -> 00000000004a9920 (0) 
    9:t: 0 hits:  50(50:0) (rip=000000000040c3a1) runtime.assertI2T2 -> 000000000040c380 (0) 
   10:t: 0 hits:  45(45:0) (rip=0000000000454f5d) runtime.getcallerpc -> 0000000000454f50 (0) 
   11:t: 0 hits:  38(38:0) (rip=0000000000430094) runtime.entersyscall -> 0000000000430060 (0) 
   12:t: 0 hits:  36(36:0) (rip=0000000000468b90) os.(*File).Write -> 0000000000468b90 (0) 
   13:t: 0 hits:  30(30:0) (rip=0000000000469ff5) os.epipecheck -> 0000000000469f80 (0) 
   14:t: 0 hits:  27(27:0) (rip=00000000004ab025) syscall.Syscall -> 00000000004ab020 (0) 
   15:t: 0 hits:  25(25:0) (rip=0000000000401231) main.main -> 0000000000401000 (0) 
   16:t: 0 hits:  23(23:0) (rip=00000000004a9f9c) syscall.write -> 00000000004a9f80 (0) 
   17:t: 0 hits:  18(18:0) (rip=0000000000455f35) runtime.memclr -> 0000000000455f30 (0) 
   18:t: 0 hits:   8(8:0) (rip=000000000048286b) runtime/internal/atomic.Load -> 0000000000482860 (0) 
   19:t: 0 hits:   7(7:0) (rip=0000000000454f85) runtime.getcallersp -> 0000000000454f80 (0) 
 Total modules: 2 total Hits:0  unknownhits:0 unown ip:0000000000000000 
Not found :lsmod: 

In normal mode:
    2:t:116 hits:2851(2851:0) (rip=00000000004aafe4) trampoline_end -> 000000000010e49c (0) 
    3:t:115 hits: 325(325:0) (rip=ffffffff80111388) syscall_entry -> ffffffff8011132b (0) 
    4:t:84 hits: 104(104:0) (rip=ffffffff8012dcbc) sc_before_syscall -> ffffffff8012dbf1 (0) 
    5:t:84 hits:  89(89:0) (rip=ffffffff80123ae2) net_bh -> ffffffff80123a3f (272) 
    6:t:84 hits:  68(68:0) (rip=ffffffff8012dd20) sc_after_syscall -> ffffffff8012dcbe (281) 
    7:t:84 hits:  50(50:0) (rip=ffffffff801481d1) fs_write -> ffffffff801480cb (263) 
    8:t:84 hits:  35(35:0) (rip=ffffffff80149feb) SYS_fs_write -> ffffffff80149feb (551) 
    9:t:84 hits:  34(34:0) (rip=ffffffff80149f9e) fs_fd_write -> ffffffff80149f4d (0) 

-->

*/

func main() {
	i:=0
	fo, _ := os.Create("/dev/null")
    buf := make([]byte, 1024)
    total:=0;
    
    fmt.Println("New Start Time:New New ", time.Now().Format(time.RFC850))
    for i = 0; i < 90000; i++ {
    	count, _ := fo.Write(buf[:30])
    	total =total+count
    }
	fmt.Printf("total data count : %d  write count:%d\n",total,i)
	fmt.Println("End Time:",time.Now().Format(time.RFC850))
}
