package trees.flatcombining;

import contention.abstractions.CompositionalMap;
import contention.abstractions.MaintenanceAlg;
import trees.flatcombining.sequential.JoinableTreeMap;

import java.util.*;
import java.util.concurrent.atomic.AtomicInteger;

import static trees.flatcombining.FCTreeMap.Status.*;
import static trees.flatcombining.FCTreeMap.OperationType.*;

/**
 * Created by vaksenov on 16.01.2017.
 */
@SuppressWarnings("ALL")
public class FCTreeMap<K, V> extends AbstractMap<K, V>
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
    private volatile Request prev_leader = null;

    public enum OperationType {
        INSERT,
        DELETE,
        CONTAINS
    }

    public enum Status {
        PUSHED,
        SEARCH_PHASE_WORKER,
        SEARCH_PHASE_FINISHED,
        UPDATE_PHASE_START,
        SPLIT_PHASE_FINISHED,
        JOIN_PHASE_FINISHED,
        FINISHED
    }

    public class Request extends FCRequest implements Comparable<Request> {
        OperationType type;
        K key;
        Comparable<? super K> kk;
        V value;

        volatile V result;

        volatile boolean leader; // is this request is hold by the leader thread

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
            allocatedRequests.set(request);
        }
        return request;
    }

    public void assertRequests() {
        FCRequest[] requests = fc.loadRequests();
        for (int i = 0; i < requests.length; i++) {
            if (((Request) requests[i]).status != PUSHED && ((Request) requests[i]).status != FINISHED) {
                System.err.println(((Request) requests[i]).status + " " + requests[i].hashCode());
            }
            assert ((Request) requests[i]).status == PUSHED || ((Request) requests[i]).status == FINISHED;
        }
    }

    public FCTreeMap() {
        TRIES = 3;//Thread.activeCount(); // 3;// 64; // TODO: number of proc
        THRESHOLD = (int) Math.ceil(TRIES / 1.7);
    }

    public FCTreeMap(Comparator<? super K> comparator) {
        super();
        this.comparator = comparator;
    }

    public boolean DEBUG_ON = false;

    public void performUpdate(Request request) {
        Request parent = request.parentWait;
        if (DEBUG_ON)
            System.err.println("PARENT: " + Thread.currentThread().getId() + " " + parent);
        while (parent != null && parent.status != SPLIT_PHASE_FINISHED) {
        }
        JoinableTreeMap<K, V>.Result result = tree.split(request.treeToWork, request.kk);

        Request left = request.leftWait;
        if (left != null) {
            left.treeToWork = result.l;
        }
        Request right = request.rightWait;
        if (right != null) {
            right.treeToWork = result.r;
        }
        request.status = SPLIT_PHASE_FINISHED;

        if (DEBUG_ON)
            System.err.println("Children: " + Thread.currentThread().getId() + " " + left + " " + right);
        while (left != null && left.status != JOIN_PHASE_FINISHED) {
        }

        while (right != null && right.status != JOIN_PHASE_FINISHED) {
        }

        if (DEBUG_ON)
            System.err.println("Proceed to finish: " + Thread.currentThread().getId());

        JoinableTreeMap.Node lNode = left == null ? result.l : left.treeToWork;
        if (left != null) {
            left.status = FINISHED; // I could free the left thread
//            System.err.println("Finished " + left.hashCode());
        }
        JoinableTreeMap.Node rNode = right == null ? result.r : right.treeToWork;
        if (right != null) {
            right.status = FINISHED; // I could free the right thread
//            System.err.println("Finished " + right.hashCode());
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
        request.status = JOIN_PHASE_FINISHED;
    }

    AtomicInteger leaderThreads = new AtomicInteger();

    public void handleRequest(Request request) {
        request.status = PUSHED;
        fc.addRequest(request);

        while (request.leader || request.holdsRequest()) {
            if (!leaderExists) { // If there is no leader
                if (fc.tryLock()) {
                    leaderExists = true;
                    request.leader = true;
                }
            }
            if (request.leader && !leaderInTransition &&
                    (request.status == PUSHED || request.status == FINISHED)) { // I am ready to become a leader
//                System.err.println("I'm the root " + request.hashCode());
                assert leaderExists;
//                if (leaderThreads.incrementAndGet() > 1) {
//                    System.err.println("Two leaders, really?!");
//                }
//                System.err.println("Took " + Thread.currentThread().getId());
//                fc.tryLock();
//                System.err.println(it++);
                fc.addRequest(request); // Retry adding. It is probably removed.

                // I'm the leader now and now perform operations TRIES times
                for (int t = 0; t < TRIES; t++) {
//                    long start = System.currentTimeMillis();
                    FCRequest[] requests = fc.loadRequests();
//                    System.err.println("Load requests " + (System.currentTimeMillis() - start));

//                    System.err.println(requests.length);

//                    for (int i = 0; i < requests.length; i++) {
//                        if (((Request)requests[i]).status != PUSHED){
//                            System.err.println(((Request)requests[i]).status + " " + requests[i].hashCode());
//                        }
//                        assert ((Request)requests[i]).status == PUSHED;
//                    }

                    if (requests.length == 0) {
                        continue;
                    }

                    if (request.status == FINISHED) { // Give the power to somebody already
                        request.leader = false;
                        ((Request) requests[0]).leader = true;
//                        System.err.println("Freely transit the leadership: " + request.hashCode() + " -> " + ((Request) requests[0]).hashCode());
                        return;
                    }

                    Arrays.sort(requests);

                    ((Request) requests[0]).status = SEARCH_PHASE_WORKER;
                    for (int i = 1; i < requests.length; i++) {
                        if (((Request) requests[i]).kk.compareTo(((Request) requests[i - 1]).key) == 0) {
                            ((Request) requests[i]).status = SEARCH_PHASE_FINISHED;
                        } else {
                            ((Request) requests[i]).status = SEARCH_PHASE_WORKER;
                        }
                    }

                    if (request.status == SEARCH_PHASE_WORKER) { // If I should search by myself
                        request.result = tree.get(request.key);
                        request.status = SEARCH_PHASE_FINISHED;
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
                        if (i == 0 || ri.kk.compareTo(((Request) requests[i - 1]).key) != 0) {
                            while (ri.status != SEARCH_PHASE_FINISHED) {
                            } // Wait all other threads
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

//                        assert ri.status == SEARCH_PHASE_FINISHED;
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
                                    ri.result = firstDelete;
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

//                    System.err.println("Half of the operations has finished after search: " + (System.currentTimeMillis() - start));

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
                            if (i + 1 != l) {
                                while (right >= l) {
                                    r /= 2;
                                    right = i + r / 2;
                                }
                            }
                            if (right < l) {
                                ((Request) requests[i]).rightWait = (Request) requests[right];
                                ((Request) requests[right]).parentWait = (Request) requests[i];
                            } else {
                                ((Request) requests[i]).rightWait = null;
                            }
                        }
                    }

//                    for (int i = 0; i < l; i++) {
//                        assert requests[i] == root || ((Request) requests[i]).parentWait != null;
//                    }

                    if (l > 0 && request != root) { // Make the root thread to be the leader
                        prev_leader = request;
                        request.leader = false;
                        leaderInTransition = true;
                        root.leader = true;
//                        System.err.println("Initialize transition " + request.hashCode() + " -> " + root.hashCode() + " " + root.status);
                    }

                    // Now start all the operations
                    for (int i = 0; i < l; i++) {
                        ((Request) requests[i]).status = UPDATE_PHASE_START;
                    }

                    // Check if I have to participate too
                    if (request.status == UPDATE_PHASE_START) {
                        performUpdate(request);
                    }

                    // force the root to finish on his own and set a new tree
                    if (request == root) { // I'm the last operation
//                        assert request.status == JOIN_PHASE_FINISHED;
                        request.status = FINISHED;
                        tree.root = root.treeToWork;
                    } else {
                        if (l > 0) {
//                            System.err.println("I'm not a root anymore " + request.hashCode() + " " + root.status);
//                            assert request.status == JOIN_PHASE_FINISHED || request.status == FINISHED;
                            while (request.status != FINISHED) {
                            }
                        }
                    }

//                    while (l > 0 && root.status != JOIN_PHASE_FINISHED) {}
//                    if (l > 0) {
//                        tree.root = root.treeToWork;
//                        root.status = FINISHED;
//                    }

//                    System.err.println("Root finished " + root + " " + request + ": " + (System.currentTimeMillis() - start));

                    fc.cleanup();
                    if (!request.leader) { // I made the root thread to be the leader
//                        leaderThreads.decrementAndGet();
                        leaderInTransition = false;
//                        System.err.println("I'm definitely not a root and exits");
//                        fc.unlock();
                        return;
                    }
//                    if (requests.length < THRESHOLD) {
//                        break;
//                    }
                }
//                leaderThreads.decrementAndGet();
                leaderInTransition = false;
                request.leader = false; // We have not given a lock to anybody
                leaderExists = false;
//                System.err.println("Give up " + request.hashCode());
                fc.unlock();
            } else {
                // I'm not a leader and should wait for synchronization (or check the lock)
                switch (request.status) {
                    case PUSHED: // Nobody has started to work with me
                        fc.addRequest(request); // Probably, I'm out of queue?
                        continue; // It is better to restart and try to become a leader
                    case SEARCH_PHASE_WORKER:
                        request.result = tree.get(request.key);
                        request.status = SEARCH_PHASE_FINISHED;
                        continue;
                    case SEARCH_PHASE_FINISHED:
                        continue;
                    case UPDATE_PHASE_START:
                        performUpdate(request);
//                        assert leaderExists;
//                        assert request.status == JOIN_PHASE_FINISHED || request.status == FINISHED;
                        if (request.leader) {
//                            System.err.println(prev_leader.hashCode() + " -> " + request.hashCode());
//                            assert prev_leader.status == FINISHED || (prev_leader.status == PUSHED && !leaderInTransition);
//                            assert request.status == JOIN_PHASE_FINISHED;
                            request.status = FINISHED;
//                            assertRequests();
                            tree.root = request.treeToWork;
                        }
                        continue;
                    default:
                        continue;
                }
            }
        }
    }

    @Override
    public V putIfAbsent(K key, V value) {
        Request request = getLocalRequest();
//        System.err.println("Insert " + request.hashCode());
        request.set(OperationType.INSERT, key, value);
        handleRequest(request);
        return request.result;
    }

    @Override
    public V remove(Object key) {
        Request request = getLocalRequest();
//        System.err.println("Remove " + request.hashCode());
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
    public Set<java.util.Map.Entry<K, V>> entrySet() {
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
