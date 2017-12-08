/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class VarHandleTypeConversionTests {
    public static class VoidReturnTypeTest extends VarHandleUnitTest {
        private int i;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = VoidReturnTypeTest.class;
                vh = MethodHandles.lookup().findVarHandle(cls, "i", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            // Void is always okay for a return type.
            vh.setVolatile(this, 33);
            vh.get(this);
            vh.compareAndSet(this, 33, 44);
            vh.compareAndSet(this, 27, 16);
            vh.weakCompareAndSet(this, 17, 19);
            vh.getAndSet(this, 200000);
            vh.getAndBitwiseXor(this, 0x5a5a5a5a);
            vh.getAndAdd(this, 99);
        }

        public static void main(String[] args) {
            new VoidReturnTypeTest().run();
        }
    }

    //
    // Tests that a null reference as a boxed primitive type argument
    // throws a NullPointerException. These vary the VarHandle type
    // with each primitive for coverage.
    //

    public static class BoxedNullBooleanThrowsNPETest extends VarHandleUnitTest {
        private static boolean z;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullBooleanThrowsNPETest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "z", boolean.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            Boolean newValue = null;
            expectExceptionOfType(NullPointerException.class);
            vh.getAndSet(newValue);
        }

        public static void main(String[] args) {
            new BoxedNullBooleanThrowsNPETest().run();
        }
    }

    public static class BoxedNullByteThrowsNPETest extends VarHandleUnitTest {
        private byte b;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullByteThrowsNPETest.class;
                vh = MethodHandles.lookup().findVarHandle(cls, "b", byte.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            Byte newValue = null;
            expectExceptionOfType(NullPointerException.class);
            vh.getAndSet(this, newValue);
        }

        public static void main(String[] args) {
            new BoxedNullByteThrowsNPETest().run();
        }
    }

    public static class BoxedNullCharacterThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(char[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            char[] values = new char[3];
            Character newValue = null;
            expectExceptionOfType(NullPointerException.class);
            vh.getAndSet(values, 0, newValue);
        }

        public static void main(String[] args) {
            new BoxedNullCharacterThrowsNPETest().run();
        }
    }

    public static class BoxedNullShortThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullShortThrowsNPETest.class;
                vh = MethodHandles.byteArrayViewVarHandle(short[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[2 * Short.SIZE];
            int index = VarHandleUnitTestHelpers.alignedOffset_short(bytes, 0);
            Short newValue = null;
            expectExceptionOfType(NullPointerException.class);
            vh.set(bytes, index, newValue);
        }

        public static void main(String[] args) {
            new BoxedNullShortThrowsNPETest().run();
        }
    }

    public static class BoxedNullIntegerThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[2 * Integer.SIZE];
            int index = VarHandleUnitTestHelpers.alignedOffset_int(bytes, 0);
            Integer newValue = null;
            expectExceptionOfType(NullPointerException.class);
            vh.setVolatile(bytes, index, newValue);
        }

        public static void main(String[] args) {
            new BoxedNullIntegerThrowsNPETest().run();
        }
    }

    public static class BoxedNullLongThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullLongThrowsNPETest.class;
                vh = MethodHandles.byteBufferViewVarHandle(long[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer bb = ByteBuffer.allocateDirect(2 * Long.SIZE);
            int index = VarHandleUnitTestHelpers.alignedOffset_long(bb, 0);
            Long newValue = null;
            expectExceptionOfType(NullPointerException.class);
            vh.getAndAdd(bb, index, newValue);
        }

        public static void main(String[] args) {
            new BoxedNullLongThrowsNPETest().run();
        }
    }

    public static class BoxedNullFloatThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullFloatThrowsNPETest.class;
                vh = MethodHandles.byteBufferViewVarHandle(float[].class, ByteOrder.BIG_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer bb = ByteBuffer.allocate(2 * Float.SIZE);
            int index = VarHandleUnitTestHelpers.alignedOffset_float(bb, 0);
            Float newValue = null;
            expectExceptionOfType(NullPointerException.class);
            vh.set(bb, index, newValue);
        }

        public static void main(String[] args) {
            new BoxedNullFloatThrowsNPETest().run();
        }
    }

    public static class BoxedNullDoubleThrowsNPETest extends VarHandleUnitTest {
        private double d;
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(double[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[3 * Double.SIZE];
            int offset = 1;
            ByteBuffer bb = ByteBuffer.wrap(bytes, offset, bytes.length - offset);
            int index = VarHandleUnitTestHelpers.alignedOffset_double(bb, 0);
            Double newValue = null;
            expectExceptionOfType(NullPointerException.class);
            vh.set(bb, index, newValue);
        }

        public static void main(String[] args) {
            new BoxedNullDoubleThrowsNPETest().run();
        }
    }

    public static void main(String[] args) {
        VoidReturnTypeTest.main(args);
        BoxedNullBooleanThrowsNPETest.main(args);
        BoxedNullByteThrowsNPETest.main(args);
        BoxedNullCharacterThrowsNPETest.main(args);
        BoxedNullShortThrowsNPETest.main(args);
        BoxedNullIntegerThrowsNPETest.main(args);
        BoxedNullLongThrowsNPETest.main(args);
        BoxedNullFloatThrowsNPETest.main(args);
        BoxedNullDoubleThrowsNPETest.main(args);
    }
}
