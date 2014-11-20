package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

class SOHashSet implements ISet
{
    private static class Node
    {
        protected int key;
        protected volatile Node next;

        private static final AtomicReferenceFieldUpdater<Node, Node> nextUpdater
            = AtomicReferenceFieldUpdater.newUpdater(Node.class, Node.class, "next");

        private Node(int k)
        {
            this.key = k;
        }

        private boolean casNext(Node o, Node n)
        {
            return nextUpdater.compareAndSet(this, o, n);
        }
    }

    private static class Marker extends Node
    {
        private Marker(Node n)
        {
            super(Integer.MIN_VALUE);
            this.next = n;
        }
    }

    static int [] RTABLE;

    static
    {
        RTABLE = new int[256];
        for (int i = 0; i < 256; i++)
            RTABLE[i] = reverseByte(i);
    }

    static final int SEGMENT_SIZE = 256;
    static final int NUM_OF_SEGMENTS = 256;

    // size of bucket array
    volatile int size;

    private static final AtomicIntegerFieldUpdater<SOHashSet> sizeUpdater
        = AtomicIntegerFieldUpdater.newUpdater(SOHashSet.class, "size");

    private boolean casSize(int o, int n)
    {
        return sizeUpdater.compareAndSet(this, o, n);
    }

    // sentinel nodes
    private Node head;
    private Node tail;

    // bucket array
    // public AtomicReferenceArray<Node> T;
    public AtomicReferenceArray< AtomicReferenceArray<Node> > ST;

    public SOHashSet()
    {
        head = new Node(0);
        tail = new Node(Integer.MAX_VALUE);
        head.next = tail;

        // size = SetBench.KEY_RANGE;
        // T = new AtomicReferenceArray<Node>(size);
        // T.set(0, head);

        size = 1;
        ST = new AtomicReferenceArray<AtomicReferenceArray<Node>>(NUM_OF_SEGMENTS);
        ST.set(0, new AtomicReferenceArray<Node>(SEGMENT_SIZE));
        ST.get(0).set(0, head);
    }

    public boolean insert(int key, int tid)
    {
        Node node = new Node(makeRegularKey(key));
        int bucket = key % size;
        Node h = getBucket(bucket);
        if (h == null)
            h = initBucket(bucket);
        int k = listInsert(h, node);
        if (k > 2) {
            int csize = size;
            casSize(csize, csize * 2);
        }
        return k > 0;
    }

    public boolean remove(int key, int tid)
    {
        int bucket = key % size;
        Node h = getBucket(bucket);
        if (h == null)
            h = initBucket(bucket);
        return listRemove(h, makeRegularKey(key));
    }

    public boolean contains(int key)
    {
        int bucket = key % size;
        Node h = getBucket(bucket);
        if (h == null)
            h = initBucket(bucket);
        return listContains(h, makeRegularKey(key));
    }

    public boolean simpleInsert(int key, int tid)
    {
        Node node = new Node(makeRegularKey(key));
        int bucket = key % size;
        Node h = getBucket(bucket);
        if (h == null)
            h = initBucket(bucket);
        int k = listInsert(h, node);
        return k > 0;
    }

    public boolean simpleRemove(int key, int tid)
    {
        int bucket = key % size;
        Node h = getBucket(bucket);
        if (h == null)
            h = initBucket(bucket);
        return listRemove(h, makeRegularKey(key));
    }

    public boolean grow()
    {
        return false;
    }

    public boolean shrink()
    {
        return false;
    }

    public int getBucketSize()
    {
        return size;
    }

    public void print()
    {
    }


    private Node initBucket(int bucket)
    {
        int parent = getParent(bucket);
        if (getBucket(parent) == null)
            initBucket(parent);
        Node dummy = new Node(makeDummyKey(bucket));
        dummy = listInsert2(getBucket(parent), dummy);
        setBucket(bucket, dummy);
        return dummy;
    }

    private int getParent(int bucket)
    {
        int parent = size;
        do {
            parent >>= 1;
        } while (parent > bucket);
        return bucket - parent;
    }

    public static int makeRegularKey(int key)
    {
        // take the lower 3 bytes
        return reverse(key | 0x00800000);
    }

    public static int makeDummyKey(int key)
    {
        return reverse(key);
    }

    public static int reverse_slow(int num)
    {
        // reverse the lower 3 bytes
        int inverse = 0;
        for (int i = 0; i < 24; ++i) {
            inverse = inverse << 1;
            if ((num & 0x01) != 0)
                inverse |= 0x01;
            num = num >> 1;
        }
        return inverse;
    }

