package linkedlists.lockbased;

import contention.abstractions.CompositionalSortedSet;

import java.util.Collection;
import java.util.Comparator;
import java.util.Iterator;
import java.util.SortedSet;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import contention.abstractions.AbstractCompositionalIntSet;

public class CoarseGrainedListBasedSet extends AbstractCompositionalIntSet {

    // sentinel nodes
    private Node head;
    private Node tail;
    private Lock lock = new ReentrantLock();

    public CoarseGrainedListBasedSet(){     
	  head = new Node(Integer.MIN_VALUE);
	  tail = new Node(Integer.MAX_VALUE);
          head.next = tail;
    }
    
    /*
     * Insert
     * 
     * @see contention.abstractions.CompositionalIntSet#addInt(int)
     */
    @Override
    public boolean addInt(int item){
         lock.lock(); 
	 try {  
	     Node pred = head;
	     Node curr = head.next;
	     while (curr.key < item){
		 pred = curr;
		 curr = pred.next;
	     }
	     if (curr.key == item) {
		 return false;
	     } else {
		 Node node = new Node(item);
		 node.next=curr;
		 pred.next=node;
		 return true;
	     }
	 } finally{
	     lock.unlock();
	 } 	 
    }
    
    /*
     * Remove
     * 
     * @see contention.abstractions.CompositionalIntSet#removeInt(int)
     */
    @Override
    public boolean removeInt(int item){
	lock.lock(); 
	try {  
	    Node pred = head;
	    Node curr = head.next;
	    while (curr.key < item) {
		pred = curr;
		curr = pred.next;
	    }
	    if (curr.key == item) {
		pred.next = curr.next;
		return true;
	    } else {
		return false;
	    }
	} finally{
	    lock.unlock();
	} 
    }
    
    /*
     * Contains
     * 
     * @see contention.abstractions.CompositionalIntSet#containsInt(int)
     */
    @Override
    public boolean containsInt(int item){
	lock.lock(); 
	try {
	    Node pred = head;
	    Node curr = head.next;
	    while (curr.key < item) {
		pred = curr;
		curr = pred.next;
	    }
	    if (curr.key == item){
		return true;
	    } else {
		return false;
	    }
        } finally{
	    lock.unlock();
	} 
    }
 
    private class Node {
	Node(int item) {
	    key = item;
	    next = null;
	}
	public int key;
	public Node next;
    }

    @Override
    public void clear() {
       head = new Node(Integer.MIN_VALUE);
       head.next = new Node(Integer.MAX_VALUE);
    }

    /**
     * Non atomic and thread-unsafe
     */
    @Override
    public int size() {
        int count = 0;

        Node curr = head.next;
        while (curr.key != Integer.MAX_VALUE) {
            curr = curr.next;
            count++;
        }
        return count;
    }
}
