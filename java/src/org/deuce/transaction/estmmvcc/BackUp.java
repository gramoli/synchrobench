package org.deuce.transaction.estmmvcc;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

public class BackUp {
	
	private AtomicInteger oldVersion = new AtomicInteger(Integer.MAX_VALUE);
	private AtomicReference<Object> oldValue = new AtomicReference<Object>();
	long field;
	Object obj;
	
	public BackUp() {
		super();	
	}
	
	public BackUp(int ver) {
		oldVersion.set(ver);
	}
	
	public void setOldVersion(Object o, long f, int version) {
		if (this.oldVersion.get() != Integer.MAX_VALUE) { 
			if (this.obj == null) {
				// this has never been set
				this.field = f;
				this.obj = o;
			}
		} 
		this.oldVersion.set(version);
	}
	
	public int getOldVersion(Object o, long f) {
		if (this.oldVersion.get() !=  Integer.MAX_VALUE) { 
			// this has alredy been set
			if (this.obj != null)
				if (!(this.obj.equals(o) && field == f)) {
					// this hash is shared by another field
					return -1;
				}
		}
		return oldVersion.get();
	}

	public void setOldValue(Object value) {
		this.oldValue.set(value);
	}
	
	public Object getOldValue(Object obj, long field) {
		return this.oldValue.get();
	}
}

