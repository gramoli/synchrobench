package linkedlists.sequential;

import java.util.Collection;
import java.util.Random;
import java.util.Stack;

import contention.abstractions.CompositionalIntSet;
import contention.abstractions.CompositionalIterator;

/**
 * Sequential linked list implementation of integer set
 * 
 * @author Vincent Gramoli
 */
public class SequentialLinkedListIntSet implements CompositionalIntSet {

	/** The first node of the list */
    final private Node head;
    /** The thread-private PRNG */
    final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
        @Override
        protected synchronized Random initialValue() {
            return new Random();
        }
    };
	
    public SequentialLinkedListIntSet() {
        Node min = new Node(Integer.MIN_VALUE);
        Node max = new Node(Integer.MAX_VALUE);
        min.setNext(max);
        head = min;
    }

    public boolean addInt(int value) {
        boolean result;

        Node previous = head;
        Node next = previous.getNext();
        int v;
        while ((v = next.getValue()) < value) {
            previous = next;
            next = previous.getNext();
        }
        result = v != value;
        if (result) {
            previous.setNext(new Node(value, next));
        }

        return result;
    }

    public boolean removeInt(int value) {
        boolean result;

        Node previous = head;
        Node next = previous.getNext();
        int v;
        while ((v = next.getValue()) < value) {
            previous = next;
            next = previous.getNext();
        }
        result = v == value;
        if (result) {
            previous.setNext(next.getNext());
        }

        return result;
    }

    public boolean containsInt(int value) {
        boolean result;

        Node previous = head;
        Node next = previous.getNext();
        int v;
        while ((v = next.getValue()) < value) {
            previous = next;
            next = previous.getNext();
        }
        result = (v == value);

        return result;
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
        int n = 0;
        Node node = head;

        while (node.getNext().getValue() < Integer.MAX_VALUE) {
            n++;
            node = node.getNext();
        }
        return n;
    }

	@Override
	public void fill(final int range, final long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}
    
    public class Node {

        final private int value;
        private Node next;

        public Node(int value, Node next) {
            this.value = value;
            this.next = next;
        }

        public Node(int value) {
            this(value, null);
        }

        public int getValue() {
            return value;
        }

        public void setNext(Node next) {
            this.next = next;
        }

        public Node getNext() {
            return next;
        }
    }
    
    
    public class LLIterator implements CompositionalIterator<Integer> {
        Node next = head;
        Stack<Node> stack = new Stack<Node>();
        
        LLIterator() {
        	while (next != null) {
        		stack.push(next.next);
        	}
        }

        public boolean hasNext() {
        	return next != null;
        }

        public void remove() {
        	throw new UnsupportedOperationException();
        }

        public Integer next() {
        	Node node = next;
        	next = stack.pop();
        	return node.getValue();
        }
    }
    
	/** 
	 * This is called after the JVM warmup phase
	 * to make sure the data structure is well initalized.
	 * No need to do anything for this.
	 */
	public void clear() {
	    Node max = new Node(Integer.MAX_VALUE);
	    head.setNext(max);
	}

	@Override
	public Object getInt(int value) {
        Node previous = head;
        Node next = previous.getNext();
        int v;
        while ((v = next.getValue()) < value) {
            previous = next;
            next = previous.getNext();
        }
        if (v == value) return next;

        return null;
	}

	@Override
	public Object putIfAbsent(int x, int y) {
		if (!containsInt(x)) addInt(y);
		return null;
	}
}
