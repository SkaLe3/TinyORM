# Configure passed auto test
function(tiny_configure_test name)

    set(options INCLUDE_MODELS)
    cmake_parse_arguments(PARSE_ARGV 1 TINY "${options}" "" "")

    target_precompile_headers(${name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:"${CMAKE_SOURCE_DIR}/include/pch.h">
    )

    set_target_properties(${name}
        PROPERTIES
            C_VISIBILITY_PRESET "hidden"
            CXX_VISIBILITY_PRESET "hidden"
            VISIBILITY_INLINES_HIDDEN YES
    )

    target_compile_features(${name} PRIVATE cxx_std_20)

    target_compile_definitions(${name}
        PRIVATE
            PROJECT_TINYORM_TEST
            TINYORM_TESTS_CODE
            TINYORM_LINKING_SHARED
    )

    target_include_directories(${name} PRIVATE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    if(TINY_INCLUDE_MODELS)
        target_include_directories(${name} PRIVATE
            "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests/models>"
        )
    endif()

    target_link_libraries(${name}
        PRIVATE
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Sql
            Qt${QT_VERSION_MAJOR}::Test
            # TODO do I need this? silverqx
#            range-v3::range-v3
            TinyOrm::CommonConfig
            TinyOrm::TinyUtils
            TinyOrm::TinyOrm
    )

endfunction()
