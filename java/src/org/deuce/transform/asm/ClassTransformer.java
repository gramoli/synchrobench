package org.deuce.transform.asm;

import java.lang.annotation.Annotation;
import java.util.LinkedList;

import org.deuce.objectweb.asm.AnnotationVisitor;
import org.deuce.objectweb.asm.FieldVisitor;
import org.deuce.objectweb.asm.MethodVisitor;
import org.deuce.objectweb.asm.Opcodes;
import org.deuce.objectweb.asm.Type;
import org.deuce.objectweb.asm.commons.Method;
import org.deuce.transaction.Context;
import org.deuce.transaction.strongiso.field.ReadFieldAccess;
import org.deuce.transform.Exclude;
import org.deuce.transform.UseStrongIso;
import org.deuce.transform.asm.method.MethodTransformer;
import org.deuce.transform.asm.method.StaticMethodTransformer;
import org.deuce.transform.asm.type.TypeCodeResolver;
import org.deuce.transform.asm.type.TypeCodeResolverFactory;
import org.deuce.transform.util.Util;

@Exclude
public class ClassTransformer extends ByteCodeVisitor implements FieldsHolder{

	final private static String ENUM_DESC = Type.getInternalName(Enum.class); 
	
	private boolean exclude = false;
	private boolean visitclinit = false;
	final private LinkedList<Field> fields = new LinkedList<Field>();
	private String staticField = null;
	
	final static public String EXCLUDE_DESC = Type.getDescriptor(Exclude.class);
	final static private String ANNOTATION_NAME = Type.getInternalName(Annotation.class);
	private boolean isInterface;
	private boolean isEnum;
	private MethodVisitor staticMethod;
	
	private final FieldsHolder fieldsHolder;

	public ClassTransformer( String className, FieldsHolder fieldsHolder){
		super( className);
		this.fieldsHolder = fieldsHolder == null ? this : fieldsHolder;
	}

	@Override
	public void visit(final int version, final int access, final String name,
			final String signature, final String superName, final String[] interfaces) {
		
		fieldsHolder.visit(superName);
		isInterface = (access & Opcodes.ACC_INTERFACE) != 0;
		isEnum = ENUM_DESC.equals(superName);
		
		for(String inter : interfaces){
			if( inter.equals(ANNOTATION_NAME)){
				exclude = true;
				break;
			}

		}
		super.visit(version, access, name, signature, superName, interfaces);
	}

	/**
	 * Checks if the class is marked as {@link Exclude @Exclude}
	 */
	@Override
	public AnnotationVisitor visitAnnotation( String desc, boolean visible) {
		exclude = exclude ? exclude : EXCLUDE_DESC.equals(desc);
		return super.visitAnnotation(desc, visible);
	}

	/**
	 * Creates a new static filed for each existing field.
	 * The field will be statically initialized to hold the field address.   
	 */
	@Override
	public FieldVisitor visitField(int access, String name, String desc, String signature,
			Object value) {

		FieldVisitor fieldVisitor = super.visitField(access, name, desc, signature, value);
		if( exclude)
			return fieldVisitor;
		
		// Define as constant
		int fieldAccess = Opcodes.ACC_FINAL | Opcodes.ACC_PUBLIC | Opcodes.ACC_STATIC | Opcodes.ACC_SYNTHETIC;
		int fieldAccess2 = Opcodes.ACC_PUBLIC | Opcodes.ACC_SYNTHETIC;
		String addressFieldName = Util.getAddressField( name);
		String objectFieldName = Util.getObjectField(name);
		String objectAddressFieldName = Util.getObjectAddressField(name);
		
		final boolean include = (access & Opcodes.ACC_FINAL) == 0;
		final boolean isStatic = (access & Opcodes.ACC_STATIC) != 0;
		if( include){ // include field if not final 
			Field field = new Field(name, addressFieldName);
			fields.add( field);
			if(isStatic)
				staticField = name;
			fieldsHolder.addField( fieldAccess, addressFieldName, Type.LONG_TYPE.getDescriptor(), null);
			if(UseStrongIso.USE_STRONG_ISO) {
				//field = new Field(name, objectFieldName);
				//fields.add( field);
				if(Type.getType(desc).getSort() == Type.ARRAY)
					fieldsHolder.addField( fieldAccess2, objectFieldName, Type.getType(ReadFieldAccess[].class).getDescriptor(), null);
				else
					fieldsHolder.addField( fieldAccess2, objectFieldName, Type.getType(ReadFieldAccess.class).getDescriptor(), null);
				field = new Field(name, objectAddressFieldName);
				fields.add( field);
				field.objName = objectFieldName;
				fieldsHolder.addField( fieldAccess, objectAddressFieldName, Type.LONG_TYPE.getDescriptor(), null);
			}
		}else{
			// If this field is final mark with a negative address.
			fieldsHolder.addField( fieldAccess, addressFieldName, Type.LONG_TYPE.getDescriptor(), -1L);
		}
		
		return fieldVisitor;
	}

