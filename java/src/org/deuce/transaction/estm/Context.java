package org.deuce.transaction.estm;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicIntegerArray;

import org.deuce.transaction.TransactionException;
import org.deuce.transaction.estm.field.ReadFieldAccess.Field;
import org.deuce.transaction.estm.field.ReadFieldAccess.Field.Type;
import org.deuce.transaction.estm.ReadSet;
import org.deuce.transaction.estm.WriteSet;
import org.deuce.transform.Exclude;

/**
 * E-STM
 * An STM implementing regular and elastic transactions.
 * 
 * See the companion paper, Elastic Transactions [DISC '09]
 * 
 * 
 * @author Vincent Gramoli
 */
@Exclude
final public class Context implements org.deuce.transaction.Context {

	/** Type of the tx, (!elastic) means regular type */
	private boolean elastic;
	/**  The time lower bound above which the tx can be serialized */
	private int lb;
	/**  The time upper bound below which the tx can be serialized */
	private int ub;
	
	final private static TransactionException BETWEEN_SUCCESSIVE_READS_EXCEPTION =
		new TransactionException("Fail to read successively.");
	final private static TransactionException BETWEEN_READ_AND_WRITE_EXCEPTION =
		new TransactionException("Fail to read successively.");
	final private static TransactionException WRITE_FAILURE_EXCEPTION =
		new TransactionException("Fail on write (read previous version).");
	final private static TransactionException EXTEND_FAILURE_EXCEPTION =
		new TransactionException("Fail on extend (regular mode).");

	final private static AtomicInteger clock = new AtomicInteger(0);
	final private static AtomicInteger threadID = new AtomicInteger(0);
	
	/**
	 * The last-read-entries contains up to k=2 entries.
	 * 
	 * This is a hack of the C-based E-STM to check that the previous read is not 
	 * being made inconsistent by the garbage collector.
	 */
	final private LastReadEntries lreSet = new LastReadEntries();
	
	/** Read set, used by any regular reads or after an elastic write */
	final private ReadSet readSet = new ReadSet(1024);
	/** Write set, usual redo-log */
	final private WriteSet writeSet = new WriteSet(32);

	private int readHash;
	private int readLock;
	private Object readValue;
	
	private int id;

	public Context() {
		// Unique identifier among active threads
		id = threadID.incrementAndGet();
	}

	/**
	 * The begin delimiter of the transaction
	 */
	public void init(int blockId, String metainf) {
        elastic = (metainf.indexOf("elastic") != -1);
		writeSet.clear();
		readSet.clear();
		lreSet.clear();
		lb = ub = clock.get();
	}

	/**
	 * The end delimiter of the transaction
	 */
	public boolean commit() {
		if (!writeSet.isEmpty()) {
			int newClock = clock.incrementAndGet();
			if (newClock != lb + 1 && !readSet.validate(id)) {
				rollback();
				return false;
			}
			// Write values and release locks
			writeSet.commit(newClock);
		}
		return true;
	}

	/**
	 * Call upon abort
	 */
	public void rollback() {
		// Release locks
		writeSet.rollback();
	}

	/**
	 * Extend the time interval [lb; ub]
	 */
	private boolean extend() {
		final int now = clock.get();
		if (readSet.validate(id)) {
			ub = now;
			return true;
		}
		return false;
	}

	public void beforeReadAccess(Object obj, long field) {
		readHash = LockTable.hash(obj, field);
		// Check if the field is locked (may throw an exception)
		readLock = LockTable.checkLock(readHash, id);
	}
	
	/**
	 * Upon reading
	 * 
	 * @param obj the object of the field
	 * @param field the field to access
	 * @param type the type 
	 * @return
	 */
	private boolean onReadAccess(Object obj, long field, Type type) {
		
		if (readLock < 0) {
			// We already own that lock
			Object v = writeSet.getValue(readHash, obj, field);
			if (v == null)
				return false;
			readValue = v;
			return true;
		}
		boolean b = false;
		
		while (true) {
			// check timestamp 
			while (readLock <= ub) {
				// check version value version
				int lock = LockTable.checkLock(readHash, id);
				if (lock != readLock) {
					readLock = lock;
					readValue = Field.getValue(obj, field, type);
					b = true;
					continue;
				}
				// We have read a valid value (in snapshot)
				// Save to read set
				if (elastic && writeSet.isEmpty()) lreSet.add(obj, field, readHash, lock);
				else readSet.add(obj, field, readHash, lock);
				return b;
			}
			
			// Partial validation
			if (elastic && writeSet.isEmpty()) {
				// check if last read entries have been updated
				if (!lreSet.validate(id, ub)) throw BETWEEN_SUCCESSIVE_READS_EXCEPTION;
				ub = readLock;
				return b;
			} else {
				// Try to extend snapshot
				if (!extend()) {
					throw EXTEND_FAILURE_EXCEPTION;
				}
			}
		}
	}

	/**
	 * Upon writing
	 * 
	 * @param obj the object of the field
	 * @param field the field to access
	 * @param type the type 
	 * @return
	 */
	private void onWriteAccess(Object obj, long field, Object value, Type type) {
		
		int hash = LockTable.hash(obj, field);
		// Lock entry (might throw an exception)
		int timestamp = LockTable.lock(hash, id);
		
		if (timestamp < 0) {
			// We already own that lock
			writeSet.append(hash, obj, field, value, type);
			return;
		}
		if (timestamp > ub) {
			// Handle write-after-read
			if ((elastic && lreSet.contains(obj, field)) || readSet.contains(obj, field)) {
				// Abort
				LockTable.setAndReleaseLock(hash, timestamp);
				throw WRITE_FAILURE_EXCEPTION;
			}
			// We delay validation until later (although we could already validate once here)
		}
		
		// Additional validation 
		if (elastic && !lreSet.validate(id, ub)) {
			LockTable.setAndReleaseLock(hash, timestamp);
			throw BETWEEN_READ_AND_WRITE_EXCEPTION;
		}
		if (elastic && !lreSet.isEmpty()) {
			readSet.copy(lreSet);
			lreSet.clear();
		}
		// Add to write set
		writeSet.add(hash, obj, field, value, type, timestamp);
	}

