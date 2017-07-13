import java.text.NumberFormat;
import java.util.Currency;
import java.util.Locale;


public class Main {
  public static void main(String[] args) {
    testCharSequenceCharAt();
    testCurrency();
  }

  private static void testCharSequenceCharAt() {
    String hello = "testCharSequenceCharAt succeeded";

    for (int i = 0; i < hello.length(); ++i) {
      System.out.print($noinline$getCharAt(hello, i));
    }
    System.out.println();

    // Test out of bounds
    for (int i = 10000; i < 10100; ++i) {
      try {
        System.out.print($noinline$getCharAt(hello, i));
      } catch (IndexOutOfBoundsException e) {}
    }
  }

  // Forbid inlining here so that the optimizing compiler doesn't get overly clever
  // -- If it inlines, it will know the CharSequence is actually a string and replace
  //    it with the String#charAt intrisic which we do not want.
  //
  // It *must* call mirror::String::charAt through JNI.
  private static char $noinline$getCharAt(CharSequence cs, int index) {
    return cs.charAt(index);
  }

  // Indirectly test CharSequence.charAt since the currency string code
  // always ends up calling it.
  private static void testCurrency() {
    testCurrencyWithLocale(Locale.US);
    testCurrencyWithLocale(Locale.UK);
    testCurrencyWithLocale(Locale.FRANCE);
    testCurrencyWithLocale(Locale.GERMANY);
    testCurrencyWithLocale(Locale.CHINA);
  }

  private static void testCurrencyWithLocale(Locale locale) {
    System.out.println(locale);
    Currency currency  = Currency.getInstance(locale);
    String currencySymbol = currency.getSymbol();
    System.out.println("Currency symbol: " + currencySymbol);
    NumberFormat fmt = NumberFormat.getCurrencyInstance(locale);
    System.out.println(fmt.format(120.00));
  }

  private static void testCurrencyWithLocale(String lang, String country) {
    Locale locale = new Locale(lang, country);

    testCurrencyWithLocale(locale);
  }


}
