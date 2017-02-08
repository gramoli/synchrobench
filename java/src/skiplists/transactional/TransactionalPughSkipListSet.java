package skiplists.transactional;

import java.util.Collection;
import java.util.Map;
import java.util.Random;
import java.util.Set;

import org.deuce.Atomic;

import contention.abstractions.CompositionalIntSet;
import contention.abstractions.CompositionalMap;

/**
 * The Transactional Skip List implementation of integer set
 * 
 * @author Vincent Gramoli
 * 
 */
public class TransactionalPughSkipListSet<K, V> implements CompositionalIntSet,
		CompositionalMap<K, V> {

	/** The maximum number of levels */
	final private int maxLevel;
	/** The first element of the list */
	final public Node head;
    /** The thread-private PRNG, used for fil(), not for height/level determination. */
	final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
		@Override
		protected synchronized Random initialValue() {
			return new Random();
		}
	};

	public TransactionalPughSkipListSet() {
		this(31);
	}

	public TransactionalPughSkipListSet(final int maxLevel) {
		this.maxLevel = maxLevel;
		this.head = new Node(maxLevel, Integer.MIN_VALUE);
		final Node tail = new Node(maxLevel, Integer.MAX_VALUE);
		for (int i = 0; i <= maxLevel; i++) {
			head.setNext(i, tail);
		}
	}

	public void fill(final int range, final long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}

	protected int randomLevel() {
	    return Math.min((maxLevel - 1), (skiplists.RandomLevelGenerator.randomLevel()));
	}

	@Atomic(metainf = "NT")
	public void test(int a) {
		a = a + 1;
	}

	@Override
	@Atomic(metainf = "elastic")
	public boolean containsInt(final int value) {
		boolean result;

		Node node = head;

		for (int i = maxLevel; i >= 0; i--) {
			Node next = node.getNext(i);
			while (next.getValue() < value) {
				node = next;
				next = node.getNext(i);
			}
		}
		node = node.getNext(0);

		result = (node.getValue() == value);

		return result;
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
	public boolean addInt(final int value) {
		if (containsInt(value))
			return false;
		boolean result;

		Node[] update = new Node[maxLevel + 1];
		Node node = head;

		for (int i = maxLevel; i >= 0; i--) {
			Node next = node.getNext(i);
			while (next.getValue() < value) {
				node = next;
				next = node.getNext(i);
			}
			update[i] = node;
		}
		node = node.getNext(0);

		if (node.getValue() == value) {
			result = false;
		} else {
			int level = randomLevel();
			node = new Node(level, value);
			for (int i = 0; i <= level; i++) {
				node.setNext(i, update[i].getNext(i));
				update[i].setNext(i, node);
			}
			result = true;
		}
		return result;
	}

	@Override
	@Atomic(metainf = "elastic")
	public boolean removeInt(int value) {
		if (!containsInt(value))
			return false;
		boolean result;

		Node[] update = new Node[maxLevel + 1];
		Node node = head;

		for (int i = maxLevel; i >= 0; i--) {
			Node next = node.getNext(i);
			while (next.getValue() < value) {
				node = next;
				next = node.getNext(i);
			}
			update[i] = node;
		}
		node = node.getNext(0);

		if (node.getValue() != value) {
			result = false;
		} else {
			int maxLevel = node.getLevel();
			for (int i = 0; i <= maxLevel; i++) {
				update[i].setNext(i, node.getNext(i));
			}
			result = true;
		}

		return result;
	}

	@Override
	@Atomic(metainf = "elastic")
	public boolean addAll(Collection<Integer> c) {
		boolean result = true;
		for (int x : c)
			result &= this.addInt(x);
		return result;
	}

	@Override
	@Atomic(metainf = "elastic")
	public boolean removeAll(Collection<Integer> c) {
		boolean result = true;
		for (int x : c)
			if (!removeInt(x))
				result = false;
		return result;
	}

	@Override
	@Atomic(metainf = "regular")
	public int size() {
		int s = 0;
		Node node = head.getNext(0).getNext(0);

		while (node != null) {
			node = node.getNext(0);
			s++;
		}
		return s;
	}

	@Override
	@Atomic
	public String toString() {
		// String str = new String();
		// Node curr = head;
		// int i, j;
		// final int[] arr = new int[maxLevel+1];
		//
		// for (i=0; i<= maxLevel; i++) arr[i] = 0;
		//
		// do {
		// str += curr.toString();
		// arr[curr.getLevel()]++;
		// curr = curr.getNext(0);
		// } while (curr != null);
		// for (j=0; j < maxLevel; j++)
		// str += arr[j] + " nodes of level " + j + " ";
		// return str;
		return getBottom();
	}

	public String getBottom() {
		String str = new String();
		Node curr = head.getNext(0);
		do {
			str += curr.toString();
			curr = curr.getNext(0);
		} while (curr != null);
		return str;
	}

	public class Node<K, V> {

		final private int value;
		final private Node[] next;

		public Node(final int level, final int value) {
			this.value = value;
			next = new Node[level + 1];
		}

		public int getValue() {
			return value;
		}

		public int getLevel() {
			return next.length - 1;
		}

		public void setNext(final int level, final Node succ) {
			next[level] = succ;
		}

		public Node getNext(final int level) {
			return next[level];
		}

		@Override
		public String toString() {
			String result = "";
			result += "<l=" + getLevel() + ",v=" + value + ">:";
			for (int i = 0; i <= getLevel(); i++) {
				result += " @[" + i + "]=";
				if (next[i] != null) {
					result += next[i].getValue();
				} else {
					result += "null";
				}
			}
			return result;
		}
	}

	// public class SLIterator implements CompositionalIterator<Integer> {
	// Node next = head;
	// Stack<Node> stack = new Stack<Node>();
	//
	// SLIterator() {
	// while (next != null) {
	// stack.push(next.next[0]);
	// }
	// }
	//
	// public boolean hasNext() {
	// return next != null;
	// }
	//
	// public void remove() {
	// throw new UnsupportedOperationException();
	// }
	//
	// public Integer next() {
	// Node node = next;
	// next = stack.pop();
	// return node.getValue();
	// }
	// }

	/**
	 * This is called after the JVM warmup phase to make sure the data structure
	 * is well initalized. No need to do anything for this.
	 */
	public void clear() {
		return;
	}

	// @Override
	// @Atomic(metainf = "regular")
	// public Object putIfAbsent(final int x, final int y) {
	// if (!contains(y))
	// add(x);
	// return null;
	// }

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
