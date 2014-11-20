package org.deuce;

import static java.lang.annotation.ElementType.TYPE;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.RetentionPolicy.CLASS;

import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/**
 * @author	Guy Korland
 * @since 1.0
 */
@Target({FIELD,TYPE})
@Retention(CLASS)
public @interface Immutable {

}
