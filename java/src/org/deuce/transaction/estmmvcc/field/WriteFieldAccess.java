package org.deuce.transaction.estmmvcc.field;

import org.deuce.transaction.estmmvcc.BackUpTable;
import org.deuce.transaction.estmmvcc.field.ReadFieldAccess.Field.Type;
import org.deuce.transform.Exclude;

@Exclude
final public class WriteFieldAccess extends ReadFieldAccess {

	final private Type type;
	private Object value;
	private WriteFieldAccess next;

	public WriteFieldAccess(Object reference, long field, Type type, Object value, int hash, int lock) {
		super(reference, field, hash, lock);
		this.type = type;
		this.value = value;
		this.next = null;
		this.lock = lock;
	}

	public void setValue(Object value) {
		this.value = value;
	}

	public Object getValue() {
		return value;
	}

	public void setNext(WriteFieldAccess next) {
		this.next = next;
	}

	public WriteFieldAccess getNext() {
		return next;
	}
	
	public void writeOldField(int hash, int id) {
		Object val = Field.getValue(reference, field, type);
		BackUpTable.setVersion(reference, field, this.getLock());
		//if (!BackUpTable.setVersion(reference, field, this.getLock()))
			//return false;
		BackUpTable.setValue(reference, field, val);
		//return true;
	}
	
	/*public boolean writeOldField(int timestamp) {
		Object val = Field.getValue(reference, field, type);
		if (!BackUpTable.setVersion(reference, field, timestamp))
			return false;
		BackUpTable.setValue(reference, field, val);
		return true;
	}*/

	/*public void resetOldField() {
		int version = BackUpTable.getVersion(reference, field);
		if (version != Integer.MAX_VALUE) {
			Object val = BackUpTable.getValue(reference, field);
			Field.putValue(reference, field, val, type);
		 	// this do not need to C&S
			LockTable.lock(LockTable.hash(reference, field), version);
		}
		//BackUpTable.setVersion(reference, field, Integer.MAX_VALUE);
	}*/

	public void writeField() {
		Field.putValue(reference, field, value, type);
	}
}
