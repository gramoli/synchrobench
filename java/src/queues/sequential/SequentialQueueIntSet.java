package queues.sequential;

import java.util.Collection;
import java.util.Random;

import org.deuce.Atomic;

import contention.abstractions.CompositionalIntSet;
import contention.benchmark.Parameters;


/**
 * A sequential Queue implementated as a linked list
 * that also exports an integer set interface.
 * 
 * @author Vincent Gramoli
 *
 */
public class SequentialQueueIntSet implements CompositionalIntSet {

    private Node m_first;
    private Node m_last;
    
    /** The thread-private PRNG */
    final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
        @Override
        protected synchronized Random initialValue() {
	    return new Random();
	}
    };

    public SequentialQueueIntSet() {
        m_first = new Node(0);
        m_last = m_first;
    }

    public void fill(final int range, final long size) {
	while (this.size() < size) {
	    this.addInt(s_random.get().nextInt(range));
	}
    }
		
    public boolean addInt(int value) {
        push(value);
        return true;
    }

    /**
     * To respect the semantics of ConcurrentLinkedQueue
     * returns true as soon as a modif occurs
     */ 
    @Override
	public boolean addAll(Collection<Integer> C) {
        boolean result = false;
        for (int x : C) result |= this.addInt(x);
        return result;
    }

    public void push(int value) {
        Node newNode = new Node(value);
        m_last.setNext(newNode);
        m_last = newNode;
    }

    public boolean removeInt(int value) {
        Node previous = m_first;
        Node node = previous.getNext();
        boolean found = false;

        while (node != null) {
            if (node.getValue() == value) {
                found = true;
                break;
            }
            previous = node;
            node = previous.getNext();
        }
        if (found) {
            Node next = node.getNext();
            previous.setNext(next);
            node.setNext(next);
	    if (next == null) m_last = previous;
        }
        return found;
    }

    /**
     * To respect the semantics of ConcurrentLinkedQueue
     * returns true as soon as a modif occurs
     */ 
    @Override
	public boolean removeAll(Collection<Integer> C) {
        boolean result = false;
        for (int x : C) result |= this.removeInt(x);
        return result;
    }

    public int pop() {
        int value = m_first.getValue();
        if (m_first == m_last) {
            return -1;                          //queue is empty
        }
        m_first = m_first.getNext();
        return value;
    }

    public boolean containsInt(int value) {
        Node previous = m_first;
        Node node = previous.getNext();

        while (node != null) {
            if (node.getValue() == value) {
                return true;
            }
            previous = node;
            node = previous.getNext();
        }

        return false;
    }

    public int size() {
        int n = 0;
        Node node = m_first;

        while (node != null) {
            n++;
            node = node.getNext();
        }
        return n;
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
     *
     * Note the ugly hack to reset the init size of the queue after warmup 
     * otherwise queue size grow as fast as its add ops execute: they are 
     * always successful, as opposed to its remove.
     */
    public void clear() {
	m_last = m_first; 
	fill(Parameters.range, Parameters.size);
    	return;	
    }
    
    public Object getInt(int value) {
        Node previous = m_first;
        Node node = previous.getNext();
	
        while (node != null) {
            if (node.getValue() == value) {
                return node;
            }
            previous = node;
            node = previous.getNext();
        }
        
	return null;
    }
    
    public Object putIfAbsent(int x, int y) {
	if (!containsInt(x)) addInt(y);
	return null;
    }
}
