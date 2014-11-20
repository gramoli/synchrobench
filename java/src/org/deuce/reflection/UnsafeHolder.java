package org.deuce.reflection;

import java.lang.reflect.Field;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.deuce.transform.Exclude;

import sun.misc.Unsafe;

@Exclude
public class UnsafeHolder {

	final private static Logger logger = Logger.getLogger("org.deuce.reflection");

	final private static Unsafe unsafe;
	static{
		Unsafe unsafeValue = null;
		try{
			Field field = Unsafe.class.getDeclaredField("theUnsafe");
			field.setAccessible(true);
			unsafeValue = (Unsafe)field.get(null);
		}catch( Exception e){
			logger.log(Level.SEVERE, "Fail to initialize Unsafe.", e);
		}
		unsafe = unsafeValue;
	}

	public static Unsafe getUnsafe() {
		return unsafe;
	}
}
