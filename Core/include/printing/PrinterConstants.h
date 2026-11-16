#ifndef PRINTERCONSTANTS_H
#define PRINTERCONSTANTS_H

/**
 * Printer constants shared across Core and all subapps.
 * Used by the Settings printer page.
 */
namespace PrinterConstants {

// ============================================================================
// DEFAULT VALUES
// ============================================================================
constexpr bool        DEFAULT_ENABLED      = false; // printer disabled by default
constexpr int         DEFAULT_BAUD_RATE    = 9600;
constexpr int         DEFAULT_DATA_BITS    = 8;
constexpr const char* DEFAULT_PARITY       = "None";
constexpr const char* DEFAULT_STOP_BITS    = "1";
constexpr const char* DEFAULT_FLOW_CONTROL = "None";
constexpr int         DEFAULT_LATENCY      = 50;
constexpr const char* DEFAULT_PAPER_SIZE   = "80mm";
constexpr const char* DEFAULT_ENCODING     = "IBM850";

// ============================================================================
// LATENCY SPINBOX RANGE
// ============================================================================
constexpr int LATENCY_MIN = 1;
constexpr int LATENCY_MAX = 1000;

} // namespace PrinterConstants

#endif // PRINTERCONSTANTS_H
