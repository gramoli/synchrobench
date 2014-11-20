package org.deuce.transaction.estmmvcc.field;

import org.deuce.reflection.UnsafeHolder;
import org.deuce.transform.Exclude;

import sun.misc.Unsafe;

/**
 * @author Pascal Felber
 */
@Exclude
public class Field {

	@Exclude
	static public enum Type {
		BYTE, BOOLEAN, CHAR, SHORT, INT, LONG, FLOAT, DOUBLE, OBJECT
	}
	
	static public Object getValue(Object reference, long field, Type type) {
		Unsafe unsafe = UnsafeHolder.getUnsafe();
		switch (type) {
		case BYTE:
			return unsafe.getByte(reference, field);
		case BOOLEAN:
			return unsafe.getBoolean(reference, field);
		case CHAR:
			return unsafe.getChar(reference, field);
		case SHORT:
			return unsafe.getShort(reference, field);
		case INT:
			return unsafe.getInt(reference, field);
		case LONG:
			return unsafe.getLong(reference, field);
		case FLOAT:
			return unsafe.getFloat(reference, field);
		case DOUBLE:
			return unsafe.getDouble(reference, field);
		case OBJECT:
			return unsafe.getObject(reference, field);
		}
		return null;
	}

	static public void putValue(Object reference, long field, Object value, Type type) {
		Unsafe unsafe = UnsafeHolder.getUnsafe();
		switch (type) {
		case BYTE:
			unsafe.putByte(reference, field, (Byte) value);
			break;
		case BOOLEAN:
			unsafe.putBoolean(reference, field, (Boolean) value);
			break;
		case CHAR:
			unsafe.putChar(reference, field, (Character) value);
			break;
		case SHORT:
			unsafe.putShort(reference, field, (Short) value);
			break;
		case INT:
			unsafe.putInt(reference, field, (Integer) value);
			break;
		case LONG:
			unsafe.putLong(reference, field, (Long) value);
			break;
		case FLOAT:
			unsafe.putFloat(reference, field, (Float) value);
			break;
		case DOUBLE:
			unsafe.putDouble(reference, field, (Double) value);
			break;
		case OBJECT:
			unsafe.putObject(reference, field, value);
			break;
		}
	}
}