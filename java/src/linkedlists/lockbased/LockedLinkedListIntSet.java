package linkedlists.lockbased;

import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Random;
import java.util.Set;

import contention.abstractions.CompositionalIntSet;
import linkedlists.sequential.SequentialLinkedListIntSet;

/*
 * Lock-based Linked List based on the sequential version
 * using the Collections.SynchronizedList wrapper that acts 
 * as a coarse-grained lock but offers reusability as 
 * described in:
 *
 * V. Gramoli and R. Guerraoui. Reusable Concurrent Data 
 * Types. ECOOP 2014.
 * 
 * @author Vincent Gramoli
 *
 */
public class LockedLinkedListIntSet implements CompositionalIntSet {

	private List<Integer> s; 
	
    /** The thread-private PRNG */
    final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
        @Override
        protected synchronized Random initialValue() {
            return new Random();
        }
    };
	
	public LockedLinkedListIntSet() {
		s = (List<Integer>) Collections.synchronizedList(new LinkedList<Integer>());
	}

	@Override
	public boolean addInt(int x) {
		return s.add(x);
	}

	@Override
	public boolean addAll(Collection<Integer> c) {
        boolean result = true;
		synchronized(s) {
			for (Integer x : c) result &= s.add(x);
		}
		return result;
	}

	@Override
	public boolean containsInt(int x) {
		return s.contains(x);
	}

	/**
	 * {@inheritDoc}
	 * Does not need to be made thread-safe
	 */
	@Override
	public void fill(int range, long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}

	@Override
	public boolean removeInt(int x) {
		return s.remove((Object) new Integer(x));
	}

	/**
	 * {@inheritDoc}
	 * Iterating method that is manually synchronized
	 */
	@Override
	public boolean removeAll(Collection<Integer> c) {
        boolean result = true;
		synchronized(s) {
			for (Integer x : c) result &= this.removeInt(x);
		}
		return result;
	}

	/**
	 * {@inheritDoc}
	 * Iterating method that is manually synchronized
	 */
	@Override
	public int size() {    
		synchronized(s) {
			return s.size();
		}
	}

	/** 
	 * This is called after the JVM warmup phase
	 * to make sure the data structure is well initalized.
	 * No need to do anything for this.
	 */
	public void clear() {
    	return;	
	}

	@Override
	public Object getInt(int x) {
		if (!containsInt(x)) return false;
		return x;
	}

	@Override
	public Object putIfAbsent(int x, int y) {
		synchronized(s) {
			if (!containsInt(x)) addInt(y);
		}
		return null;
	}
}
