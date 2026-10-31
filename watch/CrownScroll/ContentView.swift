import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var ble: BLEManager
    @EnvironmentObject private var scroller: ScrollController

    @AppStorage("sensitivity") private var sensitivity = 1.0
    @AppStorage("inverted") private var inverted = false

    @State private var crown = 0.0
    @State private var showSettings = false
    @FocusState private var crownFocused: Bool

    // Rango de la corona. Con isContinuous el valor "da la vuelta" al llegar
    // a un extremo; lo corregimos al calcular el delta.
    private let crownRange = 1000.0

    var body: some View {
        NavigationStack {
            VStack(spacing: 10) {
                Image(systemName: ble.isConnected ? "arrow.up.and.down.circle.fill"
                                                  : "antenna.radiowaves.left.and.right")
                    .font(.system(size: 44))
                    .foregroundStyle(ble.isConnected ? .green : .secondary)
                    .symbolEffect(.pulse, isActive: !ble.isConnected)

                Text(ble.status.title)
                    .font(.headline)
                    .multilineTextAlignment(.center)

                Text(ble.isConnected ? "Gira la corona para hacer scroll en el iPhone"
                                     : "Enciende el ESP32 cerca del reloj")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .focusable()
            .focused($crownFocused)
            .digitalCrownRotation($crown,
                                  from: -crownRange,
                                  through: crownRange,
                                  sensitivity: .high,
                                  isContinuous: true,
                                  isHapticFeedbackEnabled: false)
            .onChange(of: crown) { old, new in
                var delta = new - old
                if delta > crownRange { delta -= 2 * crownRange }
                if delta < -crownRange { delta += 2 * crownRange }
                scroller.add(crownDelta: delta)
            }
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        showSettings = true
                    } label: {
                        Image(systemName: "gearshape")
                    }
                }
            }
            .sheet(isPresented: $showSettings, onDismiss: { crownFocused = true }) {
                SettingsView(sensitivity: $sensitivity, inverted: $inverted)
            }
        }
        .onAppear {
            scroller.sensitivity = sensitivity
            scroller.inverted = inverted
            crownFocused = true
        }
        .onChange(of: sensitivity) { _, value in scroller.sensitivity = value }
        .onChange(of: inverted) { _, value in scroller.inverted = value }
    }
}

struct SettingsView: View {
    @EnvironmentObject private var ble: BLEManager
    @Binding var sensitivity: Double
    @Binding var inverted: Bool

    var body: some View {
        List {
            Section("Sensibilidad: \(sensitivity, specifier: "%.1f")") {
                Slider(value: $sensitivity, in: 0.1...5.0, step: 0.1)
            }
            Toggle("Invertir dirección", isOn: $inverted)
            Button("Reconectar") { ble.reconnect() }
        }
        .navigationTitle("Ajustes")
    }
}
