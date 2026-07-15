#include "PanelEditor.h"

#include <array>

namespace {
const juce::Colour panelColour{0xff35312e};
const juce::Colour buttonColour{0xff5b5955};
const juce::Colour displayColour{0xfff2d24b};
const juce::Colour rackLabelColour{0xffe8e5df};
}

void Eps16PanelEditor::VfdLabel::setCursorRange(int start, int end) {
    start = juce::jlimit(-1, 22, start);
    end = juce::jlimit(-1, 22, end);
    if (cursorStart == start && cursorEnd == end) return;
    cursorStart = start;
    cursorEnd = end;
    repaint();
}

void Eps16PanelEditor::VfdLabel::paint(juce::Graphics &graphics) {
    juce::Label::paint(graphics);
    if (cursorStart < 0 || cursorEnd <= cursorStart) return;

    const auto textArea = getBorderSize().subtractedFrom(getLocalBounds())
                                         .toFloat();
    const auto font = getFont();
    const float naturalWidth =
        juce::GlyphArrangement::getStringWidth(font, getText());
    if (naturalWidth <= 0.0f) return;
    const float horizontalScale = juce::jmin(1.0f,
                                              textArea.getWidth() / naturalWidth);
    const float cellWidth =
        juce::GlyphArrangement::getStringWidth(font, "M") * horizontalScale;
    const float renderedWidth = naturalWidth * horizontalScale;
    const float textLeft = textArea.getCentreX() - renderedWidth * 0.5f;
    const float cursorLeft = textLeft + cellWidth * (float)cursorStart;
    const float cursorWidth = cellWidth * (float)(cursorEnd - cursorStart);
    const float cursorY = textArea.getCentreY() + font.getHeight() * 0.43f;

    graphics.setColour(findColour(juce::Label::textColourId));
    graphics.fillRect(cursorLeft, cursorY, cursorWidth, 2.0f);
}