    public static int reverse(int num)
    {
        return (RTABLE[(num & 0x00FF0000) >> 16]      ) |
               (RTABLE[(num & 0x0000FF00) >> 8 ] << 8 ) |
               (RTABLE[(num & 0x000000FF)      ] << 16);
    }

    private static int reverseByte(int num)
    {
        int inverse = 0;
        for (int i = 0; i < 8; ++i) {
            inverse = inverse << 1;
            if ((num & 0x01) != 0)
                inverse |= 0x01;
            num = num >> 1;
        }
        return inverse;
    }

    // private Node getBucket(int bucket)
    // {
    //     return T.get(bucket);
    // }

    // private void setBucket(int bucket, Node node)
    // {
    //     T.set(bucket, node);
    // }

    private Node getBucket(int bucket)
    {
        int segment = bucket / SEGMENT_SIZE;
        int index = bucket % SEGMENT_SIZE;
        AtomicReferenceArray<Node> s = ST.get(segment);
        if (s == null)
            return null;
        return ST.get(segment).get(index);
    }

    private void setBucket(int bucket, Node node)
    {
        int segment = bucket / SEGMENT_SIZE;
        int index = bucket % SEGMENT_SIZE;
        AtomicReferenceArray<Node> s = ST.get(segment);

        if (s == null)
        {
            ST.compareAndSet(segment, null,
                             new AtomicReferenceArray<Node>(SEGMENT_SIZE));
            s = ST.get(segment);
        }
        s.set(index, node);
    }

    private int listInsert(Node h, Node node)
    {
        Node pred = null, curr = null, succ = null;
        int x = 1;
      retry:
        // purpose of outermost while loop is for implementing goto only..
        while (true){
            // initialization
            pred = h;
            curr = pred.next;
            // traverse linked list
            while (true) {
                succ = curr.next;
                while (succ instanceof Marker) {
                    succ = succ.next;
                    // snip curr and marker
                    if (!pred.casNext(curr, succ))
                        continue retry;
                    curr = succ;
                    succ = succ.next;
                }
                // continue searching
                if (curr.key < node.key) {
                    pred = curr;
                    curr = succ;
                }
                // key exists
                else if (curr.key == node.key)
                    return -x;
                // locate a window: do insert
                else {
                    node.next = curr;
                    if (pred.casNext(curr, node))
                        return x;
                    else {
                        x = 1;
                        continue retry;
                    }
                }
                x++;
            }
        }
    }

    private Node listInsert2(Node h, Node node)
    {
        Node pred = null, curr = null, succ = null;
      retry:
        // purpose of outermost while loop is for implementing goto only..
        while (true){
            // initialization
            pred = h;
            curr = pred.next;
            // traverse linked list
            while (true) {
                succ = curr.next;
                while (succ instanceof Marker) {
                    succ = succ.next;
                    // snip curr and marker
                    if (!pred.casNext(curr, succ))
                        continue retry;
                    curr = succ;
                    succ = succ.next;
                }
                // continue searching
                if (curr.key < node.key) {
                    pred = curr;
                    curr = succ;
                }
                // key exists
                else if (curr.key == node.key)
                    return curr;
                // locate a window: do insert
                else {
                    node.next = curr;
                    if (pred.casNext(curr, node))
                        return node;
                    else
                        continue retry;
                }
            }
        }
    }

    private boolean listRemove(Node h, int key)
    {
        Node pred = null, curr = null, succ = null;
      retry:
        // purpose of outermost while loop is for implementing goto only..
        while (true){
            // initialization
            pred = h;
            curr = pred.next;
            // traverse linked list
            while (true) {
                succ = curr.next;
                while (succ instanceof Marker) {
                    succ = succ.next;
                    if (!pred.casNext(curr, succ))
                        continue retry;
                    curr = succ;
                    succ = succ.next;
                }
                // continue searching
                if (curr.key < key) {
                    pred = curr;
                    curr = succ;
                }
                // key found: do remove
                else if (curr.key == key) {
                    if (!curr.casNext(succ, new Marker(succ))) {
                        continue retry;
                    }
                    pred.casNext(curr, succ);
                    return true;
                }
                // key not found
                else if (curr.key > key) {
                    return false;
                }
            }
        }
    }

    private boolean listContains(Node h, int key)
    {
        Node curr = h;
        while (curr.key < key) {
            curr = curr.next;
        }
        return (curr.key == key && (!(curr.next instanceof Marker)));
    }
}
