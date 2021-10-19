package org.deuce.transaction.strongiso;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transaction.strongiso.field.TransStatus;
import org.deuce.transaction.strongiso.field.WriteFieldAccess;

public class PointerReadSet {
	private static final int DEFAULT_CAPACITY = 1024;
	private WriteFieldAccess[] readSet = new WriteFieldAccess[DEFAULT_CAPACITY];
	private int nextAvaliable = 0;

	public PointerReadSet() {
	}

	public void clear() {
		nextAvaliable = 0;
	}

	public void add(WriteFieldAccess item) {
		if (nextAvaliable >= readSet.length) {
			int orignLength = readSet.length;
			WriteFieldAccess[] tmpReadSet = new WriteFieldAccess[2 * orignLength];
			System.arraycopy(readSet, 0, tmpReadSet, 0, orignLength);
			readSet = tmpReadSet;
		}
		readSet[nextAvaliable++] = item;
	}

	public boolean validateAndNullRs(int threadId) {
		boolean success = true;
		WriteFieldAccess next;
		for (int i = 0; i < nextAvaliable; i++) {
			next = readSet[i];
			if (success) {
				WriteFieldAccess check;
				if (next != (check = (WriteFieldAccess) UnsafeHolder
						.getUnsafe().getObjectVolatile(next.reference,
								next.field))) {

					if (check == null)
						continue;

					if (!(check.threadId == threadId && (check.status.get()) == TransStatus.LIVE)) {
						success = false;

					} else {
						if (!next.validateByValue(check, true))
							success = false;
					}

				}
			}
			readSet[i] = null;
		}
		return success;
	}
}
