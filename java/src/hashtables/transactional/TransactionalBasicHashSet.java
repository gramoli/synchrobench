package hashtables.transactional;

import java.util.Collection;
import java.util.Map;
import java.util.Random;
import java.util.Set;

import org.deuce.Atomic;

import contention.abstractions.CompositionalIntSet;
import contention.abstractions.CompositionalMap;

/**
 * Hash table implementation of integer set
 * 
 * @author Vincent Gramoli
 * @param <K>
 * @param <V>
 * 
 */
public class TransactionalBasicHashSet<K, V> implements CompositionalIntSet,
		CompositionalMap<K, V> {

	int load;
	final private Node[] table;
	final int tableSize;

	/** The thread-private PRNG */
	final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
		@Override
		protected synchronized Random initialValue() {
			return new Random();
		}
	};

	public TransactionalBasicHashSet(int size, int load) {
		tableSize = size / load;
		table = new Node[tableSize];
		this.load = load;
	}

	public TransactionalBasicHashSet() {
		this(4096, 1); 
	}

	@Override
	public void fill(final int range, final long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}

	private int hash(int value) {
		return value % tableSize;
	}

	@Atomic(metainf = "elastic")
	public boolean addInt(int value) {
		int h = hash(value);
		Node node = table[h];

		if (node == null) {
			table[h] = new Node(value);
			return true;
		}

		if (node.getValue() == value) {
			return false;
		}
		Node next = node.getNext();

		while (next != null) {
			node = next;
			next = node.getNext();
			if (node.getValue() == value) {
				return false;
			}
		}

		node.setNext(new Node(value));
		return true;
	}

	@Atomic(metainf = "elastic")
	public boolean removeInt(int value) {
		int h = hash(value);
		Node node = table[h];

		if (node == null) {
			return false;
		}

		Node next = node.getNext();

		if (node.getValue() == value) {
			table[h] = next;
			node.setNext(next);
			return true;
		}

		while (next != null) {
			if (next.getValue() == value) {
				node.setNext(next.getNext());
				return true;
			}
			node = next;
			next = node.getNext();
		}

		return false;
	}

	@Atomic(metainf = "elastic")
	public boolean containsInt(int value) {
		int h = hash(value);
		Node node = table[h];

		if (node == null) {
			return false;
		}

		while (node != null) {
			if (node.getValue() == value) {
				return true;
			}
			node = node.getNext();
		}

		return false;
	}

	@Override
	public Object getInt(final int value) {
		if (containsInt(value))
			return value;
		else
			return null;
	}

	@Override
	@Atomic(metainf = "elastic")
	public boolean addAll(Collection<Integer> c) {
		boolean result = true;
		for (Integer x : c)
			result &= this.addInt(x);
		return result;
	}

	@Override
	@Atomic(metainf = "elastic")
	public boolean removeAll(Collection<Integer> c) {
		boolean result = true;
		for (Integer x : c)
			result &= this.removeInt(x);
		return result;
	}

	@Atomic(metainf = "roregular")
	public int size() {
		int s = 0;

		for (int i = 0; i < tableSize; i++) {
			Node node = table[i];
			while (node != null) {
				s++;
				node = node.getNext();
			}
		}
		return s;
	}

	public class Node<K, V> {

		final private int m_value;
		private Node m_next;

		public Node(int value, Node next) {
			m_value = value;
			m_next = next;
		}

		public Node(int value) {
			this(value, null);
		}

		public int getValue() {
			return m_value;
		}

		public void setNext(Node next) {
			m_next = next;
		}

		public Node getNext() {
			return m_next;
		}
	}

	/**
	 * This is called after the JVM warmup phase to make sure the data structure
	 * is well initalized. No need to do anything for this.
	 */
	public void clear() {
		return;
	}

	@Override
	@Atomic(metainf = "regular")
	public V putIfAbsent(K x, V y) {
		if (!containsInt((Integer) y))
			addInt((Integer) x);
		return null;
	}

	@Override
	@Atomic(metainf = "regular")
	public Object putIfAbsent(int x, int y) {
		if (!containsInt(y))
			addInt(x);
		return null;
	}

	@Override
	public V get(Object key) {
		if (containsInt((Integer) key))
			return (V) key;
		else
			return null;
	}

	// @Override
	// public V putIfAbsent(K k, V v) {
	// // TODO Auto-generated method stub
	// return null;
	// }

	@Override
	public V remove(Object key) {
		if (removeInt((Integer) key))
			return (V) key;
		else
			return null;
	}

	@Override
	public boolean containsKey(Object key) {
		return containsInt((Integer) key);
	}

	@Override
	public boolean containsValue(Object value) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public Set<java.util.Map.Entry<K, V>> entrySet() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean isEmpty() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public Set<K> keySet() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public V put(K key, V value) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void putAll(Map<? extends K, ? extends V> m) {
		// TODO Auto-generated method stub

	}

	@Override
	public Collection<V> values() {
		// TODO Auto-generated method stub
		return null;
	}

}
