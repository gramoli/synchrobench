package linkedlists.lockfree;

import java.util.concurrent.atomic.AtomicReference;

/**
 * NodeLinked used to indicate the logically deleted (marked) state of a node 
 * detectable using Run-Time Type Identification (RTTI), hence speeding up 
 * Java variant of Harris-Michael. This is the code used in:
 * 
 * A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang. 2015.
 * 
 * @author Di Shang
 */
public class NodeLinked implements NodeBase {
    public final int value;
    public final AtomicReference<NodeBase> next = new AtomicReference<NodeBase>();

    public NodeLinked(final int value, final NodeBase next) {
        this.value = value;
        this.next.set(next);
    }

    @Override
    public int value() {
        return value;
    }

    @Override
    public NodeBase next() {
        return next.get();
    }

    @Override
    public boolean casNext(NodeBase old, NodeBase newN) {
        return this.next.compareAndSet(old, newN);
    }

    public void setNext(NodeLinked next) {
        this.next.set(next);
    }
}
