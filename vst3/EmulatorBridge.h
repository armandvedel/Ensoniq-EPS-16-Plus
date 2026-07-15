#ifndef EPS16_VST3_EMULATOR_BRIDGE_H
#define EPS16_VST3_EMULATOR_BRIDGE_H

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace eps16::vst3 {

constexpr std::uint64_t kCpuClockHz = 10000000;

struct MidiEvent {
    int sampleOffset{};
    std::uint8_t status{};
    std::uint8_t data1{};
    std::uint8_t data2{};
};

class EmulatorSink {
public:
    virtual ~EmulatorSink() = default;
    virtual void prepare(double dawSampleRate) = 0;
    virtual void runUntil(std::uint64_t absoluteCpuCycle) = 0;
    virtual void midi(std::uint8_t status, std::uint8_t data1,
                      std::uint8_t data2, std::uint64_t cycle) = 0;
    virtual void panelByte(std::uint8_t value, std::uint64_t cycle) = 0;
    virtual void analog(unsigned int channel, std::uint16_t value,
                        std::uint64_t cycle) = 0;
    virtual void samplingInput(float left, float right, std::uint64_t cycle) = 0;
    virtual void stereoOutput(float &left, float &right,
                              std::uint64_t cycle) = 0;
};

class SilentSink final : public EmulatorSink {
public:
    void prepare(double) override {}
    void runUntil(std::uint64_t) override {}
    void midi(std::uint8_t, std::uint8_t, std::uint8_t, std::uint64_t) override {}
    void panelByte(std::uint8_t, std::uint64_t) override {}
    void analog(unsigned int, std::uint16_t, std::uint64_t) override {}
    void samplingInput(float, float, std::uint64_t) override {}
    void stereoOutput(float &left, float &right, std::uint64_t) override {
        left = 0.0f;
        right = 0.0f;
    }
};

class DawClock {
public:
    bool prepare(double sampleRate);
    std::uint64_t advanceOneSample();
    [[nodiscard]] std::uint64_t cycles() const { return totalCycles; }
    [[nodiscard]] std::uint64_t rate() const { return sampleRateHz; }

private:
    std::uint64_t sampleRateHz{};
    std::uint64_t remainder{};
    std::uint64_t totalCycles{};
};

class EmulatorBridge {
public:
    static constexpr std::size_t controlQueueCapacity = 512;

    explicit EmulatorBridge(EmulatorSink &sinkToUse) : sink(sinkToUse) {}

    bool prepare(double sampleRate);
    bool enqueuePanelTransition(std::uint8_t rawMatrixCode, bool pressed);
    bool enqueueAnalog(unsigned int channel, std::uint16_t value);
    void process(const float *inputLeft, const float *inputRight,
                 float *outputLeft, float *outputRight, int samples,
                 const MidiEvent *midiEvents, std::size_t midiEventCount);

    [[nodiscard]] std::uint64_t cpuCycles() const {
        return publishedCycles.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t droppedControls() const {
        return controlDrops.load(std::memory_order_relaxed);
    }

private:
    enum class ControlType : std::uint8_t { panel, analog };
    struct ControlEvent {
        ControlType type{};
        std::uint8_t first{};
        std::uint16_t second{};
    };

    bool enqueue(ControlEvent event);
    bool dequeue(ControlEvent &event);
    void dispatchControls(std::uint64_t cycle);
    void dispatchMidi(const MidiEvent &event, std::uint64_t cycle);

    EmulatorSink &sink;
    DawClock clock;
    std::array<ControlEvent, controlQueueCapacity> controls{};
    std::atomic<std::size_t> controlRead{};
    std::atomic<std::size_t> controlWrite{};
    std::atomic<std::uint64_t> controlDrops{};
    std::atomic<std::uint64_t> publishedCycles{};
};

} // namespace eps16::vst3

#endif
