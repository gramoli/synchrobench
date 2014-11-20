package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

class LFListNode
{
    /* States of Nodes. */
    public static final int DATA    = 0;
    public static final int DEAD    = 1;
    public static final int INSERT  = 2;
    public static final int REMOVE  = 3;
    public static final int FREEZE  = 4;

    public int key;
    public volatile int state;
    public volatile LFListNode next;

    public static final AtomicIntegerFieldUpdater<LFListNode> stateUpdater
        = AtomicIntegerFieldUpdater.newUpdater(LFListNode.class, "state");
    public static final AtomicReferenceFieldUpdater<LFListNode, LFListNode> nextUpdater
        = AtomicReferenceFieldUpdater.newUpdater(LFListNode.class, LFListNode.class, "next");

    public boolean casState(int o, int n)
    {
        return stateUpdater.compareAndSet(this, o, n);
    }

    public boolean casNext(LFListNode o, LFListNode n)
    {
        return nextUpdater.compareAndSet(this, o, n);
    }

    public boolean immutable()
    {
        return state == FREEZE;
    }

    public LFListNode(int k, int s)
    {
        this.key = k;
        this.state = s;
    }
}
