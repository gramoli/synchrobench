package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

class WFListNode
{
    /* States of Nodes. */
    public static final int DATA    = 0;
    public static final int DEAD    = 1;
    public static final int INSERT  = 2;
    public static final int REMOVE  = 3;
    public static final int FREEZE  = 4;

    public int key;
    public int type;
    public volatile int state;
    public volatile WFListNode next;
    public volatile WFListNode prev;
    public volatile long priority;


    public static final AtomicIntegerFieldUpdater<WFListNode> stateUpdater
        = AtomicIntegerFieldUpdater.newUpdater(WFListNode.class, "state");
    public static final AtomicReferenceFieldUpdater<WFListNode, WFListNode> nextUpdater
        = AtomicReferenceFieldUpdater.newUpdater(WFListNode.class, WFListNode.class, "next");
    private static final AtomicReferenceFieldUpdater<WFListNode, WFListNode> prevUpdater
        = AtomicReferenceFieldUpdater.newUpdater(WFListNode.class, WFListNode.class, "prev");

    public static WFListNode DUMMY = new WFListNode(-1, -1);

    public boolean casState(int o, int n)
    {
        return stateUpdater.compareAndSet(this, o, n);
    }

    public boolean casNext(WFListNode o, WFListNode n)
    {
        return nextUpdater.compareAndSet(this, o, n);
    }

    public boolean casPrev(WFListNode o, WFListNode n)
    {
        return prevUpdater.compareAndSet(this, o, n);
    }

    public boolean immutable()
    {
        return state == FREEZE;
    }

    public WFListNode(int k, int s)
    {
        this.key = k;
        this.type = s;
        this.state = s;
    }
}
