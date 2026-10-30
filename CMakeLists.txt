cmake_minimum_required(VERSION 3.21)
project(snowball_round VERSION 1.0.0 LANGUAGES CXX)

set(METAMOD_SOURCE "" CACHE PATH "Path to Metamod:Source 2.0.0-git1411 sources")
set(HL2SDK_CS2 "" CACHE PATH "Path to the CS2 HL2SDK used by Metamod 1411")

if(NOT EXISTS "${METAMOD_SOURCE}/core/sourcehook/sourcehook.h")
  message(FATAL_ERROR "METAMOD_SOURCE must point to Metamod:Source tag 2.0.0.1411 (SourceHook)")
endif()
if(NOT EXISTS "${HL2SDK_CS2}/public/eiface.h")
  message(FATAL_ERROR "HL2SDK_CS2 does not point to a CS2 HL2SDK checkout")
endif()

if(WIN32)
  set(PROTOC "${HL2SDK_CS2}/devtools/bin/protoc.exe")
else()
  set(PROTOC "${HL2SDK_CS2}/devtools/bin/linux/protoc")
endif()
if(NOT EXISTS "${PROTOC}")
  message(FATAL_ERROR "The SDK protoc executable was not found: ${PROTOC}")
endif()

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_NAMES valveextensions network_connection networkbasetypes gameevents usermessages)
set(PROTO_OUTPUTS)
foreach(name IN LISTS PROTO_NAMES)
  list(APPEND PROTO_OUTPUTS "${GENERATED_DIR}/${name}.pb.cc" "${GENERATED_DIR}/${name}.pb.h")
endforeach()

add_custom_command(
  OUTPUT ${PROTO_OUTPUTS}
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DIR}"
  COMMAND "${PROTOC}"
    --cpp_out="${GENERATED_DIR}"
    --proto_path="${HL2SDK_CS2}/common"
    --proto_path="${HL2SDK_CS2}/game/shared"
    --proto_path="${HL2SDK_CS2}/thirdparty/protobuf-3.21.8/src"
    "${HL2SDK_CS2}/common/valveextensions.proto"
    "${HL2SDK_CS2}/common/network_connection.proto"
    "${HL2SDK_CS2}/common/networkbasetypes.proto"
    "${HL2SDK_CS2}/game/shared/gameevents.proto"
    "${HL2SDK_CS2}/game/shared/usermessages.proto"
  DEPENDS
    "${HL2SDK_CS2}/common/valveextensions.proto"
    "${HL2SDK_CS2}/common/network_connection.proto"
    "${HL2SDK_CS2}/common/networkbasetypes.proto"
    "${HL2SDK_CS2}/game/shared/gameevents.proto"
    "${HL2SDK_CS2}/game/shared/usermessages.proto"
  VERBATIM)

add_custom_target(generate_protos DEPENDS ${PROTO_OUTPUTS})

add_library(snowball_round SHARED
  src/plugin.cpp
  "${HL2SDK_CS2}/public/tier0/memoverride.cpp"
  "${HL2SDK_CS2}/tier1/convar.cpp"
  ${PROTO_OUTPUTS})
add_dependencies(snowball_round generate_protos)

target_compile_features(snowball_round PRIVATE cxx_std_20)
target_compile_definitions(snowball_round PRIVATE META_IS_SOURCE2=1 SE_CS2=25 SOURCE_ENGINE=SE_CS2)
target_include_directories(snowball_round PRIVATE
  src
  "${GENERATED_DIR}"
  "${METAMOD_SOURCE}/core"
  "${METAMOD_SOURCE}/core/sourcehook"
  "${HL2SDK_CS2}/thirdparty/protobuf-3.21.8/src"
  "${HL2SDK_CS2}/public"
  "${HL2SDK_CS2}/public/engine"
  "${HL2SDK_CS2}/public/mathlib"
  "${HL2SDK_CS2}/public/tier0"
  "${HL2SDK_CS2}/public/tier1"
  "${HL2SDK_CS2}/public/entity2"
  "${HL2SDK_CS2}/game/shared"
  "${HL2SDK_CS2}/game/server"
  "${HL2SDK_CS2}/common")

set_target_properties(snowball_round PROPERTIES PREFIX "" OUTPUT_NAME "snowball_round")

if(MSVC)
  set_property(TARGET snowball_round PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_definitions(snowball_round PRIVATE WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS)
  target_compile_options(snowball_round PRIVATE /W3 /utf-8 /Oy-)
  target_link_libraries(snowball_round PRIVATE
    "${HL2SDK_CS2}/lib/public/win64/2015/libprotobuf.lib"
    "${HL2SDK_CS2}/lib/public/win64/mathlib.lib"
    "${HL2SDK_CS2}/lib/public/win64/tier0.lib"
    "${HL2SDK_CS2}/lib/public/win64/interfaces.lib")
  set(PLUGIN_BIN_DIR "addons/snowball_round/bin/win64")
  set(PLUGIN_VDF_PATH "addons/snowball_round/bin/win64/snowball_round")
else()
  target_compile_definitions(snowball_round PRIVATE LINUX _LINUX POSIX _FILE_OFFSET_BITS=64 _GLIBCXX_USE_CXX11_ABI=0)
  target_compile_options(snowball_round PRIVATE -fPIC -fvisibility=hidden -fvisibility-inlines-hidden -fno-omit-frame-pointer -Wall -Wno-unused-parameter -Wno-non-virtual-dtor)
  target_link_options(snowball_round PRIVATE -static-libgcc -static-libstdc++)
  target_link_libraries(snowball_round PRIVATE
    "${HL2SDK_CS2}/lib/linux64/mathlib.a"
    "${HL2SDK_CS2}/lib/linux64/interfaces.a"
    "${HL2SDK_CS2}/lib/linux64/release/libprotobuf.a"
    "${HL2SDK_CS2}/lib/linux64/libtier0.so")
  set(PLUGIN_BIN_DIR "addons/snowball_round/bin/linuxsteamrt64")
  set(PLUGIN_VDF_PATH "addons/snowball_round/bin/linuxsteamrt64/snowball_round")
endif()

configure_file(packaging/snowball_round.vdf.in "${CMAKE_CURRENT_BINARY_DIR}/snowball_round.vdf" @ONLY)
install(TARGETS snowball_round LIBRARY DESTINATION "${PLUGIN_BIN_DIR}" RUNTIME DESTINATION "${PLUGIN_BIN_DIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/snowball_round.vdf" DESTINATION addons/metamod)
