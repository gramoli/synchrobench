package linkedlists.lockfree;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class Node {
	public int value;
	public volatile AtomicMarkableReference<Node> next;

	public Node(final int value, final Node next) {
		this.value = value;
		this.next = new AtomicMarkableReference<Node>(next, false);
	}
}
