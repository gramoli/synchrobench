package contention.abstractions;

import java.util.Collection;
import java.util.Map;
import java.util.Set;

public abstract class AbstractCompositionalIntSet implements CompositionalIntSet, CompositionalMap<Integer, Integer> {

    @Override
    public boolean containsKey(Object key) {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public boolean containsValue(Object value) {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public Set<java.util.Map.Entry<Integer, Integer>> entrySet() {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public Integer get(Object key) {
        if (containsInt((Integer) key)) {
            return (Integer) key;
        }
        return null;
    }

    @Override
    public boolean isEmpty() {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public Set<Integer> keySet() {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public Integer put(Integer key, Integer value) {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public void putAll(Map<? extends Integer, ? extends Integer> m) {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public Integer remove(Object key) {
        if (removeInt((Integer) key)) {
            return (Integer) key;
        }
        return null;
    }

    @Override
    public Collection<Integer> values() {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public Integer putIfAbsent(Integer k, Integer v) {
        if (addInt(k)) {
            return null;
        }
        return k;
    }

    @Override
    public Object putIfAbsent(int x, int y) {
        if (addInt(x)) {
            return null;
        }
        return x;
    }

    @Override
    public Object getInt(int x) {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public boolean addAll(Collection<Integer> c) {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public boolean removeAll(Collection<Integer> c) {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }

    @Override
    public void fill(int range, long size) {
        throw new RuntimeException("unimplemented method");
        // TODO Auto-generated method stub
    }
}
