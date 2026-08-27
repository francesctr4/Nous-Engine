set(CLASS_NAME "ModuleAudio")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ModuleBase
        AudioSystem_headers   # ModuleAudio.h names SoundHandle/AudioBus/IAudioBroker
        EventSystem_headers   # <Module>.h implements IEventListener
)

target_link_libraries(${CLASS_NAME} PRIVATE
        AudioSystem
        EventSystem
        Logger
        MemoryManager
        ResourceManager
)