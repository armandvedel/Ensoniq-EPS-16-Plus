#ifndef EPS16_VST3_PROBE_MACHINE_SINK_H
#define EPS16_VST3_PROBE_MACHINE_SINK_H

#include "BandlimitedResampler.h"
#include "EmulatorBridge.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace eps16::vst3 {

class ProbeMachineSink final : public EmulatorSink {
public:
    ProbeMachineSink();
    void configure(std::string romPath, std::string kpcPath,
                   std::string osDiskPath);

    void prepare(double dawSampleRate) override;
    void runUntil(std::uint64_t absoluteCpuCycle) override;
    void midi(std::uint8_t status, std::uint8_t data1,
              std::uint8_t data2, std::uint64_t cycle) override;
    void panelByte(std::uint8_t value, std::uint64_t cycle) override;
    void analog(unsigned int channel, std::uint16_t value,
                std::uint64_t cycle) override;
    void samplingInput(float left, float right, std::uint64_t cycle) override;
    void stereoOutput(float &left, float &right,
                      std::uint64_t cycle) override;

    [[nodiscard]] bool isReady() const {
        return ready.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::string status() const;
    [[nodiscard]] std::string display() const;
    [[nodiscard]] int cursorStart() const {
        return displayCursorStart.load(std::memory_order_relaxed);
    }
    [[nodiscard]] int cursorEnd() const {
        return displayCursorEnd.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint32_t decimalMask() const {
        return displayDecimalMask.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::size_t illegalInstructions() const;
    [[nodiscard]] std::vector<std::uint8_t> captureState() const;
    bool restoreState(const void *data, std::size_t size);

private:
    void publishDisplay();
    void discardQueuedAudio();

    std::string rom;
    std::string kpc;
    std::string disk;
    mutable std::mutex statusMutex;
    std::string statusText{"waiting for ROM, KPC ROM and OS disk"};
    std::array<std::atomic<char>, 23> displayCharacters{};
    std::atomic<int> displayCursorStart{-1};
    std::atomic<int> displayCursorEnd{-1};
    std::atomic<std::uint32_t> displayDecimalMask{};
    std::atomic<bool> ready{};
    std::uint64_t cycleBase{};
    BandlimitedResampler resampler;
};

} // namespace eps16::vst3

#endif
