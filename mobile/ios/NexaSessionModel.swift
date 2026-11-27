import Foundation
import Combine

@MainActor
final class NexaSessionModel: ObservableObject {
    @Published var status = "Not connected"
    @Published var console = "Nexa VM core bridge"
    private var context: OpaquePointer?
    private var session: UInt64 = 0
    private var timer: Timer?

    func create() {
        guard context == nil else { return }
        guard let ctx = nexa_context_create() else { status = "Core initialization failed"; return }
        context = ctx
        var handle: UInt64 = 0
        let rc = "iOS session".withCString { nexa_session_create(ctx, $0, 4096, &handle) }
        guard rc == 0 else { status = errorText(); return }
        session = handle
        status = "Ready"
    }

    func load(_ source: String) {
        guard let context else { return }
        let rc = source.withCString { nexa_session_load_assembly(context, session, $0) }
        status = rc == 0 ? "Program loaded" : errorText()
    }
    func start() { guard let context else { return }; update(nexa_session_start(context, session)); if timer == nil { timer = Timer.scheduledTimer(withTimeInterval: 0.015, repeats: true) { [weak self] _ in Task { @MainActor in self?.tick() } } } }
    func pause() { guard let context else { return }; timer?.invalidate(); timer = nil; update(nexa_session_pause(context, session)) }
    func stop() { guard let context else { return }; timer?.invalidate(); timer = nil; update(nexa_session_stop(context, session)) }
    func reset() { guard let context else { return }; timer?.invalidate(); timer = nil; update(nexa_session_reset(context, session)) }
    func step() { guard let context else { return }; var alive: Int32 = 0; let rc = nexa_session_step(context, session, &alive); update(rc); status = rc == 0 ? (alive == 0 ? "Halted" : "Stepped") : errorText() }
    func send(_ text: String) { guard let context else { return }; let rc = (text + "\n").withCString { nexa_keyboard_input(context, session, $0) }; update(rc) }
    private func tick() { guard let context else { return }; var state = NEXA_RUNNING; let rc = nexa_session_run(context, session, 128, &state); if rc != 0 { status = errorText(); timer?.invalidate(); timer = nil } else if state == NEXA_HALTED || state == NEXA_FAULTED { status = state == NEXA_HALTED ? "Halted" : "Faulted"; timer?.invalidate(); timer = nil } else { status = "Running" } }
    private func update(_ rc: Int32) { status = rc == 0 ? "OK" : errorText() }
    private func errorText() -> String { guard let context, let p = nexa_last_error(context) else { return "Unknown core error" }; return String(cString: p) }
    func close() { timer?.invalidate(); timer = nil; if let context { _ = nexa_session_remove(context, session); nexa_context_destroy(context); self.context = nil } }
    deinit { timer?.invalidate() }
}
