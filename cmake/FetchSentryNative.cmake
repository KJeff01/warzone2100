
# Get the URL AND SHA256 from a data file
set(_sentry_dl_data_file "${PROJECT_SOURCE_DIR}/.sentrynative")
file(STRINGS "${_sentry_dl_data_file}" _sentry_native_url_info ENCODING UTF-8)
foreach(LINE IN LISTS _sentry_native_url_info)
	if (LINE MATCHES "^URL=(.*)")
		set(SENTRY_NATIVE_DL_URL "${CMAKE_MATCH_1}")
	endif()
	if (LINE MATCHES "^SHA512=(.*)")
		set(SENTRY_NATIVE_DL_SHA512 "${CMAKE_MATCH_1}")
	endif()
endforeach()
unset(_sentry_native_url_info)
if(NOT DEFINED SENTRY_NATIVE_DL_URL OR NOT DEFINED SENTRY_NATIVE_DL_SHA512)
	message(FATAL_ERROR "Failed to load URL and hash from: ${_sentry_dl_data_file}")
endif()
unset(_sentry_dl_data_file)

include(FetchContent)
set(_sentrynative_source_dir "${FETCHCONTENT_BASE_DIR}/sentrynative-src")
if(NOT DEFINED WZ_SENTRY_PREDOWNLOADED_SENTRY_ARCHIVE)
	set(_sentrynative_fetch_download_options
		URL "${SENTRY_NATIVE_DL_URL}"
		URL_HASH SHA512=${SENTRY_NATIVE_DL_SHA512}
	)
else()
	file(MAKE_DIRECTORY "${_sentrynative_source_dir}")
	if(NOT EXISTS "${WZ_SENTRY_PREDOWNLOADED_SENTRY_ARCHIVE}")
		message(FATAL_ERROR "Unable to find suppled WZ_SENTRY_PREDOWNLOADED_SENTRY_ARCHIVE: \"${WZ_SENTRY_PREDOWNLOADED_SENTRY_ARCHIVE}\"")
	endif()
	file(SHA512 "${WZ_SENTRY_PREDOWNLOADED_SENTRY_ARCHIVE}" _predownloaded_file_hash)
	if(NOT _predownloaded_file_hash STREQUAL "${SENTRY_NATIVE_DL_SHA512}")
		message(FATAL_ERROR "SHA512 of supplied file (${_predownloaded_file_hash}) does not match expected SHA512 (${SENTRY_NATIVE_DL_SHA512}): ${WZ_SENTRY_PREDOWNLOADED_SENTRY_ARCHIVE}")
	endif()
	unset(_predownloaded_file_hash)
	set(_download_script "${CMAKE_CURRENT_BINARY_DIR}/extract-sentry-native.cmake")
	file(WRITE "${_download_script}" "
	execute_process(
	  COMMAND \"${CMAKE_COMMAND}\" -E tar xvf \"${WZ_SENTRY_PREDOWNLOADED_SENTRY_ARCHIVE}\"
	  WORKING_DIRECTORY \"${_sentrynative_source_dir}\"
	)
	")
	set(_sentrynative_fetch_download_options
		DOWNLOAD_COMMAND ${CMAKE_COMMAND} -P "${_download_script}"
	)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	# Sentry-native requires CMAKE_SYSTEM_VERSION to be properly set
	if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		set(CMAKE_SYSTEM_VERSION "10")
	else()
		set(CMAKE_SYSTEM_VERSION "6.1") # Windows 7+
	endif()
endif()
if(MINGW AND ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))
	# Sentry-native currently requires uasm when compiling via mingw-clang
	if(NOT DEFINED CMAKE_ASM_MASM_COMPILER)
		set(CMAKE_ASM_MASM_COMPILER "uasm")
	endif()
endif()
FetchContent_Declare(
	sentrynative
	${_sentrynative_fetch_download_options}
	SOURCE_DIR "${_sentrynative_source_dir}"
	PATCH_COMMAND ${CMAKE_COMMAND} "-DSOURCE_DIR=<SOURCE_DIR>" -P "${CMAKE_SOURCE_DIR}/lib/exceptionhandler/3rdparty/sentry/PatchSentryNative.cmake"
	EXCLUDE_FROM_ALL # Requires CMake 3.28+
	# Intentionally prevent FetchContent_MakeAvailable from adding the subdirectory automatically
	# Reference: https://discourse.cmake.org/t/prevent-fetchcontent-makeavailable-to-execute-cmakelists-txt/12704/3
	SOURCE_SUBDIR dummy_noexist
)
set(SENTRY_BUILD_SHARED_LIBS OFF CACHE BOOL "Sentry build shared libs" FORCE)
set(SENTRY_EXPORT_SYMBOLS OFF CACHE BOOL "Export symbols" FORCE)
set(SENTRY_BUILD_RUNTIMESTATIC OFF CACHE BOOL "Build sentry-native with static runtime" FORCE)
if(MINGW)
	SET(CRASHPAD_WER_ENABLED TRUE)
