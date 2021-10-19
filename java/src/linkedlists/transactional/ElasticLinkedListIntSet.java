package linkedlists.transactional;

import java.util.Collection;
import java.util.Random;
import java.util.Stack;

import org.deuce.Atomic;

import contention.abstractions.CompositionalIntSet;
import contention.abstractions.CompositionalIterator;

/**
 * Linked list implementation of integer set using 
 * Elastic transactions as described in:
 * 
 * P. Felber, V. Gramoli, R. Guerraoui. Elastic
 * Transactions. DISC 2009.
 * 
 * @author Vincent Gramoli 
 *
 */
public class ElasticLinkedListIntSet implements CompositionalIntSet {

	/** The first node of the list */ 
    final private Node head;
    /** The thread-private PRNG */
    final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
        @Override
        protected synchronized Random initialValue() {
            return new Random();
        }
    };
	
    public ElasticLinkedListIntSet() {
        Node min = new Node(Integer.MIN_VALUE);
        Node max = new Node(Integer.MAX_VALUE);
        min.setNext(max);
        head = min;
    }

    @Atomic(metainf = "elastic")
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

    @Atomic(metainf = "elastic")
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

    @Atomic(metainf = "elastic")
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
	@Atomic(metainf = "elastic")
	public boolean addAll(Collection<Integer> c) {
        boolean result = true;
        for (Integer x : c) result &= this.addInt(x);
        return result;
    }

	@Override
	@Atomic(metainf = "elastic")
	public boolean removeAll(Collection<Integer> c) {
        boolean result = true;
        for (Integer x : c) result &= this.removeInt(x);
        return result;
    }


    @Atomic(metainf = "regular")
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

	public void clear() {
	    Node max = new Node(Integer.MAX_VALUE);
	    head.setNext(max);
	}

	@Override
	public Object getInt(int x) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Object putIfAbsent(int x, int y) {
		// TODO Auto-generated method stub
		return null;
	}
}
