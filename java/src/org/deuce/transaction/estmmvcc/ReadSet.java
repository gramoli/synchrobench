package org.deuce.transaction.estmmvcc;

import org.deuce.transaction.estmmvcc.Context.LockTable;
import org.deuce.transaction.estmmvcc.field.ReadFieldAccess;
import org.deuce.transform.Exclude;

/**
 * @author Vincent Gramoli
 */
@Exclude
final public class ReadSet {

	/** An array of read entries */
	private ReadFieldAccess[] entries;
	/** The number of read entries */
	private int size;
	/**
	 * Initialize the read set with a given number of elements
	 * 
	 * @param initialCapacity the number of elements
	 */
	public ReadSet(int initialCapacity) {
		entries = new ReadFieldAccess[initialCapacity];
		size = 0;
		initArray(0);
	}

	public int size() {
		return size;
	}

	/**
	 * Clear the read set by resetting its size
	 */
	public void clear() {
		size = 0;
	}

	/**
	 * Copy the last-read-entries field to the readset.
	 * Used to switched from lightweight validation to full validation
	 * in an elastic transaction
	 * 
	 * @param lre last-read-entries field to be copied
	 */
	public void copy(LastReadEntries lre) {
		int l = lre.getSize();
		ReadFieldAccess[] e = new ReadFieldAccess[l];
		System.arraycopy(lre.entries, 0, e, 0, l);
		entries = e;
	}
	
	/**
	 * Add a read entry to the read set
	 * and allocates length^2 new entries if full
	 * 
	 * @param reference the object of the field
	 * @param field the field 
	 * @param hash its identifier
	 * @param lock its version / owner
	 */
	public void add(Object reference, long field, int hash, int lock) {
		if (size >= entries.length) {
			int l = entries.length;
			ReadFieldAccess[] e = new ReadFieldAccess[l << 1];
			System.arraycopy(entries, 0, e, 0, l);
			entries = e;
			initArray(l);
		}
		assert size < entries.length;
		ReadFieldAccess r = entries[size++];
		r.init(reference, field, hash, lock);
	}

	/**
	 * Return the size of the read-set
	 */
	public int getSize() {
		return size;
	}

	/**
	 * Validate the transaction
	 * Check that the version of read locations, which 
	 * are maintained by the elastic tx, are consistent
	 * 
	 * @param id the identifier
	 * @return true if the validation is successful
	 */
	public boolean validate(int id) {
		int lock;
		for (int i = 0; i < size; i++) {
			// Throws an exception if validation fails
			ReadFieldAccess r = entries[i];
			lock = LockTable.checkLock(r.getHash(), id);
			if (lock >= 0 && lock != r.getLock()) {
				// Other version: cannot validate
				return false;
			}
		}
		return true;
	}

	/**
	 * Indicates whether the given field corresponds
	 * to an existing read entry of the read set
	 * 
	 * @param obj the object of the field
	 * @param field the accessed field
	 * @return true is the read set contains the field
	 */
	public boolean contains(Object obj, long field) {
		for (int i = 0; i < size; i++) {
			ReadFieldAccess r = entries[i];
			if (r.getField() == field && r.getReference() == obj)
				return true;
		}
		return false;
	}

	/**
	 * Allocating more space for the array
	 * 
	 * @param fromIndex index from where allocation is done
	 */
	private void initArray(int fromIndex) {
		int l = entries.length;
		for (int i = fromIndex; i < l; i++)
			entries[i] = new ReadFieldAccess();
	}
	
}
