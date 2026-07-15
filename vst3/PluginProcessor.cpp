#include "PluginProcessor.h"
#include "PanelEditor.h"

const juce::Identifier Eps16PlusProcessor::romPathKey{"combinedRomPath"};
const juce::Identifier Eps16PlusProcessor::kpcPathKey{"kpcRomPath"};
const juce::Identifier Eps16PlusProcessor::osDiskPathKey{"osDiskPath"};

Eps16PlusProcessor::Eps16PlusProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Sampling Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Main Output", juce::AudioChannelSet::stereo(), true)) {}

void Eps16PlusProcessor::prepareToPlay(double sampleRate, int) {
    bridge.prepare(sampleRate);
}

bool Eps16PlusProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Eps16PlusProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                      juce::MidiBuffer &midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto samples = buffer.getNumSamples();
    const float *inputLeft = buffer.getReadPointer(0);
    const float *inputRight = buffer.getReadPointer(1);
    std::size_t eventCount = 0;
    for (const auto metadata : midi) {
        if (eventCount == midiEvents.size()) break;
        const auto message = metadata.getMessage();
        const auto *raw = message.getRawData();
        const auto length = message.getRawDataSize();
        if (length < 1 || message.isSysEx()) continue;
        midiEvents[eventCount++] = {
            juce::jlimit(0, samples, metadata.samplePosition), raw[0],
            static_cast<std::uint8_t>(length > 1 ? raw[1] : 0),
            static_cast<std::uint8_t>(length > 2 ? raw[2] : 0)};
    }

    bridge.process(inputLeft, inputRight, buffer.getWritePointer(0),
                   buffer.getWritePointer(1), samples, midiEvents.data(), eventCount);
    midi.clear();
}

juce::AudioProcessorEditor *Eps16PlusProcessor::createEditor() {
    return new Eps16PanelEditor(*this);
}

void Eps16PlusProcessor::getStateInformation(juce::MemoryBlock &destination) {
    if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void Eps16PlusProcessor::setStateInformation(const void *data, int size) {
    if (const auto xml = getXmlFromBinary(data, size)) {
        const auto restored = juce::ValueTree::fromXml(*xml);
        if (restored.hasType(state.getType())) state = restored;
    }
}

void Eps16PlusProcessor::setResourcePath(const juce::Identifier &key,
                                         const juce::String &path) {
    state.setProperty(key, path, nullptr);
}

juce::String Eps16PlusProcessor::getResourcePath(const juce::Identifier &key) const {
    return state.getProperty(key).toString();
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new Eps16PlusProcessor();
}
