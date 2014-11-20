package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class CharWriteFieldAccess extends WriteFieldAccess {

	private char value;
	private char oldValue;

	public CharWriteFieldAccess(char value, Object reference, long orgField,
			long field, AtomicInteger status, boolean isNT, int threadId) {
		super(reference, orgField, field, status, isNT, threadId);
		this.value = value;
	}

	public char getValue() {
		return value;
	}

	public char getOldValue() {
		return oldValue;
	}

	@Override
	public void setLast(WriteFieldAccess last, boolean useOld) {
		if (useOld)
			oldValue = ((CharWriteFieldAccess) last).oldValue;
		else
			oldValue = ((CharWriteFieldAccess) last).value;
	}

	@Override
	public void setLast() {
		oldValue = UnsafeHolder.getUnsafe().getChar(reference, orgField);
	}

	@Override
	public boolean validateByValue(WriteFieldAccess other, boolean useOld) {
		if (useOld)
			return ((CharWriteFieldAccess) other).getOldValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
		else
			return ((CharWriteFieldAccess) other).getValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
	}

}
