package trees.flatcombining.sequential;

import java.util.Random;

/**
 * Created by vaksenov on 13.03.2017.
 */
public class Treap<K, V> extends JoinableTreeMap<K, V> {
    private ThreadLocal<Random> randomGenerators = new ThreadLocal<>();

    private Random getLocalRandomGenerator() {
        Random rnd = randomGenerators.get();
        if (rnd == null) {
            rnd = new Random(Thread.currentThread().getId());
            randomGenerators.set(rnd);
        }
        return rnd;
    }

    public class Node extends JoinableTreeMap<K, V>.Node {
        int priority;

        public Node(K key, V value) {
            super(key, value);
            priority = getLocalRandomGenerator().nextInt();
        }
    }

    public Node createNode(K key, V value) {
        return new Node(key, value);
    }

    private boolean prior(Node x, Node y) {
        return x != null && (y == null || x.priority < y.priority);
    }

    public Node join(Node l, Node m, Node r) {
        if (prior(m, l) && prior(m, r)) {
            m.l = l;
            m.r = r;
            return m;
        }
        if (prior(l, r)) {
            l.r = join(l.r, m, r);
            return l;
        } else {
            r.l = join(l, m, r.l);
            return r;
        }
    }

    public Node join(JoinableTreeMap<K, V>.Node l, JoinableTreeMap<K, V>.Node m, JoinableTreeMap<K, V>.Node r) {
        return join((Node) l, (Node) m, (Node) r);
    }

}
