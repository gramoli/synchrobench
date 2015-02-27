Java Synchrobench
=================
Synchrobench is a micro-benchmark suite used to evaluate synchronization 
techniques on data structures. Synchrobench is written in C/C++ and Java and
currently includes arrays, binary trees, hash tables, linked lists, queues and
skip lists that are synchronized with copy-on-write, locks, read-modify-write, 
read-copy-update and transactional memory. A non-synchronized version of these 
data structures is proposed in each language as a baseline to measure the 
performance gain on multi-(/many-)core machines.

If you use Synchrobench, please cite the companion paper: 
V. Gramoli. More than You Ever Wanted to Know about Synchronization. PPoPP 2015. More information at https://sites.google.com/site/synchrobench/.

Data structures in Java
-----------------------
The Java version of synchrobench (synchrobench-java) provides variants of the 
algorithms presented in these papers:
 - V. Gramoli, P. Kuznetsov, S. Ravi, D. Shang. A Concurrency-Optimal List-Based Set. arXiv:1502.01633, 2015.
 - V. Gramoli and R. Guerraoui. Reusable Concurrent Data Types. In ECOOP 2014.
 - D. Drachsler, M. Vechev and E. Yahav. Practical concurrent binary search 
   trees via logical ordering. In PPoPP, pages 343–356, 2014.
 - V. Gramoli and R. Guerraoui. Democratizing Transactional Programming. CACM 
   57(1):86-93, 2014.
 - T. Crain, V. Gramoli and M. Raynal. A contention-friendly search tree. In 
   Euro-Par, pages 229–240, 2013.
 - T. Crain, V. Gramoli and M. Raynal. No hot spot non-blocking skip list. In 
   ICDCS, 2013.
 - T. Crain, V. Gramoli and M. Raynal. A contention-friendly methodology for 
   search structures. Technical Report RR-1989, INRIA, 2012.
 - F. Ellen, P. Fatourou, E. Ruppert and F. van Breugel. Non-blocking binary 
   search trees. In PODC, pages 131–140, 2010.
 - N. G. Bronson, J. Casper, H. Chafi and K. Olukotun. A practical 
   concurrent binary search tree. In PPoPP, 2010.
 - P. Felber, V. Gramoli and R. Guerraoui. Elastic Transactions. In DISC 2009.
 - C. Click. A lock-free hash table. Personal communication. 2007.
 - M. M. Michael and M. L. Scott. Simple, fast, and practical non-blocking and 
   blocking concurrent queue algorithms. In PODC, 1996.  
Please check the copyright notice of each implementation.

Synchronizations
-------------
The Java-like algorithms are synchronized with copy-on-write wrappers,
read-modify-write using exclusively compare-and-swap, transactional memory
in their software forms using bytecode instrumentation, locks.

The transactional memory algorithm used here is E-STM presented in:
 - P. Felber, V. Gramoli, and R. Guerraoui. Elastic transactions. In DISC, pages
   93–108, 2009.

Other Transactional Memory implemenations can be tested with Synchrobench
in Java, by adding the DeuceSTM-based libraries in the directory:
   synchrobench-java/src/org/deuce/transaction/  
The transactional memories that were tested successfully with synchrobench in 
Java are E-STM, LSA, PSTM, SwissTM and TL2 as presented in:
 - P. Felber, V. Gramoli, and R. Guerraoui. Elastic transactions. In DISC, pages
   93–108, 2009.
 - T. Riegel, P. Felber, and C. Fetzer. A lazy snapshot algorithm with eager 
   validation. In DISC, 2006.
 - V. Gramoli and R. Guerraoui. Reusable Concurrent Data Types. In ECOOP 2014
 - A. Dragojevic, R. Guerraoui, M. Kapalka. Stretching transactional memory. In
   PLDI, p.155-165, 2009.
 - D. Dice, O. Shalev and N. Shavit. Transactional locking II. In DISC, 2006.  

Parameters
---------
 - t, the number of application threads to be spawned. Note that this does not necessarily represent all threads, as it excludes JVM implicit threads and extra maintenance threads spawned by some algorithms.
 - i, the initial size of the benchmark. This corresponds to the amount of elements the data structure is initially fed with before the benchmark starts collecting statistics on the performance of operations.
 - r, the range of possible keys from which the parameters of the executed operations are taken from, not necessarily uniformly at random. This parameter is useful to adjust the evolution of the size of the data structure.
 - u, the update ratio that indicates the amount of update operations among all operations (be they effective or attempted updates).
 - U, the unbalance parameter that indicates the extent to which the workload is skewed towards smaller or larger values. This parameter is useful to test balanced structure like trees under unbalancing workloads (not available on all benchmarks).
 - d, the duration of the benchmark in milliseconds.
 - a, the ratio of write-all operations that correspond to composite operations. Note that this parameter has to be smaller or equal to the update ratio given by parameter u.
 - s, the ratio of snapshot operations that scan multiple elements of the data structure. Note that this parameter has to be set to a value lower than or equal to 100-u, where u is the update ratio.
 - W, the warmup of the benchmark corresponds to the time it runs before the statistics start being collected, this option is used in Java to give time to the JIT compiler to compile selected bytecode to native code.
 - n, the number of iterations as part of the same JVM instance.
 - b, the benchmark to use.

Composite functions
----------------
Synchrobench features composite operations to test some appealing features of synchronization techniques, like composition so that a function can invoke existing functions or reusability so that Bob does not have to understand the internals of Alice's library to use it.

There are two types of composite functions:
 - writeAll are composite operation that update the structure. Examples are a move operation that remove an element from one data structure and add it to another structure and a putIfAbsent that adds an element y only if x is absent from the same structure.
 - readAll are composite operations that do not update the structure. Examples are containsAll that checks the presence of multiple elements, returning false if at least one element is absent, and size that counts the number of elements present at some indivisible point (i.e., atomic snapshot) of the execution.

Below is a distribution of operation depending on parameters given. Note that the percentage of writeAll operations (-a) is smaller than the update ratio and the percentage of readAll operations (-s) is smaller than the read-only ratio (100-u).

	/**
	 * The distribution of functions as an array of percentiles
	 * 
	 * 0%           a             u          u+s          100%
	 * |--writeAll--|--writeSome--|--readAll--|--readSome--|
	 * |----------update----- --|-------read-only----------| 
         */
	
