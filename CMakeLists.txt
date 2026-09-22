cmake_minimum_required(VERSION 3.24)
project(TtpAac VERSION 0.1.0 LANGUAGES C CXX RC)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "ttp_aac requires MSVC and -A Win32")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(cmake/legacy_windows.cmake)
file(GLOB FAAD_SOURCES CONFIGURE_DEPENDS third_party/faad2/libfaad/*.c)
add_library(ttp_faad STATIC ${FAAD_SOURCES})
target_include_directories(ttp_faad PUBLIC third_party/faad2/include PRIVATE third_party/faad2/libfaad)
target_compile_definitions(ttp_faad PRIVATE _CRT_SECURE_NO_WARNINGS)
# Match the original floating-point decoder; avoid contraction/reassociation.
target_compile_options(ttp_faad PRIVATE /O2 /fp:precise /arch:SSE2 /utf-8 /wd4244 /wd4267 /wd4996)
configure_file(src/version.rc.in generated/version.rc @ONLY)
add_library(ttp_aac SHARED src/plugin.cpp src/decoder.cpp src/reader.cpp src/mp4.cpp src/nero.cpp src/ttp_aac.def "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc")
target_include_directories(ttp_aac PUBLIC include PRIVATE src)
target_compile_definitions(ttp_aac PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0501)
target_compile_options(ttp_aac PRIVATE /W4 /permissive- /EHsc /utf-8 /fp:precise)
target_link_libraries(ttp_aac PRIVATE ttp_faad ole32 uuid shlwapi)
set_target_properties(ttp_aac PROPERTIES PREFIX "" OUTPUT_NAME "ttp_aac")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(TARGET ttp_aac POST_BUILD
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_legacy_imports.py"
    "$<TARGET_FILE:ttp_aac>"
    --exports "${ttplayer_yy_thunks_SOURCE_DIR}/Config/x86/5.1.2600.txt"
    --exports "${ttplayer_yy_thunks_SOURCE_DIR}/Config/x86/6.1.7600.txt"
    --report "$<TARGET_FILE_DIR:ttp_aac>/legacy-imports.json" VERBATIM)
install(TARGETS ttp_aac RUNTIME DESTINATION AddIn)
install(FILES third_party/faad2/COPYING third_party/faad2/ORIGIN.txt DESTINATION licenses/ttp_aac)
install(FILES "${ttplayer_yy_thunks_SOURCE_DIR}/LICENSE" DESTINATION licenses/ttp_aac RENAME YY-Thunks-LICENSE.txt)
install(FILES docs/licenses/VC-LTL-LICENSE.txt DESTINATION licenses/ttp_aac)
install(FILES docs/INSTALL.md docs/RECONSTRUCTION.md DESTINATION docs/ttp_aac)
