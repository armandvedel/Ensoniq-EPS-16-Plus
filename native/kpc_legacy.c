#include "kpc_legacy.h"

#include <string.h>

enum {
    KPC_DEFAULT_POLL_CYCLES = 1000000,
    KPC_READY_SEARCH_BYTES = 128,
    KPC_DISPLAY_FRAME_LIMIT = 256,
    KPC_LOAD_READY_POLLS = 20000
};

void kpc_legacy_init(KpcLegacy *kpc) {
    memset(kpc, 0, sizeof(*kpc));
    kpc->poll_cycles = KPC_DEFAULT_POLL_CYCLES;
    kpc->autopoll_enabled = 1;
    kpc->live_periodic_ready = 1;
    kpc->e7_reply = 0xff;
}

void kpc_legacy_set_poll_cycles(KpcLegacy *kpc, uint64_t cycles) {
    kpc->poll_cycles = cycles;
}

void kpc_legacy_set_autopoll(KpcLegacy *kpc, int enabled) {
    kpc->autopoll_enabled = enabled != 0;
}

void kpc_legacy_set_live_periodic_ready(KpcLegacy *kpc, int ready) {
    kpc->live_periodic_ready = ready != 0;
}

void kpc_legacy_set_e7_reply(KpcLegacy *kpc, uint8_t reply) {
    kpc->e7_reply = reply;
}

void kpc_legacy_arm_display(KpcLegacy *kpc, unsigned int frames) {
    kpc->display_frames = frames;
    kpc->ready_search_remaining = frames ? KPC_READY_SEARCH_BYTES : 0;
    kpc->display_burst_remaining = 0;
}

void kpc_legacy_arm_load(KpcLegacy *kpc) {
    kpc->load_ready_budget = KPC_LOAD_READY_POLLS;
    kpc_legacy_arm_display(kpc, 64);
}

void kpc_legacy_cancel_load(KpcLegacy *kpc) {
    kpc->load_ready_budget = 0;
    kpc_legacy_arm_display(kpc, 0);
}

int kpc_legacy_reply(KpcLegacy *kpc, uint8_t value, uint8_t *reply) {
    if (value == 0xfd) {
        *reply = kpc->calibration_polls++ < 3 ? 0xc9 : 0xff;
        return 1;
    }
    if (value == 0xe7 || value == 0xf0) {
        *reply = kpc->e7_reply;
        return 1;
    }
    if (value == 0x71) {
        *reply = 0x00;
        return 1;
    }
    *reply = value;
    return 1;
}

int kpc_legacy_observe_tx(KpcLegacy *kpc, uint8_t value) {
    if (value == 0xf7 && kpc->display_frames) {
        /* Sampling level/options use short display updates terminated by f7
           instead of the ordinary ASCII 'f' frame delimiter. */
        --kpc->display_frames;
        kpc->display_burst_remaining = 0;
        kpc->ready_search_remaining =
            kpc->display_frames ? KPC_READY_SEARCH_BYTES : 0;
        return kpc->display_frames != 0;
    }
    if (value == 'f' && kpc->display_frames) {
        if (kpc->display_burst_remaining) {
            --kpc->display_frames;
            kpc->display_burst_remaining =
                kpc->display_frames ? KPC_DISPLAY_FRAME_LIMIT : 0;
        } else {
            kpc->display_burst_remaining = KPC_DISPLAY_FRAME_LIMIT;
        }
        kpc->ready_search_remaining = 0;
    }

    if (kpc->display_burst_remaining) {
        if (value != 'f') --kpc->display_burst_remaining;
        if (kpc->display_burst_remaining) return 1;
        if (kpc->display_frames) {
            kpc->ready_search_remaining = KPC_READY_SEARCH_BYTES;
            return 1;
        }
    } else if (kpc->display_frames && kpc->ready_search_remaining) {
        --kpc->ready_search_remaining;
        return 1;
    }
    return 0;
}

int kpc_legacy_service(KpcLegacy *kpc, int live_mode,
                       unsigned int disk_reads, uint64_t executed,
                       int transport_idle) {
    if (!transport_idle) return 0;
    if (kpc->load_ready_budget &&
        executed - kpc->last_poll_cycle >= kpc->poll_cycles) {
        kpc->last_poll_cycle = executed;
        --kpc->load_ready_budget;
        return 1;
    }
    if (kpc->autopoll_enabled && (!live_mode || kpc->live_periodic_ready) &&
        disk_reads >= 124 && executed >= 50000000 &&
        executed - kpc->last_poll_cycle >= kpc->poll_cycles) {
        kpc->last_poll_cycle = executed;
        return 1;
    }
    return 0;
}
