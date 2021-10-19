package org.deuce.transaction.estm;

import org.deuce.transaction.TransactionException;
import org.deuce.transaction.estm.Context.LockTable;
import org.deuce.transaction.estm.field.ReadFieldAccess;
import org.deuce.transform.Exclude;

/**
 * LastReadEntries implement a pool of k=2 read entries
 * 
 * @author Vincent Gramoli
 */
@Exclude
final public class LastReadEntries {

	protected ReadFieldAccess[] entries;
	private int size;
	private boolean first = true;	
	
	public LastReadEntries() {
		// by default, record k=2 read entries
		entries = new ReadFieldAccess[2];
		entries[0] = new ReadFieldAccess();
		entries[1] = new ReadFieldAccess();
		size = 0;
	}
	
	public void clear() {
		size = 0;
	}
	
	public void add(Object reference, long field, int hash, int lock) {
		if (size < 2) size++;
		ReadFieldAccess r = first ? entries[0] : entries[1];
		r.init(reference, field, hash, lock);
		first = !first;
	}

	public boolean isEmpty() {
		return (size==0);
	}
	
	public int getSize() {
		return size;
	}

	public boolean validate(int id, int timestamp) {
		try {
			for (int i = 0; i < size; i++) {
				// Throws an exception if validation fails
				int lock = LockTable.checkLock(entries[i].getHash(), id);
				if (lock > timestamp && lock >= 0) {
					return false;
				}
			}
		} catch (TransactionException e) {
			return false;
		}
		return true;
	}

	public boolean validate(int id) {
		try {
			for (int i = 0; i < size; i++) {
				// Throws an exception if validation fails
				ReadFieldAccess r = entries[i];
				int lock = LockTable.checkLock(r.getHash(), id);
				if (lock >= 0 && lock != r.getLock()) {
					// Other version: cannot validate
					return false;
				}
			}
		} catch (TransactionException e) {
			return false;
		}
		return true;
	}

	public boolean contains(Object obj, long field) {
		for (int i = 0; i < size; i++) {
			ReadFieldAccess r = entries[i];
			if (r.getField() == field && r.getReference() == obj)
				return true;
		}
		return false;
	}

}
