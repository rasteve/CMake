include(${CMAKE_CURRENT_LIST_DIR}/verify-snippet.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/json.cmake)

file(GLOB snippets ${v1}/data/*)
if (NOT snippets)
  add_error("No snippet files generated")
endif()

set(FOUND_SNIPPETS "")
foreach(snippet ${snippets})
  get_filename_component(filename ${snippet} NAME)

  read_json(${snippet} contents)

  # Verify snippet file is valid
  verify_snippet(${snippet} ${contents})

  # Append to list of collected snippet roles
  if (NOT role IN_LIST FOUND_SNIPPETS)
    list(APPEND FOUND_SNIPPETS ${role})
  endif()

  # Verify target
  string(JSON target ERROR_VARIABLE noTarget GET "${contents}" target)
  if (NOT target MATCHES NOTFOUND)
    set(targets "main;lib;customTarget;TARGET_NAME")
    if (NOT ${target} IN_LIST targets)
      snippet_error(${snippet} "Unexpected target: ${target}")
    endif()
  endif()

  # Verify output
  string(JSON result GET "${contents}" result)
  if (NOT ${result} EQUAL 0)
    snippet_error(${snippet} "Compile command had non-0 result")
  endif()

  # Verify contents of compile-* Snippets
  if (filename MATCHES ^compile-)
    string(JSON target GET "${contents}" target)
    string(JSON source GET "${contents}" source)
    string(JSON language GET "${contents}" language)
    if (NOT language MATCHES "C\\+\\+")
      snippet_error(${snippet} "Expected C++ compile language")
    endif()
    if (NOT source MATCHES "${target}.cxx$")
      snippet_error(${snippet} "Unexpected source file")
    endif()
  endif()

  # Verify contents of link-* Snippets
  if (filename MATCHES ^link-)
    string(JSON target GET "${contents}" target)
    string(JSON targetType GET "${contents}" targetType)
    if (target MATCHES main)
      if (NOT targetType MATCHES "EXECUTABLE")
        snippet_error(${snippet} "Expected EXECUTABLE, target type was ${targetType}")
      endif()
    endif()
    if (target MATCHES lib)
      if (NOT targetType MATCHES "STATIC_LIBRARY")
        snippet_error(${snippet} "Expected STATIC_LIBRARY, target type was ${targetType}")
      endif()
    endif()
  endif()

  # Verify contents of custom-* Snippets
  if (filename MATCHES ^custom-)
    string(JSON outputs GET "${contents}" outputs)
    if (NOT output1 MATCHES "output1" OR NOT output2 MATCHES "output2")
      snippet_error(${snippet} "Custom command missing outputs")
    endif()
  endif()

  # Verify contents of test-* Snippets
  if (filename MATCHES ^test-)
    string(JSON testName GET "${contents}" testName)
    if (NOT testName STREQUAL "test")
      snippet_error(${snippet} "Unexpected testName: ${testName}")
    endif()
  endif()
endforeach()

# Verify that listed snippets match expected roles
set(EXPECTED_SNIPPETS configure generate)
if (ARGS_BUILD)
  list(APPEND EXPECTED_SNIPPETS compile link custom cmakeBuild)
endif()
if (ARGS_TEST)
  list(APPEND EXPECTED_SNIPPETS ctest test)
endif()
if (ARGS_INSTALL)
  list(APPEND EXPECTED_SNIPPETS cmakeInstall)
  if (ARGS_INSTALL_PARALLEL)
    list(APPEND EXPECTED_SNIPPETS install)
  endif()
endif()
foreach(role ${EXPECTED_SNIPPETS})
  list(FIND FOUND_SNIPPETS ${role} found)
  if (${found} EQUAL -1)
    add_error("No snippet files of role \"${role}\" were found in ${v1}")
  endif()
endforeach()
foreach(role ${FOUND_SNIPPETS})
  list(FIND EXPECTED_SNIPPETS ${role} found)
  if (${found} EQUAL -1)
    add_error("Found unexpected snippet file of role \"${role}\" in ${v1}")
  endif()
endforeach()

# Verify test/install artifacts
if (ARGS_INSTALL AND NOT EXISTS ${RunCMake_TEST_BINARY_DIR}/install)
  add_error("ctest --instrument launcher failed to install the project")
endif()
if (ARGS_TEST AND NOT EXISTS ${RunCMake_TEST_BINARY_DIR}/Testing)
  add_error("ctest --instrument launcher failed to test the project")
endif()
