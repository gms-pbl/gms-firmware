#include <Arduino.h>

#include "ModbusRtuManager.h"
#include "AirSensorSth30.h"
#include "SoilSensorSn3002.h"

namespace {

constexpr uint8_t kDefaultExpectedAirAddress = 10;
constexpr uint8_t kDefaultExpectedSoilAddress = 1;
constexpr int kDefaultScanStartAddress = 1;
constexpr int kDefaultScanEndAddress = 32;
constexpr int kProbeRegister = 0x0000;

gms_edge::ModbusRtuManager g_modbus_manager;

void printMenu();
String readLine();
int readInt(const char* prompt, int min_value, int max_value);
bool probeAddress(int address);
bool readRegister(int address, int register_address, int16_t& value);
void scanBus();
void readAirSensor();
void readSoilSensor();
void programAirAddress();
void programAirBaud();
void programSoilAddress();
void verifyExpectedSetup();
void verifyWriteResult(uint8_t old_id, uint8_t new_id);
void setBusBaud();
void autoDetectBaudAndScan();
unsigned long detectBaudForAddress(uint8_t address);

constexpr unsigned long kBaud1200 = 1200UL;
constexpr unsigned long kBaud2400 = 2400UL;
constexpr unsigned long kBaud4800 = 4800UL;
constexpr unsigned long kBaud9600 = 9600UL;

constexpr int kAirBaudRegLegacy = 0x0067;
constexpr int kAirBaudRegAlt = 0x0101;

void haltForever() {
    while (true) {
        delay(1000);
    }
}

String readLine() {
    String line;
    while (true) {
        while (!Serial.available()) {
            delay(10);
        }

        line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            return line;
        }
    }
}

int readInt(const char* prompt, int min_value, int max_value) {
    while (true) {
        Serial.print(prompt);
        String line = readLine();
        int value = line.toInt();

        if (line == "0" || value != 0) {
            if (value >= min_value && value <= max_value) {
                return value;
            }
        }

        Serial.print("Invalid value. Expected range [");
        Serial.print(min_value);
        Serial.print(", ");
        Serial.print(max_value);
        Serial.println("]");
    }
}

bool probeAddress(int address) {
    int16_t value = 0;
    return g_modbus_manager.read_multiple_registers(address, HOLDING_REGISTERS, kProbeRegister, 1, &value);
}

bool readRegister(int address, int register_address, int16_t& value) {
    return g_modbus_manager.read_multiple_registers(address, HOLDING_REGISTERS, register_address, 1, &value);
}

void scanBus() {
    int start_address = readInt("Start address (1..254): ", 1, 254);
    int end_address = readInt("End address (1..254): ", 1, 254);
    if (start_address > end_address) {
        int temp = start_address;
        start_address = end_address;
        end_address = temp;
    }

    Serial.println("Scanning RS485 bus...");
    Serial.print("Probe register: 0x");
    Serial.println(kProbeRegister, HEX);
    int found = 0;
    for (int address = start_address; address <= end_address; ++address) {
        if (probeAddress(address)) {
            Serial.print("  Found device at address ");
            Serial.println(address);
            found++;
        }
        delay(50);
    }

    if (found == 0) {
        Serial.println("No devices found in selected range.");
    }
}

void readAirSensor() {
    int address = readInt("Air sensor ID to read (1..254): ", 1, 254);
    gms_edge::AirSensorSth30 sensor(&g_modbus_manager, static_cast<uint8_t>(address));

    if (!sensor.read_all_registers()) {
        Serial.println("Failed to read AIR sensor.");
        return;
    }

    Serial.println("AIR sensor read OK:");
    Serial.print("  Address(reg 0x0066): ");
    Serial.println(sensor.get_device_address());
    Serial.print("  Baud code(reg 0x0067): ");
    Serial.println(sensor.get_device_baud());
    Serial.print("  Humidity: ");
    Serial.println(sensor.get_humidity());
    Serial.print("  Temperature: ");
    Serial.println(sensor.get_temperature());
}

