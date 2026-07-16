#include "PanelEditor.h"

#include <array>

namespace {
constexpr int rackWidth = 1350;
constexpr int rackHeight = 285;
const juce::Colour panelColour{0xff3b3d3c};
const juce::Colour buttonColour{0xff252625};
const juce::Colour displayColour{0xfff2d24b};
const juce::Colour rackLabelColour{0xffe8e5df};
const juce::Colour accentColour{0xff9d4f82};
}

void Eps16PanelEditor::VfdLabel::setCursorRange(int start, int end) {
    start = juce::jlimit(-1, 22, start);
    end = juce::jlimit(-1, 22, end);
    if (cursorStart == start && cursorEnd == end) return;
    cursorStart = start;
    cursorEnd = end;
    repaint();
}

void Eps16PanelEditor::VfdLabel::setDecimalMask(std::uint32_t mask) {
    mask &= 0x3fffffU;
    if (decimalMask == mask) return;
    decimalMask = mask;
    repaint();
}

void Eps16PanelEditor::VfdLabel::paint(juce::Graphics &graphics) {
    graphics.fillAll(findColour(juce::Label::backgroundColourId));
    const auto textArea = getLocalBounds().reduced(8, 6).toFloat();
    const auto font = getFont();
    const float cellWidth = textArea.getWidth() / 22.0f;
    const float baselineY = textArea.getCentreY() + font.getHeight() * 0.34f;

    graphics.setColour(findColour(juce::Label::textColourId));
    graphics.setFont(font);
    const auto text = getText().paddedRight(' ', 22).substring(0, 22);
    for (int index = 0; index < 22; ++index) {
        const auto cell = juce::Rectangle<float>(
            textArea.getX() + cellWidth * (float)index, textArea.getY(),
            cellWidth, textArea.getHeight());
        graphics.drawText(text.substring(index, index + 1), cell,
                          juce::Justification::centred, false);
        if (!(decimalMask & (UINT32_C(1) << index))) continue;
        const float dotX = cell.getRight() - 3.5f;
        graphics.fillEllipse(dotX, baselineY - 2.0f, 3.0f, 3.0f);
    }
    if (cursorStart >= 0 && cursorEnd > cursorStart) {
        const float cursorLeft = textArea.getX() + cellWidth * (float)cursorStart;
        const float cursorWidth = cellWidth * (float)(cursorEnd - cursorStart);
        graphics.fillRect(cursorLeft, baselineY + 2.0f, cursorWidth, 2.0f);
    }
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
    setSize(rackWidth, rackHeight);
    setResizable(true, true);
    setResizeLimits(1080, 228, 1620, 342);
    if (auto *constrainer = getConstrainer())
        constrainer->setFixedAspectRatio((double)rackWidth / rackHeight);

    vfd.setText(juce::String::repeatedString(" ", 22), juce::dontSendNotification);
    vfd.setJustificationType(juce::Justification::centredLeft);
    vfd.setFont(juce::Font(juce::FontOptions("Menlo", 20.0f,
                                             juce::Font::plain)));
    vfd.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    vfd.setColour(juce::Label::textColourId, displayColour);
    addAndMakeVisible(vfd);

    status.setText("VST3 adapter active - waiting for authentic emulator boot",
                   juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addChildComponent(status);

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
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
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

void Eps16PanelEditor::timerCallback() {
    owner.refreshResourcePaths();
    vfd.setText(owner.machineDisplay(), juce::dontSendNotification);
    vfd.setCursorRange(owner.machineCursorStart(), owner.machineCursorEnd());
    vfd.setDecimalMask(owner.machineDecimalMask());
    status.setText("DAW-driven CPU cycles: " + juce::String(owner.cpuCycles()) +
                       " | " + owner.machineStatus() +
                       " | illegal instructions: " +
                       juce::String(owner.illegalInstructions()),
                   juce::dontSendNotification);
}

void Eps16PanelEditor::paint(juce::Graphics &graphics) {
    graphics.fillAll(panelColour);
    if (pageButtons.front() == nullptr) return;
    const float scale = juce::jmin((float)getWidth() / rackWidth,
                                   (float)getHeight() / rackHeight);
    graphics.setColour(rackLabelColour);

    auto above = [&graphics, scale](const PanelButton *button,
                                    const juce::String &text, int height = 13) {
        const auto bounds = button->getBounds();
        const int margin = juce::roundToInt(5.0f * scale);
        const int scaledHeight = juce::roundToInt((float)height * scale);
        graphics.drawText(text, bounds.getX() - margin,
                          bounds.getY() - scaledHeight,
                          bounds.getWidth() + margin * 2, scaledHeight,
                          juce::Justification::centred);
    };
    auto below = [&graphics, scale](const PanelButton *button,
                                    const juce::String &text, int height = 13) {
        const auto bounds = button->getBounds();
        const int margin = juce::roundToInt(12.0f * scale);
        const int scaledHeight = juce::roundToInt((float)height * scale);
        graphics.drawText(text, bounds.getX() - margin, bounds.getBottom(),
                          bounds.getWidth() + margin * 2, scaledHeight,
                          juce::Justification::centred);
    };

    graphics.setFont(9.5f * scale);
    static const char *pageTop[12] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "0", ""
    };
    static const char *pageBottom[12] = {
        "ENV 1", "ENV 2", "ENV 3", "PITCH", "FILTER", "AMP",
        "LFO", "WAVE", "LAYER", "SAMPLE", "TRACK", ""
    };
    for (std::size_t index = 0; index < pageButtons.size(); ++index) {
        if (*pageTop[index]) above(pageButtons[index], pageTop[index]);
        if (*pageBottom[index]) below(pageButtons[index], pageBottom[index]);
    }
    graphics.setFont(7.5f * scale);
    const auto effectBounds = pageButtons[11]->getBounds();
    graphics.drawText("EFFECT", effectBounds.getX() - 8,
                      effectBounds.getBottom(), effectBounds.getWidth() + 16,
                      juce::roundToInt(9 * scale), juce::Justification::centred);
    graphics.drawText("SELECT", effectBounds.getX() - 8,
                      effectBounds.getBottom() + juce::roundToInt(8 * scale),
                      effectBounds.getWidth() + 16, juce::roundToInt(9 * scale),
                      juce::Justification::centred);
    graphics.drawText("BYPASS", effectBounds.getX() - 8,
                      effectBounds.getBottom() + juce::roundToInt(16 * scale),
                      effectBounds.getWidth() + 16, juce::roundToInt(9 * scale),
                      juce::Justification::centred);

    graphics.setFont(8.5f * scale);
    static const char *modeLabels[7] = {
        "LOAD", "CMD", "EDIT", "INST", "SEQ SONG", "SYSTEM MIDI", "EFFECTS"
    };
    for (std::size_t index = 0; index < modeButtons.size(); ++index)
        below(modeButtons[index], modeLabels[index]);

    graphics.setFont(9.5f * scale);
    const int trackHeaderX = trackButtons.front()->getX();
    const int trackHeaderRight = trackButtons.back()->getRight();
    graphics.drawText(
        juce::CharPointer_UTF8("INSTRUMENTS  \xe2\x80\xa2  TRACKS"),
        trackHeaderX,
                      trackButtons.front()->getY() - juce::roundToInt(27 * scale),
                      trackHeaderRight - trackHeaderX,
                      juce::roundToInt(17 * scale),
                      juce::Justification::centred);

    graphics.setFont(7.5f * scale);
    static const char *sequenceLabels[3] = {"RECORD", "STOP / CONT", "PLAY"};
    for (std::size_t index = 0; index < sequencerButtons.size(); ++index)
        above(sequencerButtons[index], sequenceLabels[index], 12);
    graphics.setFont(9.0f * scale);
    graphics.drawText("SEQUENCER", sequencerButtons.front()->getX(),
                      sequencerButtons.front()->getBottom() +
                          juce::roundToInt(24 * scale),
                      sequencerButtons.back()->getRight() -
                          sequencerButtons.front()->getX(),
                      juce::roundToInt(14 * scale),
                      juce::Justification::centred);

    graphics.setFont(17.0f * scale);
    above(upButton, juce::CharPointer_UTF8("\xe2\x96\xb3"), 20);
    below(downButton, juce::CharPointer_UTF8("\xe2\x96\xbd"), 20);
    graphics.drawText(juce::CharPointer_UTF8("\xe2\x97\x81"),
                      leftButton->getX() - juce::roundToInt(23 * scale),
                      leftButton->getY(), juce::roundToInt(20 * scale),
                      leftButton->getHeight(), juce::Justification::centred);
    graphics.drawText(juce::CharPointer_UTF8("\xe2\x96\xb7"),
                      rightButton->getRight() + juce::roundToInt(3 * scale),
                      rightButton->getY(), juce::roundToInt(20 * scale),
                      rightButton->getHeight(), juce::Justification::centred);

    graphics.setFont(8.0f * scale);
    above(cancelButton, "NO", 13);
    below(cancelButton, "CANCEL", 12);
    above(enterButton, "YES", 13);
    below(enterButton, "ENTER", 12);

    auto baseRect = [this, scale](int x, int y, int width, int height) {
        const int offsetX =
            (getWidth() - juce::roundToInt(rackWidth * scale)) / 2;
        const int offsetY =
            (getHeight() - juce::roundToInt(rackHeight * scale)) / 2;
        return juce::Rectangle<int>(
            offsetX + juce::roundToInt((float)x * scale),
            offsetY + juce::roundToInt((float)y * scale),
            juce::roundToInt((float)width * scale),
            juce::roundToInt((float)height * scale));
    };
    graphics.setColour(accentColour);
    for (const auto line : {baseRect(18, 249, 92, 2),
                            baseRect(130, 249, 135, 2),
                            baseRect(281, 249, 175, 2),
                            baseRect(475, 249, 168, 2),
                            baseRect(690, 249, 444, 2),
                            baseRect(1170, 249, 162, 2)})
        graphics.fillRect(line);
    graphics.setColour(rackLabelColour);
    graphics.setFont(9.0f * scale);
    graphics.drawText("VOLUME", baseRect(25, 251, 78, 18),
                      juce::Justification::centred);
    graphics.drawText("MODE", baseRect(145, 251, 70, 18),
                      juce::Justification::centred);
    graphics.drawText("PAGE", baseRect(322, 251, 76, 18),
                      juce::Justification::centred);
    graphics.drawText("DATA ENTRY", baseRect(505, 251, 110, 18),
                      juce::Justification::centred);
    for (int index = 0; index < 8; ++index)
        graphics.drawText(juce::String(index + 1),
                          baseRect(706 + index * 55, 251, 28, 18),
                          juce::Justification::centred);
}

void Eps16PanelEditor::resized() {
    if (pageButtons.front() == nullptr) return;
    const float scale = juce::jmin((float)getWidth() / rackWidth,
                                   (float)getHeight() / rackHeight);
    const int offsetX = (getWidth() - juce::roundToInt(rackWidth * scale)) / 2;
    const int offsetY = (getHeight() - juce::roundToInt(rackHeight * scale)) / 2;
    auto rackRect = [scale, offsetX, offsetY](int x, int y,
                                              int width, int height) {
        return juce::Rectangle<int>(
            offsetX + juce::roundToInt((float)x * scale),
            offsetY + juce::roundToInt((float)y * scale),
            juce::roundToInt((float)width * scale),
            juce::roundToInt((float)height * scale));
    };

    vfd.setBounds(rackRect(696, 27, 444, 101));
    vfd.setFont(juce::Font(juce::FontOptions("Menlo", 20.0f * scale,
                                             juce::Font::plain)));
    status.setBounds({});
    masterVolume.setBounds(rackRect(36, 25, 76, 198));
    dataEntry.setBounds(rackRect(464, 26, 68, 199));

    const int pageX[3] = {294, 345, 396};
    const int pageY[3] = {51, 105, 159};
    for (std::size_t index = 0; index < 9; ++index)
        pageButtons[index]->setBounds(
            rackRect(pageX[index % 3], pageY[index / 3], 34, 18));
    pageButtons[9]->setBounds(rackRect(1270, 76, 38, 18));
    pageButtons[10]->setBounds(rackRect(396, 211, 34, 18));
    pageButtons[11]->setBounds(rackRect(1183, 76, 38, 18));

    modeButtons[0]->setBounds(rackRect(142, 69, 38, 18));
    modeButtons[1]->setBounds(rackRect(142, 119, 38, 18));
    modeButtons[2]->setBounds(rackRect(142, 169, 38, 18));
    modeButtons[3]->setBounds(rackRect(225, 47, 38, 18));
    modeButtons[4]->setBounds(rackRect(225, 99, 38, 18));
    modeButtons[5]->setBounds(rackRect(225, 151, 38, 18));
    modeButtons[6]->setBounds(rackRect(225, 203, 38, 18));

    for (std::size_t index = 0; index < trackButtons.size(); ++index)
        trackButtons[index]->setBounds(
            rackRect(698 + (int)index * 55, 181, 48, 34));

    for (std::size_t index = 0; index < sequencerButtons.size(); ++index)
        sequencerButtons[index]->setBounds(
            rackRect(1172 + (int)index * 52, 196, 39, 18));
    upButton->setBounds(rackRect(589, 50, 39, 18));
    leftButton->setBounds(rackRect(551, 84, 39, 18));
    rightButton->setBounds(rackRect(627, 84, 39, 18));
    downButton->setBounds(rackRect(589, 119, 39, 18));
    cancelButton->setBounds(rackRect(552, 183, 42, 22));
    enterButton->setBounds(rackRect(624, 183, 42, 22));

}
