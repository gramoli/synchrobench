package contention.abstractions;

import java.util.SortedSet;

public interface CompositionalSortedSet<E> extends SortedSet<E> {
	
	public int size();

	public String toString();
}

