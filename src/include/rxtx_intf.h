/***
 * This file defines the interface from device units to functions in
 * either rx_main or tx_main (or rxtx_common but exposed to other units)
 * Use this instead of drectly declaring externs in your unit
 ***/

#include "common.h"

/***
 * In both RX and TX builds
 */
void EnterBindingModeSafely();
void EnterUnbindMode();
void UpdateUID(const uint8_t *newUid);
void scheduleRebootTime(unsigned long inMs);

/***
 * TX interface
 ***/
#if defined(TARGET_TX)
void SetSyncSpam();
#endif

/***
 * RX interface
 ***/
#if defined(TARGET_RX)
uint8_t getLq();
// Fills a caller-supplied buffer with a one-line snapshot of the acquisition state
// (connection state, raw LQ, FHSS index, rate index, scan index, phase offset and the
// running connect/disconnect counts). Everything it reports lives in rx_main statics.
void GetRxLinkDiag(char *out, size_t len);
#endif
