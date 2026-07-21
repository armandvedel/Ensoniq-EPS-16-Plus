#include "PanelEditor.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

namespace {
constexpr int rackWidth = 1350;
constexpr int rackHeight = 285;
const juce::Colour panelColour{0xff3b3d3c};
const juce::Colour buttonColour{0xff252625};
const juce::Colour displayColour{0xff55eaff};
const juce::Colour displayDimColour{0xff17353a};
const juce::Colour rackLabelColour{0xffe8e5df};
const juce::Colour accentColour{0xff9d4f82};

/* FIP 22AM5R alphanumeric cell, as labelled on the EPS-16 Plus keypad/display
   schematic: fourteen directly-driven segments SA..SN plus decimal point. */
enum VfdSegment : std::uint16_t {
    segA  = UINT16_C(1) << 0,  segB  = UINT16_C(1) << 1,
    segC  = UINT16_C(1) << 2,  segD  = UINT16_C(1) << 3,
    segE  = UINT16_C(1) << 4,  segF  = UINT16_C(1) << 5,
    segG1 = UINT16_C(1) << 6,  segG2 = UINT16_C(1) << 7,
    segH  = UINT16_C(1) << 8,  segI  = UINT16_C(1) << 9,
    segJ  = UINT16_C(1) << 10, segK  = UINT16_C(1) << 11,
    segL  = UINT16_C(1) << 12, segM  = UINT16_C(1) << 13
};

constexpr std::uint16_t segG = segG1 | segG2;

std::uint16_t vfdGlyph(juce::juce_wchar character) {
    const auto c = (juce::juce_wchar)juce::CharacterFunctions::toUpperCase(
        character);
    switch (c) {
        case '0': return segA | segB | segC | segD | segE | segF;
        case '1': return segB | segC;
        case '2': return segA | segB | segD | segE | segG;
        case '3': return segA | segB | segC | segD | segG;
        case '4': return segB | segC | segF | segG;
        case '5': return segA | segC | segD | segF | segG;
        case '6': return segA | segC | segD | segE | segF | segG;
        case '7': return segA | segB | segC;
        case '8': return segA | segB | segC | segD | segE | segF | segG;
        case '9': return segA | segB | segC | segD | segF | segG;
        case 'A': return segA | segB | segC | segE | segF | segG;
        case 'B': return segA | segB | segC | segD | segG | segI | segL;
        case 'C': return segA | segD | segE | segF;
        case 'D': return segA | segB | segC | segD | segI | segL;
        case 'E': return segA | segD | segE | segF | segG;
        case 'F': return segA | segE | segF | segG;
        case 'G': return segA | segC | segD | segE | segF | segG2;
        case 'H': return segB | segC | segE | segF | segG;
        case 'I': return segA | segD | segI | segL;
        case 'J': return segB | segC | segD | segE;
        case 'K': return segE | segF | segG1 | segJ | segK;
        case 'L': return segD | segE | segF;
        case 'M': return segB | segC | segE | segF | segH | segJ;
        case 'N': return segB | segC | segE | segF | segH | segK;
        case 'O': return segA | segB | segC | segD | segE | segF;
        case 'P': return segA | segB | segE | segF | segG;
        case 'Q': return segA | segB | segC | segD | segE | segF | segK;
        case 'R': return segA | segB | segE | segF | segG | segK;
        case 'S': return segA | segC | segD | segF | segG;
        case 'T': return segA | segI | segL;
        case 'U': return segB | segC | segD | segE | segF;
        case 'V': return segE | segF | segJ | segM;
        case 'W': return segB | segC | segE | segF | segK | segM;
        case 'X': return segH | segJ | segK | segM;
        case 'Y': return segH | segJ | segL;
        case 'Z': return segA | segD | segJ | segM;
        case '-': return segG;
        case '_': return segD;
        case '=': return segG | segD;
        case '+': return segG | segI | segL;
        case '/': return segJ | segM;
        case '\\': return segH | segK;
        case '*': return segG | segH | segI | segJ | segK | segL | segM;
        case '[': return segA | segD | segE | segF;
        case ']': return segA | segB | segC | segD;
        case '(': return segJ | segK;
        case ')': return segH | segM;
        case '<': return segJ | segK;
        case '>': return segH | segM;
        case '\'': return segJ;
        case '"': return segF | segB;
        case '?': return segA | segB | segG2 | segL;
        default: return 0;
    }
}

juce::Path vfdSegmentPath(juce::Line<float> line, float width) {
    const auto vector = line.getEnd() - line.getStart();
    const auto length = vector.getDistanceFromOrigin();
    if (length <= 0.0f) return {};
    const auto along = vector / length;
    const juce::Point<float> across{-along.y, along.x};
    const auto halfWidth = width * 0.5f;
    const auto bevel = juce::jmin(width * 0.55f, length * 0.16f);
    const auto start = line.getStart();
    const auto end = line.getEnd();
    juce::Path path;
    path.startNewSubPath(start - across * halfWidth);
    path.lineTo(start - along * bevel);
    path.lineTo(start + across * halfWidth);
    path.lineTo(end + across * halfWidth);
    path.lineTo(end + along * bevel);
    path.lineTo(end - across * halfWidth);
    path.closeSubPath();
    return path;
}

void drawVfdElectrode(juce::Graphics &graphics, juce::Line<float> line,
                      float width, bool lit, juce::Colour colour) {
    const auto segment = vfdSegmentPath(line, width);
    if (lit) {
        graphics.setColour(colour.withAlpha(0.14f));
        graphics.strokePath(segment,
                            juce::PathStrokeType(width * 1.8f));
        graphics.setColour(colour);
    } else {
        graphics.setColour(displayDimColour.withAlpha(0.22f));
    }
    graphics.fillPath(segment);
}

void drawVfdCell(juce::Graphics &graphics, juce::Rectangle<float> cell,
                 std::uint16_t active, bool decimalPoint, bool cursor,
                 juce::Colour colour) {
    /* The FIP 22AM5R glass has a separate cursor electrode below every
       alphanumeric cell. The hardware close-up shows a real gap between it
       and the character's D segment; it is not the D segment itself. */
    cell = cell.reduced(0.45f, 0.35f);
    const float cursorWidth = juce::jmax(1.15f, cell.getWidth() * 0.105f);
    const float cursorGap = juce::jmax(1.35f, cell.getHeight() * 0.075f);
    const float cursorY = cell.getBottom() - cursorWidth * 0.5f;
    const auto glyph = cell.withBottom(cursorY - cursorGap);
    const float left = glyph.getX() + glyph.getWidth() * 0.12f;
    const float centre = glyph.getX() + glyph.getWidth() * 0.43f;
    const float right = glyph.getX() + glyph.getWidth() * 0.73f;
    const float top = glyph.getY() + 0.8f;
    const float middle = glyph.getCentreY();
    const float bottom = glyph.getBottom() - 0.6f;
    const float gap = juce::jmax(0.48f, glyph.getWidth() * 0.045f);
    const float tip = juce::jmax(0.65f, glyph.getWidth() * 0.075f);
    const std::array<juce::Line<float>, 14> lines{{
        {{left + tip, top}, {right - tip, top}},
        {{right, top + tip}, {right, middle - tip}},
        {{right, middle + tip}, {right, bottom - tip}},
        {{left + tip, bottom}, {right - tip, bottom}},
        {{left, middle + tip}, {left, bottom - tip}},
        {{left, top + tip}, {left, middle - tip}},
        {{left + tip, middle}, {centre - gap, middle}},
        {{centre + gap, middle}, {right - tip, middle}},
        {{left + tip, top + tip}, {centre - gap, middle - gap}},
        {{centre, top + tip}, {centre, middle - gap}},
        {{right - tip, top + tip}, {centre + gap, middle - gap}},
        {{centre + gap, middle + gap}, {right - tip, bottom - tip}},
        {{centre, middle + gap}, {centre, bottom - tip}},
        {{centre - gap, middle + gap}, {left + tip, bottom - tip}}
    }};
    const float coreWidth = juce::jmax(0.9f, cell.getWidth() * 0.082f);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto bit = (std::uint16_t)(UINT16_C(1) << index);
        drawVfdElectrode(graphics, lines[index], coreWidth,
                         (active & bit) != 0, colour);
    }
    const float dotSize = juce::jmax(1.35f, cell.getWidth() * 0.115f);
    graphics.setColour(decimalPoint ? colour
                                    : displayDimColour.withAlpha(0.22f));
    graphics.fillEllipse(glyph.getX() + glyph.getWidth() * 0.82f,
                         bottom - dotSize * 0.4f, dotSize, dotSize);

