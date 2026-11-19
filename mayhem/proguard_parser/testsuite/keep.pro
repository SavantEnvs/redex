-keep public class com.example.** { *; }
-keepclassmembers class * extends android.app.Activity {
   public void *(android.view.View);
}
-keepnames class * implements java.io.Serializable
-assumenosideeffects class android.util.Log { public static *** d(...); }
-dontobfuscate
-optimizations !code/simplification/arithmetic,!field/*
-keepattributes *Annotation*,Signature
-if class **.Foo -keep class <1>.Bar { <init>(); }
-whyareyoukeeping class com.example.Foo
