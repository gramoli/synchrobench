package org.deuce.transaction.estmmvcc;

import org.deuce.transform.Exclude;

/**
 * Multi Vesion Concurrency Control
 * This table records one old values
 * 
 */
@Exclude
public class BackUpTable {
	
	final private static int ARRAYSIZE = (1 << 21) - 1; // 2^21-1
	final private static int MASK = ARRAYSIZE - 1;
	final private static BackUp[] backups = new BackUp[ARRAYSIZE];

	static {
		for (int i = 0; i < ARRAYSIZE; i++) {
			backups[i] = new BackUp();
		}
	}
	
	public static int hash(Object obj, long field) {
		int hash = System.identityHashCode(obj) + (int) field;
		return hash & MASK;
	}
	
	public static int getVersion(Object obj, long field) {
		int index = hash(obj, field);
		return backups[index].getOldVersion(obj, field);
	}
	
	public static Object getValue(Object obj, long field) {
		int index = hash(obj, field);
		return backups[index].getOldValue(obj, field);
	}
	
	public static void setVersion(Object obj, long field, int version) {
		int index = hash(obj, field);
		backups[index].setOldVersion(obj, field, version);
	}
	
	public static void setValue(Object obj, long field, Object value) {
		int index = hash(obj, field);
		backups[index].setOldValue(value);
	}
}

