package linkedlists.lockfree;

/**
 * NodeLinked used to indicate the non-logically deleted (marked) state of a node 
 * detectable using Run-Time Type Identification (RTTI), hence speeding up 
 * Java variant of Harris-Michael. This is the code used in:
 * 
 * A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang. 2015.
 * 
 * @author Di Shang
 */
public class NodeMarked implements NodeBase {
    private final NodeLinked node;

    public NodeMarked(NodeLinked node) {
        this.node = node;
    }

    @Override
    public int value() {
        return node.value();
    }

    @Override
    public NodeBase next() {
        return node.next();
    }

    public NodeLinked getNonMarked() {
        return node;
    }

    @Override
    public boolean casNext(NodeBase old, NodeBase newN) {
        return node.casNext(old, newN);
    }
}
