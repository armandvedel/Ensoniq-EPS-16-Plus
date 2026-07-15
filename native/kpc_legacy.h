#ifndef EPS16_KPC_LEGACY_H
#define EPS16_KPC_LEGACY_H

#include <stdint.h>

typedef struct {
    uint64_t poll_cycles;
    uint64_t last_poll_cycle;
    unsigned int display_frames;
    unsigned int ready_search_remaining;
    unsigned int display_burst_remaining;
    unsigned int load_ready_budget;
    unsigned int calibration_polls;
    uint8_t e7_reply;
    int autopoll_enabled;
    int live_periodic_ready;
} KpcLegacy;

void kpc_legacy_init(KpcLegacy *kpc);
void kpc_legacy_set_poll_cycles(KpcLegacy *kpc, uint64_t cycles);
void kpc_legacy_set_autopoll(KpcLegacy *kpc, int enabled);
void kpc_legacy_set_live_periodic_ready(KpcLegacy *kpc, int ready);
void kpc_legacy_set_e7_reply(KpcLegacy *kpc, uint8_t reply);
void kpc_legacy_arm_display(KpcLegacy *kpc, unsigned int frames);
void kpc_legacy_arm_load(KpcLegacy *kpc);
void kpc_legacy_cancel_load(KpcLegacy *kpc);
int kpc_legacy_reply(KpcLegacy *kpc, uint8_t value, uint8_t *reply);
int kpc_legacy_observe_tx(KpcLegacy *kpc, uint8_t value);
int kpc_legacy_service(KpcLegacy *kpc, int live_mode,
                       unsigned int disk_reads, uint64_t executed,
                       int transport_idle);

#endif
