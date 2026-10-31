import SwiftUI

@main
struct CrownScrollApp: App {
    @StateObject private var ble: BLEManager
    @StateObject private var scroller: ScrollController

    init() {
        let ble = BLEManager()
        _ble = StateObject(wrappedValue: ble)
        _scroller = StateObject(wrappedValue: ScrollController(ble: ble))
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(ble)
                .environmentObject(scroller)
        }
    }
}
