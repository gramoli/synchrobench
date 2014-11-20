package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

class WFArrayFSet
{
    private static class FSet
    {
        public int[] arr;
        public volatile WFArrayOp op;

        public FSet(int [] a)
        {
            arr = a;
        }

        private static final AtomicReferenceFieldUpdater<FSet, WFArrayOp> opUpdater
            = AtomicReferenceFieldUpdater.newUpdater(FSet.class, WFArrayOp.class, "op");

        public boolean casOp(WFArrayOp o, WFArrayOp n)
        {
            return opUpdater.compareAndSet(this, o, n);
        }

        public boolean immutable()
        {
            return op != null && op.type == WFArrayOp.FREEZE;
        }
    }

    // field updater for head
    private static AtomicReferenceFieldUpdater<WFArrayFSet, FSet> headUpdater
        = AtomicReferenceFieldUpdater.newUpdater(WFArrayFSet.class, FSet.class, "head");

    // pointer to head fset object
    private volatile FSet head;

    // intention of freezing
    private volatile boolean fflag;

    // cas the head pointer
    private boolean casHead(FSet o, FSet n)
    {
        return headUpdater.compareAndSet(this, o, n);
    }

    // default constructor
    public WFArrayFSet()
    {
        head = new FSet(new int[0]);
    }

    // copy constructor
    public WFArrayFSet(int [] arr)
    {
        head = new FSet(arr);
    }

    public boolean invoke(WFArrayOp op)
    {
        FSet set = head;
        while (!set.immutable() && op.priority != Long.MAX_VALUE) {
            if (fflag) {
                doFreeze();
                return op.priority == Long.MAX_VALUE;
            }
            WFArrayOp pred = set.op;
            if (pred == null) {
                if (op.priority != Long.MAX_VALUE) {
                    if (set.casOp(null, op)) {
                        helpFinish(set);
                        return true;
                    }
                }
            }
            else {
                helpFinish(set);
            }
            set = head;
        }
        return op.priority == Long.MAX_VALUE;
    }

    public void freeze()
    {
        fflag = true;
        doFreeze();
    }

    private void doFreeze()
    {
        WFArrayOp h = new WFArrayOp(-1, WFArrayOp.FREEZE);
        FSet set = head;
        while (!set.immutable()) {
            WFArrayOp pred = set.op;
            if (pred == null) {
                if (set.casOp(null, h))
                    return;
            }
            else {
                helpFinish(set);
            }
            set = head;
        }
    }

    private void helpFinish(FSet set)
    {
        WFArrayOp op = set.op;
        if (op != null && op.type != WFArrayOp.FREEZE) {
            int [] n = (op.type == WFArrayOp.INSERT)
                ? arrayInsert(set.arr, op.key)
                : arrayRemove(set.arr, op.key);
            op.resp = (n == set.arr) ? -(n.length + 1) : (n.length + 1);
            op.priority = Long.MAX_VALUE;
            casHead(set, new FSet(n));
        }
    }

    public static int getResponse(WFArrayOp op)
    {
        return op.resp;
    }

    public boolean hasMember(int key)
    {
        FSet set = head;

        // must be aware of the linearized operation if exists
        WFArrayOp op = set.op;
        if (op != null && op.key == key) {
            // note that op cannot be a freeze node
            return op.type == WFArrayOp.INSERT;
        }

        return arrayContains(set.arr, key);
    }

    public WFArrayFSet split(int size, int remainder)
    {
        int [] o = head.arr;

        int count = 0;
        for (int i = 0; i < o.length; i++)
            if (o[i] % size == remainder)
                count++;

        int [] n = new int[count];
        int j = 0;
        for (int i = 0; i < o.length; i++) {
            if (o[i] % size == remainder)
                n[j++] = o[i];
        }

        return new WFArrayFSet(n);
    }

    public WFArrayFSet merge(WFArrayFSet t2)
    {
        int [] p = head.arr;
        int [] q = t2.head.arr;

        int [] n = new int[p.length + q.length];
        int j = 0;
        for (int i = 0; i < p.length; i++)
            n[j++] = p[i];
        for (int i = 0; i < q.length; i++)
            n[j++] = q[i];

        return new WFArrayFSet(n);
    }

    public void print()
    {
        if (head.immutable()) {
            System.out.print("(F) ");
        }

        for (int i : head.arr)
            System.out.print(Integer.toString(i) + " ");
        System.out.println();
    }

    private static boolean arrayContains(int [] o, int key)
    {
        for (int i = 0; i < o.length; i++) {
            if (o[i] == key)
                return true;
        }
        return false;
    }

    private static int [] arrayInsert(int [] o, int key)
    {
        if (arrayContains(o, key))
            return o;
        int [] n = new int[o.length + 1];
        for (int i = 0; i < o.length; i++)
            n[i] = o[i];
        n[n.length - 1] = key;
        return n;
    }

    private static int [] arrayRemove(int [] o, int key)
    {
        if (!arrayContains(o, key))
            return o;
        int [] n = new int[o.length - 1];
        int j = 0;
        for (int i = 0; i < o.length; i++) {
            if (o[i] != key)
                n[j++] = o[i];
        }
        return n;
    }
}