Eps16PanelEditor::PanelButton::PanelButton(Eps16PlusProcessor &processorToUse,
                                           juce::String label,
                                           std::uint8_t rawCode,
                                           bool mappingKnown)
    : processor(processorToUse), code(rawCode) {
    setName(label);
    setTooltip(label);
    setColour(buttonColourId, buttonColour);
    setColour(buttonOnColourId, buttonColour.brighter(0.18f));
    setColour(textColourOffId, rackLabelColour);
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
    setSize(1280, 650);
    setResizable(true, true);
    setResizeLimits(1100, 580, 1600, 850);

    vfd.setText(juce::String::repeatedString(" ", 22), juce::dontSendNotification);
    vfd.setJustificationType(juce::Justification::centred);
    vfd.setFont(juce::Font(juce::FontOptions("Menlo", 25.0f,
                                             juce::Font::plain)));
    vfd.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    vfd.setColour(juce::Label::textColourId, displayColour);
    addAndMakeVisible(vfd);

    status.setText("VST3 adapter active - waiting for authentic emulator boot",
                   juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(status);

    const std::array<std::pair<const char *, std::uint8_t>, 12> pages{{
        {"1 / ENV 1", 0x0d}, {"2 / ENV 2", 0x12}, {"3 / ENV 3", 0x13},
        {"4 / PITCH", 0x18}, {"5 / FILTER", 0x19}, {"6 / AMP", 0x1e},
        {"7 / LFO", 0x1f}, {"8 / WAVE", 0x24}, {"9 / LAYER", 0x25},
        {"SAMPLE", 0x20}, {"0 / TRACK", 0x0c}, {"EFFECT SELECT / BYPASS", 0x07}
    }};
    for (std::size_t index = 0; index < pages.size(); ++index)
        pageButtons[index] = &addPanelButton(pages[index].first,
                                             pages[index].second);

    const std::array<std::pair<const char *, std::uint8_t>, 7> modes{{
        {"LOAD", 0x0f}, {"CMD", 0x06}, {"EDIT", 0x05},
        {"INST", 0x1a}, {"SEQ SONG", 0x15}, {"SYSTEM MIDI", 0x1b},
        {"EFFECTS", 0x09}
    }};
    for (std::size_t index = 0; index < modes.size(); ++index)
        modeButtons[index] = &addPanelButton(modes[index].first,
                                             modes[index].second);

    const std::array<std::uint8_t, 8> tracks{
        0x02, 0x08, 0x0e, 0x14, 0x04, 0x22, 0x1c, 0x16
    };
    for (std::size_t index = 0; index < tracks.size(); ++index)
        trackButtons[index] = &addPanelButton(
            "INSTRUMENT / TRACK " + juce::String((int)index + 1), tracks[index]);

    upButton = &addPanelButton("UP", 0x0a);
    downButton = &addPanelButton("DOWN", 0x0b);
    leftButton = &addPanelButton("LEFT", 0x10);
    rightButton = &addPanelButton("RIGHT", 0x11);
    cancelButton = &addPanelButton("NO / CANCEL", 0x21);
    enterButton = &addPanelButton("YES / ENTER", 0x23);
    sequencerButtons[0] = &addPanelButton("RECORD", 0, false);
    sequencerButtons[1] = &addPanelButton("STOP / CONT", 0, false);
    sequencerButtons[2] = &addPanelButton("PLAY", 0, false);

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
    const juce::File current(row.path.getText());
    const auto initial = current.existsAsFile()
                             ? current
                             : Eps16PlusProcessor::defaultResourceDirectory();
    row.fileChooser = std::make_unique<juce::FileChooser>(
        title, initial, pattern);
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
    owner.refreshResourcePaths();
    for (auto *row : {&romRow, &kpcRow, &diskRow}) {
        const auto discovered = owner.getResourcePath(row->key);
        if (row->path.getText() != discovered)
            row->path.setText(discovered, juce::dontSendNotification);
    }
    vfd.setText(owner.machineDisplay(), juce::dontSendNotification);
    vfd.setCursorRange(owner.machineCursorStart(), owner.machineCursorEnd());
    status.setText("DAW-driven CPU cycles: " + juce::String(owner.cpuCycles()) +
                       " | " + owner.machineStatus() +
                       " | illegal instructions: " +
                       juce::String(owner.illegalInstructions()),
                   juce::dontSendNotification);
}

void Eps16PanelEditor::paint(juce::Graphics &graphics) {
    graphics.fillAll(panelColour);
    graphics.setColour(rackLabelColour);

    graphics.setFont(14.0f);
    graphics.drawText("ENSONIQ", 24, 18, 140, 22,
                      juce::Justification::centredLeft);
    graphics.setFont(31.0f);
    graphics.drawText("EPS-16", 24, 41, 145, 42,
                      juce::Justification::centredLeft);
    graphics.setFont(11.0f);
    graphics.drawText("PLUS", 126, 72, 40, 18,
                      juce::Justification::centred);
    graphics.setFont(13.0f);
    graphics.drawText("DIGITAL  SAMPLING  WORKSTATION", 410, 18, 430, 22,
                      juce::Justification::centred);

    auto above = [&graphics](const PanelButton *button,
                             const juce::String &text, int height = 17) {
        const auto bounds = button->getBounds();
        graphics.drawText(text, bounds.getX() - 8, bounds.getY() - height,
                          bounds.getWidth() + 16, height,
                          juce::Justification::centred);
    };
    auto below = [&graphics](const PanelButton *button,
                             const juce::String &text, int height = 17) {
        const auto bounds = button->getBounds();
        graphics.drawText(text, bounds.getX() - 14, bounds.getBottom(),
                          bounds.getWidth() + 28, height,
                          juce::Justification::centred);
    };

    graphics.setFont(11.5f);
    static const char *pageTop[12] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "0", ""
    };
    static const char *pageBottom[12] = {
        "ENV 1", "ENV 2", "ENV 3", "PITCH", "FILTER", "AMP",
        "LFO", "WAVE", "LAYER", "SAMPLE", "TRACK", "EFFECT SELECT"
    };
    for (std::size_t index = 0; index < pageButtons.size(); ++index) {
        if (*pageTop[index]) above(pageButtons[index], pageTop[index]);
        below(pageButtons[index], pageBottom[index]);
    }
    graphics.setFont(9.5f);
    graphics.drawText("BYPASS", pageButtons[11]->getX() - 14,
                      pageButtons[11]->getBottom() + 15,
                      pageButtons[11]->getWidth() + 28, 16,
                      juce::Justification::centred);

    graphics.setFont(11.0f);
    static const char *modeLabels[7] = {
        "LOAD", "CMD", "EDIT", "INST", "SEQ SONG", "SYSTEM MIDI", "EFFECTS"
    };
    for (std::size_t index = 0; index < modeButtons.size(); ++index)
        above(modeButtons[index], modeLabels[index], 20);

    graphics.setFont(12.0f);
    const int trackHeaderX = trackButtons.front()->getX();
    const int trackHeaderRight = trackButtons.back()->getRight();
    graphics.drawText("INSTRUMENTS  •  TRACKS", trackHeaderX,
                      trackButtons.front()->getY() - 31,
                      trackHeaderRight - trackHeaderX, 18,
                      juce::Justification::centred);
    for (std::size_t index = 0; index < trackButtons.size(); ++index)
        below(trackButtons[index], juce::String((int)index + 1));

    graphics.setFont(11.0f);
    static const char *sequenceLabels[3] = {"RECORD", "STOP • CONT", "PLAY"};
    for (std::size_t index = 0; index < sequencerButtons.size(); ++index)
        above(sequencerButtons[index], sequenceLabels[index], 20);
    graphics.drawText("SEQUENCER", sequencerButtons.front()->getX(),
                      sequencerButtons.front()->getBottom() + 10,
                      sequencerButtons.back()->getRight() -
                          sequencerButtons.front()->getX(), 18,
                      juce::Justification::centred);

    graphics.setFont(20.0f);
    above(upButton, juce::CharPointer_UTF8("\xe2\x96\xb3"), 25);
    below(downButton, juce::CharPointer_UTF8("\xe2\x96\xbd"), 25);
    graphics.drawText(juce::CharPointer_UTF8("\xe2\x97\x81"),
                      leftButton->getX() - 28, leftButton->getY(), 24,
                      leftButton->getHeight(), juce::Justification::centred);
    graphics.drawText(juce::CharPointer_UTF8("\xe2\x96\xb7"),
                      rightButton->getRight() + 4, rightButton->getY(), 24,
                      rightButton->getHeight(), juce::Justification::centred);

    graphics.setFont(10.5f);
    above(cancelButton, "NO • CANCEL", 28);
    above(enterButton, "YES • ENTER", 28);
    graphics.drawText("VOLUME", masterVolume.getX() - 8,
                      masterVolume.getY() - 21, masterVolume.getWidth() + 16,
                      18, juce::Justification::centred);
    graphics.drawText("DATA ENTRY", dataEntry.getX() - 18,
                      dataEntry.getBottom() + 3, dataEntry.getWidth() + 36,
                      18, juce::Justification::centred);

    graphics.setColour(juce::Colour(0xff76516f));
    graphics.fillRect(180, romRow.chooser.getY() - 12,
                      getWidth() - 230, 2);
}

