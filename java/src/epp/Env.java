package epp;

import java.util.HashMap;
import java.util.Map;

public final class Env {
  private final Map<String, Value> vars = new HashMap<>();
  private final Env parent;

  public Env(Env parent) {
    this.parent = parent;
  }

  public boolean get(String name, Holder<Value> out) {
    if (vars.containsKey(name)) {
      out.value = vars.get(name);
      return true;
    }
    if (parent != null) return parent.get(name, out);
    return false;
  }

  public void setLocal(String name, Value v) {
    vars.put(name, v);
  }

  public boolean assign(String name, Value v) {
    if (vars.containsKey(name)) {
      vars.put(name, v);
      return true;
    }
    if (parent != null) return parent.assign(name, v);
    return false;
  }
}

