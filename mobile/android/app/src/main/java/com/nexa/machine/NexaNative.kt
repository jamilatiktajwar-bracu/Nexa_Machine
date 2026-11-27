package com.nexa.machine

object NexaNative {
    init { System.loadLibrary("nexa_android_jni") }
    external fun createContext(): Long
    external fun destroyContext(context: Long)
    external fun createSession(context: Long, name: String, ramBytes: Int): Long
    external fun createRecoveringSession(context: Long, name: String, ramBytes: Int, vmPath: String, diskDirectory: String): Long
    external fun loadAssembly(context: Long, session: Long, source: String): Int
    external fun run(context: Long, session: Long, budget: Int): Int
    external fun start(context: Long, session: Long): Int
    external fun step(context: Long, session: Long): Int
    external fun pause(context: Long, session: Long): Int
    external fun resume(context: Long, session: Long): Int
    external fun reset(context: Long, session: Long): Int
    external fun stop(context: Long, session: Long): Int
    external fun console(context: Long, session: Long): String
    external fun state(context: Long, session: Long): Int
    external fun sendInput(context: Long, session: Long, input: String): Int
    external fun lastError(context: Long): String
    external fun attachDisk(context: Long, session: Long, path: String): Int
    external fun detachDisk(context: Long, session: Long): Int
    external fun saveSnapshot(context: Long, session: Long, root: String): Int
    external fun restoreSnapshot(context: Long, session: Long, root: String): Int
    external fun recoveryReport(context: Long): String
}
