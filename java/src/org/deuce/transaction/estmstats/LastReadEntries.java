package org.deuce.transaction.estmstats;

import org.deuce.transaction.estmstats.Context.LockTable;
import org.deuce.transaction.estmstats.field.ReadFieldAccess;
import org.deuce.transform.Exclude;

/**
 * LastReadEntries implement a rotating buffer of k read entries
 * 
 * @author Vincent Gramoli
 */
@Exclude
final public class LastReadEntries {

	protected ReadFieldAccess[] entries;
	private int size;
	//private boolean first = true;	
	// the elasticity parameter (capacity of the elastic rotating buffer)
	private final int elasticity = 2; 
	private int marker = 0;
	
	public LastReadEntries() {
		// by default, record k=2 read entries
		entries = new ReadFieldAccess[elasticity];
		for (int i=0; i<elasticity; i++) {
			entries[i] = new ReadFieldAccess();
		}
		size = 0;
	}
	
	public void clear() {
		size = 0;
	}
	
	public void add(Object reference, long field, int hash, int lock) {
		if (size < elasticity) size++;
		//ReadFieldAccess r = first ? entries[0] : entries[1];
		ReadFieldAccess r = entries[marker];
		marker = ++marker % elasticity;
		r.init(reference, field, hash, lock);
		//first = !first;
	}

	public boolean isEmpty() {
		return (size==0);
	}
	
	public int getSize() {
		return size;
	}

	/**
	 * Check if the last-read-entries are still valid
	 * 
	 * @param id the identifier of the transaction
	 * @param timestamp the timestamp we should compare it to
	 * @return the validation result
	 */
	public boolean validate(int id, int timestamp) {
		for (int i = 0; i < size; i++) {
			int lock = LockTable.checkLock(entries[i].getHash(), id);
			if (lock > timestamp && lock >= 0) {
				// Too recent version: cannot validate
				return false;
			}
		}
		return true;
	}

	/**
	 * Check if the last-read-entries are still valid
	 * 
	 * @param id the identifier of the transaction
	 * @return the validation result
	 */
	public boolean validate(int id) {
		for (int i = 0; i < size; i++) {
			ReadFieldAccess r = entries[i];
			int lock = LockTable.checkLock(r.getHash(), id);
			if (lock >= 0 && lock != r.getLock()) {
				// Other version: cannot validate
				return false;
			}
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
