package queues.lockfree;

import java.util.Collection;
import java.util.Random;
import java.util.concurrent.ConcurrentLinkedQueue;

import contention.abstractions.CompositionalIntSet;
import contention.benchmark.Parameters;

/**
 * Lock-free Queue (linked list) implementation of the integer set
 * as part of the JDK. 
 * It builds upon the Michael and Scott algorithm, PODC 1996.
 * 
 * @author Vincent Gramoli
 *
 */
public class LockFreeQueueIntSet implements CompositionalIntSet {

	private static final long serialVersionUID = 0001;
	
	private final ConcurrentLinkedQueue<Integer> queue = new ConcurrentLinkedQueue<Integer>();
    /** The thread-private PRNG */
    final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
        @Override
        protected synchronized Random initialValue() {
            return new Random();
        }
    };
    
	public void fill(final int range, final long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}
    
    public void push(int value) {
        queue.add(value);
    }
    
    public boolean addInt(int value) {
    	return queue.add(value);
    }
   
	public boolean addAll(Collection<Integer> c) {
    	return queue.addAll(c);
    }
    
    public int size() {
        return queue.size();
    }

    public int pop() {
        return queue.remove();
    }

    public boolean removeInt(int value) {
    	return queue.remove(value);
    }

	public boolean removeAll(Collection<Integer> c) {
    	return queue.removeAll(c);
    }

	public boolean containsInt(int x) {
		return queue.contains(x);
	}

	/** 
	 * This is called after the JVM warmup phase
	 * to make sure the data structure is well initalized.
	 * No need to do anything for this.
	 *
	 * Note the ugly hack to reset the init size of the queue after warmup 
	 * otherwise queue size grows as fast as its add ops execute: they are 
	 * always successful, as opposed to its remove.
	 */
	public void clear() {
		queue.clear();
		fill(Parameters.range, Parameters.size);
    	return;	
	}

	@Override
	public Object getInt(int x) {
		if (containsInt(x)) return x;
		return null;
	}

	@Override
	public Object putIfAbsent(int x, int y) {
		System.err.println("Lock-free queue cannot atomically putIfAbsent.");
		return null;
	}
}
