package linkedlists.sequential;

import java.util.Collection;
import java.util.Comparator;
import java.util.Iterator;
import java.util.SortedSet;
import java.util.Stack;

import contention.abstractions.CompositionalIterator;
import contention.abstractions.CompositionalSortedSet;

/**
 * Sequential linked list implementation of a sorted set
 * 
 * @author Vincent Gramoli
 */
public class SequentialLinkedListSortedSet<E extends Comparable<E>> implements CompositionalSortedSet<E> {

	
	/** The first node of the list */
    private Node head;

	public SequentialLinkedListSortedSet() {
		super();
		head = null;
	}

	@Override
	public int size() {
        int n = 0;
        Node curr = head;
        while (curr != null) {
        	curr = curr.getNext();
        	n++;
        }
        return n;
	}


	@Override
	public boolean add(E e) { 
		Node prev = null;
		Node next = head;
		E v;
		boolean found = false;
		
		if (next == null) { // empty
			head = new Node(e, next);
			found = false;
		} else { // non-empty
			while ((v = next.getValue()).compareTo(e) < 0) {
				prev = next;
				next = next.getNext();
				if (next == null) break;
			}
			if (v.compareTo(e) == 0) {
				found = true;
			} else {
				if (prev == null) {
					//head.setNext(new Node(e, next));
					Node n = new Node(e, next);
					head = n;
				}
				else prev.setNext(new Node(e, next));
			}
		}
		return !found;
	}

	@Override
	public boolean addAll(Collection<? extends E> c) {
        boolean result = true;
        for (E x : c) result &= this.add(x);
        return result;
	}

	/** 
	 * This is called after the JVM warmup phase
	 * to make sure the data structure is well initalized.
	 * No need to do anything for this.
	 */
	@Override
	public void clear() {
	    head = null;
	}

	@SuppressWarnings("unchecked")
	@Override
	public boolean contains(Object e) {
		Node next = head;
		E v;
		boolean found = false;
		
		if (next == null) { // empty
			head = new Node((E) e, next);
			found = false;
		} else { // non-empty
			while ((v = next.getValue()).compareTo((E) e) < 0) {
				next = next.getNext();
				if (next == null) break;
			}
			found = (v.compareTo((E) e) == 0);
		}
		return found;
	}

	@Override
	public boolean containsAll(Collection<?> c) {
        boolean result = true;
        for (Object x : c) result &= this.contains(x);
        return result;
	}

	@Override
	public boolean isEmpty() {
		return size() == 0;
	}

	@SuppressWarnings("unchecked")
	@Override
	public Iterator<E> iterator() {
		Iterator<E> iterator = (Iterator<E>) new LLIterator();
		return iterator;
	}

	@Override
	public boolean remove(Object e) {
		Node prev = null;
		Node next = head;
		E v;
		boolean found = false;
		
		if (next == null) { // empty
			head = new Node((E) e, next);
			found = false;
		} else { // non-empty
			while ((v = next.getValue()).compareTo((E) e) < 0) {
				prev = next;
				next = next.getNext();
				if (next == null) break;
			}
			if (v.compareTo((E) e) == 0) {
				if (prev == null) {
					head = null;
				} else {
					found = true;
					prev.setNext(next.getNext());
				}
			}
		}
		return found;
	}

	@Override
	public boolean removeAll(Collection<?> c) {
        boolean result = true;
        for (Object x : c) result &= this.remove(x);
        return result;
	}

	@Override
	public boolean retainAll(Collection<?> c) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public Object[] toArray() {
    	throw new UnsupportedOperationException();
	}

	@Override
	public <T> T[] toArray(T[] a) {
    	throw new UnsupportedOperationException();
	}
    
    public class Node {

        final private E value;
        private Node next;

        public Node(E value, Node next) {
            this.value = value;
            this.next = next;
        }

        public Node(E value) {
            this(value, null);
        }

        public E getValue() {
            return value;
        }

        public void setNext(Node next) {
            this.next = next;
        }

        public Node getNext() {
            return next;
        }
    }
    
    public class LLIterator implements CompositionalIterator<E> {
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

        public E next() {
        	Node node = next;
        	next = stack.pop();
        	return node.getValue();
        }
    }

    /*public class LLComparator implements Comparator<E> {

        // Comparator interface requires defining compare method.
        public int compare(E e1, E e2) {
            //... Sort directories before files,
            //    otherwise alphabetical ignoring case.
            if (e1.getClass() != e2.getClass()) {
                return -1;
            } else if (!e1.getClass() && e2.getClass()) {
                return 1;
            } else {
                return e1.getName().compareToIgnoreCase(e2.getName());
            }
        }
    }*/
    

	@Override
	public Comparator<? super E> comparator() {
    	throw new UnsupportedOperationException();
	}

	@Override
	public E first() {
    	throw new UnsupportedOperationException();
	}

	@Override
	public SortedSet<E> headSet(E toElement) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public E last() {
    	throw new UnsupportedOperationException();
	}

	@Override
	public SortedSet<E> subSet(E fromElement, E toElement) {
    	throw new UnsupportedOperationException();
	}

	@Override
	public SortedSet<E> tailSet(E fromElement) {
    	throw new UnsupportedOperationException();
	}

}
