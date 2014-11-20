package org.deuce.transaction.estmstats;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicIntegerArray;

import org.deuce.transaction.TransactionException;
import org.deuce.transaction.estmstats.field.ReadFieldAccess.Field;
import org.deuce.transaction.estmstats.field.ReadFieldAccess.Field.Type;
import org.deuce.transaction.estmstats.ReadSet;
import org.deuce.transaction.estmstats.WriteSet;
import org.deuce.transform.Exclude;

import contention.benchmark.Statistics;
import contention.benchmark.Statistics.AbortType;
import contention.benchmark.Statistics.CommitType;


/**
 * E-STM w/ Statistics
 * An STM implementing regular and elastic transactions  
 * with detailed statistics
 * 
 * See the companion paper, Elastic Transactions [DISC '09]
 * 
 * @author Vincent Gramoli
 */
@Exclude
final public class Context implements org.deuce.transaction.Context {

	/** Type of the tx, (!elastic) means regular type */
	private boolean elastic;
	/** roregular means read-only regular type (atomic snapshot alike) */
	private boolean roregular = false;
	/**  The time lower bound above which the tx can be serialized */
	private int lb;
	/**  The time upper bound below which the tx can be serialized */
	private int ub;
	
	/**
	 * The transaction exceptions indicating that the current transaction
	 * must be restarted
	 */
	final private static TransactionException BETWEEN_SUCCESSIVE_READS_EXCEPTION =
		new TransactionException("Fail to read successively.");
	final private static TransactionException BETWEEN_READ_AND_WRITE_EXCEPTION =
		new TransactionException("Fail to read successively.");
	final private static TransactionException EXTEND_ON_READ_EXCEPTION =
		new TransactionException("Fail to extend (regular mode).");
	final private static TransactionException WRITE_AFTER_READ_EXCEPTION =
		new TransactionException("Fail to lock (already read value has changed).");
	final private static TransactionException LOCKED_BEFORE_READ_EXCEPTION =
		new TransactionException("Fail before reading (already locked by other).");
	final private static TransactionException LOCKED_BEFORE_ELASTIC_READ_EXCEPTION =
		new TransactionException("Fail while reading (already locked by other).");
	final private static TransactionException LOCKED_ON_READ_EXCEPTION =
		new TransactionException("Fail while reading (already locked by other).");
	final private static TransactionException LOCKED_ON_WRITE_EXCEPTION =
		new TransactionException("Fail to write (already locked by other).");
	final private static TransactionException INVALID_SNAPSHOT_EXCEPTION =
		new TransactionException("Fail to write (already locked by other).");
	
	/** Unique monotonically increasing transaction identifier */
	final private static AtomicInteger clock = new AtomicInteger(0);
	public static final AtomicInteger threadIdCounter = new AtomicInteger(1);
	private final int threadId = threadIdCounter.getAndIncrement();
	
	/**
	 * The last-read-entry set contains up to k=2 entries.
	 * 
	 * This is a hack of the C-based E-STM to check that the previous read is not 
	 * being made inconsistent by the garbage collector.
	 */
	final private LastReadEntries lreSet = new LastReadEntries();
	
	/** Read set, used by any regular reads or after an elastic write */
	final private ReadSet readSet = new ReadSet(1024);
	/** Write set, usual redo-log */
	final private WriteSet writeSet = new WriteSet(32);
	
	private int attempts=0;
	private final Statistics stats = new Statistics(threadId);	
	private static final Context[] threads = new Context[256];
	
	private int readHash;
	private int readLock;
	private Object readValue;

	public Context() {
		// Unique identifier among active threads
		threads[threadId] = this;
	}

	/**
	 * The begin delimiter of the transaction
	 */
	public void init(int blockId, String metainf) {
        elastic = (metainf.indexOf("elastic") != -1);
        if (!elastic) roregular = (metainf.indexOf("roregular") != -1);
		writeSet.clear();
		readSet.clear();
		lreSet.clear();
		lb = ub = clock.get();
		this.stats.reportTxStart();
		this.attempts++;
	}
	
	/**
	 * Gets the Id of this thread
	 * @return Id of this thread
	 */
	public final int getThreadId() {
		return threadId;
	}

	public final Statistics getStatistics() {
		return stats;
	}
	
    /* ---------------- TM interface  -------------- */
	
