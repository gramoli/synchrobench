package org.deuce.transaction.estmstats;

import org.deuce.transaction.estmstats.field.WriteFieldAccess;
import org.deuce.transaction.estmstats.field.ReadFieldAccess.Field.Type;
import org.deuce.transaction.estmstats.Context.LockTable;
import org.deuce.transform.Exclude;
import org.deuce.trove.THashMap;

/**
 * @author Vincent Gramoli
 */
@Exclude
final public class WriteSet {

	private static final int DEFAULT_CAPACITY = 16;

	final private THashMap<Integer, WriteFieldAccess> entries;
	private boolean empty = true;

	public WriteSet(int initialCapacity) {
		entries = new THashMap<Integer, WriteFieldAccess>(initialCapacity);
	}

	public WriteSet() {
		this(DEFAULT_CAPACITY);
	}
	
	public int size() {
		return entries.size();
	}

	public void clear() {
		entries.clear();
		empty = true;
	}

	public boolean isEmpty() {
		return empty;
	}

	/**
	 * Indicates whether the given field corresponds
	 * to an existing write entry of the read set
	 * 
	 * @param hash the identifier of the field
	 * @param obj its object
	 * @param field the protected field
	 * @return the value if owned, null otherwise
	 */
	public Object getValue(int hash, Object obj, long field) {
		WriteFieldAccess w = this.get(hash, obj, field);
		return (w != null) ? w.getValue() : null;
	}
	
	/**
	 * Returns the corresponding write entry
	 * 
	 * @param hash the identifier of the field
	 * @param obj its object
	 * @param field the protected field
	 * @return the value if owned, null otherwise
	 */
	public WriteFieldAccess get(int hash, Object obj, long field) {
		// Return value from existing entry
		WriteFieldAccess w = entries.get(hash);
		while (w != null) {
			// Check if we have already written that field
			if (w.getField() == field && w.getReference() == obj)
				return w;
			w = w.getNext();
		}
		return null;
	}

	public void append(int hash, Object obj, long field, Object value, Type type) {
		// Append to existing entry
		WriteFieldAccess w = entries.get(hash);
		while (w != null) {
			// Check if we have already written that field
			if (w.getField() == field && w.getReference() == obj) {
				// Update written value
				w.setValue(value);
				return;
			}
			WriteFieldAccess next = w.getNext();
			if (next == null) {
				// We did not write this field (we must add it to write set)
				w.setNext(new WriteFieldAccess(obj, field, type, value, hash, 0));
				return;
			}
			w = next;
		}
	}

	public void add(int hash, Object obj, long field, Object value, Type type, int timestamp) {
		// Add new entry
		entries.put(hash, new WriteFieldAccess(obj, field, type, value, hash, timestamp));
		empty = false;
	}

	public void commit(int timestamp) {
		// Write values and release locks
		for (WriteFieldAccess w : entries.values()) {
			int hash = w.getHash();
			assert w.getLock() >= 0;
			do {
				w.writeField();
				w = w.getNext();
			} while (w != null);
			LockTable.setAndReleaseLock(hash, timestamp);
		}
	}

	public void rollback() {
		// Release locks
		for (WriteFieldAccess w : entries.values()) {
			assert w.getLock() >= 0;
			LockTable.setAndReleaseLock(w.getHash(), w.getLock());
		}
	}
	

}
