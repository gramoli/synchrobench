package org.deuce.transaction.strongiso.field;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.transform.Exclude;

/**
 * Represents a base class for field write access.
 * 
 * @author Guy Korland
 */
@Exclude
abstract public class WriteFieldAccess extends FinalReadFieldAccess {

	public final AtomicInteger status;
	public final boolean isNT;
	public volatile int time = Integer.MAX_VALUE;
	public final long orgField;
	public final int threadId;

	WriteFieldAccess(Object reference, long orgField, long field,
			AtomicInteger status, boolean isNT, int threadId) {
		super(reference, field);
		this.threadId = threadId;
		this.orgField = orgField;
		this.isNT = isNT;
		this.status = status;
	}

	abstract public void setLast(WriteFieldAccess last, boolean useOld);
	
	abstract public boolean validateByValue(WriteFieldAccess other, boolean useOld);

	abstract public void setLast();
}
