#include "ProbeMachineSink.h"
#include "ProbeMachine.h"

#include <utility>

namespace eps16::vst3 {

ProbeMachineSink::ProbeMachineSink() {
    for (std::size_t index = 0; index < 22; ++index)
        displayCharacters[index].store(' ', std::memory_order_relaxed);
    displayCharacters[22].store('\0', std::memory_order_relaxed);
}

void ProbeMachineSink::configure(std::string romPath, std::string kpcPath,
                                 std::string osDiskPath) {
    rom = std::move(romPath);
    kpc = std::move(kpcPath);
    disk = std::move(osDiskPath);
}

void ProbeMachineSink::prepare(double) {
    char error[256]{};
    if (eps16_probe_machine_initialize(rom.c_str(), kpc.c_str(), disk.c_str(),
                                       error, sizeof(error))) {
        cycleBase = eps16_probe_machine_cycles();
        {
            const std::lock_guard<std::mutex> lock(statusMutex);
            statusText = "authentic machine running";
        }
        ready.store(true, std::memory_order_release);
        publishDisplay();
    } else {
        {
            const std::lock_guard<std::mutex> lock(statusMutex);
            statusText = error[0] ? error : "machine initialization failed";
        }
        ready.store(false, std::memory_order_release);
    }
}

void ProbeMachineSink::runUntil(std::uint64_t absoluteCpuCycle) {
    if (!isReady()) return;
    eps16_probe_machine_run_until(cycleBase + absoluteCpuCycle);
    publishDisplay();
}

void ProbeMachineSink::midi(std::uint8_t status, std::uint8_t data1,
                            std::uint8_t data2, std::uint64_t) {
    if (isReady()) eps16_probe_machine_midi(status, data1, data2);
}

void ProbeMachineSink::panelByte(std::uint8_t value, std::uint64_t) {
    if (isReady()) eps16_probe_machine_panel_byte(value);
}

void ProbeMachineSink::analog(unsigned int channel, std::uint16_t value,
                              std::uint64_t) {
    if (isReady()) eps16_probe_machine_analog(channel, value);
}

void ProbeMachineSink::samplingInput(float left, float right, std::uint64_t) {
    if (isReady()) eps16_probe_machine_sampling_input(left, right);
}

void ProbeMachineSink::stereoOutput(float &left, float &right, std::uint64_t) {
    if (!isReady()) {
        left = 0.0f;
        right = 0.0f;
        return;
    }
    eps16_probe_machine_stereo_output(&left, &right);
}

void ProbeMachineSink::publishDisplay() {
    char text[23];
    int cursorStart = -1;
    int cursorEnd = -1;
    eps16_probe_machine_display(text);
    eps16_probe_machine_cursor(&cursorStart, &cursorEnd);
    for (std::size_t index = 0; index < 23; ++index)
        displayCharacters[index].store(text[index], std::memory_order_relaxed);
    displayCursorStart.store(cursorStart, std::memory_order_relaxed);
    displayCursorEnd.store(cursorEnd, std::memory_order_relaxed);
}

std::string ProbeMachineSink::display() const {
    std::string result(22, ' ');
    for (std::size_t index = 0; index < 22; ++index)
        result[index] = displayCharacters[index].load(std::memory_order_relaxed);
    return result;
}

std::string ProbeMachineSink::status() const {
    const std::lock_guard<std::mutex> lock(statusMutex);
    return statusText;
}

std::size_t ProbeMachineSink::illegalInstructions() const {
    return isReady() ? eps16_probe_machine_illegal_instructions() : 0;
}

} // namespace eps16::vst3
