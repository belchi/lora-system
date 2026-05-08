import hashlib
import hmac
import threading
import time

from gpiozero import Button
from LoRaRF import SX127x

from hub_db import init_db, insert_reading

# --- Inställningar ---
SHARED_SECRET = b"<SECRET_NETW_KEY>"
MY_ID = "hub"
DIO0_PIN = 22

# --- Setup LoRa ---
loRa = SX127x()
loRa.setPins(7, 25, -1)  # -1 = vi hanterar DIO0 själva

if not loRa.begin(0, 1):
    print("❌ Hårdvarufel!")
    exit()

# --- Radioinställningar (matcha ESP32) ---
loRa.setFrequency(868000000)
loRa.setSpreadingFactor(7)
loRa.setBandwidth(125000)
loRa.setCodeRate(5)  # ← Lägg till denna (4/5)
loRa.setSyncWord(0xA5)  # ← Ändra från 0x12 till 0xA5
loRa.setHeaderType(False)
loRa.setCrcEnable(True)
loRa.setInvertIq(False)


def setup_radio():
    loRa.writeRegister(0x0F, 0x00)  # FifoRxBaseAddr = 0
    loRa.writeRegister(0x0D, 0x00)  # FifoAddrPtr = 0
    current_val = loRa.readRegister(0x26)
    loRa.writeRegister(0x26, current_val | 0x04)  # AGC på
    loRa.writeRegister(0x0C, 0x20)  # LNA max gain, låt AGC styra


def start_rx():
    loRa.writeRegister(0x01, 0x81)  # Standby
    loRa.writeRegister(0x12, 0xFF)  # Rensa alla IRQ-flaggor
    loRa.writeRegister(0x0D, 0x00)  # FIFO-pekare till bas
    loRa.writeRegister(0x01, 0x85)  # RX Continuous


# --- Interrupt via gpiozero ---
rx_event = threading.Event()


def dio0_callback():
    rx_event.set()


dio0 = Button(DIO0_PIN, pull_up=False)
dio0.when_pressed = dio0_callback

# --- Starta radion ---
setup_radio()
start_rx()

init_db()
print("🚀 Hub Online. Lyssnar på 868 MHz, SF7, BW125.")

last_nonce = None

try:
    while True:
        triggered = rx_event.wait(timeout=2.0)
        rx_event.clear()

        irq = loRa.readRegister(0x12)

        if not (irq & 0x40):
            # Ingen RxDone – watchdog: kontrollera att radion är i RX-läge
            opmode = loRa.readRegister(0x01)
            if (opmode & 0x07) != 0x05:
                print("⚠️ Radio inte i RX-läge, återstartar...")
                start_rx()
            continue

        # CRC-fel
        if irq & 0x20:
            rssi = loRa.packetRssi()
            num_bytes = loRa.readRegister(0x13)
            print(f"⚠️ CRC-fel – RSSI: {rssi} dBm, bytes: {num_bytes}")
            start_rx()
            continue
        # if irq & 0x20:
        #    print("⚠️ CRC-fel – rensar och återstartar")
        #    start_rx()
        #    continue

        # Läs RSSI och payload innan radion återstartas
        rssi = loRa.packetRssi()
        num_bytes = loRa.readRegister(0x13)  # RxNbBytes
        rx_addr = loRa.readRegister(0x10)  # FifoRxCurrentAddr

        loRa.writeRegister(0x0D, rx_addr)
        payload = [loRa.read() for _ in range(num_bytes)]

        # Återstarta RX först efter att payload är läst
        start_rx()

        # --- Tolka payload ---
        try:
            data = "".join(chr(b) for b in payload if 32 <= b <= 126)
            parts = data.split("|")

            if len(parts) != 6:
                print(f"⚠️ Felaktigt antal fält ({len(parts)}): {data}")
                continue

            sender, receiver, nonce, p_type, value, signature = parts

            if nonce == last_nonce:
                print(f"   (Duplikat ignorerat, nonce={nonce})")
                continue
            last_nonce = nonce

            body = "|".join(parts[:-1])
            expected = hmac.new(
                SHARED_SECRET, body.encode("utf-8"), hashlib.sha256
            ).hexdigest()

            if hmac.compare_digest(signature, expected):
                ts = time.strftime("%Y-%m-%d %H:%M:%S")
                print(
                    f"[{ts}] ✅ {sender} → {receiver}: {value} °C  (RSSI: {rssi} dBm)"
                )
                insert_reading(ts, sender, float(value), rssi)
            else:
                print(f"⚠️ Signaturfel från {sender}")

        except Exception as e:
            print(f"❌ Tolkningsfel: {e}")

except KeyboardInterrupt:
    print("\n👋 Avslutar...")
    dio0.close()
