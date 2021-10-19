package org.deuce.transaction.tl2;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.locks.ReentrantReadWriteLock;

import org.deuce.transaction.TransactionException;
import org.deuce.transaction.tl2.field.BooleanWriteFieldAccess;
import org.deuce.transaction.tl2.field.ByteWriteFieldAccess;
import org.deuce.transaction.tl2.field.CharWriteFieldAccess;
import org.deuce.transaction.tl2.field.DoubleWriteFieldAccess;
import org.deuce.transaction.tl2.field.FloatWriteFieldAccess;
import org.deuce.transaction.tl2.field.IntWriteFieldAccess;
import org.deuce.transaction.tl2.field.LongWriteFieldAccess;
import org.deuce.transaction.tl2.field.ObjectWriteFieldAccess;
import org.deuce.transaction.tl2.field.ReadFieldAccess;
import org.deuce.transaction.tl2.field.ShortWriteFieldAccess;
import org.deuce.transaction.tl2.field.WriteFieldAccess;
import org.deuce.transaction.tl2.pool.Pool;
import org.deuce.transaction.tl2.pool.ResourceFactory;
import org.deuce.transform.Exclude;
import org.deuce.trove.TObjectProcedure;

/**
 * TL2 implementation
 * 
 * @author Guy Korland
 * @since 1.0
 */
@Exclude
final public class Context implements org.deuce.transaction.Context {

	final static AtomicInteger clock = new AtomicInteger(0);

	final private ReadSet readSet = new ReadSet();
	final private WriteSet writeSet = new WriteSet();

	private ReadFieldAccess currentReadFieldAccess = null;

	// Used by the thread to mark locks it holds.
	final private byte[] locksMarker = new byte[LockTable.LOCKS_SIZE / 8 + 1];
	final private LockProcedure lockProcedure = new LockProcedure(locksMarker);

	// Marked on beforeRead, used for the double lock check
	private int localClock;
	private int lastReadLock;

	// Global lock used to allow only one irrevocable transaction solely.
	// final private static ReentrantReadWriteLock irrevocableAccessLock = new
	// ReentrantReadWriteLock();
	// private boolean irrevocableState = false;

	final private TObjectProcedure<WriteFieldAccess> putProcedure = new TObjectProcedure<WriteFieldAccess>() {
		@Override
		public boolean execute(WriteFieldAccess writeField) {
			writeField.put();
			return true;
		}
	};

	public Context() {
		this.localClock = clock.get();
	}

	@Override
	public void init(int atomicBlockId, String metainf) {
		this.currentReadFieldAccess = null;
		this.readSet.clear();
		this.writeSet.clear();
		this.objectPool.clear();
		this.booleanPool.clear();
		this.bytePool.clear();
		this.charPool.clear();
		this.shortPool.clear();
		this.intPool.clear();
		this.longPool.clear();
		this.floatPool.clear();
		this.doublePool.clear();

		// Lock according to the transaction irrevocable state
		// if (irrevocableState)
		// irrevocableAccessLock.writeLock().lock();
		// else
		// irrevocableAccessLock.readLock().lock();

		this.localClock = clock.get();
	}

	@Override
	public boolean commit() {
		// try {
		if (writeSet.isEmpty()) // if the writeSet is empty no need to lock
								// a thing.
			return true;

		try {
			// pre commit validation phase
			writeSet.forEach(lockProcedure);
			readSet.checkClock(localClock, locksMarker);
		} catch (TransactionException exception) {
			lockProcedure.unlockAll();
			return false;
		}

		// commit new values and release locks
		writeSet.forEach(putProcedure);
		lockProcedure.setAndUnlockAll();
		return true;
		// } finally {
		// if (irrevocableState) {
		// irrevocableState = false;
		// irrevocableAccessLock.writeLock().unlock();
		// } else {
		// irrevocableAccessLock.readLock().unlock();
		// }

		// }
	}

	@Override
	public void rollback() {
		// irrevocableAccessLock.readLock().unlock();
	}

	private WriteFieldAccess onReadAccess0(Object obj, long field) {

		ReadFieldAccess current = currentReadFieldAccess;
		int hash = current.hashCode();

		// Check the read is still valid
		LockTable.checkLock(hash, localClock, lastReadLock);

		// Check if it is already included in the write set
		return writeSet.contains(current);
	}

	private void addWriteAccess0(WriteFieldAccess write) {

		// Add to write set
		writeSet.put(write);
	}

	@Override
	public void beforeReadAccess(Object obj, long field) {

		ReadFieldAccess next = readSet.getNext();
		currentReadFieldAccess = next;
		next.init(obj, field);

		// Check the read is still valid
		lastReadLock = LockTable.checkLock(next.hashCode(), localClock);
	}

	@Override
	public Object onReadAccess(Object obj, Object value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((ObjectWriteFieldAccess) writeAccess).getValue();
	}

	@Override
	public boolean onReadAccess(Object obj, boolean value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((BooleanWriteFieldAccess) writeAccess).getValue();
	}

	@Override
	public byte onReadAccess(Object obj, byte value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((ByteWriteFieldAccess) writeAccess).getValue();
	}

	@Override
	public char onReadAccess(Object obj, char value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((CharWriteFieldAccess) writeAccess).getValue();
	}

	@Override
	public short onReadAccess(Object obj, short value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((ShortWriteFieldAccess) writeAccess).getValue();

	}

	@Override
	public int onReadAccess(Object obj, int value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((IntWriteFieldAccess) writeAccess).getValue();
	}

	@Override
	public long onReadAccess(Object obj, long value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((LongWriteFieldAccess) writeAccess).getValue();
	}

