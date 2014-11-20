package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

class WFListFSet
{
    // field updater for head
    private static AtomicReferenceFieldUpdater<WFListFSet, WFListNode> headUpdater
        = AtomicReferenceFieldUpdater.newUpdater(WFListFSet.class, WFListNode.class, "head");

    private volatile WFListNode head;
    private volatile boolean fflag;

    private boolean casHead(WFListNode o, WFListNode n)
    {
        return headUpdater.compareAndSet(this, o, n);
    }

    // default constructor
    public WFListFSet()
    {
        head = new WFListNode(-1, WFListNode.REMOVE);
    }

    // copy constructor
    public WFListFSet(WFListNode h)
    {
        head = h;
    }

    public boolean invoke(WFListNode h)
    {
        return enlist(h);
    }

    public void freeze()
    {
        fflag = true;
        doFreeze();
    }

    private void doFreeze()
    {
        WFListNode h = new WFListNode(-1, WFListNode.FREEZE);
        WFListNode first = head;
        while (!first.immutable()) {
            WFListNode prev = first.prev;
            if (first == head) {
                if (prev == null) {
                    if (first.casPrev(null, h)) {
                        helpFinish(first, h);
                        return;
                    }
                }
                else {
                    helpFinish(first, prev);
                }
            }
            first = head;
        }
    }


    private boolean enlist(WFListNode h)
    {
        WFListNode first = head;
        while (!first.immutable() && h.priority != Long.MAX_VALUE) {
            if (fflag) {
                doFreeze();
                break;
            }
            WFListNode prev = first.prev;
            if (first == head) {
                if (prev == null) {
                    if (h.priority != Long.MAX_VALUE) {
                        if (first.casPrev(null, h)) {
                            helpFinish(first, h);
                            return true;
                        }
                    }
                }
                else {
                    helpFinish(first, prev);
                }
            }
            // re-read head
            first = head;
        }
        return h.priority == Long.MAX_VALUE;
    }

    private void helpFinish()
    {
        WFListNode first = head;
        WFListNode prev = first.prev;
        helpFinish(first, prev);
    }

    private void helpFinish(WFListNode first, WFListNode prev)
    {
        // requires prev != null and prev != DUMMY
        if (prev != null && prev != WFListNode.DUMMY) {
            if (first == head) {
                prev.next = first;
                prev.priority = Long.MAX_VALUE;
                casHead(first, prev);
                first.prev = WFListNode.DUMMY; // allow garbage collector to reclaim prev
            }
        }
    }

    public boolean hasMember(int key)
    {
        WFListNode curr = head;

        // must be aware of the linearized operation if there is one
        WFListNode pred = curr.prev;
        if (pred != null && pred.key == key) {
            return pred.type == WFListNode.INSERT;
        }

        while (curr != null) {
            if (curr.key == key) {
                int s = curr.state;
                if (s != WFListNode.DEAD)
                    return (s != WFListNode.REMOVE);
            }
            curr = curr.next;
        }
        return false;
    }

    public static int getResponse(WFListNode op)
    {
        return (op.type == WFListNode.INSERT) ? listInsert(op) : listRemove(op);
    }

    public WFListFSet split(int size, int remainder)
    {
        // requires this fset is immutable

        ArrayList<Integer> V        = new ArrayList<Integer>();
        WFListNode         copy     = new WFListNode(-1, WFListNode.REMOVE);
        WFListNode         curr     = head.next;

        while (curr != null) {
            int s = curr.state;
            if (s != WFListNode.DEAD && (curr.key % size == remainder) && !V.contains(curr.key)) {
                if (s != WFListNode.REMOVE) {
                    WFListNode n = new WFListNode(curr.key, WFListNode.DATA);
                    n.next = copy;
                    copy = n;
                }
                V.add(curr.key);
            }
            curr = curr.next;
        }
        return new WFListFSet(copy);
    }

    public WFListFSet merge(WFListFSet t2)
    {
        // requires this and t2 are immutable

        WFListNode copy = new WFListNode(-1, WFListNode.REMOVE);

        for (int p = 0; p < 2; p++) {

            ArrayList<Integer> V = new ArrayList<Integer>();

            WFListNode curr =
                (p == 0)
                ? head.next
                : t2.head.next;

            while (curr != null) {
                int s = curr.state;
                if (s != WFListNode.DEAD && !V.contains(curr.key)) {
                    if (s != WFListNode.REMOVE) {
                        WFListNode n = new WFListNode(curr.key, WFListNode.DATA);
                        n.next = copy;
                        copy = n;
                    }
                    V.add(curr.key);
                }
                curr = curr.next;
            }

        }
        return new WFListFSet(copy);
    }

    public void print()
    {
        WFListNode curr = head;
        while (curr != null)
        {
            if (curr.state == WFListNode.FREEZE) {
                System.out.print("(F) ");
            }
            else if (curr.key == -1) {
                System.out.print("$");
            }
            else if (curr.state != WFListNode.DEAD) {
                System.out.print(curr.key);
                if (curr.state != WFListNode.DATA) {
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
    private static int listInsert(WFListNode h)
    {
        int b = helpInsert(h, h.key);
        int s = (b > 0) ? WFListNode.DATA : WFListNode.DEAD;
        if (!h.casState(WFListNode.INSERT, s)) {
            helpRemove(h, h.key);
            h.state = WFListNode.DEAD;
        }
        return b;
    }

    // finish removal from home node h
    private static int listRemove(WFListNode h)
    {
        int b = helpRemove(h, h.key);
        h.state = WFListNode.DEAD;
        return b;
    }

    // The core insert protocol.
    private static int helpInsert(WFListNode home, int key)
    {
        WFListNode pred = home;
        WFListNode curr = pred.next;
        int x = 1;
        while (curr != null) {
            int s = curr.state;
            if (s == WFListNode.DEAD) {
                WFListNode succ = curr.next;
                pred.next = succ;
                curr = succ;
            }
            else if (curr.key != key) {
                pred = curr;
                curr = curr.next;
            }
            else {
                return (s == WFListNode.REMOVE) ? x : -x;
            }
            x++;
        }
        return x;  // true
    }

    // The core remove protocol.
    private static int helpRemove(WFListNode home, int key)
    {
        WFListNode pred = home;
        WFListNode curr = pred.next;
        int x = 1;
        while (curr != null) {
            int s = curr.state;
            if (s == WFListNode.DEAD) {
                WFListNode succ = curr.next;
                pred.next = succ;
                curr = succ;
            }
            else if (curr.key != key) {
                pred = curr;
                curr = curr.next;
            }
            else if (s == WFListNode.DATA) {
                curr.state = WFListNode.DEAD;
                return x; // true
            }
            else if (s == WFListNode.REMOVE) {
                return -x;  // false
            }
            else /* if (s == WFListNode.INSERT) */ {
                if (curr.casState(WFListNode.INSERT, WFListNode.REMOVE))
                    return x; // true
            }
            x++;
        }
        return -x;  // false
    }
}
