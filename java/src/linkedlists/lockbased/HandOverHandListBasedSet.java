package linkedlists.lockbased;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import contention.abstractions.AbstractCompositionalIntSet;

public class HandOverHandListBasedSet extends AbstractCompositionalIntSet {

    private static final class Node {
        final int key;
        Node next;
        final Lock lock = new ReentrantLock();

        Node(int key, Node next) {
            this.key = key;
            this.next = next;
        }
    }

    private Node head;

    public HandOverHandListBasedSet() {
        Node tail = new Node(Integer.MAX_VALUE, null);
        head = new Node(Integer.MIN_VALUE, tail);
    }

    @Override
    public boolean addInt(int item) {
        head.lock.lock();
        Node pred = head;
        try {
            Node curr = pred.next;
            curr.lock.lock();
            try {
                while (curr.key < item) {
                    pred.lock.unlock();
                    pred = curr;
                    curr = curr.next;
                    curr.lock.lock();
                }
                if (curr.key == item) {
                    return false;
                }
                pred.next = new Node(item, curr);
                return true;
            } finally {
                curr.lock.unlock();
            }
        } finally {
            pred.lock.unlock();
        }
    }

    @Override
    public boolean removeInt(int item) {
        head.lock.lock();
        Node pred = head;
        try {
            Node curr = pred.next;
            curr.lock.lock();
            try {
                while (curr.key < item) {
                    pred.lock.unlock();
                    pred = curr;
                    curr = curr.next;
                    curr.lock.lock();
                }
                if (curr.key == item) {
                    pred.next = curr.next;
                    return true;
                }
                return false;
            } finally {
                curr.lock.unlock();
            }
        } finally {
            pred.lock.unlock();
        }
    }

    @Override
    public boolean containsInt(int item) {
        head.lock.lock();
        Node pred = head;
        try {
            Node curr = pred.next;
            curr.lock.lock();
            try {
                while (curr.key < item) {
                    pred.lock.unlock();
                    pred = curr;
                    curr = curr.next;
                    curr.lock.lock();
                }
                return curr.key == item;
            } finally {
                curr.lock.unlock();
            }
        } finally {
            pred.lock.unlock();
        }
    }

    @Override
    public void clear() {
        Node tail = new Node(Integer.MAX_VALUE, null);
        head = new Node(Integer.MIN_VALUE, tail);
    }

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