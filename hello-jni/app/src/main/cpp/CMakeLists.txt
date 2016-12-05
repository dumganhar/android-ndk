cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -DCOCOS2D_DEBUG=1")

include_directories(
            ${CMAKE_CURRENT_SOURCE_DIR})

add_library(hello-jni SHARED
            hello-jni.c
            audio.cpp
            audio/android/audio_utils/format.c
            audio/android/audio_utils/minifloat.cpp
            audio/android/audio_utils/primitives.c
            audio/android/AssetFd.cpp
            audio/android/AudioDecoder.cpp
            audio/android/AudioMixer.cpp
            audio/android/AudioMixerController.cpp
            audio/android/AudioPlayerProvider.cpp
            audio/android/AudioResampler.cpp
            audio/android/AudioResamplerCubic.cpp
            audio/android/CCThreadPool.cpp
            audio/android/PcmAudioPlayer.cpp
            audio/android/PcmAudioService.cpp
            audio/android/PcmBufferProvider.cpp
            audio/android/PcmData.cpp
            audio/android/Track.cpp
            audio/android/UrlAudioPlayer.cpp)

# Include libraries needed for hello-jni lib
target_link_libraries(hello-jni
                      android
                      OpenSLES
                      log)
