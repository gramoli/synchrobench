package linkedlists.lockfree;

/**
 * Parent Node class used for the Run-Time Type Identification (RTTI)
 * version of Harris-Michael's variant in Java. This is the code used in:
 * 
 * A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang. 2015.
 * 
 * @author Di Shang
 */
public interface NodeBase {

    public int value();

    public NodeBase next();

    public boolean casNext(NodeBase old, NodeBase newN);

}
