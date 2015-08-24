Read-Log-Update: A Lightweight Synchronization Mechanism for Concurrent Programming
===================================================================================

Linux kernel experiments
------------------------
This directory contains 2 micro benchmarks: The singly-linked list and hash list benchmarks.
Those implementations are close to the one used in the user-space.
These tests uses the same RLU implementation as the user-space implementation and the kernel RCU implementation.
When the macro KERNEL is defined, it enables the Linux kernel specific part in RLU source code.

Building the kernel module
--------------------------
A simple 'make' creates the "sync.ko" kernel module that includes the 2 benchmarks and the RLU and RCU implementation.
This 'make' uses your current kernel (headers and objects) so it has to be recompiled to be used for another kernel.

Running tests with the kernel module
------------------------------------
To run one test, load the sync.ko module (this requires privileged mode, root or sudo).
The module accepts some parameters to run:

* benchmark : the benchmark to run, "rcuhashlist", "rluhashlist", "rcuintset", "rluintset".
* threads\_nb : the number of threads to run.
* update : the mutation rate (insert/delete) * 100 (10=0.1% update). The rest is read only.
* range : the key range, the initial size is half the key range.
* duration : the duration of the execution in ms.

Example using root:

```
 # insmod sync.ko benchmark="rculist" threads_nb=1 update=0 range=256 duration=500
```

