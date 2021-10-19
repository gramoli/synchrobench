package org.deuce.transaction.strongiso;

import java.util.Iterator;
import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.AddressUtil;
import org.deuce.reflection.UnsafeHolder;
import org.deuce.transaction.TransactionException;
import org.deuce.transaction.strongiso.field.BooleanWriteFieldAccess;
import org.deuce.transaction.strongiso.field.ByteWriteFieldAccess;
import org.deuce.transaction.strongiso.field.CharWriteFieldAccess;
import org.deuce.transaction.strongiso.field.DoubleWriteFieldAccess;
import org.deuce.transaction.strongiso.field.FloatWriteFieldAccess;
import org.deuce.transaction.strongiso.field.IntWriteFieldAccess;
import org.deuce.transaction.strongiso.field.LongWriteFieldAccess;
import org.deuce.transaction.strongiso.field.ObjectWriteFieldAccess;
import org.deuce.transaction.strongiso.field.ReadFieldAccess;
import org.deuce.transaction.strongiso.field.ShortWriteFieldAccess;
import org.deuce.transaction.strongiso.field.TransStatus;
import org.deuce.transaction.strongiso.field.WriteFieldAccess;
import org.deuce.transaction.strongiso.pool.Pool;
import org.deuce.transaction.strongiso.pool.ResourceFactory;
import org.deuce.transform.Exclude;
import org.deuce.trove.TObjectProcedure;

import contention.benchmark.Counters;
import contention.benchmark.Statistics.CommitType;

/**
 * TL2 implementation
 * 
 * @author Guy Korland
 * @since 1.0
 */
@Exclude
final public class Context implements org.deuce.transaction.Context {

	final private static TransactionException FAILURE_NT = new TransactionException(
			"Failure NT-read");
	final private static TransactionException FAILURE_T = new TransactionException(
			"Failure T-read");
	final private static TransactionException FAILURE_LOCK = new TransactionException(
			"Failure on lock");

	final static AtomicInteger clock = new AtomicInteger(1);
	final static AtomicInteger threadCounter = new AtomicInteger(0);

	final static AtomicInteger committedHolder = new AtomicInteger(
			TransStatus.COMMITTED);

	final private PointerReadSet readSet = new PointerReadSet();
	final private WriteSet writeSet = new WriteSet();
	final private int threadId;

	AtomicInteger status = null;

	// Marked on beforeRead, used for the double lock check
	private int localClock;
	private long lastField;
	
	// Mark if this is an NT transaction
	private boolean isNT = false;
	private boolean volatileTest = false;

	public Context() {
		this.localClock = clock.get();
		this.threadId = threadCounter.getAndIncrement();
	}

	public void init(int atomicBlockId, String metainf) {
		if(metainf.indexOf("NT") != -1) {
			this.isNT = true;
			this.volatileTest = false;
			return;
		}
		if(metainf.indexOf("volatile") != -1) {
			this.volatileTest = true;
			this.isNT = false;
			return;
		}
		this.volatileTest = false;
		this.isNT = false;
		this.readSet.clear();
		this.writeSet.clear();
		this.localClock = clock.get();

		if (status == null) {
			status = new AtomicInteger();
		}
		// this.objectPool.clear();
		// this.booleanPool.clear();
		// this.bytePool.clear();
		// this.charPool.clear();
		// this.shortPool.clear();
		// this.intPool.clear();
		// this.longPool.clear();
		// this.floatPool.clear();
		// this.doublePool.clear();
	}

