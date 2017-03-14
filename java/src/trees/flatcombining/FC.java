package trees.flatcombining;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by vaksenov on 16.01.2017.
 */
public class FC {
    private final static int DELTA = 1000;

    AtomicInteger lock = new AtomicInteger();
    AtomicReference<FCRequest> tail;
    final FCRequest DUMMY;
    int current_timestamp;

    public FC() {
        DUMMY = new FCRequest() {
            public boolean holdsRequest() {
                return true;
            }
        };
        tail = new AtomicReference<>(DUMMY);
    }

    public boolean tryLock() {
        return lock.get() == 0 && lock.compareAndSet(0, 1);
    }

    public void unlock() {
        lock.set(0);
    }

    public void addRequest(FCRequest request) {
        if (request.next != null || !request.holdsRequest()) { // The request is not old yet
            return;
        }
        do {
            request.next = tail.get();
        } while (!tail.compareAndSet(request.next, request));
    }

    public FCRequest[] loadRequests() {
        FCRequest tail = this.tail.get();
        ArrayList<FCRequest> requests = new ArrayList<>();
        while (tail != DUMMY) {
            if (tail.holdsRequest()) {
                tail.timestamp = current_timestamp;
                requests.add(tail);
            }
            tail = tail.next;
//            System.err.println(tail);
        }
        return requests.toArray(new FCRequest[0]);
    }

    public void cleanup() {
        current_timestamp++;
        if (current_timestamp % DELTA != 0) {
            return;
        }
        FCRequest tail = this.tail.get();
        while (tail.next != DUMMY) {
            FCRequest next = tail.next;
            if (next == null) {
                break;
            }
            if (next.timestamp + DELTA < current_timestamp && !next.holdsRequest()) { // The node has not updated for a long time, better to remove it
                tail.next = next.next;
                next.next = null;
            } else {
                tail = next;
            }
        }
    }
}
