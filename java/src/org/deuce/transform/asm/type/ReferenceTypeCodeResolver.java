package org.deuce.transform.asm.type;

import org.deuce.objectweb.asm.Opcodes;
import org.deuce.objectweb.asm.Type;

public class ReferenceTypeCodeResolver extends TypeCodeResolver {

	public ReferenceTypeCodeResolver(Type type) {
		super(type);
	}

	@Override
	public int loadCode() {
		return Opcodes.ALOAD;
	}

	@Override
	public int returnCode() {
		return Opcodes.ARETURN;
	}

	@Override
	public int storeCode() {
		return Opcodes.ASTORE;
	}

	@Override
	public int nullValueCode() {
		return Opcodes.ACONST_NULL;
	}
}
