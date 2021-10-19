package org.deuce.transaction.tl2.field;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class ShortWriteFieldAccess extends WriteFieldAccess {

	private short value;

	public void set(short value, Object reference, long field) {
		super.init(reference, field);
		this.value = value;
	}

	@Override
	public void put() {
		UnsafeHolder.getUnsafe().putShort(reference, field, value);
		clear();
	}

	public short getValue() {
		return value;
	}

}
