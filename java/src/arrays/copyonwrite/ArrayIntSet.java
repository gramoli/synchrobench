package arrays.copyonwrite;

import java.util.Collection;
import java.util.Random;
import java.util.concurrent.CopyOnWriteArraySet;

import contention.abstractions.CompositionalIntSet;

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
public class ArrayIntSet implements CompositionalIntSet {

	private CopyOnWriteArraySet<Integer> s; 
	
	/** The thread-private PRNG */
    final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
        @Override
        protected synchronized Random initialValue() {
            return new Random();
        }
    };
	
	public ArrayIntSet() {
		s = new CopyOnWriteArraySet<Integer>();
	}

	@Override
	public boolean addInt(int e) {
		return s.add(e);
	}

	@Override
	public boolean addAll(Collection<Integer> c) {
        boolean result = true;
		for (Integer x : c) result &= s.add(x);
		return result;
	}

	@Override
	public boolean containsInt(int e) {
		return s.contains(e);
	}

	@Override
	public boolean removeInt(int e) {
		return s.remove(e);
	}

	/**
	 * {@inheritDoc}
	 * Iterating method that is manually synchronized
	 */
	@Override
	public boolean removeAll(Collection<Integer> c) {
        boolean result = true;
		for (Integer x : c) result &= this.removeInt(x);
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
	public void fill(int range, long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}

	@Override
	public Object getInt(int x) {
		if (containsInt(x)) return x;
		else return null;
	}

	@Override
	public Object putIfAbsent(int x, int y) {
		System.err.println("Lock-free linked list cannot atomically putIfAbsent");
		return null;
	}

}
