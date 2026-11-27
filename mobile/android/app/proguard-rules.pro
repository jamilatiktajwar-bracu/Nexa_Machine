# Preserve JNI entry points called from Kotlin.
-keep class com.nexa.machine.NexaNative { *; }
-keepclasseswithmembers class * { native <methods>; }
