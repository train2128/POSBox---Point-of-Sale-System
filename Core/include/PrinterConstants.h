#ifndef PRINTERCONSTANTS_H
#define PRINTERCONSTANTS_H

/**
 * Printer constants shared across Core and all subapps.
 * ThermalPrinter (Core) and settingswindow (Settings) both include this.
 */
namespace PrinterConstants {

// ============================================================================
// FILE
// ============================================================================
constexpr const char* PRINTER_SETTINGS_FILENAME = "printer_settings.json";

// ============================================================================
// JSON KEYS
// ============================================================================
constexpr const char* KEY_ENABLED      = "printer_enabled";
constexpr const char* KEY_PORT         = "port";
constexpr const char* KEY_BAUD_RATE    = "baud_rate";
constexpr const char* KEY_DATA_BITS    = "data_bits";
constexpr const char* KEY_PARITY       = "parity";
constexpr const char* KEY_STOP_BITS    = "stop_bits";
constexpr const char* KEY_FLOW_CONTROL = "flow_control";
constexpr const char* KEY_LATENCY      = "latency_ms";
constexpr const char* KEY_PAPER_SIZE   = "paper_size";
constexpr const char* KEY_ENCODING     = "encoding";

// ============================================================================
// DEFAULT VALUES
// ============================================================================
constexpr bool        DEFAULT_ENABLED      = false; // printer disabled by default
constexpr const char* DEFAULT_PORT         = "";
constexpr int         DEFAULT_BAUD_RATE    = 9600;
constexpr int         DEFAULT_DATA_BITS    = 8;
constexpr const char* DEFAULT_PARITY       = "None";
constexpr const char* DEFAULT_STOP_BITS    = "1";
constexpr const char* DEFAULT_FLOW_CONTROL = "None";
constexpr int         DEFAULT_LATENCY      = 50;
constexpr const char* DEFAULT_PAPER_SIZE   = "80mm";
constexpr const char* DEFAULT_ENCODING     = "UTF-8";

// ============================================================================
// LATENCY SPINBOX RANGE
// ============================================================================
constexpr int LATENCY_MIN = 1;
constexpr int LATENCY_MAX = 1000;

} // namespace PrinterConstants

#endif // PRINTERCONSTANTS_H
