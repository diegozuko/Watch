# CrownScroll

Haz scroll en el iPhone (X, Instagram, Safari…) girando la **corona del Apple Watch**.

```
Corona del Watch ──BLE──▶ ESP32 ──BLE (ratón)──▶ iPhone
   app watchOS          firmware            scroll en la app abierta
```

iOS no deja que una app mueva otra app, pero sí acepta **ratones Bluetooth**.
El ESP32 se hace pasar por un ratón ante el iPhone y convierte los giros de
la corona en giros de rueda.

## 1. Qué comprar

Una placa **ESP32 con Bluetooth LE** (unos 3–10 € en Amazon o AliExpress). Recomendada:

- **ESP32-C3 SuperMini**: diminuta (2×2 cm), USB-C, barata. Es la opción por defecto.
- También valen una **ESP32-S3** o un **ESP32 DevKit** clásico.
- ⚠️ La **ESP32-S2 no sirve** (no tiene Bluetooth).

Además, un cable USB-C (o micro-USB, según la placa) para programarla y
alimentarla. Para llevarla encima, una batería USB pequeña o una placa con
conector de batería LiPo.

## 2. Programar el ESP32

Solo hay que hacerlo una vez.

**Con PlatformIO** (VS Code + extensión PlatformIO, o `pip install platformio`):

```bash
cd firmware
pio run -e esp32c3 -t upload    # usa esp32s3 o esp32 según tu placa
pio device monitor              # opcional: ver logs
```

**Con Arduino IDE:**

1. Instala el soporte de placas *esp32* de Espressif (Gestor de placas).
2. Instala la librería **NimBLE-Arduino** versión **2.5.x** (Gestor de librerías).
3. Abre `firmware/CrownScroll/CrownScroll.ino`, elige tu placa
   (p. ej. *ESP32C3 Dev Module*, con *USB CDC On Boot: Enabled*) y súbelo.

## 3. Emparejar con el iPhone

1. iPhone → **Ajustes → Accesibilidad → Tocar → AssistiveTouch** → actívalo
   (en iPhone es necesario para usar un ratón).
2. **Ajustes → Bluetooth** → toca **CrownScroll** → **Enlazar**.
3. Aparece un puntero redondo. Déjalo **en el centro de la pantalla**: el
   scroll se aplica donde esté el puntero.

## 4. Instalar la app en el Apple Watch

Necesitas un Mac con Xcode 15 o posterior y un Apple Watch con watchOS 10 o posterior.

```bash
brew install xcodegen
cd watch
xcodegen
open CrownScroll.xcodeproj
```

En Xcode: target **CrownScroll → Signing & Capabilities → Team** (tu Apple
ID gratuito sirve), elige tu Apple Watch como destino y pulsa ▶.

> Con un Apple ID gratuito la app caduca a los 7 días; vuelve a pulsar ▶ para
> renovarla. Con la cuenta de desarrollador (99 €/año) dura un año.

**Sin XcodeGen:** Xcode → *New Project → watchOS → App* (*Watch-only App*),
nombre `CrownScroll`; borra los `.swift` generados y arrastra los de
`watch/CrownScroll/`. En *Info* añade *Privacy – Bluetooth Always Usage
Description*.

## 5. Uso

1. Enciende el ESP32 y abre **CrownScroll** en el reloj: pasará a *Conectado*.
2. Abre X en el iPhone y gira la corona.
3. ⚙️ en el reloj: **sensibilidad** y **invertir dirección**.

Para que el reloj no vuelva a la esfera cada poco:
Watch → **Ajustes → General → Volver al reloj → CrownScroll → Tras 1 hora**.

## Cómo funciona por dentro

- **ESP32** (`firmware/`): servidor BLE con dos servicios a la vez:
  - HID ratón (botones, X, Y, rueda), al que se conecta el iPhone.
  - Servicio propio `6E7C0001-…` con una característica `6E7C0002-…`. El Watch
    escribe en ella 1 byte con signo: pasos de rueda (positivo = bajar).
- **Watch** (`watch/`): SwiftUI + CoreBluetooth. Lee la corona con
  `digitalCrownRotation`, acumula el giro y envía hasta 60 paquetes por segundo.

## Problemas conocidos

- **El scroll va a saltos:** cada paso de rueda lo mueve iOS una cantidad fija;
  baja la sensibilidad para más control.
- **No hace nada:** comprueba que AssistiveTouch está activo y que el puntero
  está sobre el contenido.
- **El Watch no encuentra el ESP32:** reinicia la placa y pulsa *Reconectar*
  en ⚙️. El reloj y el ESP32 tienen que estar a unos pocos metros.
- La app del Watch solo funciona mientras está en pantalla (watchOS suspende
  el Bluetooth de las apps en segundo plano).
