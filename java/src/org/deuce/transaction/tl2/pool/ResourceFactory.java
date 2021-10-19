package org.deuce.transaction.tl2.pool;

public interface ResourceFactory<T>{
	T newInstance();
}
