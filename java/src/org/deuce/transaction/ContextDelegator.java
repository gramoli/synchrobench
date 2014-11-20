package org.deuce.transaction;

import org.deuce.objectweb.asm.Type;
import org.deuce.reflection.AddressUtil;
import org.deuce.transform.Exclude;
import org.deuce.transform.UseStrongIso;

/**
 * Cluster static delegate methods.
 * These methods delegates calls from the dynamic generated code to the context.
 * 
 * 
 * @author	Guy Korland
 * @since	1.0
 *
 */
public class ContextDelegator {

	final static public String CONTEXT_DELEGATOR_INTERNAL = Type.getInternalName(ContextDelegator.class);
	
	final static public String BEFORE_READ_METHOD_NAME;
	final static public String BEFORE_READ_METHOD_DESC;
	static {
		if(UseStrongIso.USE_STRONG_ISO) {
			BEFORE_READ_METHOD_NAME = "beforeReadAccessStrongIso";
			BEFORE_READ_METHOD_DESC = "(Ljava/lang/Object;Ljava/lang/Object;JJ" + Context.CONTEXT_DESC +")V";
		} else {
			BEFORE_READ_METHOD_NAME = "beforeReadAccess";
			BEFORE_READ_METHOD_DESC = "(Ljava/lang/Object;J" + Context.CONTEXT_DESC +")V";
		}
	}
	
	final static public String WRITE_ARR_METHOD_NAME = "onArrayWriteAccess";
	
	final static public String WRITE_METHOD_NAME;
	final static public String STATIC_WRITE_METHOD_NAME;
	static {
		if(UseStrongIso.USE_STRONG_ISO) {
			WRITE_METHOD_NAME = "onWriteAccessStrongIso";
			STATIC_WRITE_METHOD_NAME = "addStaticWriteAccessStrongIso";
		} else {
			WRITE_METHOD_NAME = "onWriteAccess";
			STATIC_WRITE_METHOD_NAME = "addStaticWriteAccess";
		}
	}
	
	final static public String READ_METHOD_NAME = "onReadAccess";
	final static public String READ_ARR_METHOD_NAME;
	static {
		if(UseStrongIso.USE_STRONG_ISO) {
			READ_ARR_METHOD_NAME = "onArrayReadAccessStrongIso";
		} else {
			READ_ARR_METHOD_NAME = "onArrayReadAccess";
		}
	}

	final static private String WRITE_METHOD_BOOLEAN_DESC;
	final static private String WRITE_METHOD_BYTE_DESC;
	final static private String WRITE_METHOD_CHAR_DESC;
	final static private String WRITE_METHOD_SHORT_DESC;
	final static private String WRITE_METHOD_INT_DESC;
	final static private String WRITE_METHOD_LONG_DESC;
	final static private String WRITE_METHOD_FLOAT_DESC;
	final static private String WRITE_METHOD_DOUBLE_DESC;
	final static private String WRITE_METHOD_OBJ_DESC;

	final static private String STATIC_WRITE_METHOD_BOOLEAN_DESC;
	final static private String STATIC_WRITE_METHOD_BYTE_DESC;
	final static private String STATIC_WRITE_METHOD_CHAR_DESC;
	final static private String STATIC_WRITE_METHOD_SHORT_DESC;
	final static private String STATIC_WRITE_METHOD_INT_DESC;
	final static private String STATIC_WRITE_METHOD_LONG_DESC;
	final static private String STATIC_WRITE_METHOD_FLOAT_DESC;
	final static private String STATIC_WRITE_METHOD_DOUBLE_DESC;
	final static private String STATIC_WRITE_METHOD_OBJ_DESC;
	
