package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class ByteWriteFieldAccess extends WriteFieldAccess {

	private byte value;
	private byte oldValue;

	public ByteWriteFieldAccess(byte value, Object reference, long orgField,
			long field, AtomicInteger status, boolean isNT, int threadId) {
		super(reference, orgField, field, status, isNT, threadId);
		this.value = value;
	}

	public byte getValue() {
		return value;
	}

	public byte getOldValue() {
		return oldValue;
	}

	@Override
	public void setLast(WriteFieldAccess last, boolean useOld) {
		if (useOld)
			oldValue = ((ByteWriteFieldAccess) last).oldValue;
		else
			oldValue = ((ByteWriteFieldAccess) last).value;
	}

	@Override
	public void setLast() {
		oldValue = UnsafeHolder.getUnsafe().getByte(reference, orgField);
	}

	@Override
	public boolean validateByValue(WriteFieldAccess other, boolean useOld) {
		if (useOld)
			return ((ByteWriteFieldAccess) other).getOldValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
		else
			return ((ByteWriteFieldAccess) other).getValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
	}
}
