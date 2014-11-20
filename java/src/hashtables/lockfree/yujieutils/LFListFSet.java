package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

class LFListFSet
{
    // field updater for head
    private static AtomicReferenceFieldUpdater<LFListFSet, LFListNode> headUpdater
        = AtomicReferenceFieldUpdater.newUpdater(LFListFSet.class, LFListNode.class, "head");

    private volatile LFListNode head;

    private boolean casHead(LFListNode o, LFListNode n)
    {
        return headUpdater.compareAndSet(this, o, n);
    }

    // default constructor
    public LFListFSet()
    {
        head = new LFListNode(-1, LFListNode.REMOVE);
    }

    // copy constructor
    public LFListFSet(LFListNode h)
    {
        head = h;
    }

    public boolean invoke(LFListNode h)
    {
        LFListNode b = head;
        while (!b.immutable()) {
            h.next = b;
            if (casHead(b, h))
                return true;
            b = head;
        }
        return false;
    }

    public boolean hasMember(int key)
    {
        LFListNode curr = head;
        while (curr != null) {
            if (curr.key == key) {
                int s = curr.state;
                if (s != LFListNode.DEAD)
                    return (s != LFListNode.REMOVE);
            }
            curr = curr.next;
        }
        return false;
    }

    public void freeze()
    {
        LFListNode h = new LFListNode(-1, LFListNode.FREEZE);
        LFListNode b = head;
        while (!b.immutable()) {
            h.next = b;
            if (casHead(b, h))
                return;
            b = head;
        }
    }

    public static int getResponse(LFListNode op, int type)
    {
        return (type == LFListNode.INSERT) ? listInsert(op) : listRemove(op);
    }

    public LFListFSet split(int size, int remainder)
    {
        // requires this fset is immutable

        ArrayList<Integer> V        = new ArrayList<Integer>();
        LFListNode         copy     = new LFListNode(-1, LFListNode.REMOVE);
        LFListNode         curr     = head.next;

        while (curr != null) {
            int s = curr.state;
            if (s != LFListNode.DEAD && (curr.key % size == remainder) && !V.contains(curr.key)) {
                if (s != LFListNode.REMOVE) {
                    LFListNode n = new LFListNode(curr.key, LFListNode.DATA);
                    n.next = copy;
                    copy = n;
                }
                V.add(curr.key);
            }
            curr = curr.next;
        }
        return new LFListFSet(copy);
    }

    public LFListFSet merge(LFListFSet t2)
    {
        // requires this and t2 are immutable

        LFListNode copy = new LFListNode(-1, LFListNode.REMOVE);

        for (int p = 0; p < 2; p++) {

            ArrayList<Integer> V = new ArrayList<Integer>();

            LFListNode curr =
                (p == 0)
                ? head.next
                : t2.head.next;

            while (curr != null) {
                int s = curr.state;
                if (s != LFListNode.DEAD && !V.contains(curr.key)) {
                    if (s != LFListNode.REMOVE) {
                        LFListNode n = new LFListNode(curr.key, LFListNode.DATA);
                        n.next = copy;
                        copy = n;
                    }
                    V.add(curr.key);
                }
                curr = curr.next;
            }

        }
        return new LFListFSet(copy);
    }

    public void print()
    {
        LFListNode curr = head;
        while (curr != null)
        {
            if (curr.state == LFListNode.FREEZE) {
                System.out.print("(F) ");
            }
            else if (curr.key == -1) {
                System.out.print("$");
            }
            else if (curr.state != LFListNode.DEAD) {
                System.out.print(curr.key);
                if (curr.state != LFListNode.DATA) {
                    System.out.print("(");
                    System.out.print(curr.state);
                    System.out.print(")");
                }
                System.out.print(" ");
            }

            curr = curr.next;
        }
        System.out.println();
    }



    // finish insertion from home node h
    private static int listInsert(LFListNode h)
    {
        int b = helpInsert(h, h.key);
        int s = (b > 0) ? LFListNode.DATA : LFListNode.DEAD;
        if (!h.casState(LFListNode.INSERT, s)) {
            helpRemove(h, h.key);
            h.state = LFListNode.DEAD;
        }
        return b;
    }

    // finish removal from home node h
    private static int listRemove(LFListNode h)
    {
        int b = helpRemove(h, h.key);
        h.state = LFListNode.DEAD;
        return b;
    }

    // The core insert protocol.
    private static int helpInsert(LFListNode home, int key)
    {
        LFListNode pred = home;
        LFListNode curr = pred.next;
        int x = 1;
        while (curr != null) {
            int s = curr.state;
            if (s == LFListNode.DEAD) {
                LFListNode succ = curr.next;
                pred.next = succ;
                curr = succ;
            }
            else if (curr.key != key) {
                pred = curr;
                curr = curr.next;
            }
            else {
                return (s == LFListNode.REMOVE) ? x : -x;
            }
            x++;
        }
        return x;  // true
    }

    // The core remove protocol.
    private static int helpRemove(LFListNode home, int key)
    {
        LFListNode pred = home;
        LFListNode curr = pred.next;
        int x = 1;
        while (curr != null) {
            int s = curr.state;
            if (s == LFListNode.DEAD) {
                LFListNode succ = curr.next;
                pred.next = succ;
                curr = succ;
            }
            else if (curr.key != key) {
                pred = curr;
                curr = curr.next;
            }
            else if (s == LFListNode.DATA) {
                curr.state = LFListNode.DEAD;
                return x; // true
            }
            else if (s == LFListNode.REMOVE) {
                return -x;  // false
            }
            else /* if (s == LFListNode.INSERT) */ {
                if (curr.casState(LFListNode.INSERT, LFListNode.REMOVE))
                    return x; // true
            }
            x++;
        }
        return -x;  // false
    }
}
