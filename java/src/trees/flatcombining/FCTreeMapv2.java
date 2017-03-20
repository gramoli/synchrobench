package trees.flatcombining;

import contention.abstractions.CompositionalMap;
import contention.abstractions.MaintenanceAlg;
import trees.flatcombining.sequential.JoinableTreeMap;

import java.util.AbstractMap;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Set;

import static trees.flatcombining.FCTreeMapv2.OperationType.DELETE;
import static trees.flatcombining.FCTreeMapv2.OperationType.INSERT;
import static trees.flatcombining.FCTreeMapv2.Status.*;

/**
 * Created by vaksenov on 16.01.2017.
 */
@SuppressWarnings("ALL")
public class FCTreeMapv2<K, V> extends AbstractMap<K, V>
        implements CompositionalMap<K, V>, MaintenanceAlg {
    protected JoinableTreeMap<K, V> tree;
    private Comparator<? super K> comparator;

    private Comparable<? super K> comparable(final Object key) {
        if (key == null) {
            throw new NullPointerException();
        }
        if (comparator == null) {
            return (Comparable<? super K>) key;
        }
        return new Comparable<K>() {
            final Comparator<? super K> _cmp = comparator;

            @SuppressWarnings("unchecked")
            public int compareTo(final K rhs) {
                return _cmp.compare((K) key, rhs);
            }
        };
    }

    private int TRIES;
    private int THRESHOLD;
    private FC fc = new FC();
    private ThreadLocal<Request> allocatedRequests = new ThreadLocal<>();
    private volatile boolean leaderExists = false;
    private volatile boolean leaderInTransition = false;

    public enum OperationType {
        INSERT,
        DELETE,
        CONTAINS
    }

    public enum Status {
        PUSHED,
        ON_WORK,
        FINISHED
    }

    public class Request extends FCRequest implements Comparable<Request> {
        OperationType type;
        K key;
        Comparable<? super K> kk;
        V value;

        volatile V result;

        volatile boolean leader; // is this request is hold by the leader thread

        Thread owner;

        public Request() {
            status = Status.FINISHED;
        }

        public void set(OperationType operationType, K key, V value) {
            this.type = operationType;
            this.key = key;
            kk = comparable(key);
            this.value = value;
            result = null;
            searchWait = null;
            parentWait = null;
            leftWait = null;
            rightWait = null;
        }

        public int compareTo(Request operation) {
            int comparison = kk.compareTo(operation.key);
            return comparison == 0 ? type.compareTo(operation.type) : comparison;
        }

        volatile Status status;

        Request searchWait;
        volatile Request parentWait;
        volatile Request leftWait;
        volatile Request rightWait;

        volatile JoinableTreeMap<K, V>.Node treeToWork;

        public boolean holdsRequest() {
            return status != Status.FINISHED;
        }

        public String toString() {
            return status + " {" + leftWait + ", " + rightWait + "}";
        }
    }

    public Request getLocalRequest() {
        Request request = allocatedRequests.get();
        if (request == null) {
            request = new Request();
            request.owner = Thread.currentThread();
            allocatedRequests.set(request);
        }
        return request;
    }

    public FCTreeMapv2() {
        TRIES = 3;//Thread.activeCount(); // 3;// 64; // TODO: number of proc
        THRESHOLD = (int) Math.ceil(TRIES / 1.7);
    }

    public FCTreeMapv2(Comparator<? super K> comparator) {
        super();
        this.comparator = comparator;
    }

    public boolean DEBUG_ON = false;

    public void performUpdate(Request request) {
        Request parent = request.parentWait;
        JoinableTreeMap<K, V>.Result result = tree.split(request.treeToWork, request.kk);

        Request left = request.leftWait;
        if (left != null) {
            left.treeToWork = result.l;
        }
        Request right = request.rightWait;
        if (right != null) {
            right.treeToWork = result.r;
        }

        if (right != null) {
            Thread threadRight = new Thread(() -> {
                performUpdate(right);
            });
            threadRight.start();
            performUpdate(left); // left should not be null by any case
            try {
                threadRight.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        } else {
            if (left != null) {
                performUpdate(left);
            }
        }

        JoinableTreeMap.Node lNode = left == null ? result.l : left.treeToWork;
        if (left != null) {
            left.status = FINISHED; // Wake up the main thread for left request
        }
        JoinableTreeMap.Node rNode = right == null ? result.r : right.treeToWork;
        if (right != null) {
            right.status = FINISHED; // Wake up the main thread for right request
        }

        if (request.type == OperationType.INSERT) {
            request.treeToWork = tree.join(
                    lNode,
                    tree.createNode(request.key, request.value),
                    rNode
            );
        } else {
            request.treeToWork = tree.join2(lNode, rNode);
        }
    }

    public void handleRequest(Request request) {
        request.status = PUSHED;
        fc.addRequest(request);

        while (request.leader || request.holdsRequest()) {
            if (!leaderExists) {
                if (fc.tryLock()) {
                    leaderExists = true;
                    request.leader = true;
                }
            }
            if (leaderExists && request.leader && !leaderInTransition) { // If there is no leader
//                System.err.println("Took " + Thread.currentThread().getId());
                fc.tryLock();
//                System.err.println(it++);
                fc.addRequest(request); // Retry adding. It is probably removed.
                leaderExists = true;
                request.leader = true;

                // I'm the leader now and now perform operations TRIES times
                for (int t = 0; t < TRIES && request.leader; t++) {
                    long start = System.currentTimeMillis();
                    final FCRequest[] requests = fc.loadRequests();

                    Arrays.sort(requests);
                    if (requests.length == 0) {
                        break;
                    }

                    Thread[] threads = new Thread[requests.length];
                    for (int i = 0; i < requests.length; i++) {
                        ((Request) requests[i]).status = ON_WORK;
                        if (i == 0 || ((Request) requests[i]).kk.compareTo(((Request) requests[i - 1]).key) != 0) {
                            final int id = i;
                            threads[i] = new Thread(() -> {
                                ((Request) requests[id]).result = tree.get(request.key);
                            });
                            threads[i].start();
                        }
                    }

                    // TODO: contains operations do not need this second synchronization
                    V firstInsert = null;
                    V firstDelete = null;
                    V forInsert = null;
                    V forDelete = null;
                    boolean isFirstInsert = false;
                    boolean isFirstDelete = false;
                    OperationType toOperate = INSERT;
                    int l = 0;
                    for (int i = 0; i < requests.length; i++) {
                        Request ri = (Request) requests[i];
                        if (threads[i] != null) {
                            try {
                                threads[i].join();
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
                            // The operation that made a search between the same values
                            isFirstInsert = true;
                            isFirstDelete = true;
                            if (ri.result == null) { // there is no such key
                                firstInsert = null;
                                forInsert = ri.value;
                                firstDelete = forDelete = null;
                                toOperate = INSERT;
                            } else { // there is such key
                                firstInsert = forInsert = ri.result;
                                firstDelete = ri.result;
                                forDelete = null;
                                toOperate = DELETE;
                            }
                        }
                        switch (ri.type) {
                            case CONTAINS:
                                ri.result = firstInsert;
                                ri.status = FINISHED;
                                break;
                            case INSERT:
                                if (isFirstInsert) {
                                    ri.result = firstInsert;
                                    isFirstInsert = false;
                                    if (toOperate == INSERT) {
                                        requests[l++] = requests[i];
                                    } else {
                                        ri.status = FINISHED;
                                    }
                                } else {
                                    ri.result = forInsert;
                                    ri.status = FINISHED;
                                }
                                break;
                            case DELETE:
                                if (isFirstDelete) {
                                    ((Request) requests[i]).result = firstDelete;
                                    isFirstDelete = false;
                                    if (toOperate == DELETE) {
                                        requests[l++] = requests[i];
                                    } else {
                                        ri.status = FINISHED;
                                    }
                                } else {
                                    ri.result = forDelete;
                                    ri.status = FINISHED;
                                }
                        }
                    }

                    // Now requests contains only requests to perform. We form a tree out of them.
                    Request root = null;
                    for (int i = 0; i < l; i++) {
                        int r = Integer.lowestOneBit(i + 1);
                        if (r == i + 1 && 2 * r > l) {
                            root = (Request) requests[i];
                            root.treeToWork = tree.root;
                        }
                        if (r == 1) {
                            ((Request) requests[i]).leftWait = null;
                            ((Request) requests[i]).rightWait = null;
                        } else {
                            int left = i - r / 2;
                            ((Request) requests[i]).leftWait = (Request) requests[left];
                            ((Request) requests[left]).parentWait = (Request) requests[i];
                            int right = i + r / 2;
                            if (right < l) {
                                ((Request) requests[i]).rightWait = (Request) requests[right];
                                ((Request) requests[right]).parentWait = (Request) requests[i];
                            } else {
                                ((Request) requests[i]).rightWait = null;
                            }
                        }
                    }

                    if (l > 0 && request != root) { // Make the root thread to be the leader
                        request.leader = false;
                        leaderInTransition = true;
                        root.leader = true;
                    }

                    if (l > 0) {
                        final Request froot = root;
                        final int fl = l;
                        Thread last = new Thread(() -> {
                            performUpdate(froot);

                            // TODO: force the root to finish on his own and set a new tree
                            if (fl > 0) {
                                froot.status = FINISHED;
                                tree.root = froot.treeToWork;
                            }
                        });
                        last.start();

                        while (request.status != FINISHED) {
                        }

                        fc.cleanup();
                        if (!request.leader) {
                            leaderInTransition = false;
                            fc.unlock();
                            return;
                        }
                    } else {
                        break;
                    }
//                    if (requests.length < THRESHOLD) {
//                        break;
//                    }
                }
                request.leader = false; // We have not given a lock to anybody
                leaderExists = false;
                fc.unlock();
            } else {
                // I'm not a leader and should wait for synchronization (or check the lock)
                switch (request.status) {
                    case PUSHED: // Nobody has started to work with me
                        fc.addRequest(request); // Probably, I'm out of queue?
                        continue; // It is better to restart and try to become a leader
                    default:
                        if (!request.leader) {
                            while (request.status != FINISHED) {
                            }
                        }
                }
            }
        }
    }

    @Override
    public V putIfAbsent(K key, V value) {
        Request request = getLocalRequest();
        request.set(OperationType.INSERT, key, value);
        handleRequest(request);
        return request.result;
    }

    @Override
    public V remove(Object key) {
        Request request = getLocalRequest();
        request.set(OperationType.DELETE, (K) key, null);
        handleRequest(request);
        return request.result;
    }

    @Override
    public V get(Object key) {
        Request request = getLocalRequest();
        request.set(OperationType.CONTAINS, (K) key, null);
        handleRequest(request);
        return request.result;
    }

    @Override
    public boolean containsKey(Object key) {
        return get(key) != null;
    }

    @Override
    public Set<Entry<K, V>> entrySet() {
        throw new AssertionError("EntrySet is not supported");
    }

    public boolean stopMaintenance() {
        System.err.println("Depth: " + tree.depth());
        return true;
    }

    @Override
    public int size() {
        return tree.size();
    }

    @Override
    public int numNodes() {
        return size();
    }

    @Override
    public long getStructMods() {
        return 0;
    }

    @Override
    public void clear() {
        fc = new FC();
        allocatedRequests = new ThreadLocal<>();
        tree.clear();
        leaderExists = false;
        leaderInTransition = false;
    }
}
