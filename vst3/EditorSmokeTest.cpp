#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

int main(int argc, char **argv) {
    const juce::ScopedJuceInitialiser_GUI gui;
    Eps16PlusProcessor processor;
    processor.setResourcePath(Eps16PlusProcessor::romPathKey, "/tmp/test-rom.bin");
    processor.setResourcePath(Eps16PlusProcessor::mountedDiskPathKey,
                              "/tmp/TEST-DISK.hfe");
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
    auto *vfd = dynamic_cast<juce::Label *>(
        editor->findChildWithID("vfd-display"));
    if (!vfd) return 1;
    vfd->setText("MODE F1=3/LP F2=1/LP", juce::dontSendNotification);
    if (!editor->findChildWithID("os-disk-button") ||
        !editor->findChildWithID("new-disk-button") ||
        !editor->findChildWithID("load-disk-button") ||
        !editor->findChildWithID("save-disk-button"))
        return 1;
    auto *diskName = dynamic_cast<juce::Label *>(
        editor->findChildWithID("mounted-disk-name"));
    if (!diskName || diskName->getText() != "DISK: TEST-DISK.hfe" ||
        diskName->getBounds().isEmpty())
        return 1;
    auto *dataEntry = dynamic_cast<juce::Slider *>(
        editor->findChildWithID("data-entry-slider"));
    auto *volume = dynamic_cast<juce::Slider *>(
        editor->findChildWithID("volume-slider"));
    if (!dataEntry || !volume || !dataEntry->isEnabled() ||
        dataEntry->getMouseClickGrabsKeyboardFocus())
        return 1;
    bool recordEnabled = false;
    bool stopEnabled = false;
    bool playEnabled = false;
    bool playChordDocumented = false;
    for (int index = 0; index < editor->getNumChildComponents(); ++index) {
        auto *button = dynamic_cast<juce::TextButton *>(
            editor->getChildComponent(index));
        if (!button) continue;
        if (button->getName() == "RECORD") recordEnabled = button->isEnabled();
        if (button->getName() == "STOP / CONT") stopEnabled = button->isEnabled();
        if (button->getName() == "PLAY") {
            playEnabled = button->isEnabled();
            playChordDocumented = button->getTooltip().contains("Shift-click");
        }
    }
    if (!recordEnabled || !stopEnabled || !playEnabled ||
        !playChordDocumented)
        return 1;
    dataEntry->setValue(1023, juce::sendNotificationSync);
    if (dataEntry->getValue() != 1023) return 1;
    if (!editor->keyPressed(juce::KeyPress(juce::KeyPress::upKey)) ||
        !editor->keyPressed(juce::KeyPress(juce::KeyPress::downKey)) ||
        !editor->keyPressed(juce::KeyPress(juce::KeyPress::leftKey)) ||
        !editor->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) ||
        editor->keyPressed(juce::KeyPress('A')))
        return 1;
    editor->focusLost(juce::Component::focusChangedDirectly);
    dataEntry->setValue(512, juce::dontSendNotification);

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
