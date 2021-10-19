package org.deuce.transaction.estmstats.field;

import org.deuce.transaction.estmstats.field.ReadFieldAccess.Field.Type;
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

	public void writeField() {
		Field.putValue(reference, field, value, type);
	}
}
