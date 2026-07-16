if(NOT APPLE)
  message(FATAL_ERROR "The first EPS-16 Plus VST3 prototype supports macOS only")
endif()

set(EPS16_JUCE_DIR "${CMAKE_SOURCE_DIR}/work/deps/JUCE" CACHE PATH
    "Path to a local JUCE 8 checkout")
if(NOT EXISTS "${EPS16_JUCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "JUCE 8 not found at ${EPS16_JUCE_DIR}. Place a local checkout there or set EPS16_JUCE_DIR.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory("${EPS16_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

add_library(eps16_vst3_bridge STATIC
  EmulatorBridge.cpp
  EmulatorBridge.h
)
target_include_directories(eps16_vst3_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(eps16_vst3_bridge PRIVATE -Wall -Wextra -Wpedantic)

add_library(eps16_probe_machine STATIC
  ProbeMachine.c
  ProbeMachine.h
  ../native/kpc_legacy.c
  ../native/kpc_firmware.c
  ../native/kpc_device.c
  ../native/m68hc11_core.c
  ../native/es5505_core.c
  ../native/es5510_core.c
  ../native/hfe_disk.c
)
target_include_directories(eps16_probe_machine PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/native"
)
target_link_libraries(eps16_probe_machine PUBLIC musashi)

juce_add_plugin(Eps16Plus
  COMPANY_NAME "EPS-16 Plus Emulator"
  BUNDLE_ID "org.eps16plus.prototype"
  PLUGIN_MANUFACTURER_CODE EnIQ
  PLUGIN_CODE Ep16
  FORMATS VST3
  PRODUCT_NAME "EPS-16 Plus Prototype"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Eps16Plus PRIVATE
  BandlimitedResampler.cpp
  BandlimitedResampler.h
  ProbeMachineSink.cpp
  ProbeMachineSink.h
  PluginProcessor.cpp
  PluginProcessor.h
  PanelEditor.cpp
  PanelEditor.h
)
target_include_directories(Eps16Plus PRIVATE "${CMAKE_SOURCE_DIR}/native")
target_link_libraries(Eps16Plus PRIVATE
  eps16_vst3_bridge
  eps16_probe_machine
  juce::juce_audio_processors
  juce::juce_gui_basics
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
)
target_compile_definitions(Eps16Plus PRIVATE
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

add_custom_target(eps16_vst3_package
  COMMAND "${CMAKE_COMMAND}"
          "-DSOURCE_BUNDLE=$<TARGET_BUNDLE_DIR:Eps16Plus_VST3>"
          "-DPACKAGE_DIR=${CMAKE_BINARY_DIR}/vst3-package"
          "-DRESOURCE_README=${CMAKE_CURRENT_SOURCE_DIR}/ResourceFolder-README.txt"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/PackageMac.cmake"
  DEPENDS Eps16Plus_VST3
  VERBATIM
  COMMENT "Creating verified arm64 VST3 package"
)

add_executable(eps16_vst3_bridge_test BridgeTest.cpp)
target_link_libraries(eps16_vst3_bridge_test PRIVATE eps16_vst3_bridge)
target_compile_options(eps16_vst3_bridge_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME vst3_bridge COMMAND eps16_vst3_bridge_test)

add_executable(eps16_bandlimited_resampler_test
  BandlimitedResampler.cpp
  BandlimitedResamplerTest.cpp
)
target_compile_options(eps16_bandlimited_resampler_test PRIVATE
  -Wall -Wextra -Wpedantic
)
add_test(NAME vst3_bandlimited_resampler
         COMMAND eps16_bandlimited_resampler_test)

add_executable(eps16_probe_machine_test ProbeMachineTest.c)
target_link_libraries(eps16_probe_machine_test PRIVATE eps16_probe_machine)

add_executable(eps16_indicator_map_probe IndicatorMapProbe.c)
target_link_libraries(eps16_indicator_map_probe PRIVATE eps16_probe_machine)

add_executable(eps16_probe_machine_restore_test ProbeMachineRestoreTest.c)
target_link_libraries(eps16_probe_machine_restore_test PRIVATE eps16_probe_machine)

add_executable(eps16_vst3_editor_smoke_test EditorSmokeTest.cpp)
target_link_libraries(eps16_vst3_editor_smoke_test PRIVATE
  Eps16Plus
  juce::juce_gui_basics
)
add_test(NAME vst3_editor_smoke COMMAND eps16_vst3_editor_smoke_test)

add_executable(eps16_vst3_plugin_state_test PluginStateTest.cpp)
target_link_libraries(eps16_vst3_plugin_state_test PRIVATE
  Eps16Plus
  juce::juce_gui_basics
)
