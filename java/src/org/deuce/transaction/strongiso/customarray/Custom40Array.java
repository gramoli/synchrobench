package org.deuce.transaction.strongiso.customarray;

import java.util.LinkedList;

public class Custom40Array<T> implements CustomArray<T> {

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
	T twenty = null;
	T twentyone = null;
	T twentytwo = null;
	T twentythree = null;
	T twentyfour = null;
	T twentyfive = null;
	T twentysix = null;
	T twentyseven = null;
	T twentyeight = null;
	T twentynine = null;
	T thirty = null;
	T thirtyone = null;
	T thirtytwo = null;
	T thirtythree = null;
	T thirtyfour = null;
	T thirtyfive = null;
	T thirtysix = null;
	T thirtyseven = null;
	T thirtyeight = null;
	T thirtynine = null;
	LinkedList<T> theRest;

	Custom40Array(int size) {
		this.maxIndex = size - 1;
		this.length = size;
		if (size > 40) {
			theRest = new LinkedList<T>();
			for (int i = 40; i < size; i++) {
				theRest.add(null);
			}
		}
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
		case 20:
			return twenty;
		case 21:
			return twentyone;
		case 22:
			return twentytwo;
		case 23:
			return twentythree;
		case 24:
			return twentyfour;
		case 25:
			return twentyfive;
		case 26:
			return twentysix;
		case 27:
			return twentyseven;
		case 28:
			return twentyeight;
		case 29:
			return twentynine;
		case 30:
			return thirty;
		case 31:
			return thirtyone;
		case 32:
			return thirtytwo;
		case 33:
			return thirtythree;
		case 34:
			return thirtyfour;
		case 35:
			return thirtyfive;
		case 36:
			return thirtysix;
		case 37:
			return thirtyseven;
		case 38:
			return thirtyeight;
		case 39:
			return thirtynine;
		default:
			return theRest.get(index - 40);
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
		case 20:
			twenty = value;
			return;
		case 21:
			twentyone = value;
			return;
		case 22:
			twentytwo = value;
			return;
		case 23:
			twentythree = value;
			return;
		case 24:
			twentyfour = value;
			return;
		case 25:
			twentyfive = value;
			return;
		case 26:
			twentysix = value;
			return;
		case 27:
			twentyseven = value;
			return;
		case 28:
			twentyeight = value;
			return;
		case 29:
			twentynine = value;
			return;
		case 30:
			thirty = value;
			return;
		case 31:
			thirtyone = value;
			return;
		case 32:
			thirtytwo = value;
			return;
		case 33:
			thirtythree = value;
			return;
		case 34:
			thirtyfour = value;
			return;
		case 35:
			thirtyfive = value;
			return;
		case 36:
			thirtysix = value;
			return;
		case 37:
			thirtyseven = value;
			return;
		case 38:
			thirtyeight = value;
			return;
		case 39:
			thirtynine = value;
			return;
		default:
			theRest.set(index - 40, value);
			return;
		}
	}

	public int getLength() {
		return length;
	}

}
