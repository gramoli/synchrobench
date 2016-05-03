Synchrobench
============
Synchrobench is a micro-benchmark suite used to evaluate synchronization 
techniques on data structures. Synchrobench is written in C/C++ and Java and
currently includes arrays, binary trees, hash tables, linked lists, queues and
skip lists that are synchronized with copy-on-write, locks, read-modify-write, 
read-copy-update and transactional memory. A non-synchronized version of these 
data structures is proposed in each language as a baseline to measure the 
performance gain on multi-(/many-)core machines.

If you use Synchrobench, please cite the companion paper: 
V. Gramoli. More than You Ever Wanted to Know about Synchronization. PPoPP 2015. More information at https://sites.google.com/site/synchrobench/.

Data structures
---------------
Note that the proposed data structures are not synchronized with each individual
synchronization technique, 30+ algorithms from the literature are provided.
Synchrobench includes variants of the algorithms presented in these papers:
 - I. Dick, A. Fekete, V. Gramoli. A Skip List for Multicore. Concurrency and Computation: Practice and Experience. 2016.
 - M. Herlihy, N. Shavit. The Art of Multiprocessor Programming. 2008.
 - J. Gibson, V. Gramoli. Why Non-Blocking Operations Should Be
   Selfish. In DISC, 2015.
 - V. Gramoli, P. Kuznetsov, S. Ravi, D. Shang. A Concurrency-Optimal 
   List-Based Set. arXiv:1502.01633, February 2015 and DISC 2015.
 - I. Dick, A. Fekete and V. Gramoli. Logarithmic data structures for
   multicores. Technical Report 697, University of Sydney, September
   2014.
 - V. Gramoli and R. Guerraoui. Reusable Concurrent Data Types. In ECOOP 2014.
 - M. Arbel and H. Attiya. Concurrent updates with RCU: Search tree as
   an example. In PODC, 2014.
 - A. Natarajan and N. Mittal. Fast Concurrent Lock-Free Binary Search Trees.
   In PPoPP, pages 317-328, 2014
 - D. Drachsler, M. Vechev and E. Yahav. Practical concurrent binary search 
   trees via logical ordering. In PPoPP, pages 343–356, 2014.
 - V. Gramoli and R. Guerraoui. Democratizing Transactional Programming. CACM 
   57(1):86-93, 2014.
 - T. Crain, V. Gramoli and M. Raynal. A contention-friendly search tree. In 
   Euro-Par, pages 229–240, 2013.
 - T. Crain, V. Gramoli and M. Raynal. No hot spot non-blocking skip list. In 
   ICDCS, 2013.
 - T. Crain, V. Gramoli and M. Raynal. A speculation-friendly search tree. In 
   PPoPP, p.161–170, 2012.
 - T. Crain, V. Gramoli and M. Raynal. A contention-friendly methodology for 
   search structures. Technical Report RR-1989, INRIA, 2012.
 - F. Ellen, P. Fatourou, E. Ruppert and F. van Breugel. Non-blocking binary 
   search trees. In PODC, pages 131–140, 2010.
 - N. G. Bronson, J. Casper, H. Chafi and K. Olukotun. A practical 
   concurrent binary search tree. In PPoPP, 2010.
 - P. Felber, V. Gramoli and R. Guerraoui. Elastic Transactions. In DISC 2009.
 - C. Click. A lock-free hash table. Personal communication. 2007.
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
 - M. M. Michael and M. L. Scott. Simple, fast, and practical non-blocking and 
   blocking concurrent queue algorithms. In PODC, 1996.  
Please check the copyright notice of each implementation.

Parameters
---------
 - t, the number of application threads to be spawned. Note that this does not necessarily represent all threads, as it excludes JVM implicit threads and extra maintenance threads spawned by some algorithms.
 - i, the initial size of the benchmark. This corresponds to the amount of elements the data structure is initially fed with before the benchmark starts collecting statistics on the performance of operations.
 - r, the range of possible keys from which the parameters of the executed operations are taken from, not necessarily uniformly at random. This parameter is useful to adjust the evolution of the size of the data structure.
 - u, the update ratio that indicates the amount of update operations among all operations (be they effective or attempted updates).
 - f, indicates whether the update ratio is effective (1) or attempted (0). An effective update ratio tries to match the update ratio to the total amount of operations that effectively modified the data structure by writing, excluding failed updates (e.g., a remove(k) operation that fails because key k is not present).
 - A, indicates whether the benchmark alternates between inserting and removing the same value to maximize effective updates. This parameter is important to reach a high effective update ratios that could not be reached by selecting values at random.
 - U, the unbalance parameter that indicates the extent to which the workload is skewed towards smaller or larger values. This parameter is useful to test balanced structure like trees under unbalancing workloads.
 - d, the duration of the benchmark in milliseconds.
 - a, the ratio of write-all operations that correspond to composite operations. Note that this parameter has to be smaller or equal to the update ratio given by parameter u.
 - s, the ratio of snapshot operations that scan multiple elements of the data structure. Note that this parameter has to be set to a value lower than or equal to 100-u, where u is the update ratio.
 - W, the warmup of the benchmark corresponds to the time it runs before the statistics start being collected, this option is used in Java to give time to the JIT compiler to compile selected bytecode to native code.
 - n, the number of iterations as part of the same JVM instance.
 - b, the benchmark to use.
 - x, the alternative synchronization technique for the same algorithm. In the case of transactional data structures, this rep- resents the transactional model used (relaxed or strong) while it represents the type of locks used in the context of lock-based data structures (optimistic or pessimistic). 

Install
-------
To install Synchrobench, take a look at the INSTALL files of each version of Synchrobench in the java and c-cpp directories.