void readSoilSensor() {
    int address = readInt("Soil sensor ID to read (1..254): ", 1, 254);
    gms_edge::SoilSensorSn3002 sensor(&g_modbus_manager, static_cast<uint8_t>(address));

    if (!sensor.read_all_registers()) {
        Serial.println("Failed to read SOIL sensor.");
        return;
    }

    int16_t id_reg = 0;
    int16_t baud_reg = 0;
    readRegister(address, gms_edge::SoilSensorSn3002::REG_DEVICE_ADDRESS, id_reg);
    readRegister(address, gms_edge::SoilSensorSn3002::REG_DEVICE_BAUD_RATE, baud_reg);

    Serial.println("SOIL sensor read OK:");
    Serial.print("  Address(reg 0x07D0): ");
    Serial.println(id_reg);
    Serial.print("  Baud code(reg 0x07D1): ");
    Serial.println(baud_reg);
    Serial.print("  Moisture: ");
    Serial.println(sensor.get_moisture());
    Serial.print("  Temperature: ");
    Serial.println(sensor.get_temperature());
    Serial.print("  Conductivity: ");
    Serial.println(sensor.get_conductivity());
    Serial.print("  PH: ");
    Serial.println(sensor.get_ph());
}

void verifyWriteResult(uint8_t old_id, uint8_t new_id) {
    delay(500);
    bool old_ok = probeAddress(old_id);
    bool new_ok = probeAddress(new_id);

    Serial.print("Old ID ");
    Serial.print(old_id);
    Serial.print(" responding: ");
    Serial.println(old_ok ? "YES" : "NO");

    Serial.print("New ID ");
    Serial.print(new_id);
    Serial.print(" responding: ");
    Serial.println(new_ok ? "YES" : "NO");

    if (!new_ok) {
        Serial.println("If write just happened, power-cycle the sensor and re-check.");
    }
}

void programAirAddress() {
    Serial.println("WARNING: connect only the AIR sensor while programming.");
    int current_id = readInt("Current AIR sensor ID (1..254): ", 1, 254);
    int new_id = readInt("New AIR sensor ID (1..254): ", 1, 254);

    gms_edge::AirSensorSth30 sensor(&g_modbus_manager, static_cast<uint8_t>(current_id));
    if (!sensor.set_device_address(static_cast<uint8_t>(new_id))) {
        Serial.println("Failed to write AIR sensor address.");
        return;
    }

    Serial.println("AIR sensor address write sent.");
    verifyWriteResult(static_cast<uint8_t>(current_id), static_cast<uint8_t>(new_id));
}

int legacyBaudCodeFromRate(unsigned long baud_rate) {
    if (baud_rate == kBaud2400) {
        return 1;
    }
    if (baud_rate == kBaud4800) {
        return 2;
    }
    return 3;
}

int legacyCompatBaudCodeFromRate(unsigned long baud_rate) {
    if (baud_rate == kBaud2400) {
        return 0;
    }
    if (baud_rate == kBaud4800) {
        return 1;
    }
    return 2;
}

int altBaudCodeFromRate(unsigned long baud_rate) {
    if (baud_rate == kBaud1200) {
        return 1;
    }
    if (baud_rate == kBaud2400) {
        return 2;
    }
    if (baud_rate == kBaud4800) {
        return 3;
    }
    return 4;
}

unsigned long detectBaudForAddress(uint8_t address) {
    const unsigned long candidates[] = {kBaud9600, kBaud4800, kBaud2400, kBaud1200};
    for (unsigned long candidate : candidates) {
        if (!g_modbus_manager.initialize(candidate)) {
            continue;
        }

        if (probeAddress(address)) {
            return candidate;
        }
    }

    return 0UL;
}

