package org.deuce.transform.asm.type;

import org.deuce.objectweb.asm.Opcodes;
import org.deuce.objectweb.asm.Type;

public class DoubleTypeCodeResolver extends TypeCodeResolver {

	public DoubleTypeCodeResolver(Type type) {
		super(type);
	}

	@Override
	public int loadCode() {
		return Opcodes.DLOAD;
	}

	@Override
	public int returnCode() {
		return Opcodes.DRETURN;
	}

	@Override
	public int storeCode() {
		return Opcodes.DSTORE;
	}

	@Override
	public int nullValueCode() {
		return Opcodes.DCONST_0;
	}

	@Override
	public int localSize() {
		return 2; // 64 bit
	}
}
