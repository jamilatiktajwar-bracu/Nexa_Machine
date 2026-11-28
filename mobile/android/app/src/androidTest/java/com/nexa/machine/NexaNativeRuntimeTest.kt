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
    fun workspaceSnapshotRestoresIntoFreshNativeContext() {
        val app = InstrumentationRegistry.getInstrumentation().targetContext
        val root = File(app.cacheDir, "nexa-restart-test").apply { deleteRecursively(); mkdirs() }
        val disk = File(root, "guest.disk").apply { writeBytes(ByteArray(4096)) }
        val snapshot = File(root, "snapshot").apply { mkdirs() }
        val firstContext = NexaNative.createContext()
        assertTrue(firstContext != 0L)
        val firstSession = NexaNative.createSession(firstContext, "before restart", 4096)
        assertTrue(firstSession != 0L)
        assertEquals(0, NexaNative.attachDisk(firstContext, firstSession, disk.absolutePath))
        assertEquals(0, NexaNative.loadAssembly(firstContext, firstSession, "LOAD A, 71\nOUT A, 1\nHALT\n"))
        assertEquals(0, NexaNative.start(firstContext, firstSession))
        assertEquals(0, NexaNative.run(firstContext, firstSession, 64))
        val expectedConsole = NexaNative.console(firstContext, firstSession)
        assertTrue(expectedConsole.isNotEmpty())
        assertEquals(0, NexaNative.saveSnapshot(firstContext, firstSession, snapshot.absolutePath))
        NexaNative.destroyContext(firstContext)

        val restartedContext = NexaNative.createContext()
        assertTrue(restartedContext != 0L)
        try {
            val restartedSession = NexaNative.createRecoveringSession(
                restartedContext, "after restart", 4096,
                File(snapshot, "session.vm").absolutePath,
                File(snapshot, "disks").absolutePath
            )
            assertTrue("Recovery-aware session creation failed: " + NexaNative.lastError(restartedContext), restartedSession != 0L)
            assertEquals(0, NexaNative.attachDisk(restartedContext, restartedSession, disk.absolutePath))
            assertEquals(0, NexaNative.restoreSnapshot(restartedContext, restartedSession, snapshot.absolutePath))
            assertEquals(expectedConsole, NexaNative.console(restartedContext, restartedSession))
            assertEquals(4, NexaNative.state(restartedContext, restartedSession))
        } finally {
            NexaNative.destroyContext(restartedContext)
            root.deleteRecursively()
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

    @Test
    fun persistedWorkspaceSnapshotCanBeSeededForHostForceStopRecovery() {
        val app = InstrumentationRegistry.getInstrumentation().targetContext
        val snapshotRoot = File(app.filesDir, "nexa/snapshot").apply {
            deleteRecursively()
            mkdirs()
        }
        val context = NexaNative.createContext()
        assertTrue("Native context creation failed", context != 0L)
        try {
            val session = NexaNative.createSession(context, "force-stop seed", 4096)
            assertTrue("Native session creation failed", session != 0L)
            assertEquals(0, NexaNative.loadAssembly(context, session, "LOAD A, 88\\nOUT A, 1\\nHALT\\n"))
            assertEquals(0, NexaNative.start(context, session))
            assertEquals(0, NexaNative.run(context, session, 64))
            assertEquals(0, NexaNative.saveSnapshot(context, session, snapshotRoot.absolutePath))
        } finally {
            NexaNative.destroyContext(context)
        }

        assertTrue("VM snapshot was not persisted", File(snapshotRoot, "session.vm").isFile)
        assertTrue("Disk-bundle manifest was not persisted",
            File(snapshotRoot, "disks/manifest.txt").isFile)
    }
    }
}
