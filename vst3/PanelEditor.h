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
    ~Eps16PanelEditor() override = default;
    void paint(juce::Graphics &) override;
    void resized() override;

private:
    class VfdLabel final : public juce::Label {
    public:
        void setCursorRange(int start, int end);
        void paint(juce::Graphics &) override;

    private:
        int cursorStart{-1};
        int cursorEnd{-1};
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

    struct ResourceRow {
        ResourceRow(juce::Identifier keyToUse, const juce::String &buttonText)
            : key(std::move(keyToUse)), chooser(buttonText) {}
        juce::Identifier key;
        juce::TextButton chooser;
        juce::Label path;
        std::unique_ptr<juce::FileChooser> fileChooser;
    };

    PanelButton &addPanelButton(const juce::String &, std::uint8_t,
                                bool known = true);
    void chooseResource(ResourceRow &row, const juce::String &title,
                        const juce::String &pattern);
    void timerCallback() override;

    Eps16PlusProcessor &owner;
    VfdLabel vfd;
    juce::Label status;
    juce::Slider masterVolume;
    juce::Slider dataEntry;
    std::vector<std::unique_ptr<PanelButton>> buttons;
    std::array<PanelButton *, 12> pageButtons{};
    std::array<PanelButton *, 7> modeButtons{};
    std::array<PanelButton *, 8> trackButtons{};
    std::array<PanelButton *, 3> sequencerButtons{};
    PanelButton *upButton{};
    PanelButton *downButton{};
    PanelButton *leftButton{};
    PanelButton *rightButton{};
    PanelButton *cancelButton{};
    PanelButton *enterButton{};
    ResourceRow romRow{Eps16PlusProcessor::romPathKey, "ROM..."};
    ResourceRow kpcRow{Eps16PlusProcessor::kpcPathKey, "KPC..."};
    ResourceRow diskRow{Eps16PlusProcessor::osDiskPathKey, "OS Disk..."};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Eps16PanelEditor)
};

#endif
