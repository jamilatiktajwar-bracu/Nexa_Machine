import Foundation
import Combine

@MainActor
final class NexaSessionModel: ObservableObject {
    @Published var status = "Not connected"
    @Published var console = "Nexa VM core bridge"
    @Published var diskStatus = "No disk attached"
    @Published var recoveryReport = ""
    private var context: OpaquePointer?
    private var session: UInt64 = 0
    private var timer: Timer?

    func create() {
        guard context == nil else { return }
        guard let ctx = nexa_context_create() else { status = "Core initialization failed"; return }
        context = ctx
        var handle: UInt64 = 0
        do {
            let root = try FileManager.default.url(for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: true).appendingPathComponent("Nexa/Snapshot", isDirectory: true)
            try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
            let vm = root.appendingPathComponent("session.vm").path
            let rc = "iOS session".withCString { name in
                vm.withCString { vmPath in
                    root.path.withCString { diskPath in
                        nexa_session_create_recovering(ctx, name, 4096, vmPath, diskPath, &handle)
                    }
                }
            }
            guard rc == 0 else { status = errorText(); return }
            session = handle
            let persistentDisk = root.deletingLastPathComponent().appendingPathComponent("guest.disk")
            if FileManager.default.fileExists(atPath: persistentDisk.path) {
                let attributes = try FileManager.default.attributesOfItem(atPath: persistentDisk.path)
                let diskSize = (attributes[.size] as? NSNumber)?.intValue ?? 0
                let attachStatus = persistentDisk.path.withCString {
                    nexa_disk_attach(ctx, handle, $0, diskSize)
                }
                guard attachStatus == 0 else { status = "Disk recovery attach failed: " + errorText(); refreshRecoveryReport(); return }
            }
            if FileManager.default.fileExists(atPath: root.appendingPathComponent("manifest.txt").path) {
                let restoreStatus = vm.withCString { vmPath in
                    root.path.withCString { diskPath in
                        nexa_snapshot_restore_bundle(ctx, handle, vmPath, diskPath)
                    }
                }
                guard restoreStatus == 0 else { status = "Workspace restore failed: " + errorText(); refreshRecoveryReport(); return }
                refreshConsole()
                status = "Workspace restored"
            } else { status = "Ready · recovery checked" }
        } catch { status = "Recovery startup error: \(error.localizedDescription)" }
    }

