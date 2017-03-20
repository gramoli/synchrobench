package trees.flatcombining.sequential;

/**
 * Created by vaksenov on 16.03.2017.
 */
public class AVL<K, V> extends JoinableTreeMap<K, V> {
    public class Node extends JoinableTreeMap<K, V>.Node {
        int h;

        public Node(K key, V value) {
            super(key, value);
            h = 1;
        }
    }

    public Node createNode(K key, V value) {
        return new Node(key, value);
    }

    private int height(Node node) {
        return node == null ? 0 : node.h;
    }

    private void recalc(Node node) {
        node.h = Math.max(height((Node)node.l), height((Node)node.r)) + 1;
    }

    private Node rotateRight(Node node) {
        Node l = (Node) node.l;
        Node lr = (Node) l.r;
        l.r = node;
        node.l = lr;
        recalc(node);
        recalc(l);
        return l;
    }

    private Node rotateLeft(Node node) {
        Node r = (Node) node.r;
        Node rl = (Node) r.l;
        r.l = node;
        node.r = rl;
        recalc(node);
        recalc(r);
        return r;
    }

    private Node joinToRight(Node l, Node m, Node r) {
        Node rl = (Node) r.l;
        Node rr = (Node) r.r;
        if (height(rl) <= height(l) + 1) {
            m.l = l;
            m.r = rl;
            recalc(m);
            if (m.h <= height(rr) + 1) {
                r.l = m;
                return r;
            } else {
                r.l = rotateLeft(m);
                return rotateRight(r);
            }
        } else {
            rl = joinToRight(l, m, rl);
            r.l = rl;
            if (rl.h <= rr.h + 1) {
                return r;
            } else {
                return rotateRight(r);
            }
        }
    }

    private Node joinToLeft(Node l, Node m, Node r) {
        Node ll = (Node) l.l;
        Node lr = (Node) l.r;
        if (height(lr) <= height(r) + 1) {
            m.l = lr;
            m.r = r;
            recalc(m);
            if (m.h <= height(ll) + 1) {
                l.r = m;
                return l;
            } else {
                l.r = rotateRight(m);
                return rotateLeft(l);
            }
        } else {
            lr = joinToLeft(lr, m, r);
            l.r = lr;
            if (lr.h <= ll.h + 1) {
                return l;
            } else {
                return rotateLeft(l);
            }
        }
    }

    public Node join(Node l, Node m, Node r) {
        if (Math.abs(height(l) - height(r)) <= 1) {
            m.l = l;
            m.r = r;
            recalc(m);
            return m;
        }
        if (height(l) < height(r)) {
            return joinToRight(l, m, r);
        } else {
            return joinToLeft(l, m, r);
        }
    }

    public Node join(JoinableTreeMap<K, V>.Node l, JoinableTreeMap<K, V>.Node m, JoinableTreeMap<K, V>.Node r) {
        return join((Node) l, (Node) m, (Node) r);
    }
}
