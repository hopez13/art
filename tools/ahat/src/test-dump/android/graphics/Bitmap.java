package android.graphics;

/**
 *
 */
public final class Bitmap {

  private final long mNativePtr;

  private int mWidth;
  private int mHeight;

  public Bitmap(int width, int height, long nativePtr, byte[] buffer) {
    this.mWidth = width;
    this.mHeight = height;
    this.mNativePtr = nativePtr;
    dumpData.add(nativePtr, buffer);
  }

  private static final class DumpData {
    private int count;
    private int format;
    private long[] natives;
    private byte[][] buffers;
    private int max;

    public DumpData(int format, int max) {
      this.max = max;
      this.format = format;
      this.natives = new long[max];
      this.buffers = new byte[max][];
      this.count = 0;
    }

    public void add(long nativePtr, byte[] buffer) {
      natives[count] = nativePtr;
      buffers[count] = buffer;
      count = (count >= max) ? max : count + 1;
    }
  };

  private static DumpData dumpData = new DumpData(1, 10);
}
