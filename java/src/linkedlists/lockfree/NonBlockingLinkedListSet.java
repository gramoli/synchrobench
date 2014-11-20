package linkedlists.lockfree;

import java.util.Collection;
import java.util.concurrent.atomic.AtomicMarkableReference;

import contention.abstractions.AbstractCompositionalIntSet;

/**
 * This is a variant of the Harris-Michael algorithm in Java 
 * as presented in the Chapter 9 of the Art of Multiprocessor 
 * Programming by Herlihy and Shavit.
 * 
 * @author Vincent Gramoli
 *
 */

public class NonBlockingLinkedListSet extends AbstractCompositionalIntSet {
	private final Node tail;
	private final Node head;

	public NonBlockingLinkedListSet() {
		tail = new Node(Integer.MAX_VALUE, null);
		head = new Node(Integer.MIN_VALUE, tail);
	}

	class Window {
		public Node pred, curr;

		public Window(Node pred, Node curr) {
			this.pred = pred;
			this.curr = curr;
		}
	}

	public Window find(Node head, int value) {
		Node pred = null, curr = null, succ = null;
		boolean[] marked = { false };
		boolean snip;
		retry: while (true) {
			pred = head;
			curr = pred.next.getReference();
			while (true) {
				succ = curr.next.get(marked);
				while (marked[0]) {
					snip = pred.next.compareAndSet(curr, succ, false, false);
					if (!snip) {
						continue retry;
					}
					curr = succ;
					succ = curr.next.get(marked);
				}
				if (curr.value >= value) {
					return new Window(pred, curr);
				}
				pred = curr;
				curr = succ;
			}
		}
	}

	@Override
	public void fill(int range, long size) {
		throw new RuntimeException("unimplemented method");
		// TODO Auto-generated method stub

	}

	@Override
	public boolean addInt(int x) {
		while (true) {
			Window window = find(head, x);
			Node pred = window.pred, curr = window.curr;
			if (curr.value == x) {
				return false;
			} else {
				Node node = new Node(x, curr);
				if (pred.next.compareAndSet(curr, node, false, false)) {
					return true;
				}
			}
		}
	}

	@Override
	public boolean removeInt(int x) {
		boolean snip;
		while (true) {
			Window window = find(head, x);
			Node pred = window.pred, curr = window.curr;
			if (curr.value != x) {
				return false;
			} else {
				Node succ = curr.next.getReference();
				snip = curr.next.attemptMark(succ, true);
				if (!snip) {
					continue;
				}
				pred.next.compareAndSet(curr, succ, false, false);
				return true;
			}
		}
	}

	@Override
	public boolean containsInt(int x) {
		boolean[] marked = { false };
		Node curr = head;
		while (curr.value < x) {
			curr = curr.next.getReference();
			curr.next.get(marked);
		}
		return (curr.value == x && !marked[0]);
	}

	@Override
	public Object getInt(int x) {
		throw new RuntimeException("unimplemented method");
		// TODO Auto-generated method stub
	}

	@Override
	public boolean addAll(Collection<Integer> c) {
		throw new RuntimeException("unimplemented method");
		// TODO Auto-generated method stub
	}

	@Override
	public boolean removeAll(Collection<Integer> c) {
		throw new RuntimeException("unimplemented method");
		// TODO Auto-generated method stub
	}

	@Override
	public int size() {
		int size = 0;
		boolean[] marked = { false };
		for (Node curr = head.next.getReference(); curr != tail;){
			curr = curr.next.get(marked);
			if (!marked[0]){
				size++;
			}
		}
		return size;
	}

	@Override
	public void clear() {
		head.next = new AtomicMarkableReference<Node>(tail, false);
	}
}
