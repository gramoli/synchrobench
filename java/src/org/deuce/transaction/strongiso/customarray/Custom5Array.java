package org.deuce.transaction.strongiso.customarray;

public class Custom5Array<T> implements CustomArray<T> {

	final int maxIndex;
	public final int length;

	T zero = null;
	T one = null;
	T two = null;
	T three = null;
	T four = null;

	Custom5Array(int size) {
		this.maxIndex = size - 1;
		this.length = size;
	}

	public T get(int index) {
		if (index > maxIndex)
			throw new ArrayIndexOutOfBoundsException();
		switch (index) {
		case 0:
			return zero;
		case 1:
			return one;
		case 2:
			return two;
		case 3:
			return three;
		case 4:
			return four;
		default:
			throw new ArrayIndexOutOfBoundsException();
		}
	}

	public void set(int index, T value) {
		if (index > maxIndex)
			throw new ArrayIndexOutOfBoundsException();
		switch (index) {
		case 0:
			zero = value;
			return;
		case 1:
			one = value;
			return;
		case 2:
			two = value;
			return;
		case 3:
			three = value;
			return;
		case 4:
			four = value;
			return;
		default:
			throw new ArrayIndexOutOfBoundsException();
		}
	}

	public int getLength() {
		return length;
	}

}
