package linkedlists.lockbased;

import java.util.concurrent.locks.ReentrantReadWriteLock;
import contention.abstractions.AbstractCompositionalIntSet;

/**
 * A linked list implementation of an integer set that 
 * uses a reader lock for contains and a write lock for updates.
 * 
 * @author Vincent Gramoli
 */
public class RWLockCoarseGrainedListIntSet extends AbstractCompositionalIntSet {

    final private Node head;
    final private Node tail;
    private ReentrantReadWriteLock lock = new ReentrantReadWriteLock();

    public RWLockCoarseGrainedListIntSet(){     
	tail = new Node(Integer.MAX_VALUE);
	head = new Node(Integer.MIN_VALUE, tail);
    }

    public boolean addInt(int item){
	lock.writeLock().lock(); 
	try {  
	    Node pred = head;
	    Node curr = head.next;
	    while (curr.key < item) {
		pred = curr;
		curr = pred.next;
	    }
	    if (curr.key == item){
		return false;
	    } else {
		pred.next = new Node(item, curr);
		return true; 
	    }
	} finally{
	    lock.writeLock().unlock();
	} 
    }
        
    @Override
    public boolean removeInt(int item) {
	lock.writeLock().lock(); 
	try {  
	    Node pred = head;
	    Node curr = head.next;
	    while (curr.key < item) {
		pred = curr;
		curr = pred.next;}
	    if (curr.key == item) {
		pred.next = curr.next; 
		return true;
	    } else {
		return false;
	    }
	} finally {
	    lock.writeLock().unlock();
	} 
    }

    public boolean containsInt(int item){
	lock.readLock().lock(); 
	try {
	    Node pred=head;
	    Node curr=head.next;
	    while (curr.key < item) {
		pred = curr;
		curr = pred.next;
	    }
	    return (curr.key == item);
        } finally {
	    lock.readLock().unlock();
	} 
    }

    @Override
    public int size() {
	int size = 0;
	Node curr = head.next;
	Node pred;

	lock.readLock().lock();
	try {
	    while (curr.next != null) {
		pred = curr;
		curr = pred.next;
		size++;
	    }
	} finally {
	    lock.readLock().unlock();
	}
	return size;
    }

    @Override
    public void clear() {
	head.next = tail;
    }

    /**
     * The node of an integer list
     */
    private class Node {
	final public int key;
	public Node next;

	Node(int item) {
	    key = item;
	    next = null;
	}
	
	Node(int item, Node n) {
	    key = item;
	    next = n;
	}
    }
}
