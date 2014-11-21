package hashtables.lockfree;

import java.util.concurrent.atomic.*;
import java.util.*;

import contention.abstractions.CompositionalIntSet;
import hashtables.lockfree.yujieutils.ISet;
import hashtables.lockfree.yujieutils.LFArrayFSet;

public class LFArrayHashSet implements CompositionalIntSet,ISet
{
	public static class HNode
    {
        // points to old HNode
        public HNode old;

        // bucket array
        public AtomicReferenceArray<LFArrayFSet> buckets;

        // store the size [for convenience]
        public final int size;

        // constructor
        public HNode(HNode o, int s)
        {
            old = o;
            size = s;
            buckets = new AtomicReferenceArray<LFArrayFSet>(size);
        }
    }

    public final static int MIN_BUCKET_NUM = 1;
    public final static int MAX_BUCKET_NUM = 1 << 16;

    // points to the current hash table
    volatile HNode head;

    // field updater for head
    private static AtomicReferenceFieldUpdater<LFArrayHashSet, HNode> headUpdater
        = AtomicReferenceFieldUpdater.newUpdater(LFArrayHashSet.class, HNode.class, "head");

    public LFArrayHashSet()
    {
        head = new HNode(null, MIN_BUCKET_NUM);
        head.buckets.set(0, new LFArrayFSet());
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


	@Override
	public boolean containsInt(int key) {
        HNode t = head;
        LFArrayFSet b = t.buckets.get(key % t.size);
        // if the b is empty, use old table
        if (b == null) {
            HNode s = t.old;
            b = (s == null)
                ? t.buckets.get(key % t.size)
                : s.buckets.get(key % s.size);
        }
        return b.hasMember(key);
	}

    public boolean contains(int key)
    {
        HNode t = head;
        LFArrayFSet b = t.buckets.get(key % t.size);
        // if the b is empty, use old table
        if (b == null) {
            HNode s = t.old;
            b = (s == null)
                ? t.buckets.get(key % t.size)
                : s.buckets.get(key % s.size);
        }
        return b.hasMember(key);
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
                    curr.buckets.get(i).print();
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
            LFArrayFSet b = t.buckets.get(i);

            // response value
            int ret = 0;

            // if the b is empty, help finish resize
            if (b == null)
                helpResize(t, i);
            // otherwise enlist at b
            else if ((ret = b.invoke(insert, key)) != 0)
                return ret;
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
        LFArrayFSet b = t.buckets.get(i);
        HNode s = t.old;
        if (b == null && s != null) {
            LFArrayFSet set = null;
            if (s.size * 2 == t.size) /* growing */ {
                LFArrayFSet p = s.buckets.get(i % s.size);
                p.freeze();
                set = p.split(t.size, i);
            }
            else /* shrinking */ {
                LFArrayFSet p = s.buckets.get(i);
                LFArrayFSet q = s.buckets.get(i + t.size);
                p.freeze();
                q.freeze();
                set = p.merge(q);
            }
            t.buckets.compareAndSet(i, null, set);

        }
    }

    private boolean casHead(HNode o, HNode n)
    {
        return headUpdater.compareAndSet(this, o, n);
    }

	@Override
	public void fill(int range, long size) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public boolean addInt(int x) {
		// TODO Auto-generated method stub
		return insert(x, 0);
	}

	@Override
	public boolean removeInt(int x) {
		return remove(x, 0);
	}

	@Override
	public Object getInt(int x) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean addAll(Collection<Integer> c) {
		System.err.println("Method addAll not supported!");
		return false;
	}

	@Override
	public boolean removeAll(Collection<Integer> c) {
		System.err.println("Method removeAll not supported!");
		return false;
	}

	@Override
	public int size() {
		System.err.println("Method size not supported!");
		return 0;
	}

	@Override
	public void clear() {
		System.err.println("Method clear not supported!");
	}

	@Override
	public Object putIfAbsent(int x, int y) {
		System.err.println("Method putIfAbsent not supported!");
		return null;
	}

}