    const juce::Line<float> cursorLine{
        {left + tip, cursorY}, {right - tip, cursorY}};
    drawVfdElectrode(graphics, cursorLine, cursorWidth, cursor, colour);
}
}

void Eps16PanelEditor::VfdLabel::setCursorSegmentMask(std::uint32_t mask) {
    mask &= 0x3fffffU;
    if (cursorSegmentMask == mask) return;
    cursorSegmentMask = mask;
    repaint();
}

void Eps16PanelEditor::VfdLabel::setDecimalMask(std::uint32_t mask) {
    mask &= 0x3fffffU;
    if (decimalMask == mask) return;
    decimalMask = mask;
    repaint();
}

void Eps16PanelEditor::VfdLabel::setIndicators(
    const std::array<std::uint16_t, 3> &on,
    const std::array<std::uint16_t, 3> &flash, bool flashPhase) {
    if (indicatorOn == on && indicatorFlash == flash &&
        indicatorFlashPhase == flashPhase) return;
    indicatorOn = on;
    indicatorFlash = flash;
    indicatorFlashPhase = flashPhase;
    repaint();
}

void Eps16PanelEditor::VfdLabel::paint(juce::Graphics &graphics) {
    graphics.fillAll(findColour(juce::Label::backgroundColourId));
    const auto bounds = getLocalBounds().reduced(8, 5).toFloat();
    const auto font = getFont();
    const float indicatorHeight = bounds.getHeight() * 0.54f;
    const float rowHeight = indicatorHeight / 3.0f;

    struct Legend {
        const char *text;
        float x;
        float y;
        int bank;
        int bit;
    };
    /* The legends and positions are part of the physical VFD glass. Indices
       follow the serial hardware mapping recovered by an Ensoniq display
       sniffer and are driven only by original OS/KPC traffic. */
    static const Legend legends[] = {
        {"LOAD", 0.00f, 0.00f, 1, 15}, {"INST", 0.13f, 0.00f, 1, 14},
        {"MIDI", 0.25f, 0.00f, 1, 3}, {"SYSTEM", 0.36f, 0.00f, 1, 12},
        {"LAYER", 0.49f, 0.00f, 1, 11},
        {"ENV", 0.64f, 0.00f, 2, 15}, {"ODUB", 0.72f, 0.00f, 2, 14},
        {"REC", 0.81f, 0.00f, 2, 3}, {"PLAY", 0.88f, 0.00f, 2, 12},
        {"STOP", 0.95f, 0.00f, 2, 11},
        {"CMD", 0.00f, 1.00f, 1, 13}, {"SEQ", 0.13f, 1.00f, 1, 2},
        {"SONG", 0.25f, 1.00f, 1, 4}, {"PITCH", 0.36f, 1.00f, 1, 10},
        {"FILTER", 0.49f, 1.00f, 1, 6},
        {"AMP", 0.64f, 1.00f, 2, 13}, {"SONG", 0.72f, 1.00f, 2, 2},
        {"SEQ", 0.81f, 1.00f, 2, 4}, {"STEP", 0.88f, 1.00f, 2, 10},
        {"REP", 0.96f, 1.00f, 2, 6},
        {"EDIT", 0.00f, 2.00f, 1, 5}, {"MACRO", 0.13f, 2.00f, 2, 8},
        {"BANK", 0.27f, 2.00f, 1, 7}, {"LFO", 0.38f, 2.00f, 1, 9},
        {"WAVE", 0.47f, 2.00f, 1, 8},
        {"TRACK", 0.64f, 2.00f, 2, 5}, {"BAR", 0.76f, 2.00f, 2, 1},
        {"BEAT", 0.84f, 2.00f, 2, 7}, {"CLOCK", 0.92f, 2.00f, 2, 9}
    };
    graphics.setFont(juce::Font(juce::FontOptions("Helvetica Neue", 8.0f,
                                                  juce::Font::bold)));
    for (const auto &legend : legends) {
        const bool on = legend.bank >= 0 &&
            (indicatorOn[(std::size_t)legend.bank] &
             (UINT16_C(1) << legend.bit));
        const bool flashing = on &&
            (indicatorFlash[(std::size_t)legend.bank] &
             (UINT16_C(1) << legend.bit));
        const bool lit = on && (!flashing || indicatorFlashPhase);
        const float width = juce::jmax(
            28.0f, (float)std::strlen(legend.text) * 5.1f + 4.0f);
        const auto area = juce::Rectangle<float>(
            bounds.getX() + legend.x * (bounds.getWidth() - 28.0f),
            bounds.getY() + legend.y * rowHeight, width, rowHeight);
        if (lit) {
            graphics.setColour(displayColour.withAlpha(0.18f));
            for (int offset = 3; offset >= 1; --offset)
                graphics.drawText(legend.text, area.expanded((float)offset),
                                  juce::Justification::centredLeft, false);
            graphics.setColour(displayColour);
        } else {
            graphics.setColour(displayDimColour);
        }
        graphics.drawText(legend.text, area, juce::Justification::centredLeft,
                          false);
    }

    const auto textArea = juce::Rectangle<float>(
        bounds.getX(), bounds.getY() + indicatorHeight + 3.0f,
        bounds.getWidth(), bounds.getHeight() - indicatorHeight - 3.0f);
    const float cellHeight = juce::jmin(textArea.getHeight() - 1.0f,
                                        font.getHeight() * 1.32f);
    const float cellWidth = font.getHeight() * 0.82f;
    const float cellTop = textArea.getCentreY() - cellHeight * 0.5f;
    const auto text = getText().paddedRight(' ', 22).substring(0, 22);
    for (int index = 0; index < 22; ++index) {
        const auto segments = vfdGlyph(text[index]);
        const bool cursor =
            (cursorSegmentMask & (UINT32_C(1) << index)) != 0;
        const auto cell = juce::Rectangle<float>(
            textArea.getX() + cellWidth * (float)index, cellTop,
            cellWidth, cellHeight);
        drawVfdCell(graphics, cell, segments,
                    (decimalMask & (UINT32_C(1) << index)) != 0,
                    cursor,
                    findColour(juce::Label::textColourId));
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
    setWantsKeyboardFocus(false);
    setEnabled(mappingKnown);
    if (!mappingKnown) setTooltip("Wire mapping is not verified yet");
}

void Eps16PanelEditor::PanelButton::mouseDown(const juce::MouseEvent &event) {
    if (auto *parent = getParentComponent()) parent->grabKeyboardFocus();
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

Eps16PanelEditor::DiskButton::DiskButton(juce::String name,
                                         juce::String diskLabel)
    : Button(std::move(name)), label(std::move(diskLabel)) {
    setWantsKeyboardFocus(false);
}

void Eps16PanelEditor::DiskButton::paintButton(juce::Graphics &graphics,
                                               bool highlighted, bool down) {
    auto area = getLocalBounds().toFloat().reduced(1.0f);
    auto body = buttonColour;
    if (highlighted) body = body.brighter(0.10f);
    if (down) body = body.brighter(0.18f);
    if (!isEnabled()) body = body.withMultipliedAlpha(0.42f);
    graphics.setColour(body);
    graphics.fillRoundedRectangle(area, 2.5f);
    graphics.setColour(rackLabelColour.withMultipliedAlpha(
        isEnabled() ? 0.90f : 0.35f));
    graphics.drawRoundedRectangle(area, 2.5f, 1.0f);

    const auto shutter = juce::Rectangle<float>(
        area.getX() + area.getWidth() * 0.22f,
        area.getY() + area.getHeight() * 0.08f,
        area.getWidth() * 0.56f, area.getHeight() * 0.27f);
    graphics.setColour(juce::Colour(0xff777b7a).withMultipliedAlpha(
        isEnabled() ? 1.0f : 0.4f));
    graphics.fillRect(shutter);
    graphics.setColour(juce::Colour(0xff1b1c1b));
    graphics.fillRect(shutter.getRight() - shutter.getWidth() * 0.22f,
                      shutter.getY(), shutter.getWidth() * 0.12f,
                      shutter.getHeight());

    const auto labelArea = juce::Rectangle<float>(
        area.getX() + area.getWidth() * 0.11f,
        area.getY() + area.getHeight() * 0.50f,
        area.getWidth() * 0.78f, area.getHeight() * 0.34f);
    graphics.setColour(juce::Colour(0xffdedbd1).withMultipliedAlpha(
        isEnabled() ? 1.0f : 0.4f));
    graphics.fillRoundedRectangle(labelArea, 1.0f);
    graphics.setColour(juce::Colour(0xff252625).withMultipliedAlpha(
        isEnabled() ? 1.0f : 0.45f));
    graphics.setFont(juce::Font(juce::FontOptions(
        "Helvetica Neue", juce::jmax(5.5f, labelArea.getHeight() * 0.54f),
        juce::Font::bold)));
    graphics.drawText(label, labelArea, juce::Justification::centred, false);
}

Eps16PanelEditor::Eps16PanelEditor(Eps16PlusProcessor &processorToUse)
    : AudioProcessorEditor(processorToUse), owner(processorToUse) {
    setSize(rackWidth, rackHeight);
    setResizable(true, true);
    setResizeLimits(1080, 228, 1620, 342);
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    if (auto *constrainer = getConstrainer())
        constrainer->setFixedAspectRatio((double)rackWidth / rackHeight);

    vfd.setText(juce::String::repeatedString(" ", 22), juce::dontSendNotification);
    vfd.setJustificationType(juce::Justification::centredLeft);
    vfd.setComponentID("vfd-display");
    vfd.setFont(juce::Font(juce::FontOptions("Menlo", 20.0f,
                                             juce::Font::plain)));
    vfd.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    vfd.setColour(juce::Label::textColourId, displayColour);
    addAndMakeVisible(vfd);

    status.setText("Plug-in adapter active - waiting for authentic emulator boot",
                   juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addChildComponent(status);

    osDiskButton.setComponentID("os-disk-button");
    osDiskButton.setEnabled(false);
    osDiskButton.onClick = [this] { owner.insertOsDisk(); };
    addAndMakeVisible(osDiskButton);

    newDiskButton.setComponentID("new-disk-button");
    newDiskButton.setEnabled(false);
    newDiskButton.onClick = [this] {
        auto safeEditor = juce::Component::SafePointer<Eps16PanelEditor>(this);
        juce::NativeMessageBox::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon,
            "New blank EPS disk",
            "Eject the current disk and insert a new blank EPS disk?\n\n"
            "Unsaved disk changes will be lost.",
            this,
            juce::ModalCallbackFunction::create([safeEditor](int result) {
                if (result != 1) return;
                if (auto *editor = safeEditor.getComponent())
                    editor->owner.createBlankDisk();
            }));
    };
    addAndMakeVisible(newDiskButton);

    loadDiskButton.setComponentID("load-disk-button");
    loadDiskButton.setEnabled(false);
    loadDiskButton.onClick = [this] {
        juce::File initialFile(owner.getResourcePath(
            Eps16PlusProcessor::mountedDiskPathKey));
        if (!initialFile.existsAsFile())
            initialFile = juce::File(owner.getResourcePath(
                Eps16PlusProcessor::osDiskPathKey));
        const auto initialDirectory = initialFile.existsAsFile()
            ? initialFile.getParentDirectory()
            : Eps16PlusProcessor::defaultResourceDirectory();
        diskChooser = std::make_unique<juce::FileChooser>(
            "Insert EPS disk image (.IMG or .HFE)", initialDirectory, "*");
        auto safeEditor = juce::Component::SafePointer<Eps16PanelEditor>(this);
        diskChooser->launchAsync(
            juce::FileBrowserComponent::openMode |
                juce::FileBrowserComponent::canSelectFiles,
            [safeEditor](const juce::FileChooser &chooser) {
                if (auto *editor = safeEditor.getComponent()) {
                    const auto file = chooser.getResult();
                    if (!file.existsAsFile()) return;
                    const auto extension = file.getFileExtension().toLowerCase();
                    if (extension == ".img" || extension == ".hfe") {
                        editor->owner.insertDisk(file);
                    } else {
                        juce::NativeMessageBox::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Unsupported disk image",
                            "Please choose an EPS .IMG or .HFE disk image.",
                            editor);
                    }
                }
            });
    };
    addAndMakeVisible(loadDiskButton);

    saveDiskButton.setComponentID("save-disk-button");
    saveDiskButton.setEnabled(false);
    saveDiskButton.onClick = [this] {
        auto safeEditor = juce::Component::SafePointer<Eps16PanelEditor>(this);
        juce::PopupMenu formats;
        formats.addItem(1, "Save as IMG...");
        formats.addItem(2, "Save as HFE...");
        formats.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&saveDiskButton),
            [safeEditor](int choice) {
                if (auto *editor = safeEditor.getComponent()) {
                    if (choice == 1) editor->openSaveDiskDialog(false);
                    if (choice == 2) editor->openSaveDiskDialog(true);
                }
            });
    };
    addAndMakeVisible(saveDiskButton);

    diskName.setComponentID("mounted-disk-name");
    diskName.setJustificationType(juce::Justification::centredRight);
    diskName.setMinimumHorizontalScale(0.55f);
    diskName.setColour(juce::Label::textColourId,
                       rackLabelColour.withAlpha(0.86f));
    diskName.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(diskName);
    updateDiskName();

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
        slider.setComponentID(name == "DATA ENTRY" ? "data-entry-slider"
                                                    : "volume-slider");
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        /* Keep ordinary Slider mouse drag/wheel handling. A click must not
           steal the editor focus because the physical arrow keys belong to
           the panel rather than to the JUCE slider. */
        slider.setMouseClickGrabsKeyboardFocus(false);
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

