# Tests live at the target root, NOT under src/. Two reasons:
#   - t_AudioEffectRegistry covers a PUBLIC header; filing it under src/ would
#     have contradicted the very distinction this layout exists to make.
#   - src/ is then exactly "the code that ships", which matters for packaging.
# The sub-unit structure is mirrored so each unit keeps its own CMakeLists.
add_subdirectory(AudioGraph)
add_subdirectory(AudioMixer)
