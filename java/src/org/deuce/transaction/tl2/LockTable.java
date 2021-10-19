package org.deuce.transaction.tl2;

import java.util.concurrent.atomic.AtomicIntegerArray;

import org.deuce.transaction.TransactionException;
import org.deuce.transform.Exclude;

@Exclude
public class LockTable {

	// Failure transaction
	final private static TransactionException FAILURE_EXCEPTION = new TransactionException(
			"Faild on lock.");
	final public static int LOCKS_SIZE = 1 << 20; // amount of locks - TODO add
													// system property
	final public static int MASK = 0xFFFFF;
	final private static int LOCK = 1 << 31;
	final private static int UNLOCK = ~LOCK;

	final static private int MODULE_8 = 7; // Used for %8
	final static private int DIVIDE_8 = 3; // Used for /8

	final private static AtomicIntegerArray locks = new AtomicIntegerArray(
			LOCKS_SIZE); // array of 2^20 entries of 32-bit lock words

	/**
	 * 
	 * @return <code>true</code> if lock otherwise false.
	 * @throws TransactionException
	 *             incase the lock is hold by other thread.
	 */
	public static boolean lock(int lockIndex, byte[] contextLocks)
			throws TransactionException {
		final int lock = locks.get(lockIndex);
		final int selfLockIndex = lockIndex >>> DIVIDE_8;
		final byte selfLockByte = contextLocks[selfLockIndex];
		final byte selfLockBit = (byte) (1 << (lockIndex & MODULE_8));

		if ((lock & LOCK) != 0) { // is already locked?
			if ((selfLockByte & selfLockBit) != 0) // check for self locking
				return false;
			throw FAILURE_EXCEPTION;
		}

		boolean isLocked = locks.compareAndSet(lockIndex, lock, lock | LOCK);

		if (!isLocked)
			throw FAILURE_EXCEPTION;

		contextLocks[selfLockIndex] |= selfLockBit; // mark in self locks
		return true;
	}

	public static int checkLock(int lockIndex, int clock, byte[] contextLocks) {

		int lock = locks.get(lockIndex);
		final int selfLockIndex = lockIndex >>> DIVIDE_8;
		final byte selfLockByte = contextLocks[selfLockIndex];
		final byte selfLockBit = (byte) (1 << (lockIndex & MODULE_8));

		if (clock < (lock & UNLOCK) || (lock & LOCK) != 0) {
			if ((selfLockByte & selfLockBit) != 0)
				return lock;
			throw FAILURE_EXCEPTION;
		}

		return lock;
	}

	public static int checkLock(int lockIndex, int clock) {

		int lock = locks.get(lockIndex);

		if (clock < (lock & UNLOCK) || (lock & LOCK) != 0) {

			throw FAILURE_EXCEPTION;
		}

		return lock;
	}

	public static void checkLock(int lockIndex, int clock, int expected) {
		int lock = locks.get(lockIndex);

		if (lock != expected || clock < (lock & UNLOCK) || (lock & LOCK) != 0)
			throw FAILURE_EXCEPTION;
	}

	public static void unLock(int lockIndex, byte[] contextLocks) {
		int lockedValue = locks.get(lockIndex);
		int unlockedValue = lockedValue & UNLOCK;
		locks.set(lockIndex, unlockedValue);

		clearSelfLock(lockIndex, contextLocks);
	}

	public static void setAndReleaseLock(int hash, int newClock,
			byte[] contextLocks) {
		int lockIndex = hash & MASK;
		locks.set(lockIndex, newClock);
		clearSelfLock(lockIndex, contextLocks);
	}

	/**
	 * Clears lock marker from self locking array
	 */
	private static void clearSelfLock(int lockIndex, byte[] contextLocks) {
		// clear marker TODO might clear all bits
		contextLocks[lockIndex >>> DIVIDE_8] &= ~(1 << (lockIndex & MODULE_8));
	}
}
