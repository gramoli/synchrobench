package trees.flatcombining;

import trees.flatcombining.sequential.AVL;
import trees.flatcombining.sequential.Treap;

import java.util.Comparator;

/**
 * Created by vaksenov on 13.03.2017.
 */
public class FCParkAVL<K, V> extends FCParkTreeMap<K, V> {
    public FCParkAVL() {
        super();
        tree = new AVL<>();
    }

    public FCParkAVL(Comparator<? super K> comparator) {
        super(comparator);
        tree = new AVL<>();
    }
}
