#ifndef NTP_SERVICE_H
#define NTP_SERVICE_H

#include "lvgl_port.h"
#include "wifi_manager.h"
#include <time.h>

// ── Config ──────────────────
#ifndef NTP_GMT_OFFSET_SEC
  #define NTP_GMT_OFFSET_SEC    (7 * 3600)   // UTC+7
#endif
#ifndef NTP_DAYLIGHT_OFFSET_SEC
  #define NTP_DAYLIGHT_OFFSET_SEC 0
#endif
#ifndef NTP_SERVER
  #define NTP_SERVER            "time.cloudflare.com"
#endif
#ifndef NTP_SYNC_INTERVAL_MS
  #define NTP_SYNC_INTERVAL_MS  3600000UL    // Re-sync one hour
#endif
#ifndef NTP_RETRY_INTERVAL_MS
  #define NTP_RETRY_INTERVAL_MS 1000UL       // get time every second until success
#endif

// ── State ───────────────────────────────────────────────────────────────────
typedef enum {
    NTP_STATE_WAITING_WIFI = 0,  // Waiting for WiFi connection
    NTP_STATE_SYNCING,           // Waiting for NTP to return valid time
    NTP_STATE_SYNCED,            // Time is available, counting re-sync interval
} ntp_state_t;

// ── API ─────────────────────────────────────────────────────────────────────

/** Initialize NTP service (call once in setup, AFTER nvs_flash_init) */
void ntp_begin();

/** Update state machine — call in loop() or a periodic task */
void ntp_update();

//void update_ui_time();

/** Get time string "HH:MM" with blinking colon, "--:--" if not synced */
const char* ntp_get_time_str();

/** true if time has been synced at least once */
bool ntp_is_synced();

/** Get current state */
ntp_state_t ntp_get_state();

/** Force re-sync */
void ntp_force_resync();
#endif