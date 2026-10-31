import CoreBluetooth
import Foundation

/// Busca el ESP32 "CrownScroll", se conecta y le envía pasos de rueda.
final class BLEManager: NSObject, ObservableObject {
    enum Status: Equatable {
        case starting
        case bluetoothOff
        case unauthorized
        case unsupported
        case scanning
        case connecting
        case connected

        var title: String {
            switch self {
            case .starting: return "Iniciando…"
            case .bluetoothOff: return "Bluetooth apagado"
            case .unauthorized: return "Sin permiso de Bluetooth"
            case .unsupported: return "Bluetooth no disponible"
            case .scanning: return "Buscando CrownScroll…"
            case .connecting: return "Conectando…"
            case .connected: return "Conectado"
            }
        }
    }

    // Deben coincidir con el firmware (CrownScroll.ino).
    static let serviceUUID = CBUUID(string: "6E7C0001-8B3A-4F4B-9C2D-6A1F0C5E2B10")
    static let scrollUUID = CBUUID(string: "6E7C0002-8B3A-4F4B-9C2D-6A1F0C5E2B10")

    @Published private(set) var status: Status = .starting

    var isConnected: Bool { status == .connected }

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var scrollCharacteristic: CBCharacteristic?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    /// Envía `steps` pasos de rueda (positivo = bajar). Devuelve false si no se pudo.
    @discardableResult
    func send(steps: Int8) -> Bool {
        guard let peripheral, let scrollCharacteristic,
              peripheral.state == .connected,
              peripheral.canSendWriteWithoutResponse else { return false }
        peripheral.writeValue(Data([UInt8(bitPattern: steps)]),
                              for: scrollCharacteristic,
                              type: .withoutResponse)
        return true
    }

    /// Corta la conexión y vuelve a buscar.
    func reconnect() {
        if let peripheral {
            central.cancelPeripheralConnection(peripheral)
        }
        reset()
        startScan()
    }

    private func startScan() {
        guard central.state == .poweredOn else { return }
        status = .scanning
        central.scanForPeripherals(withServices: [Self.serviceUUID])
    }

    private func reset() {
        peripheral = nil
        scrollCharacteristic = nil
    }

    private func retryLater() {
        reset()
        DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [weak self] in
            self?.startScan()
        }
    }
}

extension BLEManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn: startScan()
        case .poweredOff: reset(); status = .bluetoothOff
        case .unauthorized: status = .unauthorized
        case .unsupported: status = .unsupported
        default: status = .starting
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        guard self.peripheral == nil else { return }
        central.stopScan()
        self.peripheral = peripheral
        peripheral.delegate = self
        status = .connecting
        central.connect(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([Self.serviceUUID])
    }

    func centralManager(_ central: CBCentralManager,
                        didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        retryLater()
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        retryLater()
    }
}

extension BLEManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == Self.serviceUUID }) else {
            central.cancelPeripheralConnection(peripheral)
            return
        }
        peripheral.discoverCharacteristics([Self.scrollUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let characteristic = service.characteristics?.first(where: { $0.uuid == Self.scrollUUID }) else {
            central.cancelPeripheralConnection(peripheral)
            return
        }
        scrollCharacteristic = characteristic
        status = .connected
    }
}