Eps16PanelEditor::~Eps16PanelEditor() {
    releaseArrowKeys();
}

void Eps16PanelEditor::openSaveDiskDialog(bool hfeFormat) {
    juce::File mounted(owner.getResourcePath(
        Eps16PlusProcessor::mountedDiskPathKey));
    const bool isBlankDisk = owner.blankDiskMounted();
    if (!isBlankDisk && !mounted.existsAsFile())
        mounted = juce::File(owner.getResourcePath(
            Eps16PlusProcessor::osDiskPathKey));
    const juce::String extension = hfeFormat ? ".hfe" : ".img";
    auto suggested = !isBlankDisk && mounted.existsAsFile()
        ? mounted.getSiblingFile(mounted.getFileNameWithoutExtension() +
                                 "-saved" + extension)
        : Eps16PlusProcessor::defaultResourceDirectory()
              .getChildFile(isBlankDisk ? "NEWDISK" + extension
                                        : "EPS-disk-saved" + extension);
    diskChooser = std::make_unique<juce::FileChooser>(
        hfeFormat ? "Save EPS disk as HFE" : "Save EPS disk as IMG",
        suggested, "*");
    auto safeEditor = juce::Component::SafePointer<Eps16PanelEditor>(this);
    diskChooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [safeEditor, hfeFormat](const juce::FileChooser &chooser) {
            if (auto *editor = safeEditor.getComponent()) {
                auto file = chooser.getResult();
                if (file.getFullPathName().isEmpty()) return;
                file = file.withFileExtension(hfeFormat ? ".hfe" : ".img");
                editor->owner.saveDisk(file);
            }
        });
}

