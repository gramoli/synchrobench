package org.deuce.transaction.strongiso.field;

import org.deuce.transform.Exclude;

/**
 * @author Pascal Felber
 */
@Exclude
public class LocalFieldAccess extends FinalReadFieldAccess {

	protected Object reference2;
	protected long field2;
	private int hash2;

	public LocalFieldAccess() {
		super(null, 0);
	}

	public LocalFieldAccess(Object reference, long field) {
		super(null, 0);
		init(reference, field);
	}

	public void init(Object reference, long field) {
		this.reference2 = reference;
		this.field2 = field;
		this.hash2 = System.identityHashCode(reference) + (int) field;
	}

	@Override
	public Object getReference() {
		return reference2;
	}

	@Override
	public long getField() {
		return field2;
	}

	@Override
	public boolean equals(Object o) {
		FinalReadFieldAccess r = (FinalReadFieldAccess) o;
		return reference2 == r.getReference() && field2 == r.getField();
	}

	@Override
	final public int hashCode() {
		return hash2;
	}

	@Override
	public String toString() {
		return Integer.toString(hash2);
	}
}