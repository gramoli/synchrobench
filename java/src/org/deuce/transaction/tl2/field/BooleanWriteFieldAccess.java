package org.deuce.transaction.tl2.field;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class BooleanWriteFieldAccess extends WriteFieldAccess {

	private boolean value;

	public void set(boolean value, Object reference, long field) {
		super.init(reference, field);
		this.value = value;
	}

	@Override
	public void put() {
		UnsafeHolder.getUnsafe().putBoolean(reference, field, getValue());
		clear();
	}

	public boolean getValue() {
		return value;
	}
}
