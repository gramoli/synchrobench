package trees.flatcombining;

import trees.flatcombining.sequential.Treap;

import java.util.Comparator;

/**
 * Created by vaksenov on 13.03.2017.
 */
public class FCParkTreap<K, V> extends FCParkTreeMap<K, V> {
    public FCParkTreap() {
        super();
        tree = new Treap<>();
    }

    public FCParkTreap(Comparator<? super K> comparator) {
        super(comparator);
        tree = new Treap<>();
    }
}
