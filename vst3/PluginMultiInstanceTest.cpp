#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

static void configure(Eps16PlusProcessor &processor, char **argv) {
    processor.setResourcePath(Eps16PlusProcessor::romPathKey, argv[1]);
    processor.setResourcePath(Eps16PlusProcessor::kpcPathKey, argv[2]);
    processor.setResourcePath(Eps16PlusProcessor::osDiskPathKey, argv[3]);
    processor.prepareToPlay(48000.0, 128);
}

static void processBlocks(Eps16PlusProcessor &processor, int blocks) {
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    for (int block = 0; block < blocks; ++block) {
        audio.clear();
        processor.processBlock(audio, midi);
    }
}

static std::vector<char> readFile(const char *path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    const auto size = input.tellg();
    if (size <= 0) return {};
    std::vector<char> result(static_cast<std::size_t>(size));
    input.seekg(0);
    return input.read(result.data(), size) ? result : std::vector<char>{};
}

int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) {
        std::cerr << "usage: " << argv[0] << " ROM KPC OS [SNAPSHOT]\n";
        return 2;
    }
    const juce::ScopedJuceInitialiser_GUI gui;
    Eps16PlusProcessor first;
    Eps16PlusProcessor second;
    Eps16PlusProcessor third;
    configure(first, argv);
    configure(second, argv);
    configure(third, argv);
    if (!first.machineReady() || !second.machineReady() ||
        !third.machineReady()) return 1;
    if (argc == 5) {
        const auto snapshot = readFile(argv[4]);
        if (snapshot.empty() ||
            !first.restoreMachineSnapshot(snapshot.data(), snapshot.size()) ||
            !second.restoreMachineSnapshot(snapshot.data(), snapshot.size()) ||
            !third.restoreMachineSnapshot(snapshot.data(), snapshot.size()))
            return 1;
    }

    std::thread firstThread(processBlocks, std::ref(first), 120);
    std::thread secondThread(processBlocks, std::ref(second), 180);
    std::thread thirdThread(processBlocks, std::ref(third), 240);
    firstThread.join();
    secondThread.join();
    thirdThread.join();

    juce::MemoryBlock firstState;
    juce::MemoryBlock secondState;
    juce::MemoryBlock thirdState;
    first.getStateInformation(firstState);
    second.getStateInformation(secondState);
    third.getStateInformation(thirdState);
    const bool independent = firstState.getSize() == secondState.getSize() &&
        secondState.getSize() == thirdState.getSize() &&
        firstState.getSize() > 5000000 &&
        std::memcmp(firstState.getData(), secondState.getData(),
                    firstState.getSize()) != 0 &&
        std::memcmp(secondState.getData(), thirdState.getData(),
                    secondState.getSize()) != 0;
    std::cout << "first_cycles=" << first.cpuCycles()
              << " second_cycles=" << second.cpuCycles()
              << " third_cycles=" << third.cpuCycles()
              << " first_state=" << firstState.getSize()
              << " second_state=" << secondState.getSize()
              << " third_state=" << thirdState.getSize()
              << " independent=" << independent << '\n';
    const bool correct = independent && first.cpuCycles() == 3200000 &&
        second.cpuCycles() == 4800000 && third.cpuCycles() == 6400000 &&
        !first.illegalInstructions() && !second.illegalInstructions() &&
        !third.illegalInstructions();
    if (!correct) return 1;

    const auto sequentialStart = std::chrono::steady_clock::now();
    processBlocks(first, 240);
    processBlocks(second, 240);
    processBlocks(third, 240);
    const auto sequentialEnd = std::chrono::steady_clock::now();
    const auto parallelStart = std::chrono::steady_clock::now();
    std::thread parallelFirst(processBlocks, std::ref(first), 240);
    std::thread parallelSecond(processBlocks, std::ref(second), 240);
    std::thread parallelThird(processBlocks, std::ref(third), 240);
    parallelFirst.join();
    parallelSecond.join();
    parallelThird.join();
    const auto parallelEnd = std::chrono::steady_clock::now();
    const auto sequentialUs = std::chrono::duration_cast<std::chrono::microseconds>(
        sequentialEnd - sequentialStart).count();
    const auto parallelUs = std::chrono::duration_cast<std::chrono::microseconds>(
        parallelEnd - parallelStart).count();
    std::cout << "three_instance_sequential_us=" << sequentialUs
              << " parallel_us=" << parallelUs
              << " speedup=" << static_cast<double>(sequentialUs) /
                                     static_cast<double>(parallelUs) << '\n';
    return 0;
}
