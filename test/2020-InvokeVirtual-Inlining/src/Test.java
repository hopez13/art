class Test {

    public long simplemethodMul(long jj, long ii) {
        jj = ii * jj;
        return jj;
    }

    public float simplemethodRem(float jj, float kk) {
        jj = kk % jj;
        jj = jj % kk;
        return jj;
    }

    public int simplemethodInt(int jj, int kk) {
        jj = kk | jj;
        return jj;
    }

    public long simplemethodXor(long jj, long kk) {
        jj = ~kk;
        return jj;
    }

}
