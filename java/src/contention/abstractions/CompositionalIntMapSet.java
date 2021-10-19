package contention.abstractions;

import java.util.Collection;

/*
 * Compositional integer set interface
 * 
 * @author Vincent Gramoli
 *
 */
public interface CompositionalIntMapSet extends CompositionalMap<Integer, Integer>, CompositionalIntSet{
	
	public void fill(int range, long size);
	
	public boolean add(Integer x);
	public boolean remove(Integer x);
	public boolean contains(Integer x);
	public Object get(Integer x);

	public boolean addAll(Collection<Integer> c);
	public boolean removeAll(Collection<Integer> c);
	
	public int size();
	
	public void clear();
	
	public String toString();
	
	public Object putIfAbsent(int x, int y);
	
	
}
