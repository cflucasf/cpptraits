cmake_minimum_required(VERSION 3.21)

project(CppTraits VERSION 0.1.0 LANGUAGES CXX)

function(cpptraits_configure_target target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
    )
endfunction()

add_library(cpptraits INTERFACE)
add_library(CppTraits::CppTraits ALIAS cpptraits)
target_include_directories(cpptraits INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_compile_features(cpptraits INTERFACE cxx_std_20)

option(CPPTRAITS_BUILD_EXAMPLES "Build the CppTraits examples" ${PROJECT_IS_TOP_LEVEL})
option(CPPTRAITS_BUILD_TESTS "Build the dedicated CppTraits tests (requires BUILD_TESTING)" ${PROJECT_IS_TOP_LEVEL})
option(CPPTRAITS_TEST_CLONE_NO_RTTI "Also test clone-only traits without RTTI" OFF)

include(CTest)

if(CPPTRAITS_BUILD_EXAMPLES)
    foreach(example IN ITEMS basic external_mapping ownership defaults static_traits composition
                             concrete_methods class_template associated_types iterable trait_requires)
        set(target cpptraits_example_${example})
        add_executable(${target} examples/${example}/main.cpp)
        target_link_libraries(${target} PRIVATE CppTraits::CppTraits)
        cpptraits_configure_target(${target})
        set_target_properties(${target} PROPERTIES
            OUTPUT_NAME ${example}
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/examples"
        )
        if(BUILD_TESTING)
            add_test(NAME example_${example} COMMAND ${target})
        endif()
    endforeach()
endif()

if(BUILD_TESTING AND CPPTRAITS_BUILD_TESTS)

    add_executable(traits_test tests/traits_test.cpp)
    target_link_libraries(traits_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(traits_test)
    add_test(NAME traits COMMAND traits_test)

    add_executable(vendor_motor_test tests/vendor_motor_test.cpp)
    target_link_libraries(vendor_motor_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(vendor_motor_test)
    add_test(NAME vendor_motor COMMAND vendor_motor_test)

    add_executable(static_traits_test tests/static_traits_test.cpp)
    target_link_libraries(static_traits_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(static_traits_test)
    add_test(NAME static_traits COMMAND static_traits_test)

    add_executable(trait_call_test tests/trait_call_test.cpp)
    target_link_libraries(trait_call_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(trait_call_test)
    add_test(NAME trait_call COMMAND trait_call_test)

    add_executable(trait_requires_test tests/trait_requires_test.cpp)
    target_link_libraries(trait_requires_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(trait_requires_test)
    add_test(NAME trait_requires COMMAND trait_requires_test)

    add_executable(associated_types_test tests/associated_types_test.cpp)
    target_link_libraries(associated_types_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(associated_types_test)
    add_test(NAME associated_types COMMAND associated_types_test)

    add_executable(composed_traits_test tests/composed_traits_test.cpp)
    target_link_libraries(composed_traits_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(composed_traits_test)
    add_test(NAME composed_traits COMMAND composed_traits_test)

    add_executable(namespace_isolation_test tests/namespace_isolation_test.cpp)
    target_link_libraries(namespace_isolation_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(namespace_isolation_test)
    add_test(NAME namespace_isolation COMMAND namespace_isolation_test)

    add_executable(method_declarations_test tests/method_declarations_test.cpp)
    target_link_libraries(method_declarations_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(method_declarations_test)
    add_test(NAME method_declarations COMMAND method_declarations_test)

    add_executable(default_methods_test tests/default_methods_test.cpp)
    target_link_libraries(default_methods_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(default_methods_test)
    add_test(NAME default_methods COMMAND default_methods_test)

    add_executable(concrete_operations_test tests/concrete_operations_test.cpp)
    target_link_libraries(concrete_operations_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(concrete_operations_test)
    add_test(NAME concrete_operations COMMAND concrete_operations_test)

    add_executable(concrete_methods_test tests/concrete_methods_test.cpp)
    target_link_libraries(concrete_methods_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(concrete_methods_test)
    target_compile_options(concrete_methods_test PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Werror=suggest-override>
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Werror=inconsistent-missing-override>
    )
    add_test(NAME concrete_methods COMMAND concrete_methods_test)

    add_executable(concrete_clone_test tests/concrete_clone_test.cpp)
    target_link_libraries(concrete_clone_test PRIVATE CppTraits::CppTraits)
    cpptraits_configure_target(concrete_clone_test)
    add_test(NAME concrete_clone COMMAND concrete_clone_test)


    if(CPPTRAITS_TEST_CLONE_NO_RTTI)
        add_executable(concrete_clone_no_rtti_test tests/concrete_clone_test.cpp)
        target_link_libraries(concrete_clone_no_rtti_test PRIVATE CppTraits::CppTraits)
        cpptraits_configure_target(concrete_clone_no_rtti_test)
        target_compile_definitions(concrete_clone_no_rtti_test PRIVATE CPPTRAITS_TEST_CLONE_NO_RTTI)
        target_compile_options(concrete_clone_no_rtti_test PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-rtti>
            $<$<CXX_COMPILER_ID:MSVC>:/GR->
        )
        add_test(NAME concrete_clone_no_rtti COMMAND concrete_clone_no_rtti_test)
    endif()
endif()
