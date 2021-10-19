package org.deuce;

import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.RetentionPolicy.CLASS;

import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/**
 *
 * @author	Guy Korland
 * @since 1.0
 */
@Target(METHOD)
@Retention(CLASS)
public @interface NTAtomic {
	int retries() default Integer.MAX_VALUE;
	String metainf() default "";
}
