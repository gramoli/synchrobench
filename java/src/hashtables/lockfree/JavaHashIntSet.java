package hashtables.lockfree;

import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.ConcurrentHashMap;

import contention.abstractions.CompositionalIntSet;

/**
 * This lock-free concurrent hash map invokes the j.u.c.ConcurrentHashMap 
 * from the JDK7
 * 
 * @author Vincent Gramoli
 *
 */
public class JavaHashIntSet implements CompositionalIntSet {
	
	private final ConcurrentHashMap<Integer,Integer> hash = new ConcurrentHashMap<Integer,Integer>();
    private int range = 1;
	
	 /** The thread-private PRNG */
   final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
       @Override
       protected synchronized Random initialValue() {
           return new Random();
       }
   };
   
	@Override
	public boolean addInt(int x) {
		return (hash.put(s_random.get().nextInt(range), x) == null);
	}

	/* TODO: the putAll does not return a boolean indicating 
	 * whether it has updated the hashmap. There is no 
	 * addAll available in JDK1.6.
	 */
	@Override
	public boolean addAll(Collection<Integer> c) {
		HashMap<Integer, Integer> m = new HashMap<Integer, Integer>();
		for (Integer x : c) { 
			m.put(new Integer(s_random.get().nextInt(range)), x);
		}
		hash.putAll(m);
		return true;
	}

	@Override
	public boolean containsInt(int x) {
       return hash.containsValue(x);
	}

	@Override
	public void fill(int range, long size) {
		this.range = range;
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}

	@Override
	public boolean removeInt(int x) {
       return (hash.remove(x) != null);
	}

	/* TODO: the removeAll does not exists
	 * in JDK1.6.
	 */
	@Override
	public boolean removeAll(Collection<Integer> c) {
       ConcurrentHashMap<Integer, Integer> h = new ConcurrentHashMap<Integer, Integer>(); 

       boolean result = false;
       
       while(true) {
	       Iterator i = h.entrySet().iterator();
	       while (i.hasNext()) {
	           Map.Entry<Integer, Integer> pairs = (Map.Entry) i.next();
	           h.put((Integer) pairs.getKey(), (Integer) pairs.getValue());
	       }

	       for (Integer x : c) {
	    	   result |= (h.remove(x) != null);
	       }
	       
	       // We need a lock-free transformation
	       // TODO check atomically that the data structure is unchanged.
	       // TODO compare-and-swap the result
	       //hash = h;
	       break;
       }
       return result;
	}

	@Override
	public int size() {
		return hash.size();
	}

	/** 
	 * This is called after the JVM warmup phase
	 * to make sure the data structure is well initalized.
	 * No need to do anything for this.
	 */
	public void clear() {
		hash.clear();
    	return;	
	}

	@Override
	public Object getInt(int x) {
		return hash.get(x);
	}

	@Override
	public Object putIfAbsent(int x, int y) {
		if (!containsInt(x)) addInt(y);
		return null;
	}

}
