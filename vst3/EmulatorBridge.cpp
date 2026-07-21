#include "EmulatorBridge.h"

#include <algorithm>
#include <cmath>

namespace eps16::vst3 {

bool DawClock::prepare(double sampleRate) {
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 768000.0)
        return false;
    sampleRateHz = static_cast<std::uint64_t>(std::llround(sampleRate));
    remainder = 0;
    totalCycles = 0;
    return sampleRateHz != 0;
}

std::uint64_t DawClock::advanceOneSample() {
    remainder += kCpuClockHz;
    totalCycles += remainder / sampleRateHz;
    remainder %= sampleRateHz;
    return totalCycles;
}

bool EmulatorBridge::prepare(double sampleRate) {
    if (!clock.prepare(sampleRate)) return false;
    controlRead.store(0, std::memory_order_relaxed);
    controlWrite.store(0, std::memory_order_relaxed);
    controlDrops.store(0, std::memory_order_relaxed);
    publishedCycles.store(0, std::memory_order_relaxed);
    hasPendingControl = false;
    nextPanelTransitionCycle = 0;
    sink.prepare(sampleRate);
    return true;
}

bool EmulatorBridge::resetTimeline() {
    const auto sampleRate = clock.rate();
    if (!sampleRate || !clock.prepare(static_cast<double>(sampleRate)))
        return false;
    controlRead.store(0, std::memory_order_relaxed);
    controlWrite.store(0, std::memory_order_relaxed);
    controlDrops.store(0, std::memory_order_relaxed);
    publishedCycles.store(0, std::memory_order_relaxed);
    hasPendingControl = false;
    nextPanelTransitionCycle = 0;
    return true;
}

bool EmulatorBridge::enqueue(ControlEvent event) {
    const auto write = controlWrite.load(std::memory_order_relaxed);
    const auto next = (write + 1) % controlQueueCapacity;
    if (next == controlRead.load(std::memory_order_acquire)) {
        controlDrops.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    controls[write] = event;
    controlWrite.store(next, std::memory_order_release);
    return true;
}

bool EmulatorBridge::dequeue(ControlEvent &event) {
    const auto read = controlRead.load(std::memory_order_relaxed);
    if (read == controlWrite.load(std::memory_order_acquire)) return false;
    event = controls[read];
    controlRead.store((read + 1) % controlQueueCapacity,
                      std::memory_order_release);
    return true;
}

bool EmulatorBridge::enqueuePanelTransition(std::uint8_t rawMatrixCode,
                                            bool pressed) {
    if (rawMatrixCode > 0x7f) return false;
    return enqueue({ControlType::panel, rawMatrixCode,
                    static_cast<std::uint16_t>(pressed ? 1 : 0)});
}

bool EmulatorBridge::enqueueAnalog(unsigned int channel, std::uint16_t value) {
    if (channel >= 8) return false;
    return enqueue({ControlType::analog, static_cast<std::uint8_t>(channel),
                    value});
}

void EmulatorBridge::dispatchControls(std::uint64_t cycle) {
    ControlEvent event;
    for (;;) {
        if (hasPendingControl) {
            event = pendingControl;
        } else if (!dequeue(event)) {
            return;
        }
        if (event.type == ControlType::panel) {
            if (cycle < nextPanelTransitionCycle) {
                pendingControl = event;
                hasPendingControl = true;
                return;
            }
            hasPendingControl = false;
            sink.panelByte(static_cast<std::uint8_t>(event.first |
                              (event.second ? 0x80 : 0x00)), cycle);
            sink.panelByte(0x00, cycle);
            nextPanelTransitionCycle = cycle + panelTransitionSpacingCycles;
        } else {
            hasPendingControl = false;
            sink.analog(event.first, event.second, cycle);
        }
    }
}

void EmulatorBridge::dispatchMidi(const MidiEvent &event, std::uint64_t cycle) {
    const auto type = static_cast<std::uint8_t>(event.status & 0xf0);
    if (type == 0x80 || type == 0x90 || type == 0xa0 || type == 0xb0 ||
        type == 0xc0 || type == 0xd0 || type == 0xe0)
        sink.midi(event.status, event.data1, event.data2, cycle);
}

void EmulatorBridge::process(const float *inputLeft, const float *inputRight,
                             float *outputLeft, float *outputRight, int samples,
                             const MidiEvent *midiEvents,
                             std::size_t midiEventCount) {
    if (samples <= 0 || !outputLeft || !outputRight) return;
    if (!sink.beginBlock()) {
        std::fill_n(outputLeft, samples, 0.0f);
        std::fill_n(outputRight, samples, 0.0f);
        return;
    }
    dispatchControls(clock.cycles());
    std::size_t midiIndex = 0;
    for (int sample = 0; sample < samples; ++sample) {
        if ((sample & 63) == 0) dispatchControls(clock.cycles());
        while (midiIndex < midiEventCount &&
               midiEvents[midiIndex].sampleOffset <= sample) {
            dispatchMidi(midiEvents[midiIndex], clock.cycles());
            ++midiIndex;
        }
        const float inLeft = inputLeft ? inputLeft[sample] : 0.0f;
        const float inRight = inputRight ? inputRight[sample] : inLeft;
        sink.samplingInput(inLeft, inRight, clock.cycles());
        const auto cycle = clock.advanceOneSample();
        sink.runUntil(cycle);
        float left = 0.0f;
        float right = 0.0f;
        sink.stereoOutput(left, right, cycle);
        outputLeft[sample] = left;
        outputRight[sample] = right;
    }
    while (midiIndex < midiEventCount) {
        dispatchMidi(midiEvents[midiIndex], clock.cycles());
        ++midiIndex;
    }
    publishedCycles.store(clock.cycles(), std::memory_order_relaxed);
    sink.endBlock();
}

} // namespace eps16::vst3
