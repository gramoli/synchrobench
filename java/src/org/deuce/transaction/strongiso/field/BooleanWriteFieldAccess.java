package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class BooleanWriteFieldAccess extends WriteFieldAccess {

	private boolean value;
	private boolean oldValue;

	public BooleanWriteFieldAccess(boolean value, Object reference,
			long orgField, long field, AtomicInteger status, boolean isNT,
			int threadId) {
		super(reference, orgField, field, status, isNT, threadId);
		this.value = value;
	}

	public boolean getValue() {
		return value;
	}

	public boolean getOldValue() {
		return oldValue;
	}

	@Override
	public void setLast(WriteFieldAccess last, boolean useOld) {
		if (useOld)
			oldValue = ((BooleanWriteFieldAccess) last).oldValue;
		else
			oldValue = ((BooleanWriteFieldAccess) last).value;
	}

	@Override
	public void setLast() {
		oldValue = UnsafeHolder.getUnsafe().getBoolean(reference, orgField);
	}

	@Override
	public boolean validateByValue(WriteFieldAccess other, boolean useOld) {
		if (useOld)
			return ((BooleanWriteFieldAccess) other).getOldValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
		else
			return ((BooleanWriteFieldAccess) other).getValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
	}
}
