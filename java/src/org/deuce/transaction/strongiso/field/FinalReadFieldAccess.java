package org.deuce.transaction.strongiso.field;

import org.deuce.transaction.tl2.LockTable;
import org.deuce.transform.Exclude;

/**
 * Represents a base class for field write access.
 * 
 * @author Guy Koralnd
 */
@Exclude
public class FinalReadFieldAccess {
	final public Object reference;
	final public long field;
	final private int hash;

	// public FinalReadFieldAccess(){}

	public FinalReadFieldAccess(Object reference, long field) {
		this.reference = reference;
		this.field = field;
		this.hash = (System.identityHashCode(reference) + (int) field);
	}

	// public void init( Object reference, long field){
	// this.reference = reference;
	// this.field = field;
	// this.hash = (System.identityHashCode( reference) + (int)field) &
	// LockTable.MASK;
	// }

	public Object getReference() {
		return reference;
	}

	public long getField() {
		return field;
	}

	@Override
	public boolean equals(Object obj) {
		FinalReadFieldAccess other = (FinalReadFieldAccess) obj;
		return reference == other.getReference() && field == other.getField();
	}

	@Override
	public int hashCode() {
		return hash;
	}

	// public void clear() {
	// // reference = null;
	// }
}