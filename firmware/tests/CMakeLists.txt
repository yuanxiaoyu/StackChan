cmake_minimum_required(VERSION 3.16)

project(stackchan_firmware_tests LANGUAGES C CXX)

enable_testing()

add_executable(game_menu_test game_menu_test.cpp)
target_include_directories(game_menu_test PRIVATE ../main)
target_compile_features(game_menu_test PRIVATE cxx_std_17)
add_test(NAME game_menu_test COMMAND $<TARGET_FILE:game_menu_test>)

add_executable(game_video_test game_video_test.cpp ../main/apps/app_game/game_video.cpp)
target_include_directories(game_video_test PRIVATE ../main)
target_compile_features(game_video_test PRIVATE cxx_std_17)
add_test(NAME game_video_test COMMAND $<TARGET_FILE:game_video_test>)

add_executable(game_remote_test game_remote_test.cpp)
target_include_directories(game_remote_test PRIVATE ../main ../../remote/code/main)
target_compile_features(game_remote_test PRIVATE cxx_std_17)
add_test(NAME game_remote_test COMMAND $<TARGET_FILE:game_remote_test>)

add_executable(motion_math_test
    motion_math_test.cpp
    ../main/stackchan/motion/motion_math.cpp
)

target_include_directories(motion_math_test PRIVATE
    ../main
)

target_compile_features(motion_math_test PRIVATE cxx_std_17)

add_test(NAME motion_math_test COMMAND $<TARGET_FILE:motion_math_test>)

add_executable(agent_tasks_model_test agent_tasks_model_test.cpp)
target_include_directories(agent_tasks_model_test PRIVATE ../main)
target_compile_features(agent_tasks_model_test PRIVATE cxx_std_17)
add_test(NAME agent_tasks_model_test COMMAND $<TARGET_FILE:agent_tasks_model_test>)

add_executable(home_gesture_test home_gesture_test.cpp)
target_include_directories(home_gesture_test PRIVATE ../main)
target_compile_features(home_gesture_test PRIVATE cxx_std_17)
add_test(NAME home_gesture_test COMMAND $<TARGET_FILE:home_gesture_test>)

add_executable(game_rom_test game_rom_test.cpp ../main/apps/app_game/rom_info.cpp)
target_include_directories(game_rom_test PRIVATE ../main)
target_compile_features(game_rom_test PRIVATE cxx_std_17)
add_test(NAME game_rom_test COMMAND $<TARGET_FILE:game_rom_test>)

add_executable(gamepad_hid_test gamepad_hid_test.cpp ../main/hal/gamepad/hid_parser.cpp)
target_include_directories(gamepad_hid_test PRIVATE ../main)
target_compile_features(gamepad_hid_test PRIVATE cxx_std_17)
add_test(NAME gamepad_hid_test COMMAND $<TARGET_FILE:gamepad_hid_test>)

file(GLOB NOFRENDO_HOST_SOURCES ../components/nofrendo/nes/*.c ../components/nofrendo/mappers/*.c)
add_executable(nofrendo_smoke_test nofrendo_smoke_test.c ${NOFRENDO_HOST_SOURCES})
target_include_directories(nofrendo_smoke_test PRIVATE ../components/nofrendo)
target_compile_options(nofrendo_smoke_test PRIVATE -Wno-format)
target_compile_definitions(nofrendo_smoke_test PRIVATE calloc=nofrendo_test_calloc)
target_link_libraries(nofrendo_smoke_test PRIVATE m)
add_test(NAME nofrendo_smoke_test COMMAND $<TARGET_FILE:nofrendo_smoke_test>)
