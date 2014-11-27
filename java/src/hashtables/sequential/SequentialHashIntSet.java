package hashtables.sequential;

import java.util.Collection;
import java.util.Random;

import contention.abstractions.CompositionalIntSet;

/**
 * Sequential hash table implementation of integer set
 * @author Vincent Gramoli
 *
 */ 
public class SequentialHashIntSet implements CompositionalIntSet {

    private Node[] table;
    int tableSize;

    /** The thread-private PRNG */
    final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
        @Override
        protected synchronized Random initialValue() {
            return new Random();
        }
    };
    
    public SequentialHashIntSet(int size) {
        tableSize = size;
        table = new Node[tableSize];
    }

    public SequentialHashIntSet() {
        this(contention.benchmark.Parameters.size);
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
	public boolean addAll(Collection<Integer> c) {
        boolean result = true;
        for (Integer x : c) result &= this.addInt(x);
        return result;
    }

	@Override
	public boolean removeAll(Collection<Integer> c) {
        boolean result = true;
        for (Integer x : c) result &= this.removeInt(x);
        return result;
    }

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
    
    public class Node {

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
	 * This is called after the JVM warmup phase
	 * to make sure the data structure is well initalized.
	 * No need to do anything for this.
	 */
	public void clear() {
	    tableSize = contention.benchmark.Parameters.size;
	    table = new Node[tableSize];
	}

	@Override
	public Object getInt(int value) {
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
	public Object putIfAbsent(int x, int y) {
		if (!containsInt(x)) addInt(y);
		return null;
	}
}
