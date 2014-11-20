package hashtables.lockfree.yujieutils;

import java.util.concurrent.atomic.*;
import java.util.*;

class WFArrayOp
{
    /* States of Nodes. */
    public static final int INSERT  = 2;
    public static final int REMOVE  = 3;
    public static final int FREEZE  = 4;

    public int key;
    public int type;
    public volatile int resp;
    public volatile long priority;

    public WFArrayOp(int k, int t)
    {
        this.key = k;
        this.type = t;
    }
}
