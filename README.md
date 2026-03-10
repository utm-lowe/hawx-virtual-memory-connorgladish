[![Open in Codespaces](https://classroom.github.com/assets/launch-codespace-2972f46106e565e64193e422d61a12cf1da4916b45550586e14ef0a7c637dd04.svg)](https://classroom.github.com/open-in-codespaces?assignment_repo_id=23050816)
Introduction
============
HAWX, like xv6, is an OS which runs on a simulated RISC-V system. The physical
memory layout of that system is as specified below:

               +-------------+
    0x00000000 | Unallocated | bottom 
               +-------------+
    0x00001000 | BOOT ROM    |
               +-------------+
               \/\/\/\/\/\/\/
                    ....
               /\/\/\/\/\/\/\
               +-------------+
    0x10000000 | UART 0      |  virtio pages
               +-------------+
    0x10001000 | Disk        |
               +-------------+
               \/\/\/\/\/\/\/
                    ....
               /\/\/\/\/\/\/\
    0x80000000 +-------------+
               | RAM         | ram memory
               |             |
               |             |
    PHYSTOP    +-------------+

On top of this physical layout, the kernel maps memory using the RISC-V's
memory management system. When the kernel boots, it creates a page table that
maps physical memory onto their physical addresses. The kernel maps the UART
and disk pages as well as all physical RAM, granting itself access to the 
entire contents of memory.

When we establish a user space, we will have very different memory maps for
our processes! For now, we are going to create the system which allows the kernel
to make use of the processor's memory mapping capabilities.


Getting Started
===============
Take a look at the files in the kernel directory, and at main.c. Note that in
this assignment, the kernel will be running with interrupts enabled, and it is 
possible that you will generate page fault traps. However, sometimes you will
not be able to see the panic message if your page table is really messed up!

If you try to run the kernel as it is, you will see the following output:

    
    HAWX kernel is booting
    

After which the kernel will stop. This is because right now, we are
running without a valid page table. Memory access is not possible! Really, it
is not stopped. What is happening is a series of desparate page faults because
`vm_init` will point the kernel to an invalid page table. The CPU can do nothing
but scream into the void when this happens. Let's see if we can improve the
situation!


Fixing the Boot Allocation
==========================
Let's get the kernel up and running a little better, shall we? What we need to
do is make sure the boot allocators are working. To do this, you need to fill
in code for the following functions:

* `vm_page_alloc`
* `vm_page_insert`
* `kernel_map_range`

Be sure to read all of the comments, and look around the `mem.c` file for
general hints. Once you have done this, you should at least see that you fail
most of the tests.


Finishing the Assignment
========================
To finalize the allocation and mapping functions for HAWX, you must complete
the following functions:

* `vm_page_free`
* `vm_create_pagetable`
* `vm_lookup`
* `vm_page_remove`
* `vm_map_range`

Once you have done this, you should see the following test results:

    
    HAWX kernel is booting
    
    vm_page_alloc test...PASSED
    vm_page_free test...PASSED
    vm_lookup test...PASSED
    vm_page_insert test...PASSED
    vm_page_remove test...PASSED
    panic: All done! (for now...)

As always, be sure to look around xv6 for inspiration, and read the comments.
The hints they contain will help you!