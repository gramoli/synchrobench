package org.deuce.transaction;

import org.deuce.objectweb.asm.Type;
import org.deuce.transform.Exclude;

/**
 * If thrown under the context of an active transaction the current transaction
 * context will be rolled back and *no* retry will be initialized.
 * 
 * @author Guy Korland
 * @since 1.3
 */
@Exclude
public class AbortTransactionException extends TransactionException{
	
	final static public String ABORT_TRANSACTION_EXCEPTION_INTERNAL = Type.getInternalName(AbortTransactionException.class);
	
	public AbortTransactionException(){}

	public AbortTransactionException( String msg){
		super(msg);
	}

	public AbortTransactionException( Throwable cause){
		super(cause);
	}
}
