package org.deuce.transaction.strongiso.customarray;

public class Custom10Array<T> implements CustomArray<T> {

	final int maxIndex;
	public final int length;

	T zero = null;
	T one = null;
	T two = null;
	T three = null;
	T four = null;
	T five = null;
	T six = null;
	T seven = null;
	T eight = null;
	T nine = null;

	Custom10Array(int size) {
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
		case 5:
			return five;
		case 6:
			return six;
		case 7:
			return seven;
		case 8:
			return eight;
		case 9:
			return nine;
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
		case 5:
			five = value;
			return;
		case 6:
			six = value;
			return;
		case 7:
			seven = value;
			return;
		case 8:
			eight = value;
			return;
		case 9:
			nine = value;
			return;
		default:
			throw new ArrayIndexOutOfBoundsException();
		}
	}

	public int getLength() {
		return length;
	}

}
