#ifndef EPS16_VST3_PLUGIN_PROCESSOR_H
#define EPS16_VST3_PLUGIN_PROCESSOR_H

#include "EmulatorBridge.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

class Eps16PlusProcessor final : public juce::AudioProcessor {
public:
    Eps16PlusProcessor();

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;

    bool enqueuePanelTransition(std::uint8_t code, bool pressed) {
        return bridge.enqueuePanelTransition(code, pressed);
    }
    bool enqueueAnalog(unsigned int channel, std::uint16_t value) {
        return bridge.enqueueAnalog(channel, value);
    }
    void setResourcePath(const juce::Identifier &key, const juce::String &path);
    juce::String getResourcePath(const juce::Identifier &key) const;
    std::uint64_t cpuCycles() const { return bridge.cpuCycles(); }

    static const juce::Identifier romPathKey;
    static const juce::Identifier kpcPathKey;
    static const juce::Identifier osDiskPathKey;

private:
    static constexpr std::size_t maximumMidiEvents = 1024;
    eps16::vst3::SilentSink silentSink;
    eps16::vst3::EmulatorBridge bridge{silentSink};
    std::array<eps16::vst3::MidiEvent, maximumMidiEvents> midiEvents{};
    juce::ValueTree state{"EPS16PlusPrototype"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Eps16PlusProcessor)
};

#endif