	public boolean commit() {
		if (this.isNT || this.volatileTest || writeSet.isEmpty()) // if the writeSet is empty no need to lock a
								// thing.
			return true;

		WriteFieldAccess next, check;
		int checkStatus;

		Iterator<WriteFieldAccess> wsIter = writeSet.getWsIterator();
		while (wsIter.hasNext()) {
			next = wsIter.next();
			check = (WriteFieldAccess) UnsafeHolder.getUnsafe()
					.getObjectVolatile(next.reference, next.field);
			if (check != null) {
				if (!check.isNT
						&& (checkStatus = check.status.get()) != TransStatus.COMMITTED) {
					if (checkStatus == TransStatus.LIVE) {
						status.set(TransStatus.ABORTED);
						status = null;
						return false;
					}
					next.setLast(check, true);
				} else {
					next.setLast(check, false);
				}
				// TODO: is this time setting safe?
				int time = check.time;
				if (time == Integer.MAX_VALUE) {
					next.time = clock.get();
				} else {
					next.time = check.time;
				}
			} else {
				// Here we just use the value directly in memory because this is the first write to this location
				next.setLast();
				// TODO: is this time setting safe?
				next.time = 0;
			}
			if (!UnsafeHolder.getUnsafe().compareAndSwapObject(next.reference,
					next.field, check, next)) {
				status.set(TransStatus.ABORTED);
				status = null;
				return false;
			}
		}

		// this.localClock = clock.getAndIncrement();
		this.localClock = clock.incrementAndGet();

		if (!readSet.validateAndNullRs(threadId)) {
			status.set(TransStatus.ABORTED);
			status = null;
			return false;

		}

		wsIter = writeSet.getWsIterator();

		// boolean abort = false;
		while (wsIter.hasNext()) {
			next = wsIter.next();
			next.time = this.localClock;
			if (next != (WriteFieldAccess) UnsafeHolder.getUnsafe()
					.getObjectVolatile(next.reference, next.field)) {
				// abort = true;
				status.set(TransStatus.ABORTED);
				status = null;
				return false;

			}
		}
		// if (abort) {
		// status.set(TransStatus.ABORTED);
		// status = null;
		// return false;
		// }

		if (!status.compareAndSet(TransStatus.LIVE, TransStatus.COMMITTED)) {
			status.set(TransStatus.ABORTED);
			status = null;
			return false;
		}
		status = null;
		return true;
	}

	public void rollback() {
	}

	private boolean onNTReadAccess1(WriteFieldAccess write) {
		boolean useOld = false;
		if (!write.isNT) {
			int status = write.status.get();
			// Doing only linearizability here, so don't need to check time
			if (status == TransStatus.LIVE) {
				write.status.compareAndSet(TransStatus.LIVE,
						TransStatus.ABORTED);
				status = write.status.get();
			}
			if (status == TransStatus.ABORTED) {
				useOld = true;
			}
		}

		// Time check is not needed unless we doing serializablilty
		// int time = write.time;
		// if (time == Integer.MAX_VALUE) {
		// time = clock.get();
		// }
		// this.localClock = Math.max(this.localClock, time);

		return useOld;
	}

	// private WriteFieldAccess onReadAccess0(Object obj, long field) {
	//
	// ReadFieldAccess current = currentReadFieldAccess;
	// int hash = current.hashCode();
	//
	// // Check the read is still valid
	// LockTable.checkLock(hash, localClock, lastReadLock);
	//
	// // Check if it is already included in the write set
	// return writeSet.contains(current);
	// }

	private boolean onReadAccess1(WriteFieldAccess write) {
		boolean useOld = false;
		if (write.isNT) {
			if (write.time >= localClock)
				throw FAILURE_NT;
			// TODO: Validation (note: need to add to read set before
			// validation)
		} else {
			int status = write.status.get();
			if (status != TransStatus.COMMITTED) {
				if (status == TransStatus.LIVE)
					throw FAILURE_LOCK;
				useOld = true;
			}
			if (write.time > localClock)
				throw FAILURE_T;
		}
		readSet.add(write);
		return useOld;
	}

	// private void addWriteAccess0(WriteFieldAccess write) {
	//
	// // Add to write set
	// writeSet.put(write);
	// }

	public void beforeReadAccess(Object obj, long field) {

		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
		//
		// ReadFieldAccess next = readSet.getNext();
		// currentReadFieldAccess = next;
		// next.init(obj, field);
		//
		// // Check the read is still valid
		// lastReadLock = LockTable.checkLock(next.hashCode(), localClock);
	}

	@Override
	public void beforeReadAccessStrongIso(Object obj, long field, Object obj2,
			long fieldObj) {

		this.lastField = fieldObj;
	}
	
	
	
	
	
	
//	// Non-transactional Reads
//	public Object onNTReadAccess(Object obj, Object value, long field) {
//		ObjectWriteFieldAccess objVal = (ObjectWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//
//	public boolean onReadNTAccess(Object obj, boolean value, long field) {
//		BooleanWriteFieldAccess objVal = (BooleanWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//
//	public byte onNTReadAccess(Object obj, byte value, long field) {
//		ByteWriteFieldAccess objVal = (ByteWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//
//	public char onNTReadAccess(Object obj, char value, long field) {
//		CharWriteFieldAccess objVal = (CharWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//
//	public short onNTReadAccess(Object obj, short value, long field) {
//		ShortWriteFieldAccess objVal = (ShortWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//
//	public int onNTReadAccess(Object obj, int value, long field) {
//		IntWriteFieldAccess objVal = (IntWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//
//	public long onNTReadAccess(Object obj, long value, long field) {
//		LongWriteFieldAccess objVal = (LongWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//
//	public float onNTReadAccess(Object obj, float value, long field) {
//		FloatWriteFieldAccess objVal = (FloatWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//
//	public double onNTReadAccess(Object obj, double value, long field) {
//		DoubleWriteFieldAccess objVal = (DoubleWriteFieldAccess) UnsafeHolder
//				.getUnsafe().getObjectVolatile(obj, this.lastField);
//		if (objVal == null) {
//			return value;
//		}
//		return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
//	}
//	

	
	
	
	
	
	
	
	
	
	
