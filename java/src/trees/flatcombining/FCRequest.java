package trees.flatcombining;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Created by vaksenov on 16.01.2017.
 */
public abstract class FCRequest {
    FCRequest next;

    int timestamp = Integer.MAX_VALUE;

    public abstract boolean holdsRequest();

    public boolean isRemoved() {
        return next == null;
    }
}
