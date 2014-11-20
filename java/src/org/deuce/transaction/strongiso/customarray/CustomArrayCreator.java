package org.deuce.transaction.strongiso.customarray;

public final class CustomArrayCreator {

	public static CustomArray createArray(int size) {
		if (size <= 5)
			return new Custom5Array(size);
		else if (size <= 10)
			return new Custom10Array(size);
		else if (size <= 20)
			return new Custom20Array(size);
		else if (size <= 30)
			return new Custom30Array(size);
		else
			return new Custom40Array(size);
	}
}
