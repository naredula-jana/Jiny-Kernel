##Benchmarks
###Benchmark-1(CPU centric):
**Overview:** An application execution time on the metal is compared with the same app wrapped using thin OS like Jiny and launched as vm. The single app running in Jiny vm outperforms by completing in 6 seconds when compare to the same running on the metal that took 44sec. The key reason for superior performance in Jiny os is, it accelerates the app by allowing  it to run in ring-0 so that the overhead of system calls and context switches are minimized. The protection of system from app malfunctioning is left to virtulization hardware. To run in Jiny vm, the app need to recompile without any changes using the modified c library.

**Application(app) used for testing:** A simple C application that read and writes in to a /dev/null file repeatedly in a tight loop is used, this is a system call intensive application. [The app](../master/modules/test_file/test_file.c) is executed in four environments on the metal, inside the Jiny as High priority, as normal priority inside Jiny  and inside the linux vm.   

#####Completion time of app in different environments:

1. **case-1:** app as high priority inside Jiny vm:      6 sec
2. **case-2:** same app on the metal(linux host): 44 sec
3. **case-3:** same app as normal priority in Jiny vm: 55 sec
4. **case-4:** same app on the linux vm:          43 sec

#####Reasons for High priority app superior performance when compare to the same app on metal:

- **From cpu point of view**: when app runs inside jiny vm(case-1), virtualization hardware(like intel VT-x) is active and it will protect system  from app malfunctioning, here app runs in ring-0 along with Jiny kernel. means if the app crashes it will not bring down the host, but only the vm crashes at the maximum. when app on the metal(case-2) , virtualization hardware is passive/disabled and the os surrounding(i.e host os) will make sure that the app malfunctioning will not bring down the host by running the app in ring-3. 

 One of key difference between case-1 and case-2 is, in case-1 vt-x hardware is used while in case-2 host os software does the job of system protection from app malfunctioning. Jiny OS will allow the app to run in the same address space, it does not spend cpu cycles to protect the system or os from app malfunciton, at the maximum the vm goes down since only one app is running it is equal to single app crashing without effecting the system.

        
- **From end user point of view**: To run the app inside the Jiny vm as high priority app, it need to recompile so the syscall located in libc will replaced with corresponding function calls. app is not required any change , only libc will be modified. start time in the first case will be slightly slower since vm need to be started before the app. so long running applications like servers will be suitable for high priority app.

#####Benchmark-1 summary
 1. Virtulization hardware(vt-x) along with thin OS like Jiny can function as hardware assit  layer to speedup the app. Launching single app using Jiny Vm will be useful not only from virtulization point of view but also to increase the speed.
 2. In the test,I have used syscall intensive app that as shown huge improvement when compare to app on metal, but other workload like io intensive may not give that large improvement.  Speeds of virtulization io path are improving continuously both in software or hardware,  so  io intensive  apps also will become better in the future.
 3. Most of apps as  high priority app in Jiny will  show big performance improvement when compare the same app in linux or freebsd vm's. 

###Benchmark-2(Network centric):
This benchmark concentrates on the networking speed between linux and Jiny OS. Networking in Jiny is based on the  [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf). Udp client and server are used to test the maxumum throughput of the networking stack in the os. udp client on the host sends the packet to the udp server inside the vm, udp server responds back the packet, In this way the amount of bytes/packets processes in the vm will be calculated. The below test results shows the network thoughput(in terms of bytes processed) in Jiny  and linux:

1. **Test-1**: ubuntu-14(linux vm) :  94M
2. **Test-2**: Jiny os : 155M
3. **Test-3**: Jiny os with delay in send door bell : 170M

**Test Environment**:  
**Network Driver** : Virtio+vhost.  
 **Packet size** used by udp client: 200 bytes. 
Number of cpu cores in the linux and Jiny Vm are 2.  
**Hypervisor**: kvm/qemu-2.0

#####Benchmark-2 summary
 1. Difference between Test-1 and Test-2: Processing the packet in Linux and Jiny are completely different. In Jiny , most of the cpu cycles are spend in the application context as mentioned in  [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf). whereas in linux, cpu cycles are split between the app and Network bottom half making packet to process by different cores.
 2. Difference between Test-2(155M) and Test-3(170M):  For every packet send on the NIC, issuing the door bell in virtio driver cost extra MMIO operation, that is causing the vm exits in kvm hypervisor, this was the reason test-3 got some 15Mbytes extra processing. postponing doorbell for few packets/for a duration of time as improved the throughput at load, but this cause extra delay in holding the send packet when the system is under load. 