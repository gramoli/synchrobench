package org.deuce.transaction.strongiso.customarray;

public interface CustomArray<T> {
	
	public int getLength();
	
	public abstract T get(int index);
	
	public abstract void set(int index, T value);

}
