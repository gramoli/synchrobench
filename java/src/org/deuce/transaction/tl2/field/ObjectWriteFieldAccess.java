package org.deuce.transaction.tl2.field;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class ObjectWriteFieldAccess extends WriteFieldAccess {

	private Object value;

	public void set(Object value, Object reference, long field) {
		super.init(reference, field);
		this.value = value;
	}
	
	@Override
	public void put() {
		UnsafeHolder.getUnsafe().putObject(reference, field, value);
		clear();
		value = null;
	}

	public Object getValue() {
		return value;
	}
}
