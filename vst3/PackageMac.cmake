if(NOT DEFINED SOURCE_BUNDLE OR NOT EXISTS "${SOURCE_BUNDLE}")
  message(FATAL_ERROR "SOURCE_BUNDLE does not name a built VST3 bundle")
endif()
if(NOT DEFINED PACKAGE_DIR)
  message(FATAL_ERROR "PACKAGE_DIR is required")
endif()

set(STAGE "${PACKAGE_DIR}/EPS-16 Plus Prototype.stage")
set(FINAL "${PACKAGE_DIR}/EPS-16 Plus Prototype.vst3")
set(ARCHIVE "${PACKAGE_DIR}/EPS-16-Plus-Prototype-arm64.zip")
file(REMOVE_RECURSE "${PACKAGE_DIR}")
file(MAKE_DIRECTORY "${PACKAGE_DIR}")

function(run_checked)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE RESULT)
  if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "Command failed (${RESULT}): ${ARGV}")
  endif()
endfunction()

# Documents folders managed by macOS may attach Finder/file-provider metadata
# to a directory named *.vst3. Sign under a neutral staging suffix, archive
# without extended attributes, and let installation recreate a clean bundle.
run_checked(/usr/bin/ditto --norsrc --noextattr --noqtn --noacl
            "${SOURCE_BUNDLE}" "${STAGE}")
run_checked(/usr/bin/xattr -cr "${STAGE}")
run_checked(/usr/bin/codesign --force --deep --sign - "${STAGE}")
run_checked(/usr/bin/codesign --verify --deep --strict --verbose=2 "${STAGE}")
file(RENAME "${STAGE}" "${FINAL}")
run_checked(/usr/bin/ditto -c -k --keepParent --norsrc --noextattr --noqtn
            --noacl "${FINAL}" "${ARCHIVE}")
file(REMOVE_RECURSE "${FINAL}")
message(STATUS "Verified package: ${ARCHIVE}")
