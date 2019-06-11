class Test {

    public long simple_method_mul(long jj, long ii) {
        jj = ii * jj;
        return jj;
    }
	
	public float simple_method_rem(float jj, float kk) {
        jj = kk % jj;
        jj = jj % kk;
        return jj;
    }
	
	
	 public int simple_method_int(int jj, int kk) {
        jj = kk | jj;
        return jj;
    }
	
	public long simple_method_xor(long jj, long kk) {
        jj = ~kk;
        return jj;
    }


}
