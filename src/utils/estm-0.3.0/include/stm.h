/*
 * File:
 *   stm.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   STM functions.
 *
 * Copyright (c) 2007-2009.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * @file
 *   STM functions.  This library contains the core functions for
 *   programming with E-STM.
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * @date
 *   2007-2009
 */

/**
 * @mainpage E-STM
 *
 * @section overview_sec Overview
 *
 *   E-STM is the first STM that supports elastic transactions, a new 
 *   form of transactions that enhance concurrency. This distribution
 *   includes both normal transactions and elastic transactions.
 *   The normal transactions build upon TinySTM and support the same 
 *   parameterizing options: write-back (lazy update), write-through 
 *   (eager update), encounter-time locking (eager-acquirement), 
 *   commit-time locking (lazy acquirement).
 *   TinySTM: write-back (updates are buffered until commit time),
 *   write-through (updates are directly written to memory), and
 *   commit-time locking (locks are only acquired upon commit).  The
 *   version can be selected by editing the makefile, which documents
 *   all the different compilation options.
 *
 *   E-STM compiles and runs on 32 or 64-bit architectures.
 *   Tested platforms are 
 *    - SPARC Niagara 2 running SUN OS 5.10, 
 *    - 1.6 Ghz Intel Core 2 Duo running Mac OS X 10.5.6, 
 *    - dual quad-core Intel Xeon Server x500 series running Linux,
 *    - four quad-core AMD Opteron running Linux,
 *    - eight core Opteron running Linux.
 *
 * @section install_sec Installation
 *
 *   TinySTM requires the atomic_ops library, freely available from
 *   http://www.hpl.hp.com/research/linux/atomic_ops/.  The environment
 *   variable <c>LIBAO_HOME</c> must be set to the installation
 *   directory of atomic_ops.
 *
 *   If your system does not support GCC thread-local storage, set the
 *   environment variable <c>NOTLS</c> to a non-empty value before
 *   compilation.
 *
 *   To compile TinySTM libraries, execute <c>make</c> in the main
 *   directory.  To compile test applications, execute <c>make test</c>.
 *
 * @section contact_sec Contact
 *
 *   - E-mail : vincent.gramoli@epfl.ch
 *   - Web    : http://lpd.epfl.ch/gramoli/php/estm
 */

#ifndef _STM_H_
# define _STM_H_

# include <setjmp.h>
# include <stdint.h>
# include <stdio.h>
# include <stdlib.h>