	//  Transactional Reads
	public Object onReadAccess(Object obj, Object value, long field) {
		if(this.isNT) {
			ObjectWriteFieldAccess objVal = (ObjectWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		ObjectWriteFieldAccess writeAccess = (ObjectWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		ObjectWriteFieldAccess objVal = (ObjectWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new ObjectWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
	}

	public boolean onReadAccess(Object obj, boolean value, long field) {
		if(this.isNT) {
			BooleanWriteFieldAccess objVal = (BooleanWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		BooleanWriteFieldAccess writeAccess = (BooleanWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		BooleanWriteFieldAccess objVal = (BooleanWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new BooleanWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
	}

	public byte onReadAccess(Object obj, byte value, long field) {
		if(this.isNT) {
			ByteWriteFieldAccess objVal = (ByteWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		ByteWriteFieldAccess writeAccess = (ByteWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		ByteWriteFieldAccess objVal = (ByteWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new ByteWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
	}

	public char onReadAccess(Object obj, char value, long field) {
		if(this.isNT) {
			CharWriteFieldAccess objVal = (CharWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		CharWriteFieldAccess writeAccess = (CharWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		CharWriteFieldAccess objVal = (CharWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new CharWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
	}

	public short onReadAccess(Object obj, short value, long field) {
		if(this.isNT) {
			ShortWriteFieldAccess objVal = (ShortWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		ShortWriteFieldAccess writeAccess = (ShortWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		ShortWriteFieldAccess objVal = (ShortWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new ShortWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();

	}

	public int onReadAccess(Object obj, int value, long field) {
		if(this.isNT) {
			IntWriteFieldAccess objVal = (IntWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		if(this.volatileTest) {
			return UnsafeHolder.getUnsafe().getIntVolatile(obj, field);
		}
		IntWriteFieldAccess writeAccess = (IntWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		IntWriteFieldAccess objVal = (IntWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new IntWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
	}

	public long onReadAccess(Object obj, long value, long field) {
		if(this.isNT) {
			LongWriteFieldAccess objVal = (LongWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		LongWriteFieldAccess writeAccess = (LongWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		LongWriteFieldAccess objVal = (LongWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new LongWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
	}

	public float onReadAccess(Object obj, float value, long field) {
		if(this.isNT) {
			FloatWriteFieldAccess objVal = (FloatWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		FloatWriteFieldAccess writeAccess = (FloatWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		FloatWriteFieldAccess objVal = (FloatWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new FloatWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
	}

	public double onReadAccess(Object obj, double value, long field) {
		if(this.isNT) {
			DoubleWriteFieldAccess objVal = (DoubleWriteFieldAccess) UnsafeHolder
					.getUnsafe().getObjectVolatile(obj, this.lastField);
			if (objVal == null) {
				return value;
			}
			return onNTReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
		}
		DoubleWriteFieldAccess writeAccess = (DoubleWriteFieldAccess) writeSet
				.contains(obj, this.lastField);
		if (writeAccess != null)
			return writeAccess.getValue();
		DoubleWriteFieldAccess objVal = (DoubleWriteFieldAccess) UnsafeHolder
				.getUnsafe().getObjectVolatile(obj, this.lastField);
		if (objVal == null) {
			readSet.add(new DoubleWriteFieldAccess(value, obj, field,
					this.lastField, committedHolder, false, this.threadId));
			return value;
		}
		return onReadAccess1(objVal) ? objVal.getOldValue() : objVal.getValue();
	}

	public void onWriteAccess(Object obj, Object value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	public void onWriteAccess(Object obj, boolean value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	public void onWriteAccess(Object obj, byte value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	public void onWriteAccess(Object obj, char value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	public void onWriteAccess(Object obj, short value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	public void onWriteAccess(Object obj, int value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	public void onWriteAccess(Object obj, long value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	public void onWriteAccess(Object obj, float value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	public void onWriteAccess(Object obj, double value, long field) {
		System.out.println("ERROR NONSTRONGISO NOT IMPLEMENTED");
	}

	
	
	
	
	
//	
//	// Nontransactional writes
//	public void onNTWriteAccessStrongIso(Object obj, Object value, long field,
//			long fieldObj) {
//
//		ObjectWriteFieldAccess next = new ObjectWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//		
//	}
//
//	public void onNTWriteAccessStrongIso(Object obj, boolean value, long field,
//			long fieldObj) {
//
//		BooleanWriteFieldAccess next = new BooleanWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//	}
//
//	public void onNTWriteAccessStrongIso(Object obj, byte value, long field,
//			long fieldObj) {
//		ByteWriteFieldAccess next = new ByteWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//	}
//
//	public void onNTWriteAccessStrongIso(Object obj, char value, long field,
//			long fieldObj) {
//		CharWriteFieldAccess next = new CharWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//	}
//
//	public void onNTWriteAccessStrongIso(Object obj, short value, long field,
//			long fieldObj) {
//		ShortWriteFieldAccess next = new ShortWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//	}
//
//	public void onNTWriteAccessStrongIso(Object obj, int value, long field,
//			long fieldObj) {
//		IntWriteFieldAccess next = new IntWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//	}
//
//	public void onNTWriteAccessStrongIso(Object obj, long value, long field,
//			long fieldObj) {
//		LongWriteFieldAccess next = new LongWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//	}
//
//	public void onNTWriteAccessStrongIso(Object obj, float value, long field,
//			long fieldObj) {
//		FloatWriteFieldAccess next = new FloatWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//	}
//
//	public void onNTWriteAccessStrongIso(Object obj, double value, long field,
//			long fieldObj) {
//		DoubleWriteFieldAccess next = new DoubleWriteFieldAccess(value, obj,
//				field, fieldObj, null, true, threadId);
//
//		UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
//	}
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	// Transactional writes
	public void onWriteAccessStrongIso(Object obj, Object value, long field,
			long fieldObj) {
		if(this.isNT) {
			ObjectWriteFieldAccess next = new ObjectWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		ObjectWriteFieldAccess next = new ObjectWriteFieldAccess(value, obj,
				field, fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	public void onWriteAccessStrongIso(Object obj, boolean value, long field,
			long fieldObj) {
		if(this.isNT) {
			BooleanWriteFieldAccess next = new BooleanWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		BooleanWriteFieldAccess next = new BooleanWriteFieldAccess(value, obj,
				field, fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	public void onWriteAccessStrongIso(Object obj, byte value, long field,
			long fieldObj) {
		if(this.isNT) {
			ByteWriteFieldAccess next = new ByteWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		ByteWriteFieldAccess next = new ByteWriteFieldAccess(value, obj, field,
				fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	public void onWriteAccessStrongIso(Object obj, char value, long field,
			long fieldObj) {
		if(this.isNT) {
			CharWriteFieldAccess next = new CharWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		CharWriteFieldAccess next = new CharWriteFieldAccess(value, obj, field,
				fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	public void onWriteAccessStrongIso(Object obj, short value, long field,
			long fieldObj) {
		if(this.isNT) {
			ShortWriteFieldAccess next = new ShortWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		ShortWriteFieldAccess next = new ShortWriteFieldAccess(value, obj,
				field, fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	public void onWriteAccessStrongIso(Object obj, int value, long field,
			long fieldObj) {
		if(this.isNT) {
			IntWriteFieldAccess next = new IntWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		if(this.volatileTest) {
			UnsafeHolder.getUnsafe().putIntVolatile(obj, field, value);
		}
		IntWriteFieldAccess next = new IntWriteFieldAccess(value, obj, field,
				fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	public void onWriteAccessStrongIso(Object obj, long value, long field,
			long fieldObj) {
		if(this.isNT) {
			LongWriteFieldAccess next = new LongWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		LongWriteFieldAccess next = new LongWriteFieldAccess(value, obj, field,
				fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	public void onWriteAccessStrongIso(Object obj, float value, long field,
			long fieldObj) {
		if(this.isNT) {
			FloatWriteFieldAccess next = new FloatWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		FloatWriteFieldAccess next = new FloatWriteFieldAccess(value, obj,
				field, fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	public void onWriteAccessStrongIso(Object obj, double value, long field,
			long fieldObj) {
		if(this.isNT) {
			DoubleWriteFieldAccess next = new DoubleWriteFieldAccess(value, obj,
					field, fieldObj, null, true, threadId);

			UnsafeHolder.getUnsafe().putObjectVolatile(obj, fieldObj, next);
			return;
		}
		DoubleWriteFieldAccess next = new DoubleWriteFieldAccess(value, obj,
				field, fieldObj, status, false, threadId);
		writeSet.put(next);
	}

	// private static class ObjectResourceFactory implements
	// ResourceFactory<ObjectWriteFieldAccess>{
	// @Override
	// public ObjectWriteFieldAccess newInstance() {
	// return new ObjectWriteFieldAccess();
	// }
	// }
	// final private Pool<ObjectWriteFieldAccess> objectPool = new
	// Pool<ObjectWriteFieldAccess>(new ObjectResourceFactory());
	//
	// private static class BooleanResourceFactory implements
	// ResourceFactory<BooleanWriteFieldAccess>{
	// @Override
	// public BooleanWriteFieldAccess newInstance() {
	// return new BooleanWriteFieldAccess();
	// }
	// }
	// final private Pool<BooleanWriteFieldAccess> booleanPool = new
	// Pool<BooleanWriteFieldAccess>(new BooleanResourceFactory());
	//
	// private static class ByteResourceFactory implements
	// ResourceFactory<ByteWriteFieldAccess>{
	// @Override
	// public ByteWriteFieldAccess newInstance() {
	// return new ByteWriteFieldAccess();
	// }
	// }
	// final private Pool<ByteWriteFieldAccess> bytePool = new
	// Pool<ByteWriteFieldAccess>( new ByteResourceFactory());
	//
	// private static class CharResourceFactory implements
	// ResourceFactory<CharWriteFieldAccess>{
	// @Override
	// public CharWriteFieldAccess newInstance() {
	// return new CharWriteFieldAccess();
	// }
	// }
	// final private Pool<CharWriteFieldAccess> charPool = new
	// Pool<CharWriteFieldAccess>(new CharResourceFactory());
	//
	// private static class ShortResourceFactory implements
	// ResourceFactory<ShortWriteFieldAccess>{
	// @Override
	// public ShortWriteFieldAccess newInstance() {
	// return new ShortWriteFieldAccess();
	// }
	// }
	// final private Pool<ShortWriteFieldAccess> shortPool = new
	// Pool<ShortWriteFieldAccess>( new ShortResourceFactory());
	//
	// private static class IntResourceFactory implements
	// ResourceFactory<IntWriteFieldAccess>{
	// @Override
	// public IntWriteFieldAccess newInstance() {
	// return new IntWriteFieldAccess();
	// }
	// }
	// final private Pool<IntWriteFieldAccess> intPool = new
	// Pool<IntWriteFieldAccess>( new IntResourceFactory());
	//
	// private static class LongResourceFactory implements
	// ResourceFactory<LongWriteFieldAccess>{
	// @Override
	// public LongWriteFieldAccess newInstance() {
	// return new LongWriteFieldAccess();
	// }
	// }
	// final private Pool<LongWriteFieldAccess> longPool = new
	// Pool<LongWriteFieldAccess>( new LongResourceFactory());
	//
	// private static class FloatResourceFactory implements
	// ResourceFactory<FloatWriteFieldAccess>{
	// @Override
	// public FloatWriteFieldAccess newInstance() {
	// return new FloatWriteFieldAccess();
	// }
	// }
	// final private Pool<FloatWriteFieldAccess> floatPool = new
	// Pool<FloatWriteFieldAccess>( new FloatResourceFactory());
	//
	// private static class DoubleResourceFactory implements
	// ResourceFactory<DoubleWriteFieldAccess>{
	// @Override
	// public DoubleWriteFieldAccess newInstance() {
	// return new DoubleWriteFieldAccess();
	// }
	// }
	// final private Pool<DoubleWriteFieldAccess> doublePool = new
	// Pool<DoubleWriteFieldAccess>( new DoubleResourceFactory());

	// @Override
	// public void beforeReadAccessStrongIso(Object obj, long field, Object
	// obj2, long fieldObj) {
	// try {
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("counterOne")));
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("counterOne__OBJECT__")));
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("counterTwo")));
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("counterTwo__OBJECT__")));
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("counterThree")));
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("counterThree__OBJECT__")));
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("counterFour")));
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("counterFour__OBJECT__")));
	// System.out.println(AddressUtil.getAddress(Counters.class.getDeclaredField("failuresCounter")));
	// } catch (Exception e) {
	// // TODO Auto-generated catch block
	// e.printStackTrace();
	// }
	// UnsafeHolder.getUnsafe().putObject(obj, fieldObj, new
	// ReadFieldAccess("This is a string obj", -1));
	// this.beforeReadAccess(obj, field);
	// }

}
