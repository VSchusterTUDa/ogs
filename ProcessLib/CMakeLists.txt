include(${PROJECT_SOURCE_DIR}/scripts/cmake/OGSEnabledElements.cmake)

# Collect the process libraries in interface library
add_library(Processes INTERFACE)

foreach(process ${ProcessesList})
    if(OGS_BUILD_PROCESS_${process})
        add_subdirectory(${process})
        target_link_libraries(Processes INTERFACE ${process})
    endif()
endforeach()


APPEND_SOURCE_FILES(SOURCES)
APPEND_SOURCE_FILES(SOURCES BoundaryCondition)
APPEND_SOURCE_FILES(SOURCES CalculateSurfaceFlux)
APPEND_SOURCE_FILES(SOURCES Output)
APPEND_SOURCE_FILES(SOURCES Parameter)
APPEND_SOURCE_FILES(SOURCES SourceTerms)
APPEND_SOURCE_FILES(SOURCES Utils)

add_library(ProcessLib ${SOURCES})

target_link_libraries(ProcessLib
    PUBLIC BaseLib MaterialLib MathLib MeshLib NumLib logog
)

if(OGS_USE_PYTHON)
    add_subdirectory(BoundaryCondition/Python)
    target_link_libraries(ProcessLib
        PUBLIC ProcessLibBoundaryConditionPython)
endif()

if(OGS_INSITU)
    target_link_libraries(ProcessLib InSituLib)
endif()

if(OGS_USE_PCH)
    cotire(ProcessLib)
endif()