	/**
	 * The end delimiter of the transaction
	 */
	public boolean commit() {
		if (!writeSet.isEmpty()) {
			int newClock = clock.incrementAndGet();
			if (newClock != lb + 1 && !readSet.validate(threadId)) {
				rollback();
				this.stats.reportAbort(AbortType.INVALID_COMMIT); 
				return false;
			}
			// Write values and release locks
			writeSet.commit(newClock);
		}

		//this.stats.reportCommit(attempts);
		if (elastic) { this.stats.reportCommit(CommitType.ELASTIC); }
		else if (roregular) { this.stats.reportCommit(CommitType.READONLY); }
		else { this.stats.reportCommit(CommitType.UPDATE); }
		
		this.stats.reportOnCommit(readSet.size(), writeSet.size());
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
		if (readSet.validate(threadId)) {
			ub = now;
			return true;
		}
		return false;
	}

	public void beforeReadAccess(Object obj, long field) {
		readHash = LockTable.hash(obj, field);
		// Check if the field is locked (may throw an exception)
		readLock = LockTable.checkLock(readHash, threadId);
		if (readLock == -2) {
			if (elastic) {
				this.stats.reportAbort(AbortType.LOCKED_BEFORE_ELASTIC_READ);
				throw LOCKED_BEFORE_ELASTIC_READ_EXCEPTION;
			} else {
				this.stats.reportAbort(AbortType.LOCKED_BEFORE_READ);
				throw LOCKED_BEFORE_READ_EXCEPTION;
			}
		}
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

		//if (elastic) stats.reportElasticReads();
		//if (writeSet.isEmpty()) stats.reportInROPrefix();
		
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
				int lock = LockTable.checkLock(readHash, threadId);
				if (lock == -2) {
					stats.reportAbort(AbortType.LOCKED_ON_READ);
					throw LOCKED_ON_READ_EXCEPTION;
				} 
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
				if (!lreSet.validate(threadId, ub)) {
					stats.reportAbort(AbortType.BETWEEN_SUCCESSIVE_READS);
					throw BETWEEN_SUCCESSIVE_READS_EXCEPTION;
				}
				ub = readLock;
				return b;
			} else {
				// Try to extend snapshot
				if (!extend()) {
					// stats
					stats.reportAbort(AbortType.EXTEND_ON_READ);
					throw EXTEND_ON_READ_EXCEPTION;
				}
			}
		}
	}

	private void onWriteAccess(Object obj, long field, Object value, Type type) {
		
		int hash = LockTable.hash(obj, field);
		
		int timestamp = LockTable.lock(hash, threadId);
		if (timestamp == -2) {
			stats.reportAbort(AbortType.LOCKED_ON_WRITE);
			throw LOCKED_ON_WRITE_EXCEPTION;
		}
		
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
				// stats
				stats.reportAbort(AbortType.WRITE_AFTER_READ);
				throw WRITE_AFTER_READ_EXCEPTION;
			}
			// We delay validation until later (although we could already validate once here)
		}
		
		// Additional validation 
		if (elastic && !lreSet.validate(threadId, ub)) {
			LockTable.setAndReleaseLock(hash, timestamp);
			// stats
			stats.reportAbort(AbortType.BETWEEN_READ_AND_WRITE);
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
	
	@Exclude
	public static class LockTable {
		
		final private static int ARRAYSIZE = 1 << 20; // 2^20
		final private static int MASK = ARRAYSIZE - 1;
		final private static int LOCK = 1 << 31;
		final private static int IDMASK = LOCK - 1;

		// Array of 32-bit lock words
		final private static AtomicIntegerArray locks = new AtomicIntegerArray(ARRAYSIZE);

		public static int lock(int hash, int id) {
			assert hash <= MASK;
			while (true) {
				int lock = locks.get(hash);
				if ((lock & LOCK) != 0) {
					if ((lock & IDMASK) != id) {
						// Already locked by other thread
						return -2;
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

		public static int checkLock(int hash, int id) {
			assert hash <= MASK;
			int lock = locks.get(hash);
			if ((lock & LOCK) != 0) {
				if ((lock & IDMASK) != id) {
					// Already locked by other thread
					return -2;
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
	
	@Override
	public void beforeReadAccessStrongIso(Object obj, long field, Object obj2, long fieldObj) {
		System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
	}
    
    @Override
    public void onWriteAccessStrongIso( Object obj, Object value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }
    
    @Override
    public void onWriteAccessStrongIso( Object obj, boolean value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }
    
    @Override
    public void onWriteAccessStrongIso( Object obj, byte value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }
    
    @Override
    public void onWriteAccessStrongIso( Object obj, char value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }
    
    @Override
    public void onWriteAccessStrongIso( Object obj, short value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }
    
    @Override
    public void onWriteAccessStrongIso( Object obj, int value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }
    
    @Override
    public void onWriteAccessStrongIso( Object obj, long value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }
    
    @Override
    public void onWriteAccessStrongIso( Object obj, float value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }

    @Override
    public void onWriteAccessStrongIso( Object obj, double value, long field, long fieldObj) {
    	System.out.println("ERROR UNIMPLEMENTED STRONG ISO");
    }
}
