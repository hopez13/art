
public class SimpleTests {

    public static class TestGuardSkips extends VarHandleUnitTest {
        public boolean checkGuard() {
            return false;
        }
        public void doTest() {
            throw new IllegalStateException("Not reachable");
        }
        public static void main(String[] args) {
            new TestGuardSkips().run();
        }
    }

    public static class TestEqualsOK extends VarHandleUnitTest {
        public void doTest() {
            assertEquals(true, true);
        }
    }

    public static class TestEqualsOK2 extends VarHandleUnitTest {
        public void doTest() {
            assertEquals(true, false);
        }
    }

    public static class TestExceptionsOK extends VarHandleUnitTest {
        public void doTest() {
            expectExceptionOfType(NullPointerException.class);
            throw new NullPointerException();
        }
    }

    public static class TestExceptionsFail extends VarHandleUnitTest {
        public void doTest() {
            expectExceptionOfType(NullPointerException.class);
        }
    }

    public static class TestUnexpectedExceptionsFail extends VarHandleUnitTest {
        public void doTest() {
            throw new IllegalArgumentException();
        }
    }

    public static void main(String[] args) {
        new TestGuardSkips().run();
        new TestEqualsOK().run();
        new TestEqualsOK2().run();
        new TestExceptionsOK().run();
        new TestExceptionsFail().run();
        new TestUnexpectedExceptionsFail().run();
        VarHandleUnitTest.DEFAULT_COLLECTOR.printSummary();
    }
}
