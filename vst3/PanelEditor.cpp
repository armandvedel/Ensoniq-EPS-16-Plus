#include "PanelEditor.h"

#include <array>

namespace {
const juce::Colour panelColour{0xff35312e};
const juce::Colour buttonColour{0xffd7d1c4};
const juce::Colour displayColour{0xfff2d24b};
}

Eps16PanelEditor::PanelButton::PanelButton(Eps16PlusProcessor &processorToUse,
                                           juce::String label,
                                           std::uint8_t rawCode,
                                           bool mappingKnown)
    : TextButton(std::move(label)), processor(processorToUse), code(rawCode) {
    setColour(buttonColourId, buttonColour);
    setColour(textColourOffId, juce::Colours::black);
    setEnabled(mappingKnown);
    if (!mappingKnown) setTooltip("Wire mapping is not verified yet");
}

void Eps16PanelEditor::PanelButton::mouseDown(const juce::MouseEvent &event) {
    if (isEnabled() && !pressed) {
        pressed = processor.enqueuePanelTransition(code, true);
    }
    TextButton::mouseDown(event);
}

void Eps16PanelEditor::PanelButton::releaseIfNeeded() {
    if (pressed) {
        processor.enqueuePanelTransition(code, false);
        pressed = false;
    }
}

void Eps16PanelEditor::PanelButton::mouseUp(const juce::MouseEvent &event) {
    releaseIfNeeded();
    TextButton::mouseUp(event);
}

void Eps16PanelEditor::PanelButton::mouseExit(const juce::MouseEvent &event) {
    if (!event.mods.isAnyMouseButtonDown()) releaseIfNeeded();
    TextButton::mouseExit(event);
}

Eps16PanelEditor::Eps16PanelEditor(Eps16PlusProcessor &processorToUse)
    : AudioProcessorEditor(processorToUse), owner(processorToUse) {
    setSize(1180, 600);
    setResizable(true, true);
    setResizeLimits(900, 500, 1600, 900);

    vfd.setText(juce::String::repeatedString(" ", 22), juce::dontSendNotification);
    vfd.setJustificationType(juce::Justification::centred);
    vfd.setFont(juce::Font(juce::FontOptions("Menlo", 25.0f,
                                             juce::Font::plain)));
    vfd.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    vfd.setColour(juce::Label::textColourId, displayColour);
    addAndMakeVisible(vfd);

    status.setText("VST3 adapter active - emulator core extraction pending",
                   juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(status);

    const std::array<std::pair<const char *, std::uint8_t>, 33> mapped{{
        {"LOAD", 0x0f}, {"COMMAND", 0x06}, {"EDIT", 0x05},
        {"INSTRUMENT", 0x1a}, {"SEQ-SONG", 0x15}, {"SYSTEM-MIDI", 0x1b},
        {"EFFECTS", 0x09}, {"1 / ENV1", 0x0d}, {"2 / ENV2", 0x12},
        {"3 / ENV3", 0x13}, {"4 / PITCH", 0x18}, {"5 / FILTER", 0x19},
        {"6 / AMP", 0x1e}, {"7 / LFO", 0x1f}, {"8 / WAVE", 0x24},
        {"9 / LAYER", 0x25}, {"0 / TRACK", 0x0c}, {"UP", 0x0a},
        {"DOWN", 0x0b}, {"LEFT", 0x10}, {"RIGHT", 0x11},
        {"CANCEL / NO", 0x21}, {"ENTER / YES", 0x23},
        {"TRACK 1", 0x02}, {"TRACK 2", 0x08}, {"TRACK 3", 0x0e},
        {"TRACK 4", 0x14}, {"TRACK 5", 0x04}, {"TRACK 6", 0x22},
        {"TRACK 7", 0x1c}, {"TRACK 8", 0x16},
        {"EFFECT SELECT", 0x07}, {"SAMPLE", 0x20}
    }};
    for (const auto &[label, code] : mapped) addPanelButton(label, code);
    addPanelButton("RECORD", 0, false);
    addPanelButton("STOP / CONTINUE", 0, false);
    addPanelButton("PLAY", 0, false);

    auto configureFader = [this](juce::Slider &slider, const juce::String &name) {
        slider.setName(name);
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 20);
        slider.setRange(0, 1023, 1);
        addAndMakeVisible(slider);
    };
    configureFader(masterVolume, "VOLUME");
    configureFader(dataEntry, "DATA ENTRY");
    masterVolume.setValue(1023, juce::dontSendNotification);
    dataEntry.setValue(512, juce::dontSendNotification);
    masterVolume.onValueChange = [this] {
        this->owner.enqueueAnalog(5,
            static_cast<std::uint16_t>(masterVolume.getValue()));
    };
    dataEntry.onValueChange = [this] {
        const auto gui = static_cast<unsigned int>(dataEntry.getValue());
        this->owner.enqueueAnalog(3,
            static_cast<std::uint16_t>((gui * 715U) / 1023U));
    };

    for (auto *row : {&romRow, &kpcRow, &diskRow}) {
        row->path.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        row->path.setText(owner.getResourcePath(row->key),
                          juce::dontSendNotification);
        addAndMakeVisible(row->chooser);
        addAndMakeVisible(row->path);
    }
    romRow.chooser.onClick = [this] {
        chooseResource(romRow, "Select combined 128 KiB EPS ROM", "*.bin;*.rom");
    };
    kpcRow.chooser.onClick = [this] {
        chooseResource(kpcRow, "Select external 32 KiB KPC ROM", "*.bin;*.rom");
    };
    diskRow.chooser.onClick = [this] {
        chooseResource(diskRow, "Select external EPS OS disk", "*.img;*.hfe");
    };
    /* setSize() runs resized() near the start of this constructor, before the
       dynamically-created panel buttons exist. Lay out once more after every
       child has been added so hosts that keep the initial size do not leave
       the entire button matrix at its default zero bounds. */
    resized();
    startTimerHz(4);
}

