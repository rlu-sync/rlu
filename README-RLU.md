
Read-Log-Update: A Lightweight Synchronization Mechanism for Concurrent Programming
===================================================================================

Authors
-------
Alexander Matveev (MIT)

Nir Shavit (MIT and Tel-Aviv University)

Pascal Felber (University of Neuchatel)

Patrick Marlier (University of Neuchatel)

Code Maintainer
-----------------
Name:  Alexander Matveev

Email: amatveev@csail.mit.edu

RLU v1.0 (08/23/2015) 
=====================

General
-------
This is a C implementation of Read-Log-Update (RLU).

RLU is described in: http://people.csail.mit.edu/amatveev/RLU_SOSP_2015.pdf .

RLU Types
---------
Currently we provide two flavors of RLU: (1) coarse-grained and (2) fine-grained.
The coarse-grained flavor has no support for RLU deferral and it provides writer
locks that programmers can use to serialize and coordinate writers. In this way, the
coarse-grained RLU is simpler to use since all operations take an immediate effect,
and they execute once and never abort. In contrast, the fine-grained flavor has no
support for writer locks. Instead it uses per-object locks of RLU to coordinate
writers and does provide support for RLU deferral. As a result, in fine-grained RLU,
writers can execute concurrently while avoiding RLU synchronize calls.

Brief Tutorial
--------------
(1) Add "rlu.c" and "rlu.h" to your project and include "rlu.h"

(2) On program start call "RLU_INIT(type, max_write_sets)":
  
  => Set "type" to RLU_TYPE_FINE_GRAINED or RLU_TYPE_COARSE_GRAINED
  
  => For fine-grained, "max_write_sets" defines the number of write-sets that RLU deferral can aggregate.
  
  => For coarse-grained, "max_write_sets" must be 1 (no RLU deferral)

(3) For each created thread:
  
  => On thread start call RLU_THREAD_INIT(self), where self is "rlu_thread_data_t *"
  
  => On thread finish call RLU_THREAD_FINISH(self)

(4) Allocate RLU-protected objects with RLU_ALLOC(obj_size)

(5) To execute an RLU protected section:
  
  => On section start call RLU_READER_LOCK(self)
  
  => On section finish call RLU_READER_UNLOCK(self)

(6) Inside the section:
  
  => Use RLU_DEREF(self, p_obj) to dereference pointer "p_obj"
  
  => Before modifying to an object pointed by "p_obj", you need to lock (and log) this object.
	 
	 (A) If RLU is fine-grained, then use RLU_TRY_LOCK(self, p_p_obj), where "p_p_obj" is pointer to "p_obj".
           
           * After RLU_TRY_LOCK(self, p_p_obj) returns, "p_obj" will point to a copy that you can modify.
	       
	       * This call can fail, in which case you need to call RLU_ABORT(self)
	 
	 (B) If RLU is coarse-grained, then use RLU_LOCK(self, p_p_obj), where "p_p_obj" is pointer to "p_obj".
           
           * After RLU_LOCK(self, p_p_obj) returns, "p_obj" will point to a copy that you can modify.
           
           * This call never fails. To achieve this, RLU provides writer locks that you can use to serialize 
             and coordinate writers. The API function is: 
             RLU_TRY_WRITER_LOCK(self, writer_lock_id)
               
               * When RLU completes or aborts, it ensures to release all writer locks (no need to manage them).
               
               * This call can fail, in which case you need to call RLU_ABORT(self) to release all locks.
     
  => Use RLU_ASSIGN_PTR(self, p_ptr, p_obj) to assign p_obj to location pointed by p_ptr (*p_ptr = p_obj)

  => Use RLU_IS_SAME_PTRS(p_obj_1, p_obj_2) to compare two pointers

  => Use RLU_FREE(self, p_obj) to free "p_obj"

Notes
-----
(1) For an usage example of RLU fine-grained look in "README-BENCH-HT.md" of "rlu" git repository.

(2) For an usage example of RLU coarse-grained look in "README-BENCH-RHT.md" of "rlu-rht" git repository.

