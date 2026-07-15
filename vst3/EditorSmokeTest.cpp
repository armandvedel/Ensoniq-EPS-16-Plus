#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

int main() {
    const juce::ScopedJuceInitialiser_GUI gui;
    Eps16PlusProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (!editor || editor->getWidth() <= 0 || editor->getHeight() <= 0)
        return 1;

    const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
    if (!snapshot.isValid() || snapshot.getWidth() != editor->getWidth() ||
        snapshot.getHeight() != editor->getHeight())
        return 1;
    return 0;
}
