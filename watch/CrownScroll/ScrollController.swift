import Foundation

/// Acumula el giro de la corona y lo envía al ESP32 en pasos enteros de rueda.
final class ScrollController: ObservableObject {
    /// Pasos de rueda por unidad de giro de la corona.
    var sensitivity: Double = 1.0
    /// Invierte la dirección del scroll.
    var inverted = false

    private let ble: BLEManager
    private var accumulator = 0.0
    private var timer: Timer?

    init(ble: BLEManager) {
        self.ble = ble
        // ~60 envíos por segundo como máximo.
        timer = Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
            self?.flush()
        }
    }

    deinit { timer?.invalidate() }

    /// `delta` = cambio en el valor de la corona (positivo = girar hacia arriba).
    func add(crownDelta delta: Double) {
        accumulator += delta * sensitivity * (inverted ? -1 : 1)
    }

    private func flush() {
        guard ble.isConnected else {
            accumulator = 0  // no acumular giros mientras no hay conexión
            return
        }
        let whole = accumulator.rounded(.towardZero)
        guard whole != 0 else { return }
        let steps = max(-127, min(127, whole))
        if ble.send(steps: Int8(steps)) {
            accumulator -= steps
        }
    }
}
