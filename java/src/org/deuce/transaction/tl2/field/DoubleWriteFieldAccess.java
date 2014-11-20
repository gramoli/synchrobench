package org.deuce.transaction.tl2.field;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class DoubleWriteFieldAccess extends WriteFieldAccess {

	private double value;

	public void set(double value, Object reference, long field) {
		super.init(reference, field);
		this.value = value;
	}

	@Override
	public void put() {
		UnsafeHolder.getUnsafe().putDouble(reference, field, value);
		clear();
	}

	public double getValue() {
		return value;
	}
}
