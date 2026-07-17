#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstring>
#include <iostream>
#include <thread>

static void configure(Eps16PlusProcessor &processor, char **argv) {
    processor.setResourcePath(Eps16PlusProcessor::romPathKey, argv[1]);
    processor.setResourcePath(Eps16PlusProcessor::kpcPathKey, argv[2]);
    processor.setResourcePath(Eps16PlusProcessor::osDiskPathKey, argv[3]);
    processor.prepareToPlay(48000.0, 128);
}

static void processBlocks(Eps16PlusProcessor &processor, int blocks) {
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block) {
        audio.clear();
        processor.processBlock(audio, midi);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " ROM KPC OS\n";
        return 2;
    }
    const juce::ScopedJuceInitialiser_GUI gui;
    Eps16PlusProcessor first;
    Eps16PlusProcessor second;
    configure(first, argv);
    configure(second, argv);
    if (!first.machineReady() || !second.machineReady()) return 1;

    std::thread firstThread(processBlocks, std::ref(first), 120);
    std::thread secondThread(processBlocks, std::ref(second), 180);
    firstThread.join();
    secondThread.join();

    juce::MemoryBlock firstState;
    juce::MemoryBlock secondState;
    first.getStateInformation(firstState);
    second.getStateInformation(secondState);
    const bool independent = firstState.getSize() == secondState.getSize() &&
        firstState.getSize() > 5000000 &&
        std::memcmp(firstState.getData(), secondState.getData(),
                    firstState.getSize()) != 0;
    std::cout << "first_cycles=" << first.cpuCycles()
              << " second_cycles=" << second.cpuCycles()
              << " first_state=" << firstState.getSize()
              << " second_state=" << secondState.getSize()
              << " independent=" << independent << '\n';
    return independent && first.cpuCycles() == 3200000 &&
           second.cpuCycles() == 4800000 &&
           !first.illegalInstructions() && !second.illegalInstructions()
        ? 0 : 1;
}
