package testpackage;

public class TestNestedClasses {
    public InnerA innerA = new InnerA();
    public InnerB innerB = new InnerB();
    private int hostVal = 0;

    public int testInnerAccess() {
        ++innerA.val;
        innerA.inc();
        ++innerB.val;
        innerB.inc();
        return innerA.val + innerB.val;
    }

    public void reset() {
        hostVal = 0;
        innerA.val = 0;
        innerB.val = 0;
    }

    private void hostInc() {
        ++hostVal;
    }

    public class InnerA {

        private int val = 0;

        public int testMemberAccess() {
            ++innerB.val;
            innerB.inc();
            return innerB.val;
        }

        public int testHostAccess() {
            ++hostVal;
            hostInc();
            return hostVal;
        }

        private void inc() {
            ++this.val;
        }
    }

    public class InnerB {

        private int val = 0;

        public int testMemberAccess() {
            ++innerA.val;
            innerA.inc();
            return innerA.val;
        }

        public int testHostAccess() {
            ++hostVal;
            hostInc();
            return hostVal;
        }

        private void inc() {
            ++this.val;
        }
    }
}
