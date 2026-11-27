import SwiftUI
import UniformTypeIdentifiers

@main
struct NexaApp: App {
    var body: some Scene {
        WindowGroup { SessionView().preferredColorScheme(.dark) }
    }
}

struct SessionView: View {
    @StateObject private var model = NexaSessionModel()
    @Environment(\.scenePhase) private var scenePhase
    @State private var source = "LOAD A, 19\nLOAD B, 23\nADD A, B\nOUT A, 1\nHALT"
    @State private var input = ""
    @State private var showingDiskImporter = false

    var body: some View {
        NavigationStack {
            VStack(spacing: 12) {
                Text(model.status).font(.caption).foregroundStyle(.secondary)
                Text(model.diskStatus).font(.caption2).foregroundStyle(.secondary)
                TextEditor(text: $source).font(.system(.body, design: .monospaced)).frame(minHeight: 180)
                HStack {
                    Button("Load") { model.load(source) }
                    Button("Start") { model.start() }
                    Button("Step") { model.step() }
                    Button("Pause") { model.pause() }
                    Button("Reset") { model.reset() }
                }.buttonStyle(.borderedProminent).controlSize(.small)
                HStack {
                    TextField("Guest keyboard input", text: $input)
                    Button("Send") { model.send(input); input = "" }
                }
                HStack {
                    Button("Import disk") { showingDiskImporter = true }
                    Button("Detach disk") { model.detachDisk() }
                    Button("Save snapshot") { model.saveSnapshot() }
                    Button("Restore snapshot") { model.restoreSnapshot() }
                }.buttonStyle(.bordered).controlSize(.small)
                TextEditor(text: $model.console).font(.system(.body, design: .monospaced))
            }
            .padding()
            .navigationTitle("Nexa Machine")
            .toolbar { Button("Stop") { model.stop() } }
        }
        .fileImporter(isPresented: $showingDiskImporter, allowedContentTypes: [.data], allowsMultipleSelection: false) { result in
            switch result {
            case .success(let urls): if let url = urls.first { model.attachDisk(from: url) }
            case .failure(let error): model.status = "Import error: \(error.localizedDescription)"
            }
        }
        .task { model.create() }
        .onChange(of: scenePhase) { _, phase in
            if phase == .background { model.saveSnapshot() }
        }
        .onDisappear {
            model.saveSnapshot()
            model.close()
        }
    }
}
