package trees.flatcombining;

import trees.flatcombining.sequential.Treap;

import java.util.Comparator;

/**
 * Created by vaksenov on 13.03.2017.
 */
public class FCTreapv2<K, V> extends FCTreeMapv2<K, V> {
    public FCTreapv2() {
        super();
        tree = new Treap<>();
    }

    public FCTreapv2(Comparator<? super K> comparator) {
        super(comparator);
        tree = new Treap<>();
    }
}
