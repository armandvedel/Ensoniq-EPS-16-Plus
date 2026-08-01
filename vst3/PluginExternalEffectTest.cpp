#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <iostream>

namespace {
constexpr int blockSize = 512;

void runBlocks(Eps16PlusProcessor &processor, int blocks) {
    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block) {
        audio.clear();
        processor.processBlock(audio, midi);
        midi.clear();
    }
}

bool click(Eps16PlusProcessor &processor, std::uint8_t code) {
    if (!processor.enqueuePanelTransition(code, true) ||
        !processor.enqueuePanelTransition(code, false))
        return false;
    runBlocks(processor, 300);
    return true;
}

bool testEffect(const char *rom, const char *kpc, const char *os,
                const char *disk, bool secondEffect) {
    Eps16PlusProcessor processor;
    processor.setResourcePath(Eps16PlusProcessor::romPathKey, rom);
    processor.setResourcePath(Eps16PlusProcessor::kpcPathKey, kpc);
    processor.setResourcePath(Eps16PlusProcessor::osDiskPathKey, os);
    processor.prepareToPlay(48000.0, blockSize);
    for (int block = 0; block < 3000 &&
                        !processor.machineDisplay().startsWith("NO INSTRUMENTS");
         ++block)
        runBlocks(processor, 1);
    if (!processor.machineDisplay().startsWith("NO INSTRUMENTS") ||
        !processor.insertDisk(juce::File(disk)))
        return false;
    runBlocks(processor, 200);
    if (!click(processor, 0x0f) || !click(processor, 0x09)) return false;
    if (secondEffect && !click(processor, 0x0a)) return false;
    const auto selected = processor.machineDisplay();
    std::cout << "selected=|" << selected << "|\n";
    if (!selected.contains(secondEffect ? "FM+FX" : "RESON FILTER") ||
        !click(processor, 0x23))
        return false;
    runBlocks(processor, 1000);
    const auto completed = processor.machineDisplay();
    std::cout << "completed=|" << completed << "| illegal="
              << processor.illegalInstructions() << '\n';
    return completed.startsWith("DISK COMMAND COMPLETED") &&
           !processor.illegalInstructions();
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 5) {
        std::cerr << "usage: " << argv[0] << " ROM KPC OS EFFECT_DISK\n";
        return 2;
    }
    const juce::ScopedJuceInitialiser_GUI gui;
    return testEffect(argv[1], argv[2], argv[3], argv[4], false) &&
                   testEffect(argv[1], argv[2], argv[3], argv[4], true)
               ? 0 : 1;
}
