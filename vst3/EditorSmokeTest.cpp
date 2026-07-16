#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

int main(int argc, char **argv) {
    const juce::ScopedJuceInitialiser_GUI gui;
    Eps16PlusProcessor processor;
    processor.setResourcePath(Eps16PlusProcessor::romPathKey, "/tmp/test-rom.bin");
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    if (state.getSize() < 24) return 1;
    Eps16PlusProcessor restoredProcessor;
    restoredProcessor.setStateInformation(state.getData(),
                                           static_cast<int>(state.getSize()));
    if (restoredProcessor.getResourcePath(Eps16PlusProcessor::romPathKey) !=
        "/tmp/test-rom.bin")
        return 1;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (!editor || editor->getWidth() != 1350 || editor->getHeight() != 285)
        return 1;

    const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
    if (!snapshot.isValid() || snapshot.getWidth() != editor->getWidth() ||
        snapshot.getHeight() != editor->getHeight())
        return 1;
    if (argc == 2) {
        const juce::File outputFile(argv[1]);
        outputFile.deleteFile();
        juce::FileOutputStream output(outputFile);
        juce::PNGImageFormat png;
        if (!output.openedOk() || !png.writeImageToStream(snapshot, output))
            return 1;
    }
    return 0;
}
