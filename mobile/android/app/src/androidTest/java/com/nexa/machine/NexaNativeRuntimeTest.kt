package com.nexa.machine

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class NexaNativeRuntimeTest {
    @Test
    fun nativeVmExecutesGuestProgramAndCapturesOutput() {
        val context = NexaNative.createContext()
        assertTrue("Native context creation failed", context != 0L)
        try {
            val session = NexaNative.createSession(context, "JNI runtime test", 4096)
            assertTrue("Native session creation failed", session != 0L)
            assertEquals(0, NexaNative.loadAssembly(context, session, "LOAD A, 65\nOUT A, 1\nHALT\n"))
            assertEquals(0, NexaNative.start(context, session))
            assertEquals(0, NexaNative.run(context, session, 64))
            assertEquals(4, NexaNative.state(context, session))
            assertTrue("Guest console output was empty", NexaNative.console(context, session).isNotEmpty())
        } finally {
            NexaNative.destroyContext(context)
        }
    }

    @Test
    fun nativeDiskAndSnapshotLifecycleWorks() {
        val app = InstrumentationRegistry.getInstrumentation().targetContext
        val context = NexaNative.createContext()
        assertTrue(context != 0L)
        try {
            val session = NexaNative.createSession(context, "JNI disk test", 4096)
            assertTrue(session != 0L)
            val root = File(app.cacheDir, "nexa-jni-test").apply { deleteRecursively(); mkdirs() }
            val disk = File(root, "guest.disk")
            disk.writeBytes(ByteArray(4096))
            assertEquals(0, NexaNative.attachDisk(context, session, disk.absolutePath))
            val snapshot = File(root, "snapshot")
            snapshot.mkdirs()
            assertEquals(0, NexaNative.saveSnapshot(context, session, snapshot.absolutePath))
            assertEquals(0, NexaNative.restoreSnapshot(context, session, snapshot.absolutePath))
            assertEquals(0, NexaNative.detachDisk(context, session))
            root.deleteRecursively()
        } finally {
            NexaNative.destroyContext(context)
        }
    }
}