Eps16PanelEditor::PanelButton &Eps16PanelEditor::addPanelButton(
    const juce::String &label, std::uint8_t code, bool known) {
    auto button = std::make_unique<PanelButton>(owner, label, code, known);
    auto &reference = *button;
    addAndMakeVisible(reference);
    buttons.push_back(std::move(button));
    return reference;
}

bool Eps16PanelEditor::updateArrowKey(int keyCode, bool isDown) {
    static const std::array<int, 4> keyCodes{
        juce::KeyPress::upKey, juce::KeyPress::downKey,
        juce::KeyPress::leftKey, juce::KeyPress::rightKey
    };
    static constexpr std::array<std::uint8_t, 4> panelCodes{
        0x0a, 0x0b, 0x10, 0x11
    };
    for (std::size_t index = 0; index < keyCodes.size(); ++index) {
        if (keyCode != keyCodes[index]) continue;
        if (arrowKeysDown[index] == isDown) return true;
        if (isDown) {
            arrowKeysDown[index] =
                owner.enqueuePanelTransition(panelCodes[index], true);
        } else {
            owner.enqueuePanelTransition(panelCodes[index], false);
            arrowKeysDown[index] = false;
        }
        return true;
    }
    return false;
}

bool Eps16PanelEditor::keyPressed(const juce::KeyPress &key) {
    return updateArrowKey(key.getKeyCode(), true);
}