	public Object onReadAccess(Object obj, Object value, long field) {
		return (onReadAccess(obj, field, Type.OBJECT) ? readValue : value);
	}

	public boolean onReadAccess(Object obj, boolean value, long field) {
		return (onReadAccess(obj, field, Type.BOOLEAN) ? (Boolean) readValue : value);
	}

	public byte onReadAccess(Object obj, byte value, long field) {
		return (onReadAccess(obj, field, Type.BYTE) ? ((Number) readValue).byteValue() : value);
	}

	public char onReadAccess(Object obj, char value, long field) {
		return (onReadAccess(obj, field, Type.CHAR) ? (Character) readValue : value);
	}

	public short onReadAccess(Object obj, short value, long field) {
		return (onReadAccess(obj, field, Type.SHORT) ? ((Number) readValue).shortValue() : value);
	}

	public int onReadAccess(Object obj, int value, long field) {
		return (onReadAccess(obj, field, Type.INT) ? ((Number) readValue).intValue() : value);
	}

	public long onReadAccess(Object obj, long value, long field) {
		return (onReadAccess(obj, field, Type.LONG) ? ((Number) readValue).longValue() : value);
	}

	public float onReadAccess(Object obj, float value, long field) {
		return (onReadAccess(obj, field, Type.FLOAT) ? ((Number) readValue).floatValue() : value);
	}

	public double onReadAccess(Object obj, double value, long field) {
		return (onReadAccess(obj, field, Type.DOUBLE) ? ((Number) readValue).doubleValue() : value);
	}

	public void onWriteAccess(Object obj, Object value, long field) {
		onWriteAccess(obj, field, value, Type.OBJECT);
	}

	public void onWriteAccess(Object obj, boolean value, long field) {
		onWriteAccess(obj, field, (Object) value, Type.BOOLEAN);
	}

	public void onWriteAccess(Object obj, byte value, long field) {
		onWriteAccess(obj, field, (Object) value, Type.BYTE);
	}

	public void onWriteAccess(Object obj, char value, long field) {
		onWriteAccess(obj, field, (Object) value, Type.CHAR);
	}

	public void onWriteAccess(Object obj, short value, long field) {
		onWriteAccess(obj, field, (Object) value, Type.SHORT);
	}

	public void onWriteAccess(Object obj, int value, long field) {
		onWriteAccess(obj, field, (Object) value, Type.INT);
	}

	public void onWriteAccess(Object obj, long value, long field) {
		onWriteAccess(obj, field, (Object) value, Type.LONG);
	}

	public void onWriteAccess(Object obj, float value, long field) {
		onWriteAccess(obj, field, (Object) value, Type.FLOAT);
	}

	public void onWriteAccess(Object obj, double value, long field) {
		onWriteAccess(obj, field, (Object) value, Type.DOUBLE);
	}
	@Override
	public void beforeReadAccessStrongIso(Object obj, long field, Object obj2,
										  long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, Object value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, boolean value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, byte value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, char value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, short value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, int value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, long value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, float value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Override
	public void onWriteAccessStrongIso(Object obj, double value, long field,
									   long fieldObj) {
		// TODO Auto-generated method stub
		
	}
	
	@Exclude
	static public class LockTable {

		// Failure transaction 
		final private static TransactionException FAILURE_EXCEPTION =
			new TransactionException("Fail on lock (already locked).");
		
		final private static int ARRAYSIZE = 1 << 20; // 2^20
		final private static int MASK = ARRAYSIZE - 1;
		final private static int LOCK = 1 << 31;
		final private static int IDMASK = LOCK - 1;

		// Array of 32-bit lock words
		final private static AtomicIntegerArray locks = new AtomicIntegerArray(ARRAYSIZE);

		public static int lock(int hash, int id) throws TransactionException {
			assert hash <= MASK;
			while (true) {
				int lock = locks.get(hash);
				if ((lock & LOCK) != 0) {
					if ((lock & IDMASK) != id) {
						// Already locked by other thread
						throw FAILURE_EXCEPTION;
					} else {
						// We already own this lock
						return -1;
					}
				}

				if (locks.compareAndSet(hash, lock, id | LOCK)) {
					// Return old timestamp (lock bit is not set)
					return lock;
				}
			}
		}

		public static int checkLock(int hash, int id) throws TransactionException {
			assert hash <= MASK;
			int lock = locks.get(hash);
			if ((lock & LOCK) != 0) {
				if ((lock & IDMASK) != id) {
					// Already locked by other thread
					throw FAILURE_EXCEPTION;
				} else {
					// We already own this lock
					return -1;
				}
			}

			// Return old timestamp (lock bit is not set)
			return lock;
		}

		public static void setAndReleaseLock(int hash, int lock) {
			assert hash <= MASK;
			locks.set(hash, lock);
		}

		public static int hash(Object obj, long field) {
			int hash = System.identityHashCode(obj) + (int) field;
			return hash & MASK;
		}
	}
}
