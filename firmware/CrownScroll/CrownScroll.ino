// CrownScroll — firmware para ESP32 (C3 / S3 / clásico)
//
// El ESP32 hace dos cosas a la vez por Bluetooth LE:
//   1. Se presenta ante el iPhone como un RATÓN Bluetooth (HID).
//   2. Expone un servicio propio al que se conecta la app del Apple Watch,
//      que le envía los giros de la corona.
// Cada giro que llega del Watch se convierte en un movimiento de la rueda
// del ratón, y el iPhone hace scroll en la app que tengas abierta.
//
// Requiere la librería NimBLE-Arduino 2.5.x (h2zero). La 3.x cambia la API.

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <atomic>

// ---------------------------------------------------------------------------
// Configuración
// ---------------------------------------------------------------------------
static const char* DEVICE_NAME = "CrownScroll";

// Servicio propio que usa el Watch. Deben coincidir con BLEManager.swift.
static const char* CONTROL_SERVICE_UUID = "6E7C0001-8B3A-4F4B-9C2D-6A1F0C5E2B10";
static const char* SCROLL_CHAR_UUID     = "6E7C0002-8B3A-4F4B-9C2D-6A1F0C5E2B10";

// Cada cuánto se envía un informe de ratón al iPhone (ms).
static const uint32_t REPORT_INTERVAL_MS = 15;

// iPhone + Watch conectados a la vez (y uno de margen).
static const int MAX_CONNECTIONS = 3;

// LED integrado (opcional). En muchas ESP32-C3 SuperMini es el GPIO 8 y se
// enciende con LOW. Pon -1 para desactivarlo.
static const int LED_PIN = 8;
static const bool LED_ACTIVE_LOW = true;

// ---------------------------------------------------------------------------
// Descriptor HID: ratón con 3 botones, X, Y y rueda vertical (Report ID 1)
// ---------------------------------------------------------------------------
static const uint8_t REPORT_ID_MOUSE = 1;

static const uint8_t REPORT_MAP[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_MOUSE, //   Report ID (1)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Buttons)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x03,        //     Usage Maximum (3)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x03,        //     Input (Constant) — relleno
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data, Variable, Relative)
    0xC0,              //   End Collection
    0xC0               // End Collection
};

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
static NimBLEServer* server = nullptr;
static NimBLEHIDDevice* hid = nullptr;
static NimBLECharacteristic* mouseInput = nullptr;

// Scroll acumulado pendiente de enviar al iPhone (lo escribe el Watch).
static std::atomic<int> pendingWheel{0};

static void setLed(bool on) {
    if (LED_PIN < 0) return;
    digitalWrite(LED_PIN, (on ^ LED_ACTIVE_LOW) ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
        Serial.printf("Conectado: %s (total %d)\n",
                      info.getAddress().toString().c_str(), s->getConnectedCount());
        setLed(true);
        // Seguimos anunciándonos para que pueda entrar el otro dispositivo.
        if (s->getConnectedCount() < MAX_CONNECTIONS) {
            NimBLEDevice::startAdvertising();
        }
    }

    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
        Serial.printf("Desconectado: %s (motivo %d)\n",
                      info.getAddress().toString().c_str(), reason);
        if (s->getConnectedCount() == 0) setLed(false);
        NimBLEDevice::startAdvertising();
    }

    void onAuthenticationComplete(NimBLEConnInfo& info) override {
        Serial.printf("Emparejado: %s (cifrado=%d)\n",
                      info.getAddress().toString().c_str(), info.isEncrypted());
    }
};

// El Watch escribe aquí: 1 byte con signo = pasos de rueda (+ abajo / - arriba).
class ScrollCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
        NimBLEAttValue v = c->getValue();
        for (size_t i = 0; i < v.size(); i++) {
            pendingWheel += static_cast<int8_t>(v[i]);
        }
    }
};

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    if (LED_PIN >= 0) {
        pinMode(LED_PIN, OUTPUT);
        setLed(false);
    }

    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setPower(9);  // dBm, máximo alcance
    // Emparejamiento "Just Works" con bonding: el iPhone lo recuerda.
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    server->advertiseOnDisconnect(true);

    // --- Ratón HID (para el iPhone) ---
    hid = new NimBLEHIDDevice(server);
    hid->setManufacturer("CrownScroll");
    hid->setPnp(0x02, 0xE502, 0xA111, 0x0210);
    hid->setHidInfo(0x00, 0x01);
    hid->setReportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
    mouseInput = hid->getInputReport(REPORT_ID_MOUSE);
    hid->setBatteryLevel(100);

    // --- Servicio de control (para el Watch) ---
    NimBLEService* control = server->createService(CONTROL_SERVICE_UUID);
    NimBLECharacteristic* scrollChar = control->createCharacteristic(
        SCROLL_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    scrollChar->setCallbacks(new ScrollCallbacks());

    // Arranca todos los servicios (HID + control).
    server->start();

    // --- Anuncio ---
    // Paquete principal: nombre + apariencia de ratón + servicio HID (para el iPhone).
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName(DEVICE_NAME);
    advData.setAppearance(HID_MOUSE);
    advData.addServiceUUID(hid->getHidService()->getUUID());

    // Respuesta de escaneo: nuestro servicio propio (el Watch busca este UUID).
    NimBLEAdvertisementData scanData;
    scanData.addServiceUUID(NimBLEUUID(CONTROL_SERVICE_UUID));

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAdvertisementData(advData);
    adv->setScanResponseData(scanData);
    adv->start();

    Serial.println("CrownScroll listo. Empareja el iPhone en Ajustes > Bluetooth.");
}

void loop() {
    static uint32_t lastReport = 0;
    uint32_t now = millis();
    if (now - lastReport < REPORT_INTERVAL_MS) {
        delay(1);
        return;
    }
    lastReport = now;

    int wheel = pendingWheel.exchange(0);
    if (wheel == 0) return;

    // Un informe admite -127..127; lo que sobre se envía en el siguiente.
    int step = constrain(wheel, -127, 127);
    if (wheel != step) pendingWheel += (wheel - step);

    // En HID, rueda positiva = scroll hacia arriba. El Watch manda
    // positivo = "bajar", así que invertimos aquí.
    uint8_t report[4] = {0x00, 0x00, 0x00, static_cast<uint8_t>(static_cast<int8_t>(-step))};
    mouseInput->setValue(report, sizeof(report));
    mouseInput->notify();
}
