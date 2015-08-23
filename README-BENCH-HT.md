
Read-Log-Update: A Lightweight Synchronization Mechanism for Concurrent Programming
===================================================================================

Authors
-------
Alexander Matveev (MIT)

Nir Shavit (MIT and Tel-Aviv University)

Pascal Felber (University of Neuchatel)

Patrick Marlier (University of Neuchatel)

Code Maintainer
---------------
Name:  Alexander Matveev

Email: amatveev@csail.mit.edu

RLU-HT Benchmark
================
Our RLU-HT benchmark provides:

(1) Linked-list implemented by: RCU, RLU, Harris-Michael, and Hazard-Pointers+Harris-Michael.

(2) Hash-table implemented by: RCU, RLU.

In this benchmark we use:

(1) RLU fine-grained

(2) Userspace RCU library of Arbel and Morrison (http://dl.acm.org/citation.cfm?id=2611471)

(3) Harris-Michael list that is based on synchrobench (https://github.com/gramoli/synchrobench)

(4) Hazard pointers that are based on thread-scan (https://github.com/Willtor/ThreadScan) and stack-track (https://github.com/armafire/stack_track)

Compilation
-----------
Execute "make"

Execution Options
-----------------
  -h, --help
        
        Print this message

  -a, --do-not-alternate
	    
        Do not alternate insertions and removals

  -w, --rlu-max-ws
	    
        Maximum number of write-sets aggregated in RLU deferral (default=(1))

  -b, --buckets
        
        Number of buckets (for linked-list use 1, default=(1))

  -d, --duration <int>
        
        Test duration in milliseconds (0=infinite, default=(10000))

  -i, --initial-size <int>
        
        Number of elements to insert before test (default=(256))

  -r, --range <int>
        
        Range of integer values inserted in set (default=((256) * 2))

  -s, --seed <int>
        
        RNG seed (0=time-based, default=(0))

  -u, --update-rate <int>
        
        Percentage of update transactions: 0-1000 (100%) (default=(200))

  -n, --num-threads <int>
	    
        Number of threads (default=(1))

Example
-------
./bench-rlu -a -b1000 -d10000 -i100000 -r200000 -w10 -u200 -n16

  => Initializes a 100,000 items RLU hash-table with 1000 buckets (100 items per bucket).
  
  => The key range is 200,000, and the update ratio is 20% (10% inserts and 10% removes).

  => Has no alternation: completely randomized insert/remove.

  => Uses RLU deferral with maximum number of write-sets set to 10.

  => Executes 16 threads for 10 seconds.
 
./bench-rcu -a -b1000 -d10000 -i100000 -r200000 -w10 -u200 -n16

  => Works as the previous example but uses RCU instead.

