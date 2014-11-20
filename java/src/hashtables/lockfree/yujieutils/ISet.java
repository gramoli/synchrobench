package hashtables.lockfree.yujieutils;

public interface ISet
{
    boolean insert(int key, int tid);
    boolean remove(int key, int tid);
    boolean contains(int key);

    boolean simpleInsert(int key, int tid);
    boolean simpleRemove(int key, int tid);

    boolean grow();
    boolean shrink();
    int getBucketSize();
}
