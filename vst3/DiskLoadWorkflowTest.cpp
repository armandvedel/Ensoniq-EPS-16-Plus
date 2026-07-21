#include "EmulatorBridge.h"
#include "ProbeMachineSink.h"

#include <array>
#include <iostream>
#include <string>

namespace {
constexpr int blockSize = 512;
constexpr double sampleRate = 48000.0;

void runFor(eps16::vst3::EmulatorBridge &bridge, double seconds) {
    std::array<float, blockSize> input{};
    std::array<float, blockSize> left{};
    std::array<float, blockSize> right{};
    const auto blocks = static_cast<int>(seconds * sampleRate / blockSize) + 1;
    for (int block = 0; block < blocks; ++block)
        bridge.process(input.data(), input.data(), left.data(), right.data(),
                       blockSize, nullptr, 0);
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 5 && argc != 6) {
        std::cerr << "usage: " << argv[0]
                  << " ROM KPC OS INSTRUMENT_DISK [SECOND_DISK]\n";
        return 2;
    }
    eps16::vst3::ProbeMachineSink machine;
    machine.configure(argv[1], argv[2], argv[3]);
    eps16::vst3::EmulatorBridge bridge(machine);
    if (!bridge.prepare(sampleRate) || !machine.isReady()) return 1;
    runFor(bridge, 22.0);
    if (!machine.insertDisk(argv[4], "Test disk")) return 1;
    runFor(bridge, 2.0);

    /* A normal GUI click can enqueue both edges before the next audio block.
       The bridge must retain a physical key-down interval for the KPC. */
    if (!bridge.enqueuePanelTransition(0x0f, true) ||
        !bridge.enqueuePanelTransition(0x0f, false))
        return 1;
    runFor(bridge, 2.0);
    const auto display = machine.display();
    std::cout << "display=|" << display << "| status=" << machine.status()
              << '\n';
    if (!display.starts_with("FILE ") || machine.illegalInstructions()) return 1;
    if (argc == 6) {
        if (!machine.insertDisk(argv[5], "Second test disk")) return 1;
        runFor(bridge, 2.0);
        if (!bridge.enqueuePanelTransition(0x0f, true) ||
            !bridge.enqueuePanelTransition(0x0f, false))
            return 1;
        runFor(bridge, 2.0);
        const auto secondDisplay = machine.display();
        std::cout << "second_display=|" << secondDisplay << "| status="
                  << machine.status() << '\n';
        if (!secondDisplay.starts_with("FILE ") || machine.illegalInstructions())
            return 1;
    }
    if (!machine.createBlankDisk()) return 1;
    runFor(bridge, 2.0);
    if (!bridge.enqueuePanelTransition(0x0f, true) ||
        !bridge.enqueuePanelTransition(0x0f, false))
        return 1;
    runFor(bridge, 2.0);
    const auto blankDisplay = machine.display();
    std::cout << "blank_display=|" << blankDisplay << "| status="
              << machine.status() << '\n';
    return blankDisplay.starts_with("NO INSTRUMENTS") &&
                   !machine.illegalInstructions() ? 0 : 1;
}
