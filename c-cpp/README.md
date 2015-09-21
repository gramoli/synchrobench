Synchrobench
========
Synchrobench is a micro-benchmark suite used to evaluate synchronization 
techniques on data structures. Synchrobench is written in C/C++ and Java and
currently includes arrays, binary trees, hash tables, linked lists, queues and
skip lists that are synchronized with copy-on-write, locks, read-modify-write, 
read-copy-update and transactional memory. A non-synchronized version of these 
data structures is proposed in each language as a baseline to measure the 
performance gain on multi-(/many-)core machines.

If you use Synchrobench, please cite the companion paper: 
V. Gramoli. More than You Ever Wanted to Know about Synchronization. PPoPP 2015. More information at https://sites.google.com/site/synchrobench/.

C/C++ Data structures
--------------------
Note that the proposed data structures are not synchronized with each individual
synchronization technique, 30+ algorithms from the literature are provided.
The C version of synchrobench (namely synchrobench-c) provides variants of the 
algorithms presented in these papers:
 - J. Gibson, V. Gramoli. Why Non-Blocking Operations Should Be 
   Selfish. In DISC, 2015.
 - I. Dick, A. Fekete and V. Gramoli. Logarithmic data structures for
   multicores. Technical Report 697, University of Sydney, September
   2014.
 - M. Arbel and H. Attiya. Concurrent updates with RCU: Search tree as
   an example. In PODC, 2014.
 - T. Crain, V. Gramoli and M. Raynal. A speculation-friendly search tree. In 
   PPoPP, p.161–170, 2012.
 - P. Felber, V. Gramoli and R. Guerraoui. Elastic Transactions. In DISC 2009.
 - S. Heller, M. Herlihy, V. Luchangco, M. Moir, W. N. S. III and N. Shavit. A 
   lazy concurrent list-based set algorithm. Parallel Processing Letters, 
   17(4):411–424, 2007.     
 - M. Herlihy, Y. Lev, V. Luchangco and N. Shavit. A Simple 
   Optimistic Skiplist Algorithm. In SIROCCO, p.124-138, 2007.
 - M. Fomitchev, E. Ruppert. Lock-free linked lists and skip lists. In PODC, 
   2004.
 - K. Fraser. Practical lock freedom. PhD thesis, Cambridge University, 2003.
 - M. M. Michael. High performance dynamic lock-free hash tables and
   list-based sets. In SPAA, pages 73–82, 2002.
 - T. Harris. A pragmatic implementation of non-blocking linked-lists. In DISC, 
   p.300–314, 2001.  
Please check the copyright notice of each implementation.

Synchronizations
-------------
The C/C++ algorithms are synchronized with read-copy-update, 
read-modify-write using exclusively compare-and-swap, transactional memory
in their software forms using dedicated libraries or compiler support (no 
HTM have been tested), locks (the default locks are pthread spinlocks and 
mutexes for portability reason, look for the definition of LOCK-related macros)
to change it to whatever locking library.

The transactional memory algorithm used here is E-STM presented in:
 - P. Felber, V. Gramoli, and R. Guerraoui. Elastic transactions. In DISC, pages
   93–108, 2009.
Other Transactional Memory implemenations can be tested with Synchrobench
in C/C++, by extending the file synchrobench-c/include/tm.h
The current C/C++ version uses an interface for Software Transactional 
Memory, including support for Elastic transactions.  
Transactional Primitives - API:  
     `TX_START(NL)`  --- Marks the beginning of a regular transaction  
     `TX_START(EL)`   --- Marks the beginning of an elastic transaction  
     `END`                 --- Marks the attempt to commit the current transaction  
     `TX_LOAD(x)`     --- Calls the transactional load a memory location `x`  
     `TX_STORE(x,v)` --- Calls the transactional store of `v` at `x`  
The transactional memories that were tested successfully with Synchrobench in
C/C++ are E-STM, SwissTM, TinySTM and TL2, and are described respectively in:
 - P. Felber, V. Gramoli, and R. Guerraoui. Elastic transactions. In DISC, pages
   93–108, 2009.
 - A. Dragojevic, R. Guerraoui, M. Kapalka. Stretching transactional memory. In
   PLDI, p.155-165, 2009.
 - P. Felber, C. Fetzer, and T. Riegel. Dynamic performance tuning of 
   word-based software transactional memory. In PPoPP, pages 237–246. ACM, 2008.
 - D. Dice, O. Shalev and N. Shavit. Transactional locking II. In DISC, 2006.  

Parameters
---------
 - t, the number of application threads to be spawned. Note that this does not necessarily represent all threads, as it excludes JVM implicit threads and extra maintenance threads spawned by some algorithms.
 - i, the initial size of the benchmark. This corresponds to the amount of elements the data structure is initially fed with before the benchmark starts collecting statistics on the performance of operations.
 - r, the range of possible keys from which the parameters of the executed operations are taken from, not necessarily uniformly at random. This parameter is useful to adjust the evolution of the size of the data structure.
 - u, the update ratio that indicates the amount of update operations among all operations (be they effective or attempted updates).
 - f, indicates whether the update ratio is effective (1) or attempted (0). An effective update ratio tries to match the update ratio to the total amount of operations that effectively modified the data structure by writing, excluding failed updates (e.g., a remove(k) operation that fails because key k is not present).
 - A, indicates whether the benchmark alternates between inserting and removing the same value to maximize effective updates. This parameter is important to reach a high effective update ratios that could not be reached by selecting values at random.
 - U, the unbalance parameter that indicates the extent to which the workload is skewed towards smaller or larger values. This parameter is useful to test balanced structure like trees under unbalancing workloads (not available on all benchmarks).
 - d, the duration of the benchmark in milliseconds.
 - a, the ratio of write-all operations that correspond to composite operations. Note that this parameter has to be smaller or equal to the update ratio given by parameter u.
 - s, the ratio of snapshot operations that scan multiple elements of the data structure. Note that this parameter has to be set to a value lower than or equal to 100-u, where u is the update ratio.
 - x, the alternative synchronization technique for the same algorithm. In the case of transactional data structures, this rep- resents the transactional model used (relaxed or strong) while it represents the type of locks used in the context of lock-based data structures (optimistic or pessimistic). 
