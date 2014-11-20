package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

class LFArrayHashSetOpt implements ISet
{
    static class HNode
    {
        // points to old HNode
        public HNode old;

        // bucket array
        public AtomicReferenceArray<Object> buckets;

        // store the size [for convenience]
        public final int size;

        // constructor
        public HNode(HNode o, int s)
        {
            old = o;
            size = s;
            buckets = new AtomicReferenceArray<Object>(size);
        }

        private boolean casBucket(int i, Object o, Object n)
        {
            return buckets.compareAndSet(i, o, n);
        }

        public void printBucket(int i)
        {
            Object h = buckets.get(i);
            int [] arr = null;

            if (h instanceof FreezeMarker) {
                System.out.print("(F) ");
                arr = ((FreezeMarker)h).arr;
            }
            else {
                arr = (int [])h;
            }

            for (int k : arr)
                System.out.print(Integer.toString(k) + " ");
            System.out.println();
        }
    }

    class FreezeMarker
    {
        public int [] arr;
        public FreezeMarker(int [] a) { arr = a; }
    }


    public final static int MIN_BUCKET_NUM = 1;
    public final static int MAX_BUCKET_NUM = 1 << 16;

    // points to the current hash table
    volatile HNode head;

    // field updater for head
    private static AtomicReferenceFieldUpdater<LFArrayHashSetOpt, HNode> headUpdater
        = AtomicReferenceFieldUpdater.newUpdater(LFArrayHashSetOpt.class, HNode.class, "head");

    public LFArrayHashSetOpt()
    {
        head = new HNode(null, MIN_BUCKET_NUM);
        head.buckets.set(0, new int[0]);
    }

    public boolean insert(int key, int tid)
    {
        HNode h = head;
        int result = apply(true, key);
        if (Math.abs(result) > 2)
            resize(h, true);
        return result > 0;
    }

    public boolean remove(int key, int tid)
    {
        int result = apply(false, key);
        return result > 0;
    }

    public boolean contains(int key)
    {
        HNode t = head;
        Object b = t.buckets.get(key % t.size);
        // if the b is empty, use old table
        if (b == null) {
            HNode s = t.old;
            b = (s == null)
                ? t.buckets.get(key % t.size)
                : s.buckets.get(key % s.size);
        }
        return hasMember(b, key);
    }

    public boolean simpleInsert(int key, int tid)
    {
        return apply(true, key) > 0;
    }

    public boolean simpleRemove(int key, int tid)
    {
        return apply(false, key) > 0;
    }

    public boolean grow()
    {
        HNode h = head;
        return resize(h, true);
    }

    public boolean shrink()
    {
        HNode h = head;
        return resize(h, false);
    }

    public int getBucketSize()
    {
        return head.size;
    }

    public void print()
    {
        HNode curr = head;
        int age = 0;
        while (curr != null) {
            System.out.println("HashTableNode #" + Integer.toString(age++));
            for (int i = 0; i < curr.size; i++) {
                System.out.print("  Bucket " + Integer.toString(i) + ": ");
                if (curr.buckets.get(i) != null)
                    curr.printBucket(i);
                else
                    System.out.println();
            }
            curr = curr.old;
            System.out.println();
        }
    }

    private int apply(boolean insert, int key)
    {
        while (true) {
            HNode       t = head;
            int         i = key % t.size;
            Object      b = t.buckets.get(i);

            // response value
            int ret = 0;

            // if the b is empty, help finish resize
            if (b == null)
                helpResize(t, i);
            // otherwise enlist at b
            else {
                while (b instanceof int []) {
                    int [] o = (int [])b;
                    int [] n = insert ? arrayInsert(o, key) : arrayRemove(o, key);
                    if (n == o)
                        return -(n.length + 1);
                    else if (t.casBucket(i, b, n))
                        return n.length + 1;
                    b = t.buckets.get(i);
                }
            }
        }
    }

    private boolean resize(HNode t, boolean grow)
    {
        if ((t.size == MAX_BUCKET_NUM && grow) ||
            (t.size == MIN_BUCKET_NUM && !grow))
            return false;

        if (t == head) {
            // make sure we can deprecate t's predecessor
            for (int i = 0; i < t.size; i++) {
                if (t.buckets.get(i) == null)
                    helpResize(t, i);
            }
            // deprecate t's predecessor
            t.old = null;

            // switch to a new bucket array
            if (t == head) {
                HNode n = new HNode(t, grow ? t.size * 2 : t.size / 2);
                return casHead(t, n);
            }
        }
        return false;
    }

    private void helpResize(HNode t, int i)
    {
        Object b = t.buckets.get(i);
        HNode  s = t.old;
        if (b == null && s != null) {
            Object set = null;
            if (s.size * 2 == t.size) /* growing */ {
                int [] p = freezeBucket(s, i % s.size);
                set = split(p, t.size, i);
            }
            else /* shrinking */ {
                int [] p = freezeBucket(s, i);
                int [] q = freezeBucket(s, i + t.size);
                set = merge(p, q);
            }
            t.buckets.compareAndSet(i, null, set);
        }
    }

    private boolean casHead(HNode o, HNode n)
    {
        return headUpdater.compareAndSet(this, o, n);
    }

    public boolean hasMember(Object h, int key)
    {
        int [] arr = (h instanceof int [])
            ? (int [])h
            : ((FreezeMarker)h).arr;
        return arrayContains(arr, key);
    }

    public int [] freezeBucket(HNode t, int i)
    {
        while (true) {
            Object h = t.buckets.get(i);
            if (h instanceof FreezeMarker)
                return ((FreezeMarker)h).arr;
            if (t.casBucket(i, h, new FreezeMarker((int [])h)))
                return (int [])h;
        }
    }

    public int [] split(int [] o, int size, int remainder)
    {
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
        return n;
    }

    public int [] merge(int [] p, int [] q)
    {
        int [] n = new int[p.length + q.length];
        int j = 0;
        for (int i = 0; i < p.length; i++)
            n[j++] = p[i];
        for (int i = 0; i < q.length; i++)
            n[j++] = q[i];
        return n;
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
