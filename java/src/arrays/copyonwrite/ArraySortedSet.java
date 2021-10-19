package arrays.copyonwrite;

import java.util.Collection;
import java.util.Comparator;
import java.util.Iterator;
import java.util.SortedSet; 
import java.util.Set;
import java.util.concurrent.CopyOnWriteArraySet;

import contention.abstractions.CompositionalSortedSet;
import linkedlists.sequential.SequentialLinkedListSortedSet;

/*
 * CopyOnWrite-based array implementing an integer set 
 * as described in:
 * 
 * V. Gramoli and R. Guerraoui. Reusable Concurrent 
 * Data Types. ECOOP 2014.
 * 
 * @author Vincent Gramoli
 *
 */
public class ArraySortedSet<E extends Comparable<E>> implements CompositionalSortedSet<E> {

	private CopyOnWriteArraySet<E> s; 
	
	public ArraySortedSet() {
		s = new CopyOnWriteArraySet<E>();
	}

	@Override
	public boolean add(E e) {
		return s.add(e);
	}

	@Override
	public boolean addAll(Collection<? extends E> c) {
        boolean result = true;
		for (E x : c) result &= s.add(x);
		return result;
	}

	@Override
	public boolean contains(Object e) {
		return s.contains(e);
	}

	@Override
	public boolean remove(Object e) {
		return s.remove(e);
	}

	/**
	 * {@inheritDoc}
	 * Iterating method that is manually synchronized
	 */
	@Override
	public boolean removeAll(Collection<?> c) {
        boolean result = true;
		for (Object x : c) result &= this.remove(x);
		return result;
	}

	/**
	 * {@inheritDoc}
	 * Iterating method that is manually synchronized
	 */
	@Override
	public int size() {    
		return s.size();
	}
	
	@Override
	public boolean containsAll(Collection<?> c) {
        boolean result = true;
        for (Object x : c) result &= this.contains(x);
        return result;
	}

	@Override
	public boolean isEmpty() {
		return size() == 0;
	}
	
	@Override
	public boolean retainAll(Collection<?> c) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public <T> T[] toArray(T[] a) {
    	throw new UnsupportedOperationException();
	}
	

	@Override
	public Comparator<E> comparator() {
    	throw new UnsupportedOperationException();
	}

	/** 
	 * This is called after the JVM warmup phase
	 * to make sure the data structure is initalized.
	 * No need to do anything for this.
	 */
	@Override
	public void clear() {
	    s.clear();
	}

	@Override
	public Iterator<E> iterator() {
    	throw new UnsupportedOperationException();
	}

	@Override
	public Object[] toArray() {
    	throw new UnsupportedOperationException();
	}

	@Override
	public E first() {
    	throw new UnsupportedOperationException();
	}

	@Override
	public SortedSet<E> headSet(E toElement) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public E last() {
    	throw new UnsupportedOperationException();
	}

	@Override
	public SortedSet<E> subSet(E fromElement, E toElement) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public SortedSet<E> tailSet(E fromElement) {
    	throw new UnsupportedOperationException();
	}
}
