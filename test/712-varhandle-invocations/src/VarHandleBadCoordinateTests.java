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
import java.lang.invoke.WrongMethodTypeException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class VarHandleBadCoordinateTests {
    public static class FieldCoordinateTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        public static class A {
            public byte field;
        }

        public static class B extends A {
            private byte other_field;
        }

        public static class C {
        }

        static {
            try {
                vh = MethodHandles.lookup().findVarHandle(A.class, "field", byte.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            vh.compareAndSet(new A(), (byte) 0, (byte) 3);
            vh.compareAndSet(new B(), (byte) 0, (byte) 3);
            expectExceptionOfType(ClassCastException.class);
            vh.compareAndSet(new C(), (byte) 0, (byte) 3);
            expectExceptionOfType(ClassCastException.class);
            vh.compareAndSet(0xbad0bad0, (byte) 0, (byte) 3);
            expectExceptionOfType(ClassCastException.class);
            vh.compareAndSet(0xbad0bad0, (byte) 0, Integer.MAX_VALUE);
            expectExceptionOfType(ClassCastException.class);
            vh.compareAndSet(0xbad0bad0, (byte) 0);
            expectExceptionOfType(ClassCastException.class);
            vh.compareAndSet(new A(), (byte) 0, Integer.MAX_VALUE);
            expectExceptionOfType(NullPointerException.class);
            vh.compareAndSet((A) null, (byte) 0, (byte) 3);
        }

        public static void main(String[] args) {
            new FieldCoordinateTypeTest().run();
        }
    }

    public static class ArrayElementOutOfBoundsIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            long[] values = new long[33];
            expectExceptionOfType(ArrayIndexOutOfBoundsException.class);
            vh.get(values, -1);
            expectExceptionOfType(ArrayIndexOutOfBoundsException.class);
            vh.get(values, values.length);
            expectExceptionOfType(ArrayIndexOutOfBoundsException.class);
            vh.get(values, Integer.MAX_VALUE - 1);
        }

        public static void main(String[] args) {
            new ArrayElementOutOfBoundsIndexTest().run();
        }
    }

    public static class ArrayElementBadIndexTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            long[] values = new long[33];
            vh.set(values, Integer.valueOf(3), Long.MIN_VALUE);
            vh.set(values, Byte.valueOf((byte) 0), Long.MIN_VALUE);
            expectExceptionOfType(WrongMethodTypeException.class);
            vh.set(values, 3.3f, Long.MAX_VALUE);
        }

        public static void main(String[] args) {
            new ArrayElementBadIndexTypeTest().run();
        }
    }

    public static class ArrayElementNullArrayTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            long[] values = null;
            expectExceptionOfType(WrongMethodTypeException.class);
            vh.get(values);
        }

        public static void main(String[] args) {
            new ArrayElementNullArrayTest().run();
        }
    }

    public static class ArrayElementWrongArrayTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            expectExceptionOfType(ClassCastException.class);
            vh.get(new char[10], 0);
        }

        public static void main(String[] args) {
            new ArrayElementWrongArrayTypeTest().run();
        }
    }

    public static class ArrayElementMissingIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            long[] values = new long[33];
            expectExceptionOfType(WrongMethodTypeException.class);
            vh.get(values);
        }

        public static void main(String[] args) {
            new ArrayElementMissingIndexTest().run();
        }
    }

    public static class ByteArrayViewOutOfBoundsIndexTest extends VarHandleUnitTest {
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
            byte[] bytes = new byte[16];
            expectExceptionOfType(IndexOutOfBoundsException.class);
            vh.get(bytes, -1);
            expectExceptionOfType(IndexOutOfBoundsException.class);
            vh.get(bytes, bytes.length);
            expectExceptionOfType(IndexOutOfBoundsException.class);
            vh.get(bytes, Integer.MAX_VALUE - 1);
            expectExceptionOfType(IndexOutOfBoundsException.class);
            vh.get(bytes, bytes.length - Integer.SIZE + 1);
            assertNoExceptionsExpected();
            vh.get(bytes, bytes.length - Integer.SIZE);
        }

        public static void main(String[] args) {
            new ByteArrayViewOutOfBoundsIndexTest().run();
        }
    }

    public static class ByteArrayViewUnalignedAccessesIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        private void maybeExpectIllegalStateException(boolean isAligned) {
            assertNoExceptionsExpected();
            if (!isAligned) {
                expectExceptionOfType(IllegalStateException.class);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[33];

            int alignedIndex = VarHandleUnitTestHelpers.alignedOffset_int(bytes, 0);
            for (int i = alignedIndex; i < Integer.SIZE; ++i) {
                // No exceptions are expected for GET and SET
                // accessors irrespective of the access alignment.
                vh.set(bytes, i, 380);
                vh.get(bytes, i);
                // Other accessors raise an IllegalStateException if
                // the access is unaligned.
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.compareAndExchange(bytes, i, 777, 320);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.compareAndExchangeAcquire(bytes, i, 320, 767);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.compareAndExchangeRelease(bytes, i, 767, 321);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.compareAndSet(bytes, i, 767, 321);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAcquire(bytes, i);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndAdd(bytes, i, 117);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndAddAcquire(bytes, i, 117);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndAddRelease(bytes, i, 117);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseAnd(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseAndAcquire(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseAndRelease(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseOr(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseOrAcquire(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseOrRelease(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseXor(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseXorAcquire(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndBitwiseXorRelease(bytes, i, 118);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndSet(bytes, i, 117);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndSetAcquire(bytes, i, 117);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getAndSetRelease(bytes, i, 117);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getOpaque(bytes, i);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.getVolatile(bytes, i);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.setOpaque(bytes, i, 777);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.setRelease(bytes, i, 319);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.setVolatile(bytes, i, 787);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.weakCompareAndSet(bytes, i, 787, 340);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.weakCompareAndSetAcquire(bytes, i, 787, 340);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.weakCompareAndSetPlain(bytes, i, 787, 340);
                maybeExpectIllegalStateException(i == alignedIndex);
                vh.weakCompareAndSetRelease(bytes, i, 787, 340);
            }
        }

        public static void main(String[] args) {
            new ByteArrayViewUnalignedAccessesIndexTest().run();
        }
    }

    public static class ByteArrayViewBadIndexTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[16];
            // Boxed index goes through argument conversion so no exception expected.
            vh.get(bytes, Integer.valueOf(3));
            vh.get(bytes, Short.valueOf((short) 3));

            expectExceptionOfType(WrongMethodTypeException.class);
            vh.get(bytes, System.out);
        }

        public static void main(String[] args) {
            new ByteArrayViewBadIndexTypeTest().run();
        }
    }

    public static class ByteArrayViewMissingIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[16];
            expectExceptionOfType(WrongMethodTypeException.class);
            vh.get(bytes);
        }

        public static void main(String[] args) {
            new ByteArrayViewMissingIndexTest().run();
        }
    }

    public static class ByteArrayViewBadByteArrayTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = null;
            expectExceptionOfType(NullPointerException.class);
            vh.get(bytes, Integer.valueOf(3));
            expectExceptionOfType(WrongMethodTypeException.class);
            vh.get(System.err, Integer.valueOf(3));
        }

        public static void main(String[] args) {
            new ByteArrayViewBadByteArrayTest().run();
        }
    }

    public static class ByteBufferViewOutOfBoundsIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(float[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer [] buffers = new ByteBuffer [] {
                ByteBuffer.allocateDirect(16),
                ByteBuffer.allocate(37),
                ByteBuffer.wrap(new byte[27], 3, 27 - 3)
            };
            for (ByteBuffer buffer : buffers) {
                expectExceptionOfType(IndexOutOfBoundsException.class);
                vh.get(buffer, -1);
                expectExceptionOfType(IndexOutOfBoundsException.class);
                vh.get(buffer, buffer.limit());
                expectExceptionOfType(IndexOutOfBoundsException.class);
                vh.get(buffer, Integer.MAX_VALUE - 1);
                expectExceptionOfType(IndexOutOfBoundsException.class);
                vh.get(buffer, buffer.limit() - Integer.SIZE + 1);
                assertNoExceptionsExpected();
                vh.get(buffer, buffer.limit() - Integer.SIZE);
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewOutOfBoundsIndexTest().run();
        }
    }


    public static class ByteBufferViewUnalignedAccessesIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        private void maybeExpectIllegalStateException(boolean isAligned) {
            assertNoExceptionsExpected();
            if (!isAligned) {
                expectExceptionOfType(IllegalStateException.class);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer [] buffers = new ByteBuffer [] {
                ByteBuffer.allocateDirect(16),
                ByteBuffer.allocate(37),
                ByteBuffer.wrap(new byte[27], 3, 27 - 3)
            };

            for (ByteBuffer buffer : buffers) {
                int alignedIndex = VarHandleUnitTestHelpers.alignedOffset_int(buffer, 0);
                for (int i = alignedIndex; i < Integer.SIZE; ++i) {
                    // No exceptions are expected for GET and SET
                    // accessors irrespective of the access alignment.
                    assertNoExceptionsExpected();
                    vh.set(buffer, i, 380);
                    vh.get(buffer, i);
                    // Other accessors raise an IllegalStateException if
                    // the access is unaligned.
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.compareAndExchange(buffer, i, 777, 320);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.compareAndExchangeAcquire(buffer, i, 320, 767);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.compareAndExchangeRelease(buffer, i, 767, 321);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.compareAndSet(buffer, i, 767, 321);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAcquire(buffer, i);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndAdd(buffer, i, 117);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndAddAcquire(buffer, i, 117);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndAddRelease(buffer, i, 117);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseAnd(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseAndAcquire(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseAndRelease(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseOr(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseOrAcquire(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseOrRelease(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseXor(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseXorAcquire(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndBitwiseXorRelease(buffer, i, 118);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndSet(buffer, i, 117);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndSetAcquire(buffer, i, 117);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getAndSetRelease(buffer, i, 117);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getOpaque(buffer, i);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.getVolatile(buffer, i);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.setOpaque(buffer, i, 777);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.setRelease(buffer, i, 319);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.setVolatile(buffer, i, 787);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.weakCompareAndSet(buffer, i, 787, 340);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.weakCompareAndSetAcquire(buffer, i, 787, 340);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.weakCompareAndSetPlain(buffer, i, 787, 340);
                    maybeExpectIllegalStateException(i == alignedIndex);
                    vh.weakCompareAndSetRelease(buffer, i, 787, 340);
                }
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewUnalignedAccessesIndexTest().run();
        }
    }

    public static class ByteBufferViewBadIndexTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer [] buffers = new ByteBuffer [] {
                ByteBuffer.allocateDirect(16),
                ByteBuffer.allocate(16),
                ByteBuffer.wrap(new byte[32], 4, 32 - 4)
            };

            for (ByteBuffer buffer : buffers) {
                // Boxed index goes through argument conversion so no exception expected.
                assertNoExceptionsExpected();
                vh.get(buffer, Integer.valueOf(3));
                vh.get(buffer, Short.valueOf((short) 3));
                vh.get(buffer, Byte.valueOf((byte) 7));
                expectExceptionOfType(WrongMethodTypeException.class);
                vh.get(buffer, System.out);
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewBadIndexTypeTest().run();
        }
    }

    public static class ByteBufferViewMissingIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer [] buffers = new ByteBuffer [] {
                ByteBuffer.allocateDirect(16),
                ByteBuffer.allocate(16),
                ByteBuffer.wrap(new byte[32], 4, 32 - 4)
            };
            for (ByteBuffer buffer : buffers) {
                expectExceptionOfType(WrongMethodTypeException.class);
                vh.get(buffer);
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewMissingIndexTest().run();
        }
    }

    public static class ByteBufferViewBadByteBufferTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            if (VarHandleUnitTestHelpers.isRunningOnAndroid()) {
                // The RI does not like this test
                expectExceptionOfType(NullPointerException.class);
                ByteBuffer buffer = null;
                vh.get(buffer, 3);
            }
            expectExceptionOfType(ClassCastException.class);
            vh.get(System.err, 3);
        }

        public static void main(String[] args) {
            new ByteBufferViewBadByteBufferTest().run();
        }
    }

    public static void main(String[] args) {
        FieldCoordinateTypeTest.main(args);

        ArrayElementOutOfBoundsIndexTest.main(args);
        ArrayElementBadIndexTypeTest.main(args);
        ArrayElementNullArrayTest.main(args);
        ArrayElementWrongArrayTypeTest.main(args);
        ArrayElementMissingIndexTest.main(args);

        ByteArrayViewOutOfBoundsIndexTest.main(args);
        ByteArrayViewUnalignedAccessesIndexTest.main(args);
        ByteArrayViewBadIndexTypeTest.main(args);
        ByteArrayViewMissingIndexTest.main(args);
        ByteArrayViewBadByteArrayTest.main(args);

        ByteBufferViewOutOfBoundsIndexTest.main(args);
        ByteBufferViewUnalignedAccessesIndexTest.main(args);
        ByteBufferViewBadIndexTypeTest.main(args);
        ByteBufferViewMissingIndexTest.main(args);
        ByteBufferViewBadByteBufferTest.main(args);
    }
}