void programAirBaud() {
    Serial.println("WARNING: connect only the AIR sensor while programming baud.");
    int current_id = readInt("Current AIR sensor ID (1..254): ", 1, 254);
    int baud_choice = readInt("Target baud: 0=2400, 1=4800, 2=9600: ", 0, 2);
    Serial.println("Select AIR baud profile (single deterministic write):");
    Serial.println("1) reg 0x0067 with code map 1=2400,2=4800,3=9600");
    Serial.println("2) reg 0x0067 with code map 0=2400,1=4800,2=9600");
    Serial.println("3) reg 0x0101 with code map 1=1200,2=2400,3=4800,4=9600");
    int profile = readInt("Profile (1..3): ", 1, 3);

    unsigned long target_baud = kBaud9600;
    if (baud_choice == 0) {
        target_baud = kBaud2400;
    } else if (baud_choice == 1) {
        target_baud = kBaud4800;
    }

    const unsigned long start_baud = detectBaudForAddress(static_cast<uint8_t>(current_id));
    if (start_baud == 0UL) {
        Serial.println("Could not detect current AIR baud.");
        Serial.println("Set bus manually with option 9, then retry.");
        return;
    }

    Serial.print("Detected current AIR baud: ");
    Serial.println(start_baud);

    int reg = kAirBaudRegLegacy;
    int code = legacyBaudCodeFromRate(target_baud);
    if (profile == 2) {
        reg = kAirBaudRegLegacy;
        code = legacyCompatBaudCodeFromRate(target_baud);
    } else if (profile == 3) {
        reg = kAirBaudRegAlt;
        code = altBaudCodeFromRate(target_baud);
    }

    g_modbus_manager.initialize(start_baud);
    Serial.print("Writing AIR baud once on reg 0x");
    Serial.print(reg, HEX);
    Serial.print(" with code ");
    Serial.println(code);

    bool write_ok = false;
    for (int retry = 1; retry <= 3; ++retry) {
        write_ok = g_modbus_manager.write_single_register(current_id, reg, static_cast<uint16_t>(code));
        Serial.print("  write attempt ");
        Serial.print(retry);
        Serial.println(write_ok ? " OK" : " FAILED");
        if (write_ok) {
            break;
        }
        delay(150);
    }

    if (!write_ok) {
        Serial.println("Write never acknowledged. No additional writes performed.");
        return;
    }

    Serial.println("Write acknowledged. Waiting for sensor to apply setting...");
    delay(500);

    const unsigned long final_baud = detectBaudForAddress(static_cast<uint8_t>(current_id));
    if (final_baud == 0UL) {
        Serial.println("Final result: sensor not reachable.");
        Serial.println("Try power cycle and run auto-detect.");
    } else {
        Serial.print("Final detected AIR baud: ");
        Serial.println(final_baud);
        Serial.print("Desired AIR baud: ");
        Serial.println(target_baud);
        if (final_baud == target_baud) {
            Serial.println("SUCCESS: reached desired baud.");
        } else {
            Serial.println("Did not reach desired baud. Try a different profile.");
        }
        g_modbus_manager.initialize(final_baud);
    }
}

void programSoilAddress() {
    Serial.println("WARNING: connect only the SOIL sensor while programming.");
    int current_id = readInt("Current SOIL sensor ID (1..254): ", 1, 254);
    int new_id = readInt("New SOIL sensor ID (1..254): ", 1, 254);

    gms_edge::SoilSensorSn3002 sensor(&g_modbus_manager, static_cast<uint8_t>(current_id));
    if (!sensor.set_device_address(static_cast<uint8_t>(new_id))) {
        Serial.println("Failed to write SOIL sensor address.");
        return;
    }

    Serial.println("SOIL sensor address write sent.");
    verifyWriteResult(static_cast<uint8_t>(current_id), static_cast<uint8_t>(new_id));
}

void verifyExpectedSetup() {
    int air_id = readInt("Expected AIR ID (1..254): ", 1, 254);
    int soil_id = readInt("Expected SOIL ID (1..254): ", 1, 254);

    bool air_ok = probeAddress(air_id);
    bool soil_ok = probeAddress(soil_id);

    Serial.print("AIR @");
    Serial.print(air_id);
    Serial.print(": ");
    Serial.println(air_ok ? "OK" : "NOT FOUND");

    Serial.print("SOIL @");
    Serial.print(soil_id);
    Serial.print(": ");
    Serial.println(soil_ok ? "OK" : "NOT FOUND");

    if (air_ok && soil_ok) {
        Serial.println("Dual-sensor setup looks good on shared Modbus bus.");
    } else {
        Serial.println("Setup not complete yet. Check IDs/wiring/baud and rescan.");
    }
}

void printMenu() {
    Serial.println();
    Serial.println("=== Sensor Utility Menu ===");
    Serial.println("1) Scan bus for active IDs");
    Serial.println("2) Read AIR sensor by ID");
    Serial.println("3) Read SOIL sensor by ID");
    Serial.println("4) Program AIR sensor ID");
    Serial.println("5) Program AIR sensor baud");
    Serial.println("6) Program SOIL sensor ID");
    Serial.println("7) Verify planned dual setup (AIR + SOIL)");
    Serial.println("8) Quick scan defaults (1..32)");
    Serial.println("9) Set Modbus bus baud");
    Serial.println("10) Auto-detect baud and quick-scan");
    Serial.println("0) Print menu again");
}

