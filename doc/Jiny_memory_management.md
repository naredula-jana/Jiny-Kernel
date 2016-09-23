##Memory Management

### Memory Ballooning related optimizations:

**1. Huge Page(2M size) in Ballooning:**

The following are advantages:
  -  Adding and Removing the pages from/to host host is fast, because hypervisor(qemu) need one madvise call instead of 512 system calls.
  -  Huge pages in host machines will be fragmented because ballooning 4k pages, where as in 2M it will be intact. Here THP and Virtio-ballooning are working orthogonolly, THP is consolidating 4k pages, wheras ballooning breaking 2M into 4K pages. These wastage of cycles will be saved with 2M page.
 -  With 2M pages, Nested paging work faster, this makes app inside the vm faster.

*Issues with Huge Page:*
  - for a small vm(with small ammount of guest physical memory) while inflating the balloon, getting the free 2M page may be some time difficult.
  Solution: Incase 2M free page is not found, fallback to 4k page.
  
*QEMU/KVM Hypervisor Changes for Huge Page:*
  -   512 4k-pages transported to hypervisor need to check if it all belong to same 2M page, if it is then use one madvise call
   instead of 512 system calls, this is same for inflate and deflate.
   The above change is optional, This is a best effort solution. and also does not need any changes to virtio protocol between guest and kvm. The changes in the guest and qemu changes are independent, means both are compatable with older or newer versions.
 
 *code: Enable or disable Huge Page in virtio Balloon driver:*
   -   use [MEMBALLOON_2M_PAGE_SIZE](../master/drivers/virtio/virtio_memballoon.cc)  flag in the code to turn or off. 
 
 
### KSM related optimizations:

 **1. Zero Pages in free list:**
 convert the free pages in to zero pages to get picked up by KSM. In this way the pages in the freelist will get compressed.