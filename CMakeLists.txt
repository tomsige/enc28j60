cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(enc28j60)
#set(CMAKE_C_STANDARD 11)
#set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()
add_executable(enc28j60 main.c enc28j60.c ip_arp_udp_tcp.c)
pico_enable_stdio_uart(enc28j60 1)

pico_add_extra_outputs(enc28j60)
target_link_libraries(enc28j60 pico_stdlib hardware_spi)
