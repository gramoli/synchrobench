package org.deuce.transaction.tl2;

import org.deuce.transaction.tl2.field.WriteFieldAccess;
import org.deuce.trove.TIntArrayList;
import org.deuce.trove.TIntProcedure;
import org.deuce.trove.TObjectProcedure;

/**
 * Procedure used to scan the WriteSet on commit.
 * 
 * @author Guy
 * @since 1.4 
 */
public class LockProcedure implements TObjectProcedure<WriteFieldAccess>{
		
		private final TIntArrayList lockSet = new TIntArrayList();
		private final byte[] locksMarker;
		private final TIntProcedure unlockProcedure = new TIntProcedure(){
			@Override
			public boolean execute(int value) {
				LockTable.unLock( value,locksMarker);
				return true;
			}
		};
		
		private static class SetAndUnlockProcedure implements TIntProcedure{
			
			private final byte[] locksMarker;
			private int newClock;

			public SetAndUnlockProcedure(byte[] locksMarker){
				this.locksMarker = locksMarker;
			}
			
			@Override
			public boolean execute(int value) {
				LockTable.setAndReleaseLock( value, newClock, locksMarker);
				return true;
			}
			
			public void retrieveNewClock(){
				this.newClock = Context.clock.incrementAndGet();
			}
		}
		
		private final SetAndUnlockProcedure setAndUnlockProcedure;
		
		public LockProcedure(byte[] locksMarker){
			this.locksMarker = locksMarker;
			setAndUnlockProcedure = new SetAndUnlockProcedure(locksMarker);
		}
		
		@Override
		public boolean execute(WriteFieldAccess writeField) {
			int hashCode = writeField.hashCode();
			if( LockTable.lock( hashCode, locksMarker))
				lockSet.add( hashCode);
			return true;
		}
		
		public void unlockAll(){
			lockSet.forEach(unlockProcedure);
			lockSet.resetQuick();
		}
		
		public void setAndUnlockAll(){
			setAndUnlockProcedure.retrieveNewClock();
			lockSet.forEach(setAndUnlockProcedure);
			lockSet.resetQuick();
		}
		
	}