bool Eps16PanelEditor::keyStateChanged(bool) {
    const bool hadArrowDown = std::any_of(arrowKeysDown.begin(),
                                          arrowKeysDown.end(),
                                          [](bool down) { return down; });
    static const std::array<int, 4> keyCodes{
        juce::KeyPress::upKey, juce::KeyPress::downKey,
        juce::KeyPress::leftKey, juce::KeyPress::rightKey
    };
    for (const auto keyCode : keyCodes)
        updateArrowKey(keyCode, juce::KeyPress::isKeyCurrentlyDown(keyCode));
    const bool hasArrowDown = std::any_of(arrowKeysDown.begin(),
                                          arrowKeysDown.end(),
                                          [](bool down) { return down; });
    return hadArrowDown || hasArrowDown;
}

void Eps16PanelEditor::releaseArrowKeys() {
    static const std::array<int, 4> keyCodes{
        juce::KeyPress::upKey, juce::KeyPress::downKey,
        juce::KeyPress::leftKey, juce::KeyPress::rightKey
    };
    for (const auto keyCode : keyCodes) updateArrowKey(keyCode, false);
}

void Eps16PanelEditor::focusLost(FocusChangeType cause) {
    releaseArrowKeys();
    AudioProcessorEditor::focusLost(cause);
}

