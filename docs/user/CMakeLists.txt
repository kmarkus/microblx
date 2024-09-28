# CMakeLists.txt for building Sphinx documentation

# Set variables for the Sphinx build
set(SPHINXBUILD sphinx-build)
set(SOURCEDIR "${CMAKE_SOURCE_DIR}/docs/user")
set(BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SPHINXOPTS "")

# Ensure that Sphinx is available
find_program(SPHINX_EXECUTABLE ${SPHINXBUILD})

if(NOT SPHINX_EXECUTABLE)
  message(FATAL_ERROR "Sphinx executable (${SPHINXBUILD}) not found. Please install Sphinx.")
endif()

# Custom target for help command
add_custom_target(doc-help
  COMMAND ${SPHINX_EXECUTABLE} -M help "${SOURCEDIR}" "${BUILDDIR}" ${SPHINXOPTS}
  COMMENT "Building Sphinx help"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Function to add a Sphinx build command for each possible target
function(add_sphinx_target target)
  add_custom_target(doc-${target}
    COMMAND ${SPHINX_EXECUTABLE} -M ${target} "${SOURCEDIR}" "${BUILDDIR}" ${SPHINXOPTS}
    COMMENT "Building Sphinx target: ${target}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
endfunction()

# Define common Sphinx targets
add_sphinx_target(html)
add_sphinx_target(latexpdf)
add_sphinx_target(man)
add_sphinx_target(text)

# Catch-all target: for any other Sphinx build target
add_custom_target(doc-all
  COMMAND ${SPHINX_EXECUTABLE} -M $ENV{TARGET} "${SOURCEDIR}" "${BUILDDIR}" ${SPHINXOPTS}
  COMMENT "Building Sphinx target: $ENV{TARGET}"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add a custom target that will always be run if "make doc" is used
add_custom_target(doc ALL
  DEPENDS doc-html
  COMMENT "Build all Sphinx documentation"
)
