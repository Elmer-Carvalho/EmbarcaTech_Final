# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(adc C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(monitorador_de_sons 
    monitorador_de_sons.c 
    ./inc/ssd1306.c
)

# Gera o cabeçalho PIO a partir do arquivo pio_matrix.pio
pico_generate_pio_header(monitorador_de_sons ${CMAKE_CURRENT_LIST_DIR}/inc/pio_matrix.pio)

pico_set_program_name(monitorador_de_sons "monitorador_de_sons")
pico_set_program_version(monitorador_de_sons "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(monitorador_de_sons 1)
pico_enable_stdio_usb(monitorador_de_sons 1)

# Add the standard library to the build
target_link_libraries(monitorador_de_sons
        pico_stdlib
        pico_time
        hardware_adc
        hardware_pwm
        hardware_i2c
        hardware_pio
        hardware_dma
)
        

# Add the standard include files to the build
target_include_directories(monitorador_de_sons PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/inc  # Garante que ./inc esteja no caminho de inclusão
)

# Add any user requested libraries
target_link_libraries(monitorador_de_sons 
        
        )

pico_add_extra_outputs(monitorador_de_sons)

