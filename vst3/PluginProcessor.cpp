#include "PluginProcessor.h"
#include "PanelEditor.h"

#include <dlfcn.h>

namespace {
void modulePathAnchor() {}

juce::File moduleFile() {
    Dl_info information{};
    if (dladdr(reinterpret_cast<const void *>(&modulePathAnchor), &information) &&
        information.dli_fname)
        return juce::File(juce::String::fromUTF8(information.dli_fname));
    return {};
}

juce::File firstExisting(const juce::File &directory,
                         std::initializer_list<const char *> names) {
    for (const auto *name : names) {
        const auto candidate = directory.getChildFile(name);
        if (candidate.existsAsFile()) return candidate;
    }
    return {};
}

juce::File firstFileWithSize(const juce::File &directory,
                             const juce::String &pattern,
                             std::int64_t expectedSize) {
    auto files = directory.findChildFiles(juce::File::findFiles, false, pattern);
    files.sort();
    for (const auto &file : files)
        if (file.getSize() == expectedSize) return file;
    return {};
}
} // namespace

const juce::Identifier Eps16PlusProcessor::romPathKey{"combinedRomPath"};
const juce::Identifier Eps16PlusProcessor::kpcPathKey{"kpcRomPath"};
const juce::Identifier Eps16PlusProcessor::osDiskPathKey{"osDiskPath"};

Eps16PlusProcessor::Eps16PlusProcessor()
    : AudioProcessor(BusesProperties()
          /* Keep the instrument's main input disabled and expose sampling as
             an auxiliary input. Hosts such as Ableton Live then present it as
             a routable sidechain source on a MIDI/instrument track. */
          .withInput("Main Input", juce::AudioChannelSet::stereo(), false)
          .withInput("Sampling Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Main Output", juce::AudioChannelSet::stereo(), true)) {
    refreshResourcePaths();
}

void Eps16PlusProcessor::prepareToPlay(double sampleRate, int) {
    refreshResourcePaths();
    machineSink.configure(getResourcePath(romPathKey).toStdString(),
                          getResourcePath(kpcPathKey).toStdString(),
                          getResourcePath(osDiskPathKey).toStdString());
    bridge.prepare(sampleRate);
}

bool Eps16PlusProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
    return layouts.inputBuses.size() == 2 &&
           layouts.getChannelSet(true, 0).isDisabled() &&
           layouts.getChannelSet(true, 1) == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Eps16PlusProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                      juce::MidiBuffer &midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto samples = buffer.getNumSamples();
    auto samplingInput = getBusBuffer(buffer, true, 1);
    auto mainOutput = getBusBuffer(buffer, false, 0);
    const float *inputLeft = samplingInput.getReadPointer(0);
    const float *inputRight = samplingInput.getReadPointer(1);
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

    bridge.process(inputLeft, inputRight, mainOutput.getWritePointer(0),
                   mainOutput.getWritePointer(1), samples, midiEvents.data(), eventCount);
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
        if (restored.hasType(state.getType())) {
            state = restored;
            refreshResourcePaths();
        }
    }
}

void Eps16PlusProcessor::setResourcePath(const juce::Identifier &key,
                                         const juce::String &path) {
    state.setProperty(key, path, nullptr);
}

juce::String Eps16PlusProcessor::getResourcePath(const juce::Identifier &key) const {
    return state.getProperty(key).toString();
}

juce::File Eps16PlusProcessor::defaultResourceDirectory() {
    const auto executable = moduleFile();
    if (!executable.existsAsFile()) return {};
    const auto bundle = executable.getParentDirectory()  // MacOS
                                  .getParentDirectory()  // Contents
                                  .getParentDirectory(); // *.vst3
    return bundle.getParentDirectory().getChildFile("EPS_files");
}

void Eps16PlusProcessor::refreshResourcePaths() {
    const auto directory = defaultResourceDirectory();
    if (!directory.isDirectory()) return;

    auto discover = [this, &directory](const juce::Identifier &key,
                                       std::initializer_list<const char *> names,
                                       std::int64_t fallbackSize,
                                       const juce::String &fallbackPattern) {
        const juce::File selected(getResourcePath(key));
        if (selected.existsAsFile()) return;
        auto found = firstExisting(directory, names);
        if (!found.existsAsFile() && fallbackSize > 0)
            found = firstFileWithSize(directory, fallbackPattern, fallbackSize);
        if (found.existsAsFile()) setResourcePath(key, found.getFullPathName());
    };

    discover(romPathKey, {"eps16plus-rom.bin"}, 131072, "*.bin;*.rom");
    discover(kpcPathKey,
             {"eps16plus-kpc.bin", "Ensoniq EPS KPC2 v2.33 27c256.BIN"},
             32768, "*.bin;*.rom");
    discover(osDiskPathKey, {"EPS130OS.img", "EPS130OS.hfe"}, 819200, "*.img");
    if (!juce::File(getResourcePath(osDiskPathKey)).existsAsFile()) {
        auto hfeFiles = directory.findChildFiles(juce::File::findFiles, false,
                                                 "*.hfe;*.HFE");
        hfeFiles.sort();
        if (!hfeFiles.isEmpty())
            setResourcePath(osDiskPathKey, hfeFiles.getFirst().getFullPathName());
    }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new Eps16PlusProcessor();
}
