package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class ShortWriteFieldAccess extends WriteFieldAccess {

	private short value;
	private short oldValue;

	public ShortWriteFieldAccess(short value, Object reference, long orgField,
			long field, AtomicInteger status, boolean isNT, int threadId) {
		super(reference, orgField, field, status, isNT, threadId);
		this.value = value;
	}

	public short getValue() {
		return value;
	}

	public short getOldValue() {
		return oldValue;
	}

	@Override
	public void setLast(WriteFieldAccess last, boolean useOld) {
		if (useOld)
			oldValue = ((ShortWriteFieldAccess) last).oldValue;
		else
			oldValue = ((ShortWriteFieldAccess) last).value;
	}

	@Override
	public void setLast() {
		oldValue = UnsafeHolder.getUnsafe().getShort(reference, orgField);
	}

	@Override
	public boolean validateByValue(WriteFieldAccess other, boolean useOld) {
		if (useOld)
			return ((ShortWriteFieldAccess) other).getOldValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
		else
			return ((ShortWriteFieldAccess) other).getValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
	}

}