void Eps16PanelEditor::resized() {
    const float scale = juce::jmin((float)getWidth() / 1280.0f,
                                   (float)(getHeight() - 120) / 530.0f);
    const int offsetX = (getWidth() - juce::roundToInt(1280.0f * scale)) / 2;
    auto rackRect = [scale, offsetX](int x, int y, int width, int height) {
        return juce::Rectangle<int>(
            offsetX + juce::roundToInt((float)x * scale),
            juce::roundToInt((float)y * scale),
            juce::roundToInt((float)width * scale),
            juce::roundToInt((float)height * scale));
    };

    vfd.setBounds(rackRect(405, 48, 440, 72));
    status.setBounds(rackRect(405, 125, 440, 20));
    masterVolume.setBounds(rackRect(105, 125, 66, 245));
    dataEntry.setBounds(rackRect(1135, 135, 72, 255));

    for (std::size_t index = 0; index < pageButtons.size(); ++index) {
        const int column = (int)(index % 3);
        const int row = (int)(index / 3);
        pageButtons[index]->setBounds(rackRect(205 + column * 62,
                                               105 + row * 74, 50, 23));
    }

    for (std::size_t index = 0; index < modeButtons.size(); ++index) {
        const int gap = index >= 3 ? 22 : 0;
        modeButtons[index]->setBounds(rackRect(410 + (int)index * 66 + gap,
                                               235, 55, 24));
    }
    for (std::size_t index = 0; index < trackButtons.size(); ++index)
        trackButtons[index]->setBounds(rackRect(410 + (int)index * 61,
                                                330, 50, 27));

    for (std::size_t index = 0; index < sequencerButtons.size(); ++index)
        sequencerButtons[index]->setBounds(rackRect(900 + (int)index * 61,
                                                    80, 52, 23));
    upButton->setBounds(rackRect(960, 170, 56, 25));
    leftButton->setBounds(rackRect(900, 235, 56, 25));
    downButton->setBounds(rackRect(960, 235, 56, 25));
    rightButton->setBounds(rackRect(1020, 235, 56, 25));
    cancelButton->setBounds(rackRect(900, 335, 58, 32));
    enterButton->setBounds(rackRect(1020, 335, 58, 32));

    int resourceY = getHeight() - 103;
    const int margin = 18;
    for (auto *row : {&romRow, &kpcRow, &diskRow}) {
        row->chooser.setBounds(margin, resourceY, 105, 28);
        row->path.setBounds(margin + 112, resourceY, getWidth() - 150, 28);
        resourceY += 31;
    }
}
