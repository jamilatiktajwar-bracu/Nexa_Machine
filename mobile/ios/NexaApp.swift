import SwiftUI

@main
struct NexaApp: App {
    var body: some Scene {
        WindowGroup { SessionView().preferredColorScheme(.dark) }
    }
}

struct SessionView: View {
    @StateObject private var model = NexaSessionModel()
    @State private var source = "LOAD A, 19\nLOAD B, 23\nADD A, B\nOUT A, 1\nHALT"
    @State private var input = ""

    var body: some View {
        NavigationStack {
            VStack(spacing: 12) {
                Text(model.status).font(.caption).foregroundStyle(.secondary)
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
                TextEditor(text: $model.console).font(.system(.body, design: .monospaced))
            }
            .padding()
            .navigationTitle("Nexa Machine")
            .toolbar { Button("Stop") { model.stop() } }
        }
        .task { model.create() }
        .onDisappear { model.close() }
    }
}
