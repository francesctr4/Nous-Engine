set(CLASS_NAME "NOUS_Job")

# Private half only: the public header is API and lives at
# include/NOUS_Multithreading/NOUS_Job.h, listed in the target root's CMakeLists.
target_sources(NOUS_Multithreading PRIVATE
        ${CLASS_NAME}.cpp
)
