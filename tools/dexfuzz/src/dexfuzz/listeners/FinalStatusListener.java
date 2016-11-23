package dexfuzz.listeners;

import java.util.List;
import java.util.Map;

import dexfuzz.executors.Executor;

public class FinalStatusListener extends BaseListener {
  private long divergence;
  private long selfDivergent;
  private long architectureSplit;

  @Override
  public void handleDivergences(Map<String, List<Executor>> outputMap) {
    divergence++;
  }

  @Override
  public void handleSelfDivergence() {
    selfDivergent++;
  }

  @Override
  public void handleArchitectureSplit() {
    architectureSplit++;
  }

  public boolean isSuccessful() {
    return (divergence - selfDivergent - architectureSplit) == 0;
  }

}
