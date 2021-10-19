package org.deuce.transaction.strongiso.customarray;

public class Custom20Array<T> implements CustomArray<T> {

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
	T ten = null;
	T eleven = null;
	T twelve = null;
	T thirteen = null;
	T fourteen = null;
	T fifteen = null;
	T sixteen = null;
	T seventeen = null;
	T eighteen = null;
	T nineteen = null;

	Custom20Array(int size) {
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
		case 10:
			return ten;
		case 11:
			return eleven;
		case 12:
			return twelve;
		case 13:
			return thirteen;
		case 14:
			return fourteen;
		case 15:
			return fifteen;
		case 16:
			return sixteen;
		case 17:
			return seventeen;
		case 18:
			return eighteen;
		case 19:
			return nineteen;
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
		case 10:
			ten = value;
			return;
		case 11:
			eleven = value;
			return;
		case 12:
			twelve = value;
			return;
		case 13:
			thirteen = value;
			return;
		case 14:
			fourteen = value;
			return;
		case 15:
			fifteen = value;
			return;
		case 16:
			sixteen = value;
			return;
		case 17:
			seventeen = value;
			return;
		case 18:
			eighteen = value;
			return;
		case 19:
			nineteen = value;
			return;
		default:
			throw new ArrayIndexOutOfBoundsException();
		}
	}

	public int getLength() {
		return length;
	}

}