endif()
if(CMAKE_CXX_STANDARD)
	set(_old_CMAKE_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
	unset(CMAKE_CXX_STANDARD) # Allow sentry-native to set its desired default
endif()
if(CMAKE_CXX_EXTENSIONS)
	set(_old_CMAKE_CXX_EXTENSIONS "${CMAKE_CXX_EXTENSIONS}")
	unset(CMAKE_CXX_EXTENSIONS) # Allow sentry-native to set its desired default
endif()
if(CMAKE_SYSTEM_NAME MATCHES "Darwin|Linux" AND NOT DEFINED SENTRY_BACKEND)
	set(SENTRY_BACKEND "breakpad" CACHE STRING
	"The sentry backend responsible for reporting crashes, can be either 'none', 'inproc', 'breakpad' or 'crashpad'." FORCE)
endif()
FetchContent_MakeAvailable(sentrynative)
add_subdirectory("${sentrynative_SOURCE_DIR}" "${sentrynative_BINARY_DIR}" EXCLUDE_FROM_ALL) # Because FetchContent_Declare EXCLUDE_FROM_ALL requires CMake 3.28+
message(STATUS "Enabling crash-handling backend: sentry-native ($CACHE{SENTRY_BACKEND}) for (${CMAKE_SYSTEM_NAME}:${CMAKE_SYSTEM_PROCESSOR})")

####################
# Silencing warnings

if(NOT MSVC)

	include(CheckCompilerFlagsOutput)

	set(_supported_sentry_c_compiler_flags "")
	set(_supported_sentry_cxx_compiler_flags "")

	# -Wshadow					(GCC 3.4+, Clang 3.2+)
	check_compiler_flags_output("-Werror -Wno-shadow -Wno-error=cpp" COMPILER_TYPE C   OUTPUT_FLAGS "-Wno-shadow" OUTPUT_VARIABLE _supported_sentry_c_compiler_flags APPEND)
	check_compiler_flags_output("-Werror -Wno-shadow -Wno-error=cpp" COMPILER_TYPE CXX   OUTPUT_FLAGS "-Wno-shadow" OUTPUT_VARIABLE _supported_sentry_cxx_compiler_flags APPEND)

	# -Wunused-but-set-variable
	check_compiler_flags_output("-Werror -Wno-unused-but-set-variable -Wno-error=cpp" COMPILER_TYPE C   OUTPUT_FLAGS "-Wno-unused-but-set-variable" OUTPUT_VARIABLE _supported_sentry_c_compiler_flags APPEND)
	check_compiler_flags_output("-Werror -Wno-unused-but-set-variable -Wno-error=cpp" COMPILER_TYPE CXX   OUTPUT_FLAGS "-Wno-unused-but-set-variable" OUTPUT_VARIABLE _supported_sentry_cxx_compiler_flags APPEND)

	# -Wconditional-uninitialized
	check_compiler_flags_output("-Werror -Wno-conditional-uninitialized -Wno-error=cpp" COMPILER_TYPE C   OUTPUT_FLAGS "-Wno-conditional-uninitialized" OUTPUT_VARIABLE _supported_sentry_c_compiler_flags APPEND)
	check_compiler_flags_output("-Werror -Wno-conditional-uninitialized -Wno-error=cpp" COMPILER_TYPE CXX   OUTPUT_FLAGS "-Wno-conditional-uninitialized" OUTPUT_VARIABLE _supported_sentry_cxx_compiler_flags APPEND)

	# -Wassign-enum
	check_compiler_flags_output("-Werror -Wno-assign-enum -Wno-error=cpp" COMPILER_TYPE C   OUTPUT_FLAGS "-Wno-assign-enum" OUTPUT_VARIABLE _supported_sentry_c_compiler_flags APPEND)
	check_compiler_flags_output("-Werror -Wno-assign-enum -Wno-error=cpp" COMPILER_TYPE CXX   OUTPUT_FLAGS "-Wno-assign-enum" OUTPUT_VARIABLE _supported_sentry_cxx_compiler_flags APPEND)

	# -Wunknown-pragmas (caused by breakpad header)
	check_compiler_flags_output("-Werror -Wno-unknown-pragmas -Wno-error=cpp" COMPILER_TYPE C   OUTPUT_FLAGS "-Wno-unknown-pragmas" OUTPUT_VARIABLE _supported_sentry_c_compiler_flags APPEND)
	check_compiler_flags_output("-Werror -Wno-unknown-pragmas -Wno-error=cpp" COMPILER_TYPE CXX   OUTPUT_FLAGS "-Wno-unknown-pragmas" OUTPUT_VARIABLE _supported_sentry_cxx_compiler_flags APPEND)

	if (NOT _supported_sentry_c_compiler_flags STREQUAL "")
		string(REPLACE " " ";" _supported_sentry_c_compiler_flags "${_supported_sentry_c_compiler_flags}")
	endif()
	if (NOT _supported_sentry_cxx_compiler_flags STREQUAL "")
		string(REPLACE " " ";" _supported_sentry_cxx_compiler_flags "${_supported_sentry_cxx_compiler_flags}")
	endif()

	if(TARGET sentry)
		target_compile_options(sentry PRIVATE "$<$<COMPILE_LANGUAGE:C>:${_supported_sentry_c_compiler_flags}>")
		target_compile_options(sentry PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${_supported_sentry_cxx_compiler_flags}>")
	endif()

endif()

####################

if(_old_CMAKE_CXX_STANDARD)
	set(CMAKE_CXX_STANDARD "${_old_CMAKE_CXX_STANDARD}")
endif()
if(_old_CMAKE_CXX_EXTENSIONS)
	set(CMAKE_CXX_EXTENSIONS "${_old_CMAKE_CXX_EXTENSIONS}")
endif()
