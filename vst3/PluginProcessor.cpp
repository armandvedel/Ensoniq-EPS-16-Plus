#include "PluginProcessor.h"
#include "PanelEditor.h"

#include <algorithm>
#include <dlfcn.h>

namespace {
constexpr std::uint32_t vstStateMagic = 0x45505356U; // EPSV
constexpr std::uint32_t vstStateVersion = 1;

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
const juce::Identifier Eps16PlusProcessor::mountedDiskPathKey{"mountedDiskPath"};
const juce::Identifier Eps16PlusProcessor::blankDiskMountedKey{"blankDiskMounted"};

Eps16PlusProcessor::Eps16PlusProcessor()
    : AudioProcessor(
#if JucePlugin_Build_AU
        BusesProperties()
            .withInput("Sampling Input",
                       juce::AudioChannelSet::stereo(),
                       false)
            .withOutput("Main Output",
                        juce::AudioChannelSet::stereo(),
                        true)
#else
        BusesProperties()
            .withInput("Main Input",
                       juce::AudioChannelSet::stereo(),
                       false)
            .withInput("Sampling Input",
                       juce::AudioChannelSet::stereo(),
                       true)
            .withOutput("Main Output",
                        juce::AudioChannelSet::stereo(),
                        true)
#endif
    )
{
    refreshResourcePaths();
}

void Eps16PlusProcessor::prepareToPlay(double sampleRate, int) {
    refreshResourcePaths();
    machineSink.configure(getResourcePath(romPathKey).toStdString(),
                          getResourcePath(kpcPathKey).toStdString(),
                          getResourcePath(osDiskPathKey).toStdString());
    bridge.prepare(sampleRate);
    hostMidiClock.prepare(sampleRate);
    if (machineSink.isReady() && pendingMachineState.getSize() > 0 &&
        machineSink.restoreState(pendingMachineState.getData(),
                                 pendingMachineState.getSize())) {
        bridge.resetTimeline();
        pendingMachineState.reset();
    }
    setLatencySamples(eps16::vst3::BandlimitedResampler::latencySamples(sampleRate));
}

bool Eps16PlusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if JucePlugin_Build_AU

    // AU: one optional stereo input
    if (layouts.inputBuses.size() == 0)
        return true;

    if (layouts.inputBuses.size() == 1)
    {
        auto input = layouts.getChannelSet (true, 0);
        return input.isDisabled()
            || input == juce::AudioChannelSet::stereo();
    }

    return false;

#else

    // VST3: disabled main input + stereo sampling input
    if (layouts.inputBuses.size() != 2)
        return false;

    return layouts.getChannelSet (true, 0).isDisabled()
        && layouts.getChannelSet (true, 1) == juce::AudioChannelSet::stereo();

#endif
}

void Eps16PlusProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                      juce::MidiBuffer &midi) 
{
    juce::ScopedNoDenormals noDenormals;
    const auto samples = buffer.getNumSamples();
    
    
#if JucePlugin_Build_AU
    constexpr int samplingBus = 0;
#else
    constexpr int samplingBus = 1;
#endif

    auto samplingInput = getBusBuffer(buffer, true, samplingBus);
    auto mainOutput    = getBusBuffer(buffer, false, 0);

    const float* inputLeft  = nullptr;
    const float* inputRight = nullptr;

    if (samplingInput.getNumChannels() > 0)
        inputLeft = samplingInput.getReadPointer(0);

    if (samplingInput.getNumChannels() > 1)
        inputRight = samplingInput.getReadPointer(1);
    else
        inputRight = inputLeft;

    std::size_t eventCount = 0;
    if (const auto *playHead = getPlayHead()) {
        if (const auto position = playHead->getPosition()) {
            const auto bpm = position->getBpm();
            const auto ppq = position->getPpqPosition();
            if (bpm && ppq)
                eventCount += hostMidiClock.generate(
                    true, position->getIsPlaying(), *bpm, *ppq, samples,
                    midiEvents.data(), midiEvents.size());
        }
    }
    for (const auto metadata : midi)
    {
        if (eventCount == midiEvents.size())
            break;
        
        const auto message = metadata.getMessage();

        if (message.isSysEx())
            continue;
        
        const auto* raw = message.getRawData();
        const auto length = message.getRawDataSize();

        if (length < 1)
            continue;

        midiEvents[eventCount++] =
        {
            juce::jlimit(0, samples, metadata.samplePosition),
            raw[0],
            static_cast<std::uint8_t>(length > 1 ? raw[1] : 0),
            static_cast<std::uint8_t>(length > 2 ? raw[2] : 0)};

    std::stable_sort(midiEvents.begin(), midiEvents.begin() + eventCount,
                     [](const auto &left, const auto &right) {
                         return left.sampleOffset < right.sampleOffset;
                     });
    }

    

    bridge.process(inputLeft,
                   inputRight,
                   mainOutput.getWritePointer(0),
                   mainOutput.getWritePointer(1),
                   samples,
                   midiEvents.data(),
                   eventCount);

    midi.clear();
}

juce::AudioProcessorEditor *Eps16PlusProcessor::createEditor() {
    return new Eps16PanelEditor(*this);
}

void Eps16PlusProcessor::getStateInformation(juce::MemoryBlock &destination) {
    const juce::ScopedLock lock(getCallbackLock());
    const auto xmlText = state.toXmlString();
    const auto machine = machineSink.captureState();
    juce::MemoryOutputStream output(destination, false);
    output.writeInt(static_cast<int>(vstStateMagic));
    output.writeInt(static_cast<int>(vstStateVersion));
    output.writeInt64(static_cast<juce::int64>(xmlText.getNumBytesAsUTF8()));
    output.writeInt64(static_cast<juce::int64>(machine.size()));
    output.write(xmlText.toRawUTF8(), xmlText.getNumBytesAsUTF8());
    if (!machine.empty()) output.write(machine.data(), machine.size());
}

