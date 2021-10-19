package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class FloatWriteFieldAccess extends WriteFieldAccess {

	private float value;
	private float oldValue;

	public FloatWriteFieldAccess(float value, Object reference, long orgField,
			long field, AtomicInteger status, boolean isNT, int threadId) {
		super(reference, orgField, field, status, isNT, threadId);
		this.value = value;
	}

	public float getValue() {
		return value;
	}

	public float getOldValue() {
		return oldValue;
	}

	@Override
	public void setLast(WriteFieldAccess last, boolean useOld) {
		if (useOld)
			oldValue = ((FloatWriteFieldAccess) last).oldValue;
		else
			oldValue = ((FloatWriteFieldAccess) last).value;
	}

	@Override
	public void setLast() {
		oldValue = UnsafeHolder.getUnsafe().getFloat(reference, orgField);
	}

	@Override
	public boolean validateByValue(WriteFieldAccess other, boolean useOld) {
		if (useOld)
			return ((FloatWriteFieldAccess) other).getOldValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
		else
			return ((FloatWriteFieldAccess) other).getValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
	}
}
