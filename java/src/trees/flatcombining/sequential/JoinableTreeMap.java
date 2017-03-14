package trees.flatcombining.sequential;

import java.util.Comparator;

/**
 * Created by vaksenov on 16.01.2017.
 */
public abstract class JoinableTreeMap<K, V> {
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

    public JoinableTreeMap() {}

    public JoinableTreeMap(final Comparator<? super K> comparator) {
        this.comparator = comparator;
    }

    public class Node {
        K key;
        V value;

        volatile Node l, r;

        public Node(K key, V value) {
            this.key = key;
            this.value = value;
        }

        public void clear() {
            l = null;
            r = null;
        }
    }

    public volatile Node root;

    public V get(K k) {
        Comparable<? super K> key = comparable(k);
        Node now = root;
        while (now != null) {
            int diff = key.compareTo(now.key);
            if (diff == 0) {
                return now.value;
            }
            if (diff < 0) {
                now = now.l;
            } else {
                now = now.r;
            }
        }
        return null;
    }

    public class Result {
        public Node l;
        public Node m;
        public Node r;

        public Result(Node l, Node m, Node r) {
            this.l = l;
            this.m = m;
            this.r = r;
        }
    }

    public abstract Node createNode(K key, V value);

    public abstract Node join(Node l, Node m, Node r);

    private Result splitLast(Node t) {
        if (t == null) {
            return new Result(null, null, null);
        }
        if (t.r == null) {
            return new Result(t.l, t, null);
        }
        Result result = splitLast(t.r);
        result.l = join(t.l, t, result.l);
        return result;
    }

    public Node join2(Node l, Node r) {
//        return l;
        if (l == null) {
            return r;
        }
        if (r == null) {
            return l;
        }
        Result result = splitLast(l);
        return join(result.l, result.m, r);
    }

    public Result split(Node t, Comparable<? super K> key) {
//        return new Result(t, null, t);
        if (t == null) {
            return new Result(null, null, null);
        }
        int comparison = key.compareTo(t.key);
        if (comparison == 0) {
            return new Result(t.l, t, t.r);
        }
        if (comparison < 0) {
            Result result = split(t.l, key);
//            Node r = t.r;
//            t.clear();
            result.r = join(result.r, t, t.r); // Do not forget that t should be empty
            return result;
        } else {
            Result result = split(t.r, key);
//            Node l = t.l;
//            t.clear();
            result.l = join(t.l, t, result.l); // Do not forget that t should be empty
            return result;
        }
    }

    // it is know that there is no such value
    public void insert(K key, V value) {
        Node node = new Node(key, value);
        Result result = split(root, comparable(key));
        root = join(result.l, node, result.r);
    }

    // we know that it is here
    public void remove(K key) {
        Result result = split(root, comparable(key));
        root = join2(result.l, result.r);
    }

    private int size(Node v) {
        return v == null ? 0 : 1 + size(v.l) + size(v.r);
    }

    public int size() {
        return size(root);
    }

    public String toString(Node v) {
        return v == null ? "{}" : "{" + (v.l == null ? "" : toString(v.l)) + " , " + v.key + " , " + (v.r == null ? "" : toString(v.r)) + "}";
    }

    public String toString() {
        return toString(root);
    }
}
