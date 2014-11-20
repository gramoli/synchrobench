package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class LongWriteFieldAccess extends WriteFieldAccess {

	private long value;
	private long oldValue;

	public LongWriteFieldAccess(long value, Object reference, long orgField,
			long field, AtomicInteger status, boolean isNT, int threadId) {
		super(reference, orgField, field, status, isNT, threadId);
		this.value = value;
	}

	public long getValue() {
		return value;
	}

	public long getOldValue() {
		return oldValue;
	}

	@Override
	public void setLast(WriteFieldAccess last, boolean useOld) {
		if (useOld)
			oldValue = ((LongWriteFieldAccess) last).oldValue;
		else
			oldValue = ((LongWriteFieldAccess) last).value;
	}

	@Override
	public void setLast() {
		oldValue = UnsafeHolder.getUnsafe().getLong(reference, orgField);
	}

	@Override
	public boolean validateByValue(WriteFieldAccess other, boolean useOld) {
		if (useOld)
			return ((LongWriteFieldAccess) other).getOldValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
		else
			return ((LongWriteFieldAccess) other).getValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
	}
}