	@Override
	public MethodVisitor visitMethod(int access, String name, String desc, String signature,
			String[] exceptions) {

		MethodVisitor originalMethod =  super.visitMethod(access, name, desc, signature, exceptions);
		
		if( exclude)
			return originalMethod;

		final boolean isNative = (access & Opcodes.ACC_NATIVE) != 0;
		if(isNative){
			createNativeMethod(access, name, desc, signature, exceptions);
			return originalMethod;
		}
		
		if( name.equals("<clinit>")) {
			staticMethod = originalMethod;
			visitclinit = true;

			if( isInterface){
				return originalMethod;
			}

			int fieldAccess = Opcodes.ACC_PUBLIC | Opcodes.ACC_STATIC | Opcodes.ACC_SYNTHETIC;
			fieldsHolder.addField( fieldAccess, StaticMethodTransformer.CLASS_BASE,
					Type.getDescriptor(Object.class), null);
			
			MethodVisitor staticMethodVisitor = fieldsHolder.getStaticMethodVisitor();
			return createStaticMethodTransformer( originalMethod, staticMethodVisitor);
		}
		Method newMethod = createNewMethod(name, desc);

		// Create a new duplicate SYNTHETIC method and remove the final marker if has one. 
		MethodVisitor copyMethod =  super.visitMethod((access | Opcodes.ACC_SYNTHETIC) & ~Opcodes.ACC_FINAL, name, newMethod.getDescriptor(),
				signature, exceptions);

		return new MethodTransformer( originalMethod, copyMethod, className,
				access, name, desc, newMethod, fieldsHolder);
	}

	/**
	 * Build a dummy method that delegates the call to the native method
	 */
	private void createNativeMethod(int access, String name, String desc,
			String signature, String[] exceptions) {
		Method newMethod = createNewMethod(name, desc);
		final int newAccess = access & ~Opcodes.ACC_NATIVE;
		MethodVisitor copyMethod =  super.visitMethod(newAccess | Opcodes.ACC_SYNTHETIC, name, newMethod.getDescriptor(),
				signature, exceptions);
		copyMethod.visitCode();
		
		// load the arguments before calling the original method
		final boolean isStatic = (access & ~Opcodes.ACC_STATIC) != 0;
		int place = 0; // place on the stack
		if(!isStatic){
			copyMethod.visitVarInsn(Opcodes.ALOAD, 0); // load this
			place = 1;
		}
		
		Type[] argumentTypes = newMethod.getArgumentTypes();
		for(int i=0 ; i<(argumentTypes.length-1) ; ++i){
			Type type = argumentTypes[i];
			copyMethod.visitVarInsn(type.getOpcode(Opcodes.ILOAD), place);
			place += type.getSize();
		}
		
		// call the original method
		copyMethod.visitMethodInsn(isStatic ? Opcodes.INVOKESTATIC : Opcodes.INVOKEVIRTUAL, className, name, desc);
		TypeCodeResolver returnReolver = TypeCodeResolverFactory.getReolver(newMethod.getReturnType());
		if( returnReolver == null) {
			copyMethod.visitInsn( Opcodes.RETURN); // return;
		}else {
			copyMethod.visitInsn(returnReolver.returnCode());
		}
		copyMethod.visitMaxs(1, 1);
		copyMethod.visitEnd();
	}

	@Override
	public void visitEnd() {
		//Didn't see any static method till now, so creates one.
		if(!exclude){
			super.visitAnnotation(EXCLUDE_DESC, false);
			if( !visitclinit && fields.size() > 0) { // creates a new <clinit> in case we didn't see one already. 

				//TODO avoid creating new static method in case of external fields holder
				visitclinit = true;
				MethodVisitor method = visitMethod(Opcodes.ACC_STATIC, "<clinit>", "()V", null, null);
				method.visitCode();
				method.visitInsn(Opcodes.RETURN);
				method.visitMaxs(100, 100); // TODO set the right value
				method.visitEnd();

			}
			if(isEnum){ // Build a dummy ordinal() method
				MethodVisitor ordinalMethod = 
					super.visitMethod(Opcodes.ACC_PUBLIC | Opcodes.ACC_SYNTHETIC, "ordinal", "(Lorg/deuce/transaction/Context;)I", null, null);
				ordinalMethod.visitCode();
				ordinalMethod.visitVarInsn(Opcodes.ALOAD, 0);
				ordinalMethod.visitMethodInsn(Opcodes.INVOKEVIRTUAL, className, "ordinal", "()I");
				ordinalMethod.visitInsn(Opcodes.IRETURN);
				ordinalMethod.visitMaxs(1, 2);
				ordinalMethod.visitEnd();
			}
		}
		super.visitEnd();
		fieldsHolder.close();
	}

	private StaticMethodTransformer createStaticMethodTransformer(MethodVisitor originalMethod, MethodVisitor staticMethod){
		return new StaticMethodTransformer( originalMethod, staticMethod, fields, staticField,
				className, fieldsHolder.getFieldsHolderName(className));
	}
	
	public static Method createNewMethod(String name, String desc) {
		Method method = new Method( name, desc);
		Type[] arguments = method.getArgumentTypes();

		Type[] newArguments = new Type[ arguments.length + 1];
		System.arraycopy( arguments, 0, newArguments, 0, arguments.length);
		newArguments[newArguments.length - 1] = Context.CONTEXT_TYPE; // add as a constant

		return new Method( name, method.getReturnType(), newArguments);
	}
	
	@Override
	public void addField(int fieldAccess, String addressFieldName, String desc, Object value){
		super.visitField( fieldAccess, addressFieldName, desc, null, value);
	}
	
	@Override
	public void close(){
	}
	
	@Override
	public MethodVisitor getStaticMethodVisitor(){
		return staticMethod;
	}
	
	@Override
	public String getFieldsHolderName(String owner){
		return owner;
	}

	@Override
	public void visit(String superName) {
		//nothing to do
	}
}
