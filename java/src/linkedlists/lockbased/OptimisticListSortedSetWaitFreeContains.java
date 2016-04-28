package linkedlists.lockbased;

import java.util.Collection;
import java.util.Random;
import java.util.Stack;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import contention.abstractions.AbstractCompositionalIntSet;

/**
 * Linked list implementation of integer set using 
 * optimistic locking except for the contains.
 * 
 * @author Vincent Gramoli
 *
 */
public class OptimisticListSortedSetWaitFreeContains extends AbstractCompositionalIntSet {
 
    /** The first node of the list */ 
    final private Node head;
   	
    public OptimisticListSortedSetWaitFreeContains() {
        Node min = new Node(Integer.MIN_VALUE);
        Node max = new Node(Integer.MAX_VALUE);
        min.setNext(max);
        head = min;
    }

    private boolean validate(Node pred, Node curr) {
	Node node = head;
	while (node.getValue() <= pred.getValue()) {
	    if (node == pred)
		return pred.getNext() == curr;
	    node = node.getNext();
	}
	return false;
    }
    
    public boolean addInt(int value) {
	while (true) {
	    Node pred = head;
	    Node curr = pred.getNext();
	    while (curr.getValue() < value) {
		pred = curr;
		curr = curr.getNext();
	    }
	    pred.lock();
	    curr.lock();
	    try {
		if (validate(pred,curr)) {
		    if (curr.getValue() == value) {
			return false;
		    } else {
			Node node = new Node(value, curr);
			pred.setNext(node);
			return true;
		    }
		}
	    } finally {
		pred.unlock();
		curr.unlock();
	    }
	}
    }

    public boolean removeInt(int value) {
	while (true) {
	    Node pred = head;
	    Node curr = pred.getNext();
	    while (curr.getValue() < value) {
		pred = curr;
		curr = curr.getNext();
	    }
	    pred.lock();
	    curr.lock();
	    try {
		if (validate(pred,curr)) {
		    if (curr.getValue() == value) {
			pred.setNext(curr.getNext());
			return true;
		    } else {
			return false;
		    }
		}
	    } finally {
		pred.unlock();
		curr.unlock();
	    }
	}
    }

    /** 
     * The contains does not use any locks
     * It is wait-free provided that not an infinite
     * number of insertions are concurrent
     */
    public boolean containsInt(int value) {
	Node pred = head;
	Node curr = pred.getNext();
	while (curr.getValue() < value) {
	    pred = curr;
	    curr = curr.getNext();
	}
	return (curr.getValue() == value);
    }

    /**
     * This method cannot be supported with
     * such locking mechanism
     */	
    @Override
    public boolean addAll(Collection<Integer> c) {
       throw new UnsupportedOperationException();
    }

    /**
     * This method cannot be supported with
     * such locking mechanism
     */	
    @Override
    public boolean removeAll(Collection<Integer> c) {
        throw new UnsupportedOperationException();
    }

    /**
     * This method is not thread-safe. It cannot 
     * be made atomic with such locking mechanism
     */
    public int size() {
        int n = 0;
        Node node = head;

        while (node.getNext().getValue() < Integer.MAX_VALUE) {
            n++;
            node = node.getNext();
        }
        return n;
    }

    public void clear() {
	Node max = new Node(Integer.MAX_VALUE);
	head.setNext(max);
    }

    @Override
    public Object getInt(int x) {
	throw new UnsupportedOperationException();	
    }
    
    @Override
    public Object putIfAbsent(int x, int y) {
	throw new UnsupportedOperationException();
    }

    
    public class Node {

        final private int value;
        private volatile Node next;
	final private Lock lock;

        public Node(int value, Node next) {
            this.value = value;
            this.next = next;
            this.lock = new ReentrantLock();
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

	public void lock() {
	    this.lock.lock();
	}
	
	public void unlock() {
	    this.lock.unlock();
	}
    }    
}
