RELEASE NOTES
-------------
Synchrobench is a micro-benchmark suite used to evaluate synchronization 
techniques on data structures. Synchrobench is written in C/C++ and Java and
currently includes arrays, binary trees, linked lists, queues and skip lists 
data stuctures that are synchronized with copy-on-write, locks, 
read-modify-write, read-copy-update and transactional memory. A non-synchronized
version of these data structures is proposed in each language as a baseline to 
measure the performance gain on multi-(/many-)core machines.

DATA STRUCTURES
---------------
Note that the proposed data structures are not synchronized with each individual
synchrobization technique, 30 algorithms from the literature are provided.
The C version of synchrobench (namely synchrobench-c) provides the following
algorithms (among others):
 - I. Dick. The no hot spot non-blocking skip list for unmanaged languages. 
   Honours thesis, University of Sydney, 2013.
 - T. Crain, V. Gramoli, and M. Raynal. A speculation-friendly search tree. In 
   PPoPP, p.161–170, 2012.
 - A. T. Clements, M. F. Kaashoek, and N. Zeldovich. Scalable address spaces 
   using rcu balanced trees. In ASPLOS, p.199–210, 2012.
 - S. Heller, M. Herlihy, V. Luchangco, M. Moir, W. N. S. III, and N. Shavit. A 
   lazy concurrent list-based set algorithm. Parallel Processing Letters, 
   17(4):411–424, 2007.     
 - Maurice Herlihy, Yossi Lev, Victor Luchangco, Nir Shavit. A Simple 
   Optimistic Skiplist Algorithm. In SIROCCO, p.124-138, 2007.
 - K. Fraser. Practical lock freedom. PhD thesis, Cambridge University, 2003.
 - M. M. Michael. High performance dynamic lock-free hash tables and list-based
   sets. In SPAA, p.73–82, 2002.
 - T. Harris. A pragmatic implementation of non-blocking linked-lists. In DISC, 
   p.300–314, 2001.
The Java version of synchrobench (synchrobench-java) provides algorithms:
 - D. Drachsler, M. Vechev, and E. Yahav. Practical concurrent binary search 
   trees via logical ordering. In PPoPP, pages 343–356, 2014.
 - T. Crain, V. Gramoli, and M. Raynal. A contention-friendly search tree. In 
   Euro-Par, pages 229–240, 2013.
 - T. Crain, V. Gramoli, and M. Raynal. No hot spot non-blocking skip list. In 
   ICDCS, 2013.
 - T. Crain, V. Gramoli, and M. Raynal. A contention-friendly methodology for 
   search structures. Technical Report RR-1989, INRIA, 2012.
 - F. Ellen, P. Fatourou, E. Ruppert, and F. van Breugel. Non-blocking binary 
   search trees. In PODC, pages 131–140, 2010.
 - N. G. Bronson, J. Casper, H. Chafi, and K. Olukotun. A practical 
   concurrent binary search tree. In PPoPP, 2010.
 - java.util.concurrent.ConcurrentSkipListMap (Java SE 7)
 - java.util.Vector (Java SE 7)
 - M. M. Michael and M. L. Scott. Simple, fast, and practical non-blocking and 
   blocking concurrent queue algorithms. In PODC, 1996.
Please check the COPYRIGHT notice of each implementation in their respective 
folder.

SYNCHRONIZATION
---------------
The existing algorithms are synchronized with read-copy-update using dedicated
wrappers from the JDK, read-modify-write using exclusively compare-and-swap 
(there are no load-link/store-conditional algorithms), transactional memory
in their software forms using dedicated libraries or compiler support (no 
HTM have been tested), locks (the default locks are pthreads spinlocks and 
mutexes for portability reason (look for the definition of LOCK-related macros)
to change it to whatever locking library.

Other Transactional Memory implemenations can be tested with Synchrobench
 - in C/C++, by extending the file synchrobench-c/include/tm.h
 - in Java, by adding the DeuceSTM-based libraries in the directory:
   synchrobench-java/src/org/deuce/transaction/
The current C/C++ version uses an interface for Software Transactional 
Memory, including support for Elastic transactions. 
Transactional Primitives - API:  
     `TX_START(NL)`  --- Marks the beginning of a regular transaction  
     `TX_START(EL)`  --- Marks the beginning of an elastic transaction  
     `END`           --- Marks the attempt to commit the current transaction  
     `TX_LOAD(x)`    --- Calls the transactional load a memory location `x`  
     `TX_STORE(x,v)` --- Calls the transactional store of `v` at `x`  
The transactional memories that were tested successfully with synchrobench are 
E-STM (C/C++ & Java), LSA (Java), SwissTM (C/C++), TinySTM (C/C++), TL2 (C/C++ &
Java) and are described respectively in:
 - P. Felber, V. Gramoli, and R. Guerraoui. Elastic transactions. In DISC, pages
   93–108, 2009.
 - T. Riegel, P. Felber, and C. Fetzer. A lazy snapshot algorithm with eager 
   validation. In DISC, 2006.
 - A. Dragojevic, R. Guerraoui, M. Kapalka. Stretching transactional memory. In
   PLDI, p.155-165, 2009.
 - P. Felber, C. Fetzer, and T. Riegel. Dynamic performance tuning of 
   word-based software transactional memory. In PPoPP, pages 237–246. ACM, 2008.
 - D. Dice, O. Shalev and N. Shavit. Transactional locking II. In DISC, 2006.
You can download E-STM A [here](http://www.it.usyd.edu.au/~gramoli/tmp/doc/sw/estm-0.3.0.tgz).

USE
---
To use Microbench, take a look at the INSTALL file