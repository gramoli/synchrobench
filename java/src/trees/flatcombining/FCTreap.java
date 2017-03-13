package trees.flatcombining;

import trees.flatcombining.sequential.Treap;

import java.util.Comparator;

/**
 * Created by vaksenov on 13.03.2017.
 */
public class FCTreap<K, V> extends FCTreeMap<K, V> {
    public FCTreap() {
        super();
        tree = new Treap<>();
    }

    public FCTreap(Comparator<? super K> comparator) {
        super(comparator);
        tree = new Treap<>();
    }
}
