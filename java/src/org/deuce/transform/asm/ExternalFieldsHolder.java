package org.deuce.transform.asm;

import org.deuce.objectweb.asm.ClassWriter;
import org.deuce.objectweb.asm.MethodAdapter;
import org.deuce.objectweb.asm.MethodVisitor;
import org.deuce.objectweb.asm.Opcodes;

/**
 * Creates a class to hold the fields address, used by the offline instrumentation. 
 * @author guy
 * @since 1.1
 */
public class ExternalFieldsHolder implements FieldsHolder {

	final static private String FIELDS_HOLDER = "DeuceFieldsHolder";
	
	final private ClassWriter classWriter;
	final private String className;
	private ExternalMethodVisitor staticMethod;
	
	public ExternalFieldsHolder(String className){
		this.className = getFieldsHolderName(className);
		this.classWriter = new ClassWriter(ClassWriter.COMPUTE_MAXS | ClassWriter.COMPUTE_FRAMES);
	}
	
	public void visit(String superName){
		String superFieldHolder = ExcludeIncludeStore.exclude(superName) ? "java/lang/Object" : getFieldsHolderName(superName);
		classWriter.visit(Opcodes.V1_6, Opcodes.ACC_PUBLIC + Opcodes.ACC_SUPER, 
				this.className, null, superFieldHolder, null);
		classWriter.visitAnnotation(ClassTransformer.EXCLUDE_DESC, false);
		staticMethod = new ExternalMethodVisitor(classWriter.visitMethod(Opcodes.ACC_STATIC, "<clinit>", "()V", null, null));
		staticMethod.visitCode();
	}
	
	public ClassByteCode getClassByteCode(){
		return new ClassByteCode(className, classWriter.toByteArray());
	}

	@Override
	public void addField(int fieldAccess, String addressFieldName, String desc,
			Object value) {
		classWriter.visitField(fieldAccess, addressFieldName, desc, null, value);
	}
	
	@Override
	public void close(){
		staticMethod.visitEnd();
		classWriter.visitEnd();
	}
	
	@Override
	public MethodVisitor getStaticMethodVisitor(){
		return staticMethod;
	}
	
	@Override
	public String getFieldsHolderName(String owner){
		return owner +  FIELDS_HOLDER;
	}
	
	/**
	* A wrapper method that is used to close the new <clinit>.
	*/
	private static class ExternalMethodVisitor extends MethodAdapter{

		private boolean ended = false;
		
		public ExternalMethodVisitor(MethodVisitor mv) {
			super(mv);
		}

		@Override
		public void visitEnd(){
			if(ended)
				return;

			ended = true;
			super.visitInsn(Opcodes.RETURN);
			super.visitMaxs(1, 1); // Dummy call 
			super.visitEnd();
		}
	}
}