	@Override
	public float onReadAccess(Object obj, float value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((FloatWriteFieldAccess) writeAccess).getValue();
	}

	@Override
	public double onReadAccess(Object obj, double value, long field) {
		WriteFieldAccess writeAccess = onReadAccess0(obj, field);
		if (writeAccess == null)
			return value;

		return ((DoubleWriteFieldAccess) writeAccess).getValue();
	}

	@Override
	public void onWriteAccess(Object obj, Object value, long field) {
		ObjectWriteFieldAccess next = objectPool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	@Override
	public void onWriteAccess(Object obj, boolean value, long field) {

		BooleanWriteFieldAccess next = booleanPool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	@Override
	public void onWriteAccess(Object obj, byte value, long field) {

		ByteWriteFieldAccess next = bytePool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	@Override
	public void onWriteAccess(Object obj, char value, long field) {

		CharWriteFieldAccess next = charPool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	@Override
	public void onWriteAccess(Object obj, short value, long field) {

		ShortWriteFieldAccess next = shortPool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	@Override
	public void onWriteAccess(Object obj, int value, long field) {

		IntWriteFieldAccess next = intPool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	@Override
	public void onWriteAccess(Object obj, long value, long field) {

		LongWriteFieldAccess next = longPool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	@Override
	public void onWriteAccess(Object obj, float value, long field) {

		FloatWriteFieldAccess next = floatPool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	@Override
	public void onWriteAccess(Object obj, double value, long field) {

		DoubleWriteFieldAccess next = doublePool.getNext();
		next.set(value, obj, field);
		addWriteAccess0(next);
	}

	private static class ObjectResourceFactory implements
			ResourceFactory<ObjectWriteFieldAccess> {
		@Override
		public ObjectWriteFieldAccess newInstance() {
			return new ObjectWriteFieldAccess();
		}
	}

	final private Pool<ObjectWriteFieldAccess> objectPool = new Pool<ObjectWriteFieldAccess>(
			new ObjectResourceFactory());

	private static class BooleanResourceFactory implements
			ResourceFactory<BooleanWriteFieldAccess> {
		@Override
		public BooleanWriteFieldAccess newInstance() {
			return new BooleanWriteFieldAccess();
		}
	}

	final private Pool<BooleanWriteFieldAccess> booleanPool = new Pool<BooleanWriteFieldAccess>(
			new BooleanResourceFactory());

	private static class ByteResourceFactory implements
			ResourceFactory<ByteWriteFieldAccess> {
		@Override
		public ByteWriteFieldAccess newInstance() {
			return new ByteWriteFieldAccess();
		}
	}

	final private Pool<ByteWriteFieldAccess> bytePool = new Pool<ByteWriteFieldAccess>(
			new ByteResourceFactory());

	private static class CharResourceFactory implements
			ResourceFactory<CharWriteFieldAccess> {
		@Override
		public CharWriteFieldAccess newInstance() {
			return new CharWriteFieldAccess();
		}
	}

	final private Pool<CharWriteFieldAccess> charPool = new Pool<CharWriteFieldAccess>(
			new CharResourceFactory());

	private static class ShortResourceFactory implements
			ResourceFactory<ShortWriteFieldAccess> {
		@Override
		public ShortWriteFieldAccess newInstance() {
			return new ShortWriteFieldAccess();
		}
	}

	final private Pool<ShortWriteFieldAccess> shortPool = new Pool<ShortWriteFieldAccess>(
			new ShortResourceFactory());

	private static class IntResourceFactory implements
			ResourceFactory<IntWriteFieldAccess> {
		@Override
		public IntWriteFieldAccess newInstance() {
			return new IntWriteFieldAccess();
		}
	}

	final private Pool<IntWriteFieldAccess> intPool = new Pool<IntWriteFieldAccess>(
			new IntResourceFactory());

	private static class LongResourceFactory implements
			ResourceFactory<LongWriteFieldAccess> {
		@Override
		public LongWriteFieldAccess newInstance() {
			return new LongWriteFieldAccess();
		}
	}

	final private Pool<LongWriteFieldAccess> longPool = new Pool<LongWriteFieldAccess>(
			new LongResourceFactory());

	private static class FloatResourceFactory implements
			ResourceFactory<FloatWriteFieldAccess> {
		@Override
		public FloatWriteFieldAccess newInstance() {
			return new FloatWriteFieldAccess();
		}
	}

	final private Pool<FloatWriteFieldAccess> floatPool = new Pool<FloatWriteFieldAccess>(
			new FloatResourceFactory());

	private static class DoubleResourceFactory implements
			ResourceFactory<DoubleWriteFieldAccess> {
		@Override
		public DoubleWriteFieldAccess newInstance() {
			return new DoubleWriteFieldAccess();
		}
	}

	final private Pool<DoubleWriteFieldAccess> doublePool = new Pool<DoubleWriteFieldAccess>(
			new DoubleResourceFactory());

	// @Override
	// public void onIrrevocableAccess() {
	// if(irrevocableState) // already in irrevocable state so no need to
	// restart transaction.
	// return;
	//
	// irrevocableState = true;
	// throw TransactionException.STATIC_TRANSACTION;
	// }

	@Override
	public void beforeReadAccessStrongIso(Object obj, long field, Object obj2,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, Object value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, boolean value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, byte value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, char value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, short value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, int value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, long value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, float value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

	@Override
	public void onWriteAccessStrongIso(Object obj, double value, long field,
			long fieldObj) {
		System.out.println("ERROR STRONG ISO NOT IMPLEMENTED");

	}

}
