package trees.flatcombining;

import trees.flatcombining.sequential.AVL;

import java.util.Comparator;

/**
 * Created by vaksenov on 13.03.2017.
 */
public class FCAVL<K, V> extends FCTreeMap<K, V> {
    public FCAVL() {
        super();
        tree = new AVL<>();
    }

    public FCAVL(Comparator<? super K> comparator) {
        super(comparator);
        tree = new AVL<>();
    }
}
