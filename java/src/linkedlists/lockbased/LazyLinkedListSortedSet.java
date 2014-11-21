package linkedlists.lockbased;

import java.util.Collection;
import java.util.Comparator;
import java.util.Iterator;
import java.util.SortedSet;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

import linkedlists.lockbased.lazyutils.LazyList;

import contention.abstractions.CompositionalSortedSet;

/**
 * This is an implementation variant of the lazy linked 
 * list presented in: 
 * S. Heller, M. Herlihy, V. Luchangco, M. Moir, W. N. 
 * Sherer, N. Shavit. A Lazy Concurrent List-Based Set 
 * Algorithm. OPODIS 2005.
 */
public class LazyLinkedListSortedSet<E extends Comparable<E>> implements CompositionalSortedSet<E> {

	/** LazyLinkedList needs consecutive thread ids for the snapshot counter. */
	private static int THREADNUM = 65;
	private static AtomicInteger MAXID = new AtomicInteger(0);
	private static ConcurrentHashMap<Long, Integer> THREADMAP = new ConcurrentHashMap<Long, Integer>();
	
	/** The Lazy linked list */
	private LazyList<E> list = new LazyList<E>(THREADNUM);
	
	private int getSmallThreadId() {
		Long threadId = Thread.currentThread().getId();
		if (!THREADMAP.containsKey((long) threadId)) {
			assert(MAXID.get() < THREADNUM);
			THREADMAP.put(threadId, (int) MAXID.getAndIncrement());
		}
		return THREADMAP.get(threadId);
	}
	
	@Override
	public int size() {
		return list.size(getSmallThreadId());
	}

	@Override
	public Comparator<? super E> comparator() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public E first() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public SortedSet<E> headSet(E toElement) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public E last() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public SortedSet<E> subSet(E fromElement, E toElement) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public SortedSet<E> tailSet(E fromElement) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean add(E e) {
		return list.add(e, getSmallThreadId());
	}

	@Override
	public boolean addAll(Collection<? extends E> c) {
    	throw new UnsupportedOperationException();
	}

	/** 
	 * Useful method for resetting thread
	 * counts after a WarmUp phase.
	 */
	@Override
	public void clear() {
		MAXID.set(0);
		THREADMAP.clear();
	}

	@Override
	public boolean contains(Object o) {
		return list.contains((E) o, getSmallThreadId());
	}

	@Override
	public boolean containsAll(Collection<?> c) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public boolean isEmpty() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public Iterator<E> iterator() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean remove(Object o) {
		// TODO Auto-generated method stub
		return list.remove((E) o, getSmallThreadId());
	}

	@Override
	public boolean removeAll(Collection<?> c) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public boolean retainAll(Collection<?> c) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public Object[] toArray() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public <T> T[] toArray(T[] a) {
		// TODO Auto-generated method stub
		return null;
	}

}
