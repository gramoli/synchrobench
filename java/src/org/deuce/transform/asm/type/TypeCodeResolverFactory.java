package org.deuce.transform.asm.type;

import org.deuce.objectweb.asm.Type;

/**
 * Converts {@link Type} to {@link TypeCodeResolver}
 * @author Guy Korland
 *
 * @since 1.0
 */
public class TypeCodeResolverFactory {

	/**
	 * Static factory
	 */
	private TypeCodeResolverFactory() {}

	final private static IntTypeCodeResolver BOOLEAN_RESOLVER = 
		new IntTypeCodeResolver(Type. BOOLEAN_TYPE);
	final private static IntTypeCodeResolver BYTE_RESOLVER = 
		new IntTypeCodeResolver(Type.BYTE_TYPE);
	final private static IntTypeCodeResolver CHAR_RESOLVER = 
		new IntTypeCodeResolver(Type.CHAR_TYPE);
	final private static IntTypeCodeResolver SHORT_RESOLVER = 
		new IntTypeCodeResolver(Type.SHORT_TYPE);
	final private static IntTypeCodeResolver INT_RESOLVER = 
		new IntTypeCodeResolver(Type.INT_TYPE);
	final private static LongTypeCodeResolver LONG_RESOLVER = 
		new LongTypeCodeResolver(Type.LONG_TYPE);
	final private static FloatTypeCodeResolver FLOAT_RESOLVER = 
		new FloatTypeCodeResolver(Type.FLOAT_TYPE);
	final private static DoubleTypeCodeResolver DOUBLE_RESOLVER = 
		new DoubleTypeCodeResolver(Type.DOUBLE_TYPE);

	static public TypeCodeResolver getReolver( Type type) {
		switch( type.getSort()) {
		case Type.VOID:
			return null;
		case Type.BOOLEAN:
			return BOOLEAN_RESOLVER;
		case Type.BYTE:
			return BYTE_RESOLVER;
		case Type.CHAR:
			return CHAR_RESOLVER;
		case Type.SHORT:
			return SHORT_RESOLVER;
		case Type.INT:
			return INT_RESOLVER;
		case Type.LONG:
			return LONG_RESOLVER;
		case Type.FLOAT:
			return FLOAT_RESOLVER;
		case Type.DOUBLE:
			return DOUBLE_RESOLVER;
		default:
			return new ReferenceTypeCodeResolver(type);
		}
	}

}
