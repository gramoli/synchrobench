package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

@Exclude
public class ObjectWriteFieldAccess extends WriteFieldAccess {

	private Object value;
	private Object oldValue;

	public ObjectWriteFieldAccess(Object value, Object reference,
			long orgField, long field, AtomicInteger status, boolean isNT,
			int threadId) {
		super(reference, orgField, field, status, isNT, threadId);
		this.value = value;
	}

	public Object getValue() {
		return value;
	}

	public Object getOldValue() {
		return oldValue;
	}

	@Override
	public void setLast(WriteFieldAccess last, boolean useOld) {
		if (useOld)
			oldValue = ((ObjectWriteFieldAccess) last).oldValue;
		else
			oldValue = ((ObjectWriteFieldAccess) last).value;
	}

	@Override
	public void setLast() {
		oldValue = UnsafeHolder.getUnsafe().getObject(reference, orgField);
	}

	@Override
	public boolean validateByValue(WriteFieldAccess other, boolean useOld) {
		if (useOld)
			return ((ObjectWriteFieldAccess) other).getOldValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
		else
			return ((ObjectWriteFieldAccess) other).getValue() == (this.status
					.get() == TransStatus.COMMITTED ? value : oldValue);
	}

}
