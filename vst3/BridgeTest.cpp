#include "EmulatorBridge.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace eps16::vst3;

struct CaptureSink final : EmulatorSink {
    struct Message { std::uint8_t value; std::uint64_t cycle; };
    struct AnalogMessage { unsigned int channel; std::uint16_t value; };
    void prepare(double rate) override { preparedRate = rate; }
    void runUntil(std::uint64_t cycle) override {
        assert(cycle >= lastRunCycle);
        lastRunCycle = cycle;
    }
    void midi(std::uint8_t status, std::uint8_t data1, std::uint8_t data2,
              std::uint64_t cycle) override {
        midiMessages.push_back({status, cycle});
        lastMidiData1 = data1;
        lastMidiData2 = data2;
    }
    void panelByte(std::uint8_t value, std::uint64_t cycle) override {
        panelMessages.push_back({value, cycle});
    }
    void analog(unsigned int channel, std::uint16_t value,
                std::uint64_t cycle) override {
        analogChannel = channel;
        analogValue = value;
        analogCycle = cycle;
        analogMessages.push_back({channel, value});
    }
    void samplingInput(float left, float right, std::uint64_t) override {
        inputSum += left + right;
        ++inputFrames;
    }
    void stereoOutput(float &left, float &right, std::uint64_t cycle) override {
        left = static_cast<float>(cycle & 1U);
        right = -left;
    }
    double preparedRate{};
    std::uint64_t lastRunCycle{};
    std::uint8_t lastMidiData1{}, lastMidiData2{};
    unsigned int analogChannel{};
    std::uint16_t analogValue{};
    std::uint64_t analogCycle{};
    double inputSum{};
    std::size_t inputFrames{};
    std::vector<Message> midiMessages;
    std::vector<Message> panelMessages;
    std::vector<AnalogMessage> analogMessages;
};

int main() {
    CaptureSink sink;
    EmulatorBridge bridge(sink);
    assert(bridge.prepare(48000.0));
    assert(sink.preparedRate == 48000.0);
    assert(bridge.enqueuePanelTransition(0x23, true));
    assert(bridge.enqueuePanelTransition(0x23, false));
    assert(bridge.enqueueAnalog(3, 128));
    assert(bridge.enqueueAnalog(3, 384));
    assert(bridge.enqueueAnalog(3, 715));

    constexpr int frames = 48000;
    std::array<float, frames> inputLeft{};
    std::array<float, frames> inputRight{};
    std::array<float, frames> outputLeft{};
    std::array<float, frames> outputRight{};
    inputLeft.fill(0.25f);
    inputRight.fill(-0.125f);
    const MidiEvent midi[] = {{0, 0x90, 60, 100}, {24000, 0x80, 60, 0}};
    bridge.process(inputLeft.data(), inputRight.data(), outputLeft.data(),
                   outputRight.data(), frames, midi, 2);

    assert(bridge.cpuCycles() == kCpuClockHz);
    assert(sink.lastRunCycle == kCpuClockHz);
    assert(sink.inputFrames == frames);
    assert(std::abs(sink.inputSum - 6000.0) < 0.01);
    assert(sink.panelMessages.size() == 4);
    assert(sink.panelMessages[0].value == 0xa3);
    assert(sink.panelMessages[1].value == 0x00);
    assert(sink.panelMessages[2].value == 0x23);
    assert(sink.panelMessages[3].value == 0x00);
    assert(sink.panelMessages[2].cycle >= 500000);
    assert(sink.analogChannel == 3 && sink.analogValue == 715);
    assert(sink.analogMessages.size() == 3);
    assert(sink.analogMessages[0].value == 128);
    assert(sink.analogMessages[1].value == 384);
    assert(sink.analogMessages[2].value == 715);
    assert(sink.midiMessages.size() == 2);
    assert(sink.midiMessages[0].cycle == 0);
    assert(sink.midiMessages[1].cycle == 5000000);
    assert(sink.lastMidiData1 == 60 && sink.lastMidiData2 == 0);
    assert(bridge.droppedControls() == 0);

    DawClock clock;
    assert(clock.prepare(44100.0));
    for (int i = 0; i < 44100; ++i) clock.advanceOneSample();
    assert(clock.cycles() == kCpuClockHz);

    CaptureSink partitionedSink;
    EmulatorBridge partitionedBridge(partitionedSink);
    assert(partitionedBridge.prepare(48000.0));
    for (int block = 0; block < 48; ++block) {
        partitionedBridge.process(inputLeft.data(), inputRight.data(),
                                  outputLeft.data(), outputRight.data(), 1000,
                                  nullptr, 0);
    }
    assert(partitionedBridge.cpuCycles() == bridge.cpuCycles());
    assert(partitionedSink.lastRunCycle == sink.lastRunCycle);
    assert(partitionedBridge.resetTimeline());
    partitionedBridge.process(inputLeft.data(), inputRight.data(),
                              outputLeft.data(), outputRight.data(), 1,
                              nullptr, 0);
    assert(partitionedBridge.cpuCycles() == 208);
    return 0;
}
