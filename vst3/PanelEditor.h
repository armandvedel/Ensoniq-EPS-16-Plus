#ifndef EPS16_VST3_PANEL_EDITOR_H
#define EPS16_VST3_PANEL_EDITOR_H

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

class Eps16PanelEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer {
public:
    explicit Eps16PanelEditor(Eps16PlusProcessor &);
    ~Eps16PanelEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress &) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType) override;

private:
    class VfdLabel final : public juce::Label {
    public:
        void setCursorSegmentMask(std::uint32_t mask);
        void setDecimalMask(std::uint32_t mask);
        void setIndicators(const std::array<std::uint16_t, 3> &on,
                           const std::array<std::uint16_t, 3> &flash,
                           bool flashPhase);
        void paint(juce::Graphics &) override;

    private:
        std::uint32_t cursorSegmentMask{};
        std::uint32_t decimalMask{};
        std::array<std::uint16_t, 3> indicatorOn{};
        std::array<std::uint16_t, 3> indicatorFlash{};
        bool indicatorFlashPhase{};
    };

    class PanelButton final : public juce::TextButton {
    public:
        PanelButton(Eps16PlusProcessor &, juce::String label,
                    std::uint8_t rawCode, bool mappingKnown = true);
        void mouseDown(const juce::MouseEvent &) override;
        void mouseUp(const juce::MouseEvent &) override;
        void mouseExit(const juce::MouseEvent &) override;

    private:
        void releaseIfNeeded();
        Eps16PlusProcessor &processor;
        const std::uint8_t code;
        bool pressed{};
    };

    class DiskButton final : public juce::Button {
    public:
        DiskButton(juce::String name, juce::String diskLabel);
        void paintButton(juce::Graphics &, bool highlighted,
                         bool down) override;

    private:
        juce::String label;
    };

    PanelButton &addPanelButton(const juce::String &, std::uint8_t,
                                bool known = true);
    bool updateArrowKey(int keyCode, bool isDown);
    void releaseArrowKeys();
    void openSaveDiskDialog(bool hfeFormat);
    void timerCallback() override;

    Eps16PlusProcessor &owner;
    VfdLabel vfd;
    juce::Label status;
    juce::Slider masterVolume;
    juce::Slider dataEntry;
    DiskButton osDiskButton{"Insert OS disk", "OS"};
    DiskButton newDiskButton{"New blank disk", "NEW"};
    DiskButton loadDiskButton{"Load disk image", "LOAD"};
    DiskButton saveDiskButton{"Save disk image", "SAVE"};
    std::unique_ptr<juce::FileChooser> diskChooser;
    std::vector<std::unique_ptr<PanelButton>> buttons;
    std::array<PanelButton *, 12> pageButtons{};
    std::array<PanelButton *, 7> modeButtons{};
    std::array<PanelButton *, 8> trackButtons{};
    std::uint16_t trackLedOn{};
    std::uint16_t trackLedFlash{};
    bool trackLedFlashPhase{};
    std::array<PanelButton *, 3> sequencerButtons{};
    PanelButton *upButton{};
    PanelButton *downButton{};
    PanelButton *leftButton{};
    PanelButton *rightButton{};
    PanelButton *cancelButton{};
    PanelButton *enterButton{};
    std::array<bool, 4> arrowKeysDown{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Eps16PanelEditor)
};

#endif
