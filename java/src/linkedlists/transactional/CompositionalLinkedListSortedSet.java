package linkedlists.transactional;

import java.util.Collection;
import java.util.Comparator;
import java.util.Iterator;
import java.util.SortedSet;
import java.util.Stack;

import org.deuce.Atomic;

import contention.abstractions.CompositionalIterator;
import contention.abstractions.CompositionalSortedSet;

public class CompositionalLinkedListSortedSet<E extends Comparable<E>> implements CompositionalSortedSet<E> {
	
	/** The first node of the list */
    private Node<E> head;

	public CompositionalLinkedListSortedSet() {
		super();
		head = null;
	}

    @Atomic (metainf = "roregular")
	public int size() {
        int n = 0;
        Node<E> curr = head;
        while (curr != null) {
        	curr = curr.getNext();
        	n++;
        }
        return n;
	}

    @Atomic (metainf = "elastic")
	public boolean add(E e) { 
		Node<E> prev = null;
		Node<E> next = head;
		E v;
		boolean found = false;
		
		if (next == null) { // empty
			head = new Node<E>(e, next);
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
					Node<E> n = new Node<E>(e, next);
					head = n;
				}
				else prev.setNext(new Node<E>(e, next));
			}
		}
		return !found;
	}

    @Atomic (metainf = "elastic")
	public boolean addAll(Collection<? extends E> c) {
        boolean result = true;
        for (E x : c) result &= this.add(x);
        return result;
	}

	/** 
	 * This is called after the JVM warmup phase
	 * to make sure the data structure is well initialized.
	 * No need to do anything for this.
	 */
	public void clear() {
    	return;	
	}

    @SuppressWarnings("unchecked")
	@Atomic (metainf = "elastic")
	public boolean contains(Object e) {
		Node<E> next = head;
		E v;
		boolean found = false;
		
		if (next != null) {
			while ((v = next.getValue()).compareTo((E) e) < 0) {
				next = next.getNext();
				if (next == null) break;
			}
			found = (v.compareTo((E) e) == 0);
		}
		return found;
	}

    @Atomic (metainf = "roregular")
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

    @SuppressWarnings("unchecked")
	@Atomic (metainf = "elastic")
	public boolean remove(Object e) {
		Node<E> prev = null;
		Node<E> next = head;
		E v;
		boolean found = false;
		
		if (next != null) {
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

    @Atomic (metainf = "elastic")
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

    public class Node<E extends Comparable<E>> {

        final private E value;
        private Node<E> next;

        public Node(E value, Node<E> next) {
            this.value = value;
            this.next = next;
        }

        public Node(E value) {
            this(value, null);
        }

        public E getValue() {
            return value;
        }

        public void setNext(Node<E> next) {
            this.next = next;
        }

        public Node<E> getNext() {
            return next;
        }
    }
    
    public class LLIterator implements CompositionalIterator<E> {
        Node<E> next = head;
        Stack<Node<E>> stack = new Stack<Node<E>>();
        
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
        	Node<E> node = next;
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
    	return head.getValue();
	}

	@Override
	public SortedSet<E> headSet(E toElement) {
    	return subSet(first(), toElement);
	}

    @Atomic (metainf = "elastic")
	@Override
	public E last() {
        Node<E> curr = head;
        while (curr != null) {
        	curr = curr.getNext();
        }
        return curr.getValue();
	}

    @Atomic (metainf = "elastic")
	@Override
	public SortedSet<E> subSet(E fromElement, E toElement) {
		Node<E> next = head;
		E v;
		CompositionalLinkedListSortedSet<E> set = new CompositionalLinkedListSortedSet<E>();
		
		while (next != null  & (next.getValue().compareTo((E) fromElement) < 0)) {
			next = next.getNext();
		}
		while (next != null  & ((v = next.getValue()).compareTo((E) toElement) < 0)) {
			set.add(v);
			next = next.getNext();
		}
		return set;
	}

	@Override
	public SortedSet<E> tailSet(E fromElement) {
    	return subSet(fromElement, last());
	}

}
