package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

public class LFArrayFSet
{
    // field updater for head
    private static AtomicReferenceFieldUpdater<LFArrayFSet, Object> headUpdater
        = AtomicReferenceFieldUpdater.newUpdater(LFArrayFSet.class, Object.class, "head");

    private volatile Object head;

    static class FreezeMarker
    {
        public int [] arr;

        public FreezeMarker(int [] a)
        {
            arr = a;
        }
    }

    private boolean casHead(Object o, Object n)
    {
        return headUpdater.compareAndSet(this, o, n);
    }

    // default constructor
    public LFArrayFSet()
    {
        head = new int[0];
    }

    // copy constructor
    public LFArrayFSet(int [] arr)
    {
        head = arr;
    }

    public int invoke(boolean insert, int key)
    {
        Object h = head;
        while (h instanceof int []) {
            int [] o = (int [])h;
            int [] n = insert ? arrayInsert(o, key) : arrayRemove(o, key);
            if (n == o)
                return -(n.length + 1);
            else if (casHead(h, n))
                return n.length + 1;
            h = head;
        }
        return 0;
    }

    public boolean hasMember(int key)
    {
        Object h = head;
        int [] arr = (h instanceof int [])
            ? (int [])h
            : ((FreezeMarker)h).arr;
        return arrayContains(arr, key);
    }

    public void freeze()
    {
        while (true) {
            Object h = head;
            if (h instanceof FreezeMarker)
                return;
            FreezeMarker m = new FreezeMarker((int [])h);
            if (casHead(h, m))
                return;
        }
    }

    public LFArrayFSet split(int size, int remainder)
    {
        int [] o = ((FreezeMarker)head).arr;

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

        return new LFArrayFSet(n);
    }

    public LFArrayFSet merge(LFArrayFSet t2)
    {
        int [] p = ((FreezeMarker)head).arr;
        int [] q = ((FreezeMarker)t2.head).arr;

        int [] n = new int[p.length + q.length];
        int j = 0;
        for (int i = 0; i < p.length; i++)
            n[j++] = p[i];
        for (int i = 0; i < q.length; i++)
            n[j++] = q[i];

        return new LFArrayFSet(n);
    }

    public void print()
    {
        Object h = head;
        int [] arr = null;

        if (h instanceof FreezeMarker) {
            System.out.print("(F) ");
            arr = ((FreezeMarker)h).arr;
        }
        else {
            arr = (int [])h;
        }

        for (int i : arr)
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