# ifdef __cplusplus
extern "C" {
# endif

/*
 * The library does not require to pass the current transaction as a
 * parameter to the functions (the current transaction is stored in a
 * thread-local variable).  One can, however, compile the library with
 * explicit transaction parameters.  This is useful, for instance, for
 * performance on architectures that do not support TLS or for easier
 * compiler integration.
 */
# ifdef EXPLICIT_TX_PARAMETER
struct stm_tx;
#  define TXTYPE                        struct stm_tx *
#  define TXPARAM                       struct stm_tx *tx
#  define TXPARAMS                      struct stm_tx *tx,
#  define TXARG                         (struct stm_tx *)tx
#  define TXARGS                        (struct stm_tx *)tx,
struct stm_tx *stm_current_tx();
# else /* ! EXPLICIT_TX_PARAMETER */
#  define TXTYPE                        void
#  define TXPARAM                       /* Nothing */
#  define TXPARAMS                      /* Nothing */
#  define TXARG                         /* Nothing */
#  define TXARGS                        /* Nothing */
#endif /* ! EXPLICIT_TX_PARAMETER */
		
#define EL								0
#define NL								1
	
/* ################################################################### *
 * TYPES
 * ################################################################### */

/**
 * Size of a word (accessible atomically) on the target architecture.
 * The library supports 32-bit and 64-bit architectures.
 */
typedef uintptr_t stm_word_t;

/**
 * Transaction attributes specified by the application.
 */
typedef struct stm_tx_attr {
  /**
   * Application-specific identifier for the transaction.  Typically,
   * each transactional construct (atomic block) should have a different
   * identifier.  This identifier can be used by the infrastructure for
   * improving performance, for instance by not scheduling together
   * atomic blocks that have conflicted often in the past.
   */
  int id;
  /**
   * Indicates whether the transaction is read-only.  This information
   * is used as a hint.  If a read-only transaction performs a write, it
   * is aborted and restarted in read-write mode.  In that case, the
   * value of the read-only flag is changed to false.
   */
  int ro;
} stm_tx_attr_t;

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/**
 * Initialize the STM library.  This function must be called once, from
 * the main thread, before any access to the other functions of the
 * library.
 */
void stm_init();

/**
 * Clean up the STM library.  This function must be called once, from
 * the main thread, after all transactional threads have completed.
 */
void stm_exit();

/**
 * Initialize a transactional thread.  This function must be called once
 * from each thread that performs transactional operations, before the
 * thread calls any other functions of the library.
 */
TXTYPE stm_init_thread();

/**
 * Clean up a transactional thread.  This function must be called once
 * from each thread that performs transactional operations, upon exit.
 */
void stm_exit_thread(TXPARAM);
	
/**
 * Start a transaction. The transaction can be either of two types, 
 * elastic or normal. If type is null, the transaction is normal.
 *
 * @param env
 *   Specifies the environment (stack context) to be used to jump back
 *   upon abort.  If null, the transaction will continue even after
 *   abort and the application should explicitely check its status.  If
 *   the transaction is nested, this parameter is ignored as an abort
 *   will restart the top-level transaction (flat nesting).
 * @param attr
 *   Specifies optional attributes associated to the transaction.  If
 *   null, the transaction uses default attributes.
 * @param type
 *   Specifies the type of the transaction. If null, the transaction is 
 *   normal.
 */
void stm_start(TXPARAMS sigjmp_buf *env, stm_tx_attr_t *attr, int type);	
	
/**
 * Try to commit the current transaction.  If successful, the function 
 * returns 1. Otherwise, execution continues at the point specified by 
 * the environment passed as parameter to stm_start() (for the outermost
 * transaction upon nesting).  If the environment was null, the function
 * returns 0 if commit is unsuccessful.
 */
int stm_commit(TXPARAM);
	
/**
 * Explicitly abort the transaction.  Execution continues at the point
 * specified by the environment passed as parameter to stm_start() (for
 * the outermost transaction upon nesting), unless the environment was
 * null.
 */
void stm_abort(TXPARAM);
	
/**
 * Transactional load whose execution depends on the current transaction 
 * type, elastic or normal:
 * 
 * Elastic transactional load.  Read the specified memory location in the
 * context of the current transaction and return its value. If not 
 * preceded by a write operation in the same transaction, this operation
 * success depends only on the previous read operation (if it exists).
 * For further details about cases where an elastic transaction aborts
 * see the EPFL technical report about Elastic Transactions,
 * #LPD-REPORT-2009-002.
 *
 * Normal transactional load.  Read the specified memory location in the
 * context of the current transaction and return its value. The 
 * transaction may abort while reading the memory location. Note that the 
 * value returned is consistent with respect to previous reads from the 
 * same transaction.
 *
 * @param addr
 *   Address of the memory location.
 * @return
 *   Value read from the specified address.
 */
stm_word_t stm_load(TXPARAMS volatile stm_word_t *addr);
	
/**
 * Transactional store whose execution depends on the current transaction 
 * type, elastic or normal:
 *
 * Elastic transactional store.  Write a word-sized value to the 
 * specified memory location in the context of the current transaction.  
 * If this operation is the first write operation of the current 
 * transaction, then its success depends only on the previous read 
 * operation that occurred in the same transaction. See the EPFL 
 * technical report about Elastic Transactions #LPD-REPORT-2009-002,
 * for further details.
 *
 * Normal transactional store.  Write a word-sized value to the 
 * specified memory location in the context of the current transaction.  
 * The transaction may abort while writing to the memory location.
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 */
void stm_store(TXPARAMS volatile stm_word_t *addr, stm_word_t value);

/**
 * Transactional store, writes a value to the specified memory location
 * in the context of the current transaction.  The value may be smaller
 * than a word on the target architecture, in which case a mask is used
 * to indicate the bits of the words that must be updated.  
 *
 * @param addr
 *   Address of the memory location.
 * @param value
 *   Value to be written.
 * @param mask
 *   Mask specifying the bits to be written.
 */
void stm_store2(TXPARAMS volatile stm_word_t *addr, stm_word_t value, stm_word_t mask);

/**
 * Check if the current transaction is still active.
 *
 * @return
 *   True (non-zero) if the transaction is active, false (zero) otherwise.
 */
int stm_active(TXPARAM);

/**
 * Check if the current transaction has aborted.
 *
 * @return
 *   True (non-zero) if the transaction has aborted, false (zero) otherwise.
 */
int stm_aborted(TXPARAM);

/**
 * Get the environment used by the current thread to jump back upon
 * abort.  This environment should be used when calling sigsetjmp()
 * before starting the transaction and passed as parameter to
 * stm_start().  If the current thread is already executing a
 * transaction, i.e., the new transaction will be nested, the function
 * returns NULL and one should not call sigsetjmp().
 *
 * @return
 *   The environment to use for saving the stack context, or NULL if the
 *   transaction is nested.
 */
sigjmp_buf *stm_get_env(TXPARAM);

/**
 * Get attributes associated with the current transactions, if any.
 * These attributes were passed as parameters when starting the
 * transaction.
 *
 * @return Attributes associated with the current transaction, or NULL
 *   if no attributes were specified when starting the transaction.
 */
stm_tx_attr_t *stm_get_attributes(TXPARAM);

/**
 * Get various statistics about the current thread/transaction.  See the
 * source code (stm.c) for a list of supported statistics.
 *
 * @param name
 *   Name of the statistics.
 * @param val
 *   Pointer to the variable that should hold the value of the
 *   statistics.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_get_stats(TXPARAMS const char *name, void *val);

/**
 * Get various parameters of the STM library.  See the source code
 * (stm.c) for a list of supported parameters.
 *
 * @param name
 *   Name of the parameter.
 * @param val
 *   Pointer to the variable that should hold the value of the
 *   parameter.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_get_parameter(const char *name, void *val);

/**
 * Set various parameters of the STM library.  See the source code
 * (stm.c) for a list of supported parameters.
 *
 * @param name
 *   Name of the parameter.
 * @param val
 *   Pointer to a variable that holds the new value of the parameter.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_set_parameter(const char *name, void *val);

/**
 * Create a key to associate application-specific data to the current
 * thread/transaction.  This mechanism can be combined with callbacks to
 * write modules.
 *
 * @return
 *   The new key.
 */
int stm_create_specific();

/**
 * Get application-specific data associated to the current
 * thread/transaction and a given key.
 *
 * @param key
 *   Key designating the data to read.
 * @return
 *   Data stored under the given key.
 */
void *stm_get_specific(TXPARAMS int key);

/**
 * Set application-specific data associated to the current
 * thread/transaction and a given key.
 *
 * @param key
 *   Key designating the data to read.
 * @param data
 *   Data to store under the given key.
 */
void stm_set_specific(TXPARAMS int key, void *data);

/**
 * Register application-specific callbacks that are triggered when
 * particular events occur.
 *
 * @param on_thread_init
 *   Function called upon initialization of a transactional thread.
 * @param on_thread_exit
 *   Function called upon cleanup of a transactional thread.
 * @param on_start
 *   Function called upon start of a transaction.
 * @param on_commit
 *   Function called upon successful transaction commit.
 * @param on_abort
 *   Function called upon transaction abort.
 * @param arg
 *   Parameter to be passed to the callback functions.
 * @return
 *   1 if the callbacks have been successfully registered, 0 otherwise.
 */
int stm_register(void (*on_thread_init)(TXPARAMS void *arg),
                 void (*on_thread_exit)(TXPARAMS void *arg),
                 void (*on_start)(TXPARAMS void *arg),
                 void (*on_commit)(TXPARAMS void *arg),
                 void (*on_abort)(TXPARAMS void *arg),
                 void *arg);

/**
 * Read the current value of the global clock (used for timestamps).
 * This function is useful when programming with unit loads and stores.
 *
 * @return
 *   Value of the global clock.
 */
stm_word_t stm_get_clock();

#ifdef __cplusplus
}
#endif

#endif /* _STM_H_ */