void Eps16PanelEditor::updateDiskName() {
    juce::String name;
    juce::String tooltip;
    if (owner.blankDiskMounted()) {
        name = "NEWDISK (UNSAVED)";
        tooltip = "New blank EPS disk (not saved to a host file)";
    } else {
        const auto mountedPath = owner.getResourcePath(
            Eps16PlusProcessor::mountedDiskPathKey);
        if (mountedPath.isNotEmpty()) {
            const juce::File mounted(mountedPath);
            name = mounted.getFileName();
            tooltip = mounted.getFullPathName();
        } else {
            const juce::File osDisk(owner.getResourcePath(
                Eps16PlusProcessor::osDiskPathKey));
            if (owner.machineReady() && osDisk.existsAsFile()) {
                name = osDisk.getFileName();
                tooltip = osDisk.getFullPathName();
            }
        }
    }
    diskName.setText("DISK: " + (name.isNotEmpty() ? name : "NONE"),
                     juce::dontSendNotification);
    diskName.setTooltip(tooltip);
}

void Eps16PanelEditor::timerCallback() {
    owner.refreshResourcePaths();
    const juce::File osDisk(owner.getResourcePath(
        Eps16PlusProcessor::osDiskPathKey));
    osDiskButton.setEnabled(owner.machineReady() && osDisk.existsAsFile());
    osDiskButton.setTooltip(osDisk.existsAsFile()
        ? "Insert OS disk: " + osDisk.getFileName()
        : "OS disk not found in EPS_files");
    newDiskButton.setEnabled(owner.machineReady());
    newDiskButton.setTooltip("Insert a new blank formatted EPS disk");
    loadDiskButton.setEnabled(owner.machineReady());
    loadDiskButton.setTooltip("Insert an EPS .IMG or .HFE disk image");
    saveDiskButton.setEnabled(owner.machineReady());
    saveDiskButton.setTooltip("Save the inserted disk as .IMG or .HFE");
    updateDiskName();
    vfd.setText(owner.machineDisplay(), juce::dontSendNotification);
    vfd.setCursorSegmentMask(owner.machineCursorSegmentMask());
    vfd.setDecimalMask(owner.machineDecimalMask());
    std::array<std::uint16_t, 3> indicatorOn{};
    std::array<std::uint16_t, 3> indicatorFlash{};
    for (unsigned int bank = 0; bank < indicatorOn.size(); ++bank) {
        indicatorOn[bank] = owner.machineIndicatorOn(bank);
        indicatorFlash[bank] = owner.machineIndicatorFlash(bank);
    }
    vfd.setIndicators(indicatorOn, indicatorFlash,
                      ((owner.cpuCycles() / 2500000U) & 1U) != 0);
    const auto nextTrackOn = owner.machineIndicatorOn(0);
    const auto nextTrackFlash = owner.machineIndicatorFlash(0);
    const bool nextTrackPhase = ((owner.cpuCycles() / 2500000U) & 1U) != 0;
    if (trackLedOn != nextTrackOn || trackLedFlash != nextTrackFlash ||
        trackLedFlashPhase != nextTrackPhase) {
        trackLedOn = nextTrackOn;
        trackLedFlash = nextTrackFlash;
        trackLedFlashPhase = nextTrackPhase;
        repaint();
    }
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
                      trackButtons.front()->getY() - juce::roundToInt(38 * scale),
                      trackHeaderRight - trackHeaderX,
                      juce::roundToInt(13 * scale),
                      juce::Justification::centred);

    const auto ledPhase = [this](unsigned int bit) {
        const auto mask = (std::uint16_t)(UINT16_C(1) << bit);
        return (trackLedOn & mask) &&
               (!(trackLedFlash & mask) || trackLedFlashPhase);
    };
    const auto loadedColour = juce::Colour(0xffdc8732);
    const auto selectedColour = juce::Colour(0xffffd84b);
    const auto unlitColour = juce::Colour(0xff252725);
    for (unsigned int index = 0; index < trackButtons.size(); ++index) {
        const auto button = trackButtons[index]->getBounds();
        const int ledWidth = juce::roundToInt(17.0f * scale);
        const int ledHeight = juce::jmax(2, juce::roundToInt(4.0f * scale));
        const int ledX = button.getCentreX() - ledWidth / 2;
        const int loadedY = button.getY() - juce::roundToInt(20.0f * scale);
        const int selectedY = button.getY() - juce::roundToInt(11.0f * scale);
        auto drawLed = [&graphics, unlitColour](juce::Rectangle<int> area,
                                                juce::Colour colour,
                                                bool lit) {
            graphics.setColour(lit ? colour.withAlpha(0.20f) : unlitColour);
            if (lit) graphics.fillRoundedRectangle(area.expanded(3).toFloat(),
                                                    2.0f);
            graphics.setColour(lit ? colour : unlitColour);
            graphics.fillRoundedRectangle(area.toFloat(), 1.0f);
        };
        drawLed({ledX, loadedY, ledWidth, ledHeight}, loadedColour,
                ledPhase(index));
        drawLed({ledX, selectedY, ledWidth, ledHeight}, selectedColour,
                ledPhase(index + 8));
    }
    graphics.setFont(5.8f * scale);
    graphics.setColour(rackLabelColour.withAlpha(0.72f));
    graphics.drawText("LOADED", trackHeaderX - juce::roundToInt(40 * scale),
                      trackButtons.front()->getY() - juce::roundToInt(22 * scale),
                      juce::roundToInt(38 * scale), juce::roundToInt(8 * scale),
                      juce::Justification::centredRight);
    graphics.drawText("SELECTED", trackHeaderRight + juce::roundToInt(2 * scale),
                      trackButtons.front()->getY() - juce::roundToInt(13 * scale),
                      juce::roundToInt(45 * scale), juce::roundToInt(8 * scale),
                      juce::Justification::centredLeft);

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
    vfd.setFont(juce::Font(juce::FontOptions("Menlo", 17.0f * scale,
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
    osDiskButton.setBounds(rackRect(1167, 20, 32, 36));
    newDiskButton.setBounds(rackRect(1203, 20, 32, 36));
    loadDiskButton.setBounds(rackRect(1239, 20, 32, 36));
    saveDiskButton.setBounds(rackRect(1275, 20, 32, 36));
    diskName.setBounds(rackRect(1167, 57, 140, 14));
    diskName.setFont(juce::Font(juce::FontOptions(
        "Helvetica Neue", 7.5f * scale, juce::Font::plain)));

}
