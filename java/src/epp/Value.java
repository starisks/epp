package epp;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

public final class Value {
  public enum Kind { NULL, NUMBER, STRING, BOOL, ARRAY, FUNCTION }

  public final Kind kind;
  public final Double number;
  public final String string;
  public final Boolean bool;
  public final List<Value> array;
  public final FunctionValue function;

  private Value(Kind kind, Double number, String string, Boolean bool, List<Value> array, FunctionValue function) {
    this.kind = kind;
    this.number = number;
    this.string = string;
    this.bool = bool;
    this.array = array;
    this.function = function;
  }

  public static Value nul() { return new Value(Kind.NULL, null, null, null, null, null); }
  public static Value num(double v) { return new Value(Kind.NUMBER, v, null, null, null, null); }
  public static Value str(String v) { return new Value(Kind.STRING, null, v, null, null, null); }
  public static Value bol(boolean v) { return new Value(Kind.BOOL, null, null, v, null, null); }
  public static Value arr(List<Value> v) { return new Value(Kind.ARRAY, null, null, null, v, null); }
  public static Value fun(FunctionValue f) { return new Value(Kind.FUNCTION, null, null, null, null, f); }

  public static String toDisplay(Value v) {
    return switch (v.kind) {
      case NULL -> "null";
      case NUMBER -> {
        double d = v.number;
        if (Math.abs(d - Math.rint(d)) < 1e-9) yield Long.toString(Math.round(d));
        yield Double.toString(d);
      }
      case STRING -> v.string;
      case BOOL -> v.bool ? "true" : "false";
      case ARRAY -> {
        StringBuilder sb = new StringBuilder();
        sb.append("[");
        for (int i = 0; i < v.array.size(); i++) {
          if (i > 0) sb.append(", ");
          sb.append(toDisplay(v.array.get(i)));
        }
        sb.append("]");
        yield sb.toString();
      }
      case FUNCTION -> "<function>";
    };
  }

  public static boolean truthy(Value v) {
    return switch (v.kind) {
      case NULL -> false;
      case BOOL -> v.bool;
      case NUMBER -> v.number != 0.0;
      case STRING -> v.string != null && !v.string.isEmpty();
      case ARRAY -> v.array != null && !v.array.isEmpty();
      case FUNCTION -> true;
    };
  }

  public static boolean equalsValue(Value a, Value b) {
    if (a.kind != b.kind) return false;
    return switch (a.kind) {
      case NULL -> true;
      case NUMBER -> Double.compare(a.number, b.number) == 0;
      case STRING -> Objects.equals(a.string, b.string);
      case BOOL -> Objects.equals(a.bool, b.bool);
      case ARRAY -> Objects.equals(a.array, b.array);
      case FUNCTION -> a.function == b.function;
    };
  }

  public static List<Value> copyArray(List<Value> src) {
    return new ArrayList<>(src);
  }
}

