package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class DoubleWriteFieldAccess extends WriteFieldAccess {

	private double value;
	private double oldValue;

	public DoubleWriteFieldAccess(double value, Object reference,
			long orgField, long field, AtomicInteger status, boolean isNT,
			int threadId) {
		super(reference, orgField, field, status, isNT, threadId);
		this.value = value;
	}

	public double getValue() {
		return value;
	}

	public double getOldValue() {
		return oldValue;
	}

	@Override
	public void setLast(WriteFieldAccess last, boolean useOld) {
		if (useOld)
			oldValue = ((DoubleWriteFieldAccess) last).oldValue;
		else
			oldValue = ((DoubleWriteFieldAccess) last).value;
	}

	@Override
	public void setLast() {
		oldValue = UnsafeHolder.getUnsafe().getDouble(reference, orgField);
	}

	@Override
	public boolean validateByValue(WriteFieldAccess other, boolean useOld) {
		if (useOld)
			return ((DoubleWriteFieldAccess) other).getOldValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
		else
			return ((DoubleWriteFieldAccess) other).getValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
	}
}