	static{
		if(UseStrongIso.USE_STRONG_ISO) {
			WRITE_METHOD_BOOLEAN_DESC = "(Ljava/lang/Object;ZJJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_BYTE_DESC = "(Ljava/lang/Object;BJJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_CHAR_DESC = "(Ljava/lang/Object;CJJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_SHORT_DESC = "(Ljava/lang/Object;SJJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_INT_DESC = "(Ljava/lang/Object;IJJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_LONG_DESC = "(Ljava/lang/Object;JJJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_FLOAT_DESC = "(Ljava/lang/Object;FJJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_DOUBLE_DESC = "(Ljava/lang/Object;DJJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_OBJ_DESC = "(Ljava/lang/Object;Ljava/lang/Object;JJ" + Context.CONTEXT_DESC +")V";
		
			STATIC_WRITE_METHOD_BOOLEAN_DESC = "(ZLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_BYTE_DESC = "(BLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_CHAR_DESC = "(CLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_SHORT_DESC = "(SLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_INT_DESC = "(ILjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_LONG_DESC = "(JLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_FLOAT_DESC = "(FLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_DOUBLE_DESC = "(DLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_OBJ_DESC = "(Ljava/lang/Object;Ljava/lang/Object;J" + Context.CONTEXT_DESC +")V";
		} else {
			WRITE_METHOD_BOOLEAN_DESC = "(Ljava/lang/Object;ZJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_BYTE_DESC = "(Ljava/lang/Object;BJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_CHAR_DESC = "(Ljava/lang/Object;CJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_SHORT_DESC = "(Ljava/lang/Object;SJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_INT_DESC = "(Ljava/lang/Object;IJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_LONG_DESC = "(Ljava/lang/Object;JJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_FLOAT_DESC = "(Ljava/lang/Object;FJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_DOUBLE_DESC = "(Ljava/lang/Object;DJ" + Context.CONTEXT_DESC +")V";
			WRITE_METHOD_OBJ_DESC = "(Ljava/lang/Object;Ljava/lang/Object;J" + Context.CONTEXT_DESC +")V";
		
			STATIC_WRITE_METHOD_BOOLEAN_DESC = "(ZLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_BYTE_DESC = "(BLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_CHAR_DESC = "(CLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_SHORT_DESC = "(SLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_INT_DESC = "(ILjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_LONG_DESC = "(JLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_FLOAT_DESC = "(FLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_DOUBLE_DESC = "(DLjava/lang/Object;J" + Context.CONTEXT_DESC +")V";
			STATIC_WRITE_METHOD_OBJ_DESC = "(Ljava/lang/Object;Ljava/lang/Object;J" + Context.CONTEXT_DESC +")V";
		}
	}
	
	
	
	

	final static private String READ_METHOD_BOOLEAN_DESC = "(Ljava/lang/Object;ZJ" + Context.CONTEXT_DESC +")Z";
	final static private String READ_METHOD_BYTE_DESC = "(Ljava/lang/Object;BJ" + Context.CONTEXT_DESC +")B";
	final static private String READ_METHOD_CHAR_DESC = "(Ljava/lang/Object;CJ" + Context.CONTEXT_DESC +")C";
	final static private String READ_METHOD_SHORT_DESC = "(Ljava/lang/Object;SJ" + Context.CONTEXT_DESC +")S";
	final static private String READ_METHOD_INT_DESC = "(Ljava/lang/Object;IJ" + Context.CONTEXT_DESC +")I";
	final static private String READ_METHOD_LONG_DESC = "(Ljava/lang/Object;JJ" + Context.CONTEXT_DESC +")J";
	final static private String READ_METHOD_FLOAT_DESC = "(Ljava/lang/Object;FJ" + Context.CONTEXT_DESC +")F";
	final static private String READ_METHOD_DOUBLE_DESC = "(Ljava/lang/Object;DJ" + Context.CONTEXT_DESC +")D";
	final static private String READ_METHOD_OBJ_DESC = "(Ljava/lang/Object;Ljava/lang/Object;J" + Context.CONTEXT_DESC +")Ljava/lang/Object;";

	final static public String WRITE_ARRAY_METHOD_BYTE_DESC = "([BIB" + Context.CONTEXT_DESC +")V";
	final static public String WRITE_ARRAY_METHOD_CHAR_DESC = "([CIC" + Context.CONTEXT_DESC +")V";
	final static public String WRITE_ARRAY_METHOD_SHORT_DESC = "([SIS" + Context.CONTEXT_DESC +")V";
	final static public String WRITE_ARRAY_METHOD_INT_DESC = "([III" + Context.CONTEXT_DESC +")V";
	final static public String WRITE_ARRAY_METHOD_LONG_DESC = "([JIJ" + Context.CONTEXT_DESC +")V";
	final static public String WRITE_ARRAY_METHOD_FLOAT_DESC = "([FIF" + Context.CONTEXT_DESC +")V";
	final static public String WRITE_ARRAY_METHOD_DOUBLE_DESC = "([DID" + Context.CONTEXT_DESC +")V";
	final static public String WRITE_ARRAY_METHOD_OBJ_DESC = "([Ljava/lang/Object;ILjava/lang/Object;" + Context.CONTEXT_DESC +")V";
	

	final static public String READ_ARRAY_METHOD_BYTE_DESC;
	final static public String READ_ARRAY_METHOD_CHAR_DESC;
	final static public String READ_ARRAY_METHOD_SHORT_DESC;
	final static public String READ_ARRAY_METHOD_INT_DESC;
	final static public String READ_ARRAY_METHOD_LONG_DESC;
	final static public String READ_ARRAY_METHOD_FLOAT_DESC;
	final static public String READ_ARRAY_METHOD_DOUBLE_DESC;
	final static public String READ_ARRAY_METHOD_OBJ_DESC;
	static {
		if(UseStrongIso.USE_STRONG_ISO) {
//			READ_ARRAY_METHOD_BYTE_DESC = "([BI" + Type.getType(ArrayReadFieldAccess.class).getDescriptor() + Context.CONTEXT_DESC +")B";
//			READ_ARRAY_METHOD_CHAR_DESC = "([CI" + Type.getType(ArrayReadFieldAccess.class).getDescriptor() + Context.CONTEXT_DESC +")C";
//			READ_ARRAY_METHOD_SHORT_DESC = "([SI" + Type.getType(ArrayReadFieldAccess.class).getDescriptor() + Context.CONTEXT_DESC +")S";
//			READ_ARRAY_METHOD_INT_DESC = "([II" + Type.getType(ArrayReadFieldAccess.class).getDescriptor() + Context.CONTEXT_DESC +")I";
//			READ_ARRAY_METHOD_LONG_DESC = "([JI" + Type.getType(ArrayReadFieldAccess.class).getDescriptor() + Context.CONTEXT_DESC +")J";
//			READ_ARRAY_METHOD_FLOAT_DESC = "([FI" + Type.getType(ArrayReadFieldAccess.class).getDescriptor() + Context.CONTEXT_DESC +")F";
//			READ_ARRAY_METHOD_DOUBLE_DESC = "([DI" + Type.getType(ArrayReadFieldAccess.class).getDescriptor() + Context.CONTEXT_DESC +")D";
//			READ_ARRAY_METHOD_OBJ_DESC = "([Ljava/lang/Object;I" + Type.getType(ArrayReadFieldAccess.class).getDescriptor() + Context.CONTEXT_DESC +")Ljava/lang/Object;";
			READ_ARRAY_METHOD_BYTE_DESC = "([BI" + "J" + Context.CONTEXT_DESC +")B";
			READ_ARRAY_METHOD_CHAR_DESC = "([CI" + "J" + Context.CONTEXT_DESC +")C";
			READ_ARRAY_METHOD_SHORT_DESC = "([SI" + "J" + Context.CONTEXT_DESC +")S";
			READ_ARRAY_METHOD_INT_DESC = "([II" + "J" + Context.CONTEXT_DESC +")I";
			READ_ARRAY_METHOD_LONG_DESC = "([JI" + "J" + Context.CONTEXT_DESC +")J";
			READ_ARRAY_METHOD_FLOAT_DESC = "([FI" + "J" + Context.CONTEXT_DESC +")F";
			READ_ARRAY_METHOD_DOUBLE_DESC = "([DI" + "J" + Context.CONTEXT_DESC +")D";
			READ_ARRAY_METHOD_OBJ_DESC = "([Ljava/lang/Object;I" + "J" + Context.CONTEXT_DESC +")Ljava/lang/Object;";
		} else {
			READ_ARRAY_METHOD_BYTE_DESC = "([BI" + Context.CONTEXT_DESC +")B";
			READ_ARRAY_METHOD_CHAR_DESC = "([CI" + Context.CONTEXT_DESC +")C";
			READ_ARRAY_METHOD_SHORT_DESC = "([SI" + Context.CONTEXT_DESC +")S";
			READ_ARRAY_METHOD_INT_DESC = "([II" + Context.CONTEXT_DESC +")I";
			READ_ARRAY_METHOD_LONG_DESC = "([JI" + Context.CONTEXT_DESC +")J";
			READ_ARRAY_METHOD_FLOAT_DESC = "([FI" + Context.CONTEXT_DESC +")F";
			READ_ARRAY_METHOD_DOUBLE_DESC = "([DI" + Context.CONTEXT_DESC +")D";
			READ_ARRAY_METHOD_OBJ_DESC = "([Ljava/lang/Object;I" + Context.CONTEXT_DESC +")Ljava/lang/Object;";
		}
	}

	final static private int BYTE_ARR_BASE = AddressUtil.arrayBaseOffset(byte[].class);
	final static private int CHAR_ARR_BASE = AddressUtil.arrayBaseOffset(char[].class);
	final static private int SHORT_ARR_BASE = AddressUtil.arrayBaseOffset(short[].class);
	final static private int INT_ARR_BASE = AddressUtil.arrayBaseOffset(int[].class);
	final static private int LONG_ARR_BASE = AddressUtil.arrayBaseOffset(long[].class);
	final static private int FLOAT_ARR_BASE = AddressUtil.arrayBaseOffset(float[].class);
	final static private int DOUBLE_ARR_BASE = AddressUtil.arrayBaseOffset(double[].class);
	final static private int OBJECT_ARR_BASE = AddressUtil.arrayBaseOffset(Object[].class);

	final static private int BYTE_ARR_SCALE = AddressUtil.arrayIndexScale(byte[].class);
	final static private int CHAR_ARR_SCALE = AddressUtil.arrayIndexScale(char[].class);
	final static private int SHORT_ARR_SCALE = AddressUtil.arrayIndexScale(short[].class);
	final static private int INT_ARR_SCALE = AddressUtil.arrayIndexScale(int[].class);
	final static private int LONG_ARR_SCALE = AddressUtil.arrayIndexScale(long[].class);
	final static private int FLOAT_ARR_SCALE = AddressUtil.arrayIndexScale(float[].class);
	final static private int DOUBLE_ARR_SCALE = AddressUtil.arrayIndexScale(double[].class);
	final static private int OBJECT_ARR_SCALE = AddressUtil.arrayIndexScale(Object[].class);


	final private static ContextThreadLocal THREAD_CONTEXT = new ContextThreadLocal();

	@Exclude
	private static class ContextThreadLocal extends ThreadLocal<Context>
	{
		private Class<?> contextClass;  

		public ContextThreadLocal(){
			String className = System.getProperty( "org.deuce.transaction.contextClass");
			if( className != null){
				try {
					this.contextClass = Class.forName(className);
					System.out.println("Found!!! stm " + className);
					return;
				} catch (ClassNotFoundException e) {
					System.out.println("Did not find!!! stm " + className);
					e.printStackTrace(); // TODO add logger
				}
			}
			System.out.println("Did not find!!! stm " + className);
			this.contextClass = org.deuce.transaction.estm.Context.class;
			//System.out.println("Using GTM");
			//this.contextClass = org.deuce.transaction.gtm.Context.class;
		}

		@Override
		protected synchronized Context initialValue() {
			try {
				return (Context)this.contextClass.newInstance();
			} catch (Exception e) {
				throw new TransactionException( e);
			}
		}
	}

	public static Context getInstance(){
		return THREAD_CONTEXT.get();
	}

	public static String getWriteMethodDesc( Type type) {
		switch( type.getSort()) {
		case Type.BOOLEAN:
			return WRITE_METHOD_BOOLEAN_DESC;
		case Type.BYTE:
			return WRITE_METHOD_BYTE_DESC;
		case Type.CHAR:
			return WRITE_METHOD_CHAR_DESC;
		case Type.SHORT:
			return WRITE_METHOD_SHORT_DESC;
		case Type.INT:
			return WRITE_METHOD_INT_DESC;
		case Type.LONG:
			return WRITE_METHOD_LONG_DESC;
		case Type.FLOAT:
			return WRITE_METHOD_FLOAT_DESC;
		case Type.DOUBLE:
			return WRITE_METHOD_DOUBLE_DESC;
		default:
			return WRITE_METHOD_OBJ_DESC;
		}
	}

	public static String getStaticWriteMethodDesc( Type type) {
		switch( type.getSort()) {
		case Type.BOOLEAN:
			return STATIC_WRITE_METHOD_BOOLEAN_DESC;
		case Type.BYTE:
			return STATIC_WRITE_METHOD_BYTE_DESC;
		case Type.CHAR:
			return STATIC_WRITE_METHOD_CHAR_DESC;
		case Type.SHORT:
			return STATIC_WRITE_METHOD_SHORT_DESC;
		case Type.INT:
			return STATIC_WRITE_METHOD_INT_DESC;
		case Type.LONG:
			return STATIC_WRITE_METHOD_LONG_DESC;
		case Type.FLOAT:
			return STATIC_WRITE_METHOD_FLOAT_DESC;
		case Type.DOUBLE:
			return STATIC_WRITE_METHOD_DOUBLE_DESC;
		default:
			return STATIC_WRITE_METHOD_OBJ_DESC;
		}
	}

	public static String getReadMethodDesc( Type type) {
		switch( type.getSort()) {
		case Type.BOOLEAN:
			return READ_METHOD_BOOLEAN_DESC;
		case Type.BYTE:
			return READ_METHOD_BYTE_DESC;
		case Type.CHAR:
			return READ_METHOD_CHAR_DESC;
		case Type.SHORT:
			return READ_METHOD_SHORT_DESC;
		case Type.INT:
			return READ_METHOD_INT_DESC;
		case Type.LONG:
			return READ_METHOD_LONG_DESC;
		case Type.FLOAT:
			return READ_METHOD_FLOAT_DESC;
		case Type.DOUBLE:
			return READ_METHOD_DOUBLE_DESC;
		default:
			return READ_METHOD_OBJ_DESC;
		}
	}


	static public void beforeReadAccess( Object obj, long field, Context context) {
		context.beforeReadAccess(obj, field);
	}
	
	static public void beforeReadAccessStrongIso( Object obj, Object obj2, long field, long fieldObj, Context context) {
		context.beforeReadAccessStrongIso(obj, field, obj2, fieldObj);
	}

	static public Object onReadAccess( Object obj, Object value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}
	static public boolean onReadAccess( Object obj, boolean value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}
	static public byte onReadAccess( Object obj, byte value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}
	static public char onReadAccess( Object obj, char value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}
	static public short onReadAccess( Object obj, short value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}
	static public int onReadAccess( Object obj, int value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}
	static public long onReadAccess( Object obj, long value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}
	static public float onReadAccess( Object obj, float value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}
	static public double onReadAccess( Object obj, double value, long field, Context context) {
		return context.onReadAccess(obj, value, field);
	}

	
	
	static public void onWriteAccess( Object obj, Object value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void onWriteAccess( Object obj, boolean value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void onWriteAccess( Object obj, byte value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void onWriteAccess( Object obj, char value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void onWriteAccess( Object obj, short value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void onWriteAccess( Object obj, int value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void onWriteAccess( Object obj, long value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void onWriteAccess( Object obj, float value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void onWriteAccess( Object obj, double value, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}

	static public void addStaticWriteAccess( Object value, Object obj, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void addStaticWriteAccess( boolean value, Object obj, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void addStaticWriteAccess( byte value, Object obj, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void addStaticWriteAccess( char value, Object obj, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void addStaticWriteAccess( short value, Object obj, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void addStaticWriteAccess( int value, Object obj, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void addStaticWriteAccess( long value, Object obj, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void addStaticWriteAccess( float value, Object obj, long field, Context context) {
		context.onWriteAccess(obj, value, field);
	}
	static public void addStaticWriteAccess( double value, Object obj, long field, Context context) { 
		context.onWriteAccess(obj, value, field);
	}

	
	
	
	
	

	static public void onWriteAccessStrongIso( Object obj, Object value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void onWriteAccessStrongIso( Object obj, boolean value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void onWriteAccessStrongIso( Object obj, byte value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void onWriteAccessStrongIso( Object obj, char value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void onWriteAccessStrongIso( Object obj, short value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void onWriteAccessStrongIso( Object obj, int value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void onWriteAccessStrongIso( Object obj, long value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void onWriteAccessStrongIso( Object obj, float value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void onWriteAccessStrongIso( Object obj, double value, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}

	static public void addStaticWriteAccessStrongIso( Object value, Object obj, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void addStaticWriteAccessStrongIso( boolean value, Object obj, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void addStaticWriteAccessStrongIso( byte value, Object obj, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void addStaticWriteAccessStrongIso( char value, Object obj, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void addStaticWriteAccessStrongIso( short value, Object obj, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void addStaticWriteAccessStrongIso( int value, Object obj, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void addStaticWriteAccessStrongIso( long value, Object obj, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void addStaticWriteAccessStrongIso( float value, Object obj, long field, long fieldObj, Context context) {
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}
	static public void addStaticWriteAccessStrongIso( double value, Object obj, long field, long fieldObj, Context context) { 
		context.onWriteAccessStrongIso(obj, value, field, fieldObj);
	}

	
	
	
	
	
	
	
	
//	static public Object onArrayReadAccessStrongIso( Object[] arr, int index, ArrayReadFieldAccess objArr, Context context) {
//		int address = OBJECT_ARR_BASE + OBJECT_ARR_SCALE*index;
//		context.beforeReadAccess(arr, address);
//		return context.onReadAccess(arr, arr[index], address);
//	}
//	static public byte onArrayReadAccessStrongIso( byte[] arr, int index, ArrayReadFieldAccess objArr, Context context) {
//		int address = BYTE_ARR_BASE + BYTE_ARR_SCALE*index;
//		context.beforeReadAccess(arr, address);
//		return context.onReadAccess(arr, arr[index], address);
//	}
//	static public char onArrayReadAccessStrongIso( char[] arr, int index, ArrayReadFieldAccess objArr, Context context) {
//		int address = CHAR_ARR_BASE + CHAR_ARR_SCALE*index;
//		context.beforeReadAccess(arr, address);
//		return context.onReadAccess(arr, arr[index], address);
//	}
//	static public short onArrayReadAccessStrongIso( short[] arr, int index, ArrayReadFieldAccess objArr, Context context) {
//		int address = SHORT_ARR_BASE + SHORT_ARR_SCALE*index;
//		context.beforeReadAccess(arr, address);
//		return context.onReadAccess(arr, arr[index], address);
//	}
//	static public int onArrayReadAccessStrongIso( int[] arr, int index, ArrayReadFieldAccess objArr, Context context) {
//		int address = INT_ARR_BASE + INT_ARR_SCALE*index;
//		context.beforeReadAccess(arr, address);
//		return context.onReadAccess(arr, arr[index], address);
//	}
//	static public long onArrayReadAccessStrongIso( long[] arr, int index, ArrayReadFieldAccess objArr, Context context) {
//		int address = LONG_ARR_BASE + LONG_ARR_SCALE*index;
//		context.beforeReadAccess(arr, address);
//		return context.onReadAccess(arr, arr[index], address);
//	}
//	static public float onArrayReadAccessStrongIso( float[] arr, int index, ArrayReadFieldAccess objArr, Context context) {
//		int address = FLOAT_ARR_BASE + FLOAT_ARR_SCALE*index;
//		context.beforeReadAccess(arr, address);
//		return context.onReadAccess(arr, arr[index], address);
//	}
//	static public double onArrayReadAccessStrongIso( double[] arr, int index, ArrayReadFieldAccess objArr, Context context) {
//		int address = DOUBLE_ARR_BASE + DOUBLE_ARR_SCALE*index;
//		context.beforeReadAccess(arr, address);
//		return context.onReadAccess(arr, arr[index], address);
//	}
	
	
	static public Object onArrayReadAccessStrongIso( Object[] arr, int index, long objArrAdd, Context context) {
		System.out.println("ERROR ARRAYS NOT SUPPORTED");
		int address = OBJECT_ARR_BASE + OBJECT_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public byte onArrayReadAccessStrongIso( byte[] arr, int index, long objArrAdd, Context context) {
		System.out.println("ERROR ARRAYS NOT SUPPORTED");
		int address = BYTE_ARR_BASE + BYTE_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public char onArrayReadAccessStrongIso( char[] arr, int index, long objArrAdd, Context context) {
		System.out.println("ERROR ARRAYS NOT SUPPORTED");
		int address = CHAR_ARR_BASE + CHAR_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public short onArrayReadAccessStrongIso( short[] arr, int index, long objArrAdd, Context context) {
		System.out.println("ERROR ARRAYS NOT SUPPORTED");
		int address = SHORT_ARR_BASE + SHORT_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public int onArrayReadAccessStrongIso( int[] arr, int index, long objArrAdd, Context context) {
		System.out.println("ERROR ARRAYS NOT SUPPORTED");
		int address = INT_ARR_BASE + INT_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public long onArrayReadAccessStrongIso( long[] arr, int index, long objArrAdd, Context context) {
		System.out.println("ERROR ARRAYS NOT SUPPORTED");
		int address = LONG_ARR_BASE + LONG_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public float onArrayReadAccessStrongIso( float[] arr, int index, long objArrAdd, Context context) {
		System.out.println("ERROR ARRAYS NOT SUPPORTED");
		int address = FLOAT_ARR_BASE + FLOAT_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public double onArrayReadAccessStrongIso( double[] arr, int index, long objArrAdd, Context context) {
		System.out.println("ERROR ARRAYS NOT SUPPORTED");
		int address = DOUBLE_ARR_BASE + DOUBLE_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	

	static public Object onArrayReadAccess( Object[] arr, int index, Context context) {
		int address = OBJECT_ARR_BASE + OBJECT_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public byte onArrayReadAccess( byte[] arr, int index, Context context) {
		int address = BYTE_ARR_BASE + BYTE_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public char onArrayReadAccess( char[] arr, int index, Context context) {
		int address = CHAR_ARR_BASE + CHAR_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public short onArrayReadAccess( short[] arr, int index, Context context) {
		int address = SHORT_ARR_BASE + SHORT_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public int onArrayReadAccess( int[] arr, int index, Context context) {
		int address = INT_ARR_BASE + INT_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public long onArrayReadAccess( long[] arr, int index, Context context) {
		int address = LONG_ARR_BASE + LONG_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public float onArrayReadAccess( float[] arr, int index, Context context) {
		int address = FLOAT_ARR_BASE + FLOAT_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	static public double onArrayReadAccess( double[] arr, int index, Context context) {
		int address = DOUBLE_ARR_BASE + DOUBLE_ARR_SCALE*index;
		context.beforeReadAccess(arr, address);
		return context.onReadAccess(arr, arr[index], address);
	}
	
	static public <T> void onArrayWriteAccess( T[] arr,  int index, T value, Context context) {
		T t = arr[index]; // dummy access just to check the index in range
		context.onWriteAccess(arr, value, OBJECT_ARR_BASE + OBJECT_ARR_SCALE*index);
	}
	static public void onArrayWriteAccess( byte[] arr, int index, byte value, Context context) {
		byte t = arr[index]; // dummy access just to check the index in range
		context.onWriteAccess(arr, value, BYTE_ARR_BASE + BYTE_ARR_SCALE*index);
	}
	static public void onArrayWriteAccess( char[] arr, int index, char value, Context context) {
		char t = arr[index]; // dummy access just to check the index in range
		context.onWriteAccess(arr, value, CHAR_ARR_BASE + CHAR_ARR_SCALE*index);
	}
	static public void onArrayWriteAccess( short[] arr, int index, short value, Context context) {
		short t = arr[index]; // dummy access just to check the index in range
		context.onWriteAccess(arr, value, SHORT_ARR_BASE + SHORT_ARR_SCALE*index);
	}
	static public void onArrayWriteAccess( int[] arr, int index, int value, Context context) {
		int t = arr[index]; // dummy access just to check the index in range
		context.onWriteAccess(arr, value, INT_ARR_BASE + INT_ARR_SCALE*index);
	}
	static public void onArrayWriteAccess( long[] arr, int index, long value, Context context) {
		long t = arr[index]; // dummy access just to check the index in range
		context.onWriteAccess(arr, value, LONG_ARR_BASE + LONG_ARR_SCALE*index);
	}
	static public void onArrayWriteAccess( float[] arr, int index, float value, Context context) {
		float t = arr[index]; // dummy access just to check the index in range
		context.onWriteAccess(arr, value, FLOAT_ARR_BASE + FLOAT_ARR_SCALE*index);
	}
	static public void onArrayWriteAccess( double[] arr, int index, double value, Context context) {
		double t = arr[index]; // dummy access just to check the index in range
		context.onWriteAccess(arr, value, DOUBLE_ARR_BASE + DOUBLE_ARR_SCALE*index);
	}
	
}
