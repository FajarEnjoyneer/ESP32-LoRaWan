cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# PERBAIKAN FINAL: Gunakan idf_build_set_property
# Ini adalah cara paling kuat di ESP-IDF untuk memaksa flag compiler ke SEMUA komponen (termasuk managed components)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=overloaded-virtual" APPEND)

project(ESP32-LoRaWan)