    func load(_ source: String) {
        guard let context else { return }
        let rc = source.withCString { nexa_session_load_assembly(context, session, $0) }
        status = rc == 0 ? "Program loaded" : errorText(); refreshConsole()
    }
    func start() { guard let context else { return }; update(nexa_session_start(context, session)); if timer == nil { timer = Timer.scheduledTimer(withTimeInterval: 0.015, repeats: true) { [weak self] _ in Task { @MainActor in self?.tick() } } } }
    func pause() { guard let context else { return }; timer?.invalidate(); timer = nil; update(nexa_session_pause(context, session)) }
    func stop() { guard let context else { return }; timer?.invalidate(); timer = nil; update(nexa_session_stop(context, session)) }
    func reset() { guard let context else { return }; timer?.invalidate(); timer = nil; update(nexa_session_reset(context, session)) }
    func step() { guard let context else { return }; var alive: Int32 = 0; let rc = nexa_session_step(context, session, &alive); update(rc); status = rc == 0 ? (alive == 0 ? "Halted" : "Stepped") : errorText(); refreshConsole() }
    func send(_ text: String) { guard let context else { return }; let rc = (text + "\n").withCString { nexa_keyboard_input(context, session, $0) }; update(rc) }
    func attachDisk(from url: URL) {
        guard let context else { return }
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        do {
            let root = try FileManager.default.url(for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: true).appendingPathComponent("Nexa", isDirectory: true)
            try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
            let destination = root.appendingPathComponent("guest.disk")
            if FileManager.default.fileExists(atPath: destination.path) { try FileManager.default.removeItem(at: destination) }
            try FileManager.default.copyItem(at: url, to: destination)
            let size = try destination.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0
            guard size >= 512 && size % 512 == 0 else { throw NSError(domain: "NexaDisk", code: 1, userInfo: [NSLocalizedDescriptionKey: "Disk image must be a multiple of 512 bytes"]) }
            let rc = destination.path.withCString { nexa_disk_attach(context, session, $0, size) }
            guard rc == 0 else { throw NSError(domain: "NexaDisk", code: Int(rc), userInfo: [NSLocalizedDescriptionKey: errorText()]) }
            diskStatus = "Attached · \(size / 512) sectors"
        } catch { diskStatus = "Disk error: \(error.localizedDescription)" }
    }
    func detachDisk() {
        guard let context else { return }
        update(nexa_disk_detach(context, session, 0, 1))
        if status == "OK" { diskStatus = "No disk attached" }
    }
    func saveSnapshot() {
        guard let context else { return }
        do {
            let root = try FileManager.default.url(for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: true).appendingPathComponent("Nexa/Snapshot", isDirectory: true)
            try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
            let vm = root.appendingPathComponent("session.vm").path
            let rc = vm.withCString { vmPath in root.path.withCString { diskPath in nexa_snapshot_save_bundle(context, session, vmPath, diskPath) } }
            update(rc)
        } catch { status = "Snapshot error: \(error.localizedDescription)" }
    }
    func restoreSnapshot() {
        guard let context else { return }
        do {
            let root = try FileManager.default.url(for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: false).appendingPathComponent("Nexa/Snapshot", isDirectory: true)
            let vm = root.appendingPathComponent("session.vm").path
            let rc = vm.withCString { vmPath in root.path.withCString { diskPath in nexa_snapshot_restore_bundle(context, session, vmPath, diskPath) } }
            update(rc); refreshConsole()
        } catch { status = "Snapshot error: \(error.localizedDescription)" }
    }
    private func refreshRecoveryReport() {
        guard let context else { return }
        var report = NexaRecoveryReport()
        guard nexa_snapshot_recovery_report_get(context, &report) == 0, report.attempted != 0 else { recoveryReport = ""; return }
        func field<T>(_ value: inout T) -> String {
            withUnsafePointer(to: &value) { pointer in
                pointer.withMemoryRebound(to: CChar.self, capacity: MemoryLayout<T>.size) { String(cString: $0) }
            }
        }
        let phase = field(&report.phase)
        let path = field(&report.affected_path)
        let reason = field(&report.reason)
        let action = field(&report.recommended_action)
        recoveryReport = "Recovery: \(report.success != 0 ? "SUCCESS" : "NEEDS ATTENTION")\nPhase: \(phase)\nAffected path: \(path)\nReason: \(reason)\nRecommended action: \(action)"
    }
    private func tick() { guard let context else { return }; var state = NEXA_RUNNING; let rc = nexa_session_run(context, session, 128, &state); refreshConsole(); if rc != 0 { status = errorText(); timer?.invalidate(); timer = nil } else if state == NEXA_HALTED || state == NEXA_FAULTED { status = state == NEXA_HALTED ? "Halted" : "Faulted"; timer?.invalidate(); timer = nil } else { if state == NEXA_PAUSED { _ = nexa_session_resume(context, session) }; status = "Running" } }
    private func refreshConsole() { guard let context else { return }; var required = 0; guard nexa_console_read(context, session, nil, 0, &required) == 0, required > 0 else { return }; var bytes = [CChar](repeating: 0, count: required); let rc = bytes.withUnsafeMutableBufferPointer { nexa_console_read(context, session, $0.baseAddress, $0.count, &required) }; if rc == 0 { console = String(cString: bytes) } }
    private func update(_ rc: Int32) { status = rc == 0 ? "OK" : errorText() }
    private func errorText() -> String { guard let context, let p = nexa_last_error(context) else { return "Unknown core error" }; return String(cString: p) }
    func close() { timer?.invalidate(); timer = nil; if let context { _ = nexa_session_remove(context, session); nexa_context_destroy(context); self.context = nil } }
    deinit { timer?.invalidate() }
}
