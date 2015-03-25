package linkedlists.lockbased;

import java.util.Collection;
import java.util.Random;

import linkedlists.lockbased.lazyutils.Node;


import contention.abstractions.AbstractCompositionalIntSet;

/**
 * The code follows the lazy list-based set of Ch.9 of Herlihy and Shavit's book:
 * "The Art of Multiprocessor Programming".
 * 
 * @author gramoli
 * 
 */
public class LazyListBasedSet extends AbstractCompositionalIntSet {

    final public Node head;
    final public Node tail;

    public LazyListBasedSet() {
        head = new Node(Integer.MIN_VALUE);
        tail = new Node(Integer.MAX_VALUE);
        head.next = tail;
        tail.next = null;
    }

    @Override
    public void fill(int range, long size) {
        int i = 0;
        while (i < size)
            if (addInt(new Random().nextInt(range)))
                i++;
    }

    private boolean validate(Node pred, Node curr) {
        return !pred.marked && pred.next == curr;
    }

    @Override
    public boolean addInt(int v) {
        while (true) {
            Node pred = head;
            Node curr = head.next;
            while (curr.value < v) {
                pred = curr;
                curr = curr.next;
            }
            pred.lock();
            curr.lock();
            try {
                if (validate(pred, curr)) {
                    if (curr.value == v) {
                        return false;
                    } else {
                        Node node = new Node(v);
                        node.next = curr;
                        pred.next = node;
                        return true;
                    }
                }
            } finally {
                curr.unlock();
                pred.unlock();
            }
        }
    }

    @Override
    public boolean removeInt(int v) {
        while (true) {
            Node pred = head;
            Node curr = head.next;
            while (curr.value < v) {
                pred = curr;
                curr = curr.next;
            }
            pred.lock();
            try {
                curr.lock();
                try {
                    if (validate(pred, curr)) {
                        if (curr.value != v) {
                            return false;
                        } else {
                            curr.marked = true;
                            pred.next = curr.next;
                            return true;
                        }
                    }
                } finally {
                    curr.unlock();
                }
            } finally {
                pred.unlock();
            }
        }
    }

    @Override
    public boolean containsInt(int v) {
        Node curr = head;
        while (curr.value < v) {
            curr = curr.next;
        }
        return curr.value == v && !curr.marked;
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
        int cpt = 0;
        Node curr = head;
        while (curr.value < Integer.MAX_VALUE)
            curr = curr.next;
        if (!curr.marked)
            cpt++;
        return cpt;
    }

    @Override
    public void clear() {
        head.next = tail;
    }
}