void quickScanDefaults() {
    Serial.println("Quick scan 1..32");
    Serial.print("Current bus baud: ");
    Serial.println(g_modbus_manager.current_baud_rate());
    Serial.print("Probe register: 0x");
    Serial.println(kProbeRegister, HEX);
    int found = 0;
    for (int address = kDefaultScanStartAddress; address <= kDefaultScanEndAddress; ++address) {
        if (probeAddress(address)) {
            Serial.print("  Found device at address ");
            Serial.println(address);
            found++;
        }
        delay(50);
    }

    if (found == 0) {
        Serial.println("No devices found in default range.");
    }
}

void setBusBaud() {
    Serial.println("Select Modbus baud code:");
    Serial.println("0) 1200");
    Serial.println("1) 2400");
    Serial.println("2) 4800");
    Serial.println("3) 9600");

    int baud_code = readInt("Baud code (0..3): ", 0, 3);
    unsigned long baud = kBaud9600;
    if (baud_code == 0) {
        baud = kBaud1200;
    } else if (baud_code == 1) {
        baud = kBaud2400;
    } else if (baud_code == 2) {
        baud = kBaud4800;
    } else if (baud_code == 3) {
        baud = kBaud9600;
    }

    if (!g_modbus_manager.initialize(baud)) {
        Serial.println("Failed to reinitialize Modbus at selected baud.");
        return;
    }

    Serial.print("Modbus bus reinitialized at ");
    Serial.println(baud);
}

void autoDetectBaudAndScan() {
    const unsigned long baud_candidates[] = {kBaud9600, kBaud4800, kBaud2400, kBaud1200};
    bool any_found = false;

    for (unsigned long baud : baud_candidates) {
        Serial.println();
        Serial.print("Trying baud ");
        Serial.println(baud);
        Serial.print("  Probe register: 0x");
        Serial.println(kProbeRegister, HEX);

        if (!g_modbus_manager.initialize(baud)) {
            Serial.println("  Modbus init failed on this baud.");
            continue;
        }

        int found = 0;
        for (int address = kDefaultScanStartAddress; address <= kDefaultScanEndAddress; ++address) {
            if (probeAddress(address)) {
                Serial.print("  Found device at address ");
                Serial.println(address);
                found++;
            }
            delay(30);
        }

        if (found > 0) {
            any_found = true;
            Serial.print("  Found ");
            Serial.print(found);
            Serial.print(" device(s) at baud ");
            Serial.println(baud);
        } else {
            Serial.println("  No devices found on this baud.");
        }
    }

    if (!any_found) {
        Serial.println("No devices discovered on 9600/4800/2400/1200.");
        Serial.println("Check wiring A/B, sensor power, and RS485 port connection.");
    }
}

} // namespace

void setup() {
    Serial.begin(115200);

    uint32_t start = millis();
    while (!Serial && millis() - start < 5000) {
        delay(10);
    }

    Serial.println("\n\n--- GMS Sensor Utility (RS485 Modbus) ---");
    Serial.println("Tip: when programming IDs, keep only one sensor connected.");

    if (!g_modbus_manager.initialize(kBaud9600)) {
        Serial.println("Modbus initialization failed.");
        haltForever();
    }

    printMenu();
    Serial.print("Active Modbus baud: ");
    Serial.println(g_modbus_manager.current_baud_rate());
    Serial.print("Suggested target IDs -> AIR: ");
    Serial.print(kDefaultExpectedAirAddress);
    Serial.print(", SOIL: ");
    Serial.println(kDefaultExpectedSoilAddress);
}

void loop() {
    if (!Serial.available()) {
        delay(10);
        return;
    }

    String input = readLine();
    int option = input.toInt();

    switch (option) {
        case 0:
            printMenu();
            break;
        case 1:
            scanBus();
            break;
        case 2:
            readAirSensor();
            break;
        case 3:
            readSoilSensor();
            break;
        case 4:
            programAirAddress();
            break;
        case 5:
            programAirBaud();
            break;
        case 6:
            programSoilAddress();
            break;
        case 7:
            verifyExpectedSetup();
            break;
        case 8:
            quickScanDefaults();
            break;
        case 9:
            setBusBaud();
            break;
        case 10:
            autoDetectBaudAndScan();
            break;
        default:
            Serial.println("Unknown option. Type 0 to print menu.");
            break;
    }

    Serial.println();
    Serial.println("Done. Select next option:");
}