Eps16PanelEditor::PanelButton &Eps16PanelEditor::addPanelButton(
    const juce::String &label, std::uint8_t code, bool known) {
    auto button = std::make_unique<PanelButton>(owner, label, code, known);
    auto &reference = *button;
    addAndMakeVisible(reference);
    buttons.push_back(std::move(button));
    return reference;
}

void Eps16PanelEditor::chooseResource(ResourceRow &row, const juce::String &title,
                                      const juce::String &pattern) {
    row.fileChooser = std::make_unique<juce::FileChooser>(
        title, juce::File(row.path.getText()), pattern);
    auto *rowPointer = &row;
    row.fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                     juce::FileBrowserComponent::canSelectFiles,
                                 [this, rowPointer](const juce::FileChooser &chooser) {
        const auto file = chooser.getResult();
        if (file.existsAsFile()) {
            const auto fullPath = file.getFullPathName();
            owner.setResourcePath(rowPointer->key, fullPath);
            rowPointer->path.setText(fullPath, juce::dontSendNotification);
        }
        rowPointer->fileChooser.reset();
    });
}

void Eps16PanelEditor::timerCallback() {
    status.setText("VST3 adapter active | DAW-driven CPU cycles: " +
                       juce::String(owner.cpuCycles()) +
                       " | emulator core extraction pending",
                   juce::dontSendNotification);
}

void Eps16PanelEditor::paint(juce::Graphics &graphics) {
    graphics.fillAll(panelColour);
    graphics.setColour(juce::Colours::white);
    graphics.setFont(22.0f);
    graphics.drawText("ENSONIQ  EPS-16 PLUS", 18, 8, 310, 30,
                      juce::Justification::centredLeft);
    graphics.setFont(12.0f);
    graphics.drawText("MUSICAL INSTRUMENT / DIGITAL SAMPLING WORKSTATION",
                      18, 34, 390, 18, juce::Justification::centredLeft);
    graphics.drawText("VOLUME", getWidth() - 170, 58, 70, 18,
                      juce::Justification::centred);
    graphics.drawText("DATA ENTRY", getWidth() - 90, 58, 78, 18,
                      juce::Justification::centred);
}

void Eps16PanelEditor::resized() {
    const int margin = 18;
    vfd.setBounds(420, 12, juce::jmax(300, getWidth() - 620), 42);
    status.setBounds(margin, 58, getWidth() - 220, 22);
    masterVolume.setBounds(getWidth() - 172, 78, 70, 210);
    dataEntry.setBounds(getWidth() - 91, 78, 72, 210);

    int y = 92;
    const int availableWidth = getWidth() - 220;
    const int columns = 7;
    const int gap = 6;
    const int buttonWidth = (availableWidth - (columns - 1) * gap) / columns;
    const int buttonHeight = 42;
    for (std::size_t index = 0; index < buttons.size(); ++index) {
        const int column = static_cast<int>(index % columns);
        const int row = static_cast<int>(index / columns);
        buttons[index]->setBounds(margin + column * (buttonWidth + gap),
                                  y + row * (buttonHeight + gap),
                                  buttonWidth, buttonHeight);
    }

    int resourceY = y + 6 * (buttonHeight + gap) + 12;
    for (auto *row : {&romRow, &kpcRow, &diskRow}) {
        row->chooser.setBounds(margin, resourceY, 105, 28);
        row->path.setBounds(margin + 112, resourceY, getWidth() - 150, 28);
        resourceY += 31;
    }
}