bool Eps16PlusProcessor::restoreMachineSnapshot(const void *data,
                                                std::size_t size) {
    const juce::ScopedLock lock(getCallbackLock());
    if (!machineSink.restoreState(data, size)) return false;
    bridge.resetTimeline();
    hostMidiClock.reset();
    return true;
}

void Eps16PlusProcessor::setStateInformation(const void *data, int size) {
    juce::ValueTree restoredTree;
    juce::MemoryBlock restoredMachine;
    bool recognizedContainer = false;
    if (data && size >= 24) {
        juce::MemoryInputStream input(data, static_cast<std::size_t>(size), false);
        const auto magic = static_cast<std::uint32_t>(input.readInt());
        const auto version = static_cast<std::uint32_t>(input.readInt());
        const auto xmlSize = input.readInt64();
        const auto machineSize = input.readInt64();
        const auto remaining = static_cast<juce::int64>(size) - 24;
        recognizedContainer = magic == vstStateMagic;
        if (magic == vstStateMagic && version == vstStateVersion &&
            xmlSize >= 0 && machineSize >= 0 &&
            xmlSize + machineSize == remaining && xmlSize <= 1024 * 1024 &&
            machineSize <= 128 * 1024 * 1024) {
            juce::MemoryBlock xmlData(static_cast<std::size_t>(xmlSize) + 1, true);
            if (input.read(xmlData.getData(), static_cast<int>(xmlSize)) == xmlSize) {
                const auto xml = juce::parseXML(juce::String::fromUTF8(
                    static_cast<const char *>(xmlData.getData()),
                    static_cast<int>(xmlSize)));
                if (xml) restoredTree = juce::ValueTree::fromXml(*xml);
                restoredMachine.setSize(static_cast<std::size_t>(machineSize));
                if (machineSize > 0 &&
                    input.read(restoredMachine.getData(),
                               static_cast<int>(machineSize)) != machineSize)
                    restoredMachine.reset();
            }
        }
    }
    if (!recognizedContainer)
        if (const auto xml = getXmlFromBinary(data, size))
            restoredTree = juce::ValueTree::fromXml(*xml);

    if (!restoredTree.hasType(state.getType())) return;
    const juce::ScopedLock lock(getCallbackLock());
    state = restoredTree;
    refreshResourcePaths();
    pendingMachineState = restoredMachine;
    if (machineSink.isReady() && pendingMachineState.getSize() > 0) {
        bridge.resetTimeline();
        hostMidiClock.reset();
        if (machineSink.restoreState(pendingMachineState.getData(),
                                     pendingMachineState.getSize()))
            pendingMachineState.reset();
    }
}

void Eps16PlusProcessor::setResourcePath(const juce::Identifier &key,
                                         const juce::String &path) {
    state.setProperty(key, path, nullptr);
}

juce::String Eps16PlusProcessor::getResourcePath(const juce::Identifier &key) const {
    return state.getProperty(key).toString();
}

bool Eps16PlusProcessor::insertOsDisk() {
    refreshResourcePaths();
    const juce::File diskFile(getResourcePath(osDiskPathKey));
    if (!diskFile.existsAsFile()) return false;
    const juce::ScopedLock lock(getCallbackLock());
    if (!machineSink.insertDisk(diskFile.getFullPathName().toStdString(),
                                "OS disk"))
        return false;
    state.setProperty(mountedDiskPathKey, diskFile.getFullPathName(), nullptr);
    state.setProperty(blankDiskMountedKey, false, nullptr);
    return true;
}

bool Eps16PlusProcessor::insertDisk(const juce::File &diskFile) {
    if (!diskFile.existsAsFile()) return false;
    const auto extension = diskFile.getFileExtension().toLowerCase();
    if (extension != ".img" && extension != ".hfe") return false;
    const juce::ScopedLock lock(getCallbackLock());
    if (!machineSink.insertDisk(diskFile.getFullPathName().toStdString(),
                                "Disk"))
        return false;
    state.setProperty(mountedDiskPathKey, diskFile.getFullPathName(), nullptr);
    state.setProperty(blankDiskMountedKey, false, nullptr);
    return true;
}

bool Eps16PlusProcessor::createBlankDisk() {
    const juce::ScopedLock lock(getCallbackLock());
    if (!machineSink.createBlankDisk()) return false;
    state.setProperty(mountedDiskPathKey, juce::String(), nullptr);
    state.setProperty(blankDiskMountedKey, true, nullptr);
    return true;
}

bool Eps16PlusProcessor::blankDiskMounted() const {
    return static_cast<bool>(state.getProperty(blankDiskMountedKey, false));
}

bool Eps16PlusProcessor::saveDisk(const juce::File &diskFile) {
    auto output = diskFile;
    auto extension = output.getFileExtension().toLowerCase();
    if (extension != ".img" && extension != ".hfe") {
        output = output.withFileExtension(".img");
        extension = ".img";
    }
    const juce::ScopedLock lock(getCallbackLock());
    if (!machineSink.saveDisk(output.getFullPathName().toStdString(),
                              extension == ".hfe"))
        return false;
    state.setProperty(mountedDiskPathKey, output.getFullPathName(), nullptr);
    state.setProperty(blankDiskMountedKey, false, nullptr);
    return true;
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
