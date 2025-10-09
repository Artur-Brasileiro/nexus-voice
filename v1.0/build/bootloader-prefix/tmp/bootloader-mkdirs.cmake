# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/artur/esp/esp-idf/components/bootloader/subproject"
  "/home/artur/esp/NexusVoice_Com_WakeWord/v1.0/build/bootloader"
  "/home/artur/esp/NexusVoice_Com_WakeWord/v1.0/build/bootloader-prefix"
  "/home/artur/esp/NexusVoice_Com_WakeWord/v1.0/build/bootloader-prefix/tmp"
  "/home/artur/esp/NexusVoice_Com_WakeWord/v1.0/build/bootloader-prefix/src/bootloader-stamp"
  "/home/artur/esp/NexusVoice_Com_WakeWord/v1.0/build/bootloader-prefix/src"
  "/home/artur/esp/NexusVoice_Com_WakeWord/v1.0/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/artur/esp/NexusVoice_Com_WakeWord/v1.0/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/artur/esp/NexusVoice_Com_WakeWord/v1.0/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
