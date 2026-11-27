package com.nexa.machine

import android.os.Bundle
import android.net.Uri
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.compose.ui.platform.LocalContext
import java.io.File
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { MaterialTheme { NexaScreen() } }
    }
}

@Composable
private fun NexaScreen() {
    var context by remember { mutableLongStateOf(0L) }
    var session by remember { mutableLongStateOf(0L) }
    var source by remember { mutableStateOf("LOAD A, 19\nLOAD B, 23\nADD A, B\nOUT A, 1\nHALT") }
    var console by remember { mutableStateOf("") }
    var status by remember { mutableStateOf("Initializing") }
    var input by remember { mutableStateOf("") }
    var running by remember { mutableStateOf(false) }
    var diskStatus by remember { mutableStateOf("No disk attached") }
    val appContext = LocalContext.current
    val diskPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null && context != 0L && session != 0L) {
            runCatching {
                val directory = File(appContext.filesDir, "nexa")
                directory.mkdirs()
                val image = File(directory, "guest-disk.img")
                appContext.contentResolver.openInputStream(uri)?.use { input -> image.outputStream().use { output -> input.copyTo(output) } } ?: error("Cannot open selected disk image")
                require(image.length() >= 512L && image.length() % 512L == 0L) { "Disk image must be a multiple of 512 bytes" }
                val rc = NexaNative.attachDisk(context, session, image.absolutePath)
                if (rc != 0) error(NexaNative.lastError(context))
                diskStatus = "Attached: ${image.length() / 512} sectors"
            }.onFailure { diskStatus = "Disk error: ${it.message}" }
        }
    }

    DisposableEffect(Unit) {
        val ctx = NexaNative.createContext()
        context = ctx
        session = if (ctx != 0L) NexaNative.createSession(ctx, "Android session", 4096) else 0L
        status = if (session != 0L) "Ready" else "Core initialization failed"
        onDispose {
            running = false
            if (ctx != 0L) NexaNative.destroyContext(ctx)
        }
    }

    LaunchedEffect(running, context, session) {
        while (running && context != 0L && session != 0L) {
            val rc = NexaNative.run(context, session, 128)
            console = NexaNative.console(context, session)
            if (rc != 0) { status = NexaNative.lastError(context); running = false }
            else { val state = NexaNative.state(context, session); if (state == 4) { status = "Halted"; running = false } else if (state == 5) { status = "Faulted"; running = false } else { if (state == 2) NexaNative.resume(context, session); status = "Running"; delay(15) } }
        }
    }

    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("Nexa Machine", style = MaterialTheme.typography.headlineMedium)
        Text(status, style = MaterialTheme.typography.labelMedium)
        Text(diskStatus, style = MaterialTheme.typography.labelSmall)
        OutlinedTextField(source, { source = it }, label = { Text("Guest assembly") }, modifier = Modifier.fillMaxWidth().height(190.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Button(onClick = { val rc = NexaNative.loadAssembly(context, session, source); status = if (rc == 0) "Program loaded" else NexaNative.lastError(context) }) { Text("Load") }
            Button(onClick = { val rc = NexaNative.start(context, session); if (rc == 0) { running = true; status = "Running" } else status = NexaNative.lastError(context) }) { Text("Start") }
            Button(onClick = { running = false; NexaNative.pause(context, session); status = "Paused" }) { Text("Pause") }
            Button(onClick = { running = false; NexaNative.stop(context, session); status = "Stopped" }) { Text("Stop") }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            OutlinedButton(onClick = { running = false; val rc = NexaNative.step(context, session); status = if (rc == 0) "Stepped" else NexaNative.lastError(context); console = NexaNative.console(context, session) }) { Text("Step") }
            OutlinedButton(onClick = { running = false; val rc = NexaNative.reset(context, session); status = if (rc == 0) "Reset" else NexaNative.lastError(context) }) { Text("Reset") }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            OutlinedButton(onClick = { diskPicker.launch(arrayOf("*/*")) }) { Text("Attach disk image") }
            OutlinedButton(onClick = { val rc = NexaNative.detachDisk(context, session); diskStatus = if (rc == 0) "Disk detached" else NexaNative.lastError(context) }) { Text("Detach disk") }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            OutlinedButton(onClick = { val root = File(appContext.filesDir, "nexa/snapshot"); root.mkdirs(); val rc = NexaNative.saveSnapshot(context, session, root.absolutePath); status = if (rc == 0) "Snapshot saved" else NexaNative.lastError(context) }) { Text("Save snapshot") }
            OutlinedButton(onClick = { running = false; val root = File(appContext.filesDir, "nexa/snapshot"); val rc = NexaNative.restoreSnapshot(context, session, root.absolutePath); status = if (rc == 0) "Snapshot restored" else NexaNative.lastError(context); console = NexaNative.console(context, session) }) { Text("Restore snapshot") }
        }
        OutlinedTextField(input, { input = it }, label = { Text("Guest keyboard input") }, modifier = Modifier.fillMaxWidth())
        Button(onClick = { NexaNative.sendInput(context, session, input + "\n"); input = "" }) { Text("Send input") }
        Text("Guest output")
        Text(console, modifier = Modifier.weight(1f).fillMaxWidth().verticalScroll(rememberScrollState()))
    }
}
