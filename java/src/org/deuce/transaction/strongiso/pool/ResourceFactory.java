package org.deuce.transaction.strongiso.pool;

public interface ResourceFactory<T>{
	T newInstance();
}
