if(NOT ENABLE_DOCS)
	message(STATUS "Skipping documentation generation")
	return()
endif()

add_custom_target(wz2100_doc ALL)
set_property(TARGET wz2100_doc PROPERTY FOLDER "doc")

set(wz2100_doc_FILES)

#######################
# Scripting docs

add_custom_command(
	TARGET wz2100_doc
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/GenerateJSDocs.cmake
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	VERBATIM
)

configure_file(Scripting.md "${CMAKE_CURRENT_BINARY_DIR}/Scripting.md" COPYONLY)

list(APPEND wz2100_doc_FILES
	"${CMAKE_CURRENT_BINARY_DIR}/js-globals.md"
	"${CMAKE_CURRENT_BINARY_DIR}/js-events.md"
	"${CMAKE_CURRENT_BINARY_DIR}/js-functions.md"
	"${CMAKE_CURRENT_BINARY_DIR}/js-objects.md"
	"${CMAKE_CURRENT_BINARY_DIR}/js-campaign.md"
	"${CMAKE_CURRENT_BINARY_DIR}/Scripting.md"

	"${CMAKE_CURRENT_SOURCE_DIR}/ScriptingManual.htm"
)

#######################
# Hosting documentation

set(host_doc_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/hosting/AutohostConfig.md"
	"${CMAKE_CURRENT_SOURCE_DIR}/hosting/AutoratingServer.md"
	"${CMAKE_CURRENT_SOURCE_DIR}/hosting/DedicatedHost.md"
	)
set(host_doc_linux_scripts_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/hosting/linux_scripts/common.sh"
	"${CMAKE_CURRENT_SOURCE_DIR}/hosting/linux_scripts/config_sample.sh"
	"${CMAKE_CURRENT_SOURCE_DIR}/hosting/linux_scripts/game_sample.sh"
	"${CMAKE_CURRENT_SOURCE_DIR}/hosting/linux_scripts/wz2100host_sample.service"
	)

#######################
# Main documentation

set(SKIPPED_DOC_GENERATION FALSE)
find_package(Asciidoctor 1.5.3) # 1.5.3 adds built-in manpage backend; 1.5.1 adds "-S flag in cli recognizes safe mode name as lowercase string"
if(NOT Asciidoctor_FOUND)
	find_package(A2X)
	if(NOT A2X_FOUND)
		message( WARNING "Neither Asciidoctor nor a2x was found. Documentation cannot be generated unless at least one is installed. (Skipping documentation generation.)" )
		set(SKIPPED_DOC_GENERATION TRUE)
	endif()
endif()

set(doc_IMAGES
	"${CMAKE_CURRENT_SOURCE_DIR}/images/artillery-far-away.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/artillery-sensor.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/attackrange.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/awaymission.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/building.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/building-select.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/cb-sensor.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/cb-sensor-vtol.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/cheapweapon.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/commander-factory-assignment.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/commander-panel.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/commander.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/commandpanel.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/design-bars.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/design.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/design-more.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/design-screen.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/design-unit.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/expensiveweapon.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/firing.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/hq.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/indirect-fire-support.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/intelligencedisplay.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/interface.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/logo.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/manufacture.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/manufacture-select.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/minimap.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/movement.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/oilresource.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/powerbar.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/powerupgrade.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/rallypoints.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/recycling.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/research.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/research-select.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/retreatthreshold.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/return.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/satellite-uplink.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/sensor.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/sensor-tower.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/transport.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/unitordersmenu.jpg"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/wss.png"
	"${CMAKE_CURRENT_SOURCE_DIR}/images/www.png"
	)

if(Asciidoctor_FOUND)
	# Prefer Asciidoctor if it's available

	set(_asciidoctor_option_failurelevel)
	if((Asciidoctor_VERSION VERSION_GREATER "1.5.7") OR (Asciidoctor_VERSION VERSION_EQUAL "1.5.7"))
		# Asciidoctor 1.5.7+ supports the "--failure-level" CLI option
		set(_asciidoctor_option_failurelevel "--failure-level=ERROR")
	endif()

	configure_file("quickstartguide.asciidoc" "${CMAKE_CURRENT_BINARY_DIR}/quickstartguide.asciidoc" COPYONLY)

	# Quickstart Guide - HTML format
	add_custom_command(
		TARGET wz2100_doc
		POST_BUILD
		COMMAND ${Asciidoctor_COMMAND}
			-S server
			${_asciidoctor_option_failurelevel}
			-B ${CMAKE_CURRENT_BINARY_DIR}
			-b html5
			-D ${CMAKE_CURRENT_BINARY_DIR}
			-a toc
			${CMAKE_CURRENT_BINARY_DIR}/quickstartguide.asciidoc
		VERBATIM
	)

	configure_file("warzone2100.6.asciidoc" "${CMAKE_CURRENT_BINARY_DIR}/warzone2100.6.asciidoc" COPYONLY)

	# warzone2100.6 - HTML format
	add_custom_command(
		TARGET wz2100_doc
		POST_BUILD
		COMMAND ${Asciidoctor_COMMAND}
			-S server
			${_asciidoctor_option_failurelevel}
			-B ${CMAKE_CURRENT_BINARY_DIR}
			-b html5
			-D ${CMAKE_CURRENT_BINARY_DIR}
			${CMAKE_CURRENT_BINARY_DIR}/warzone2100.6.asciidoc
		VERBATIM
	)

	# warzone2100.6 - man-page format
	add_custom_command(
		TARGET wz2100_doc
		POST_BUILD
		COMMAND ${Asciidoctor_COMMAND}
			-S server
			${_asciidoctor_option_failurelevel}
			-B ${CMAKE_CURRENT_BINARY_DIR}
			-b manpage
			-D ${CMAKE_CURRENT_BINARY_DIR}
			-o warzone2100.6
			${CMAKE_CURRENT_BINARY_DIR}/warzone2100.6.asciidoc
		VERBATIM
	)

	unset(_asciidoctor_option_failurelevel)

elseif(A2X_FOUND)

	set(_xml_catalog_files_env_var)

	if(CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
		# Custom handling for Homebrew-installed A2X on macOS

		execute_process(
			COMMAND brew --prefix
			OUTPUT_VARIABLE _BREW_PREFIX
			ERROR_VARIABLE _BREW_ERROR
			RESULT_VARIABLE _BREW_RESULT
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		if((_BREW_RESULT EQUAL 0) AND (A2X_COMMAND MATCHES "^${_BREW_PREFIX}/*"))
			# HOMEBREW WORKAROUND:
			# Workaround an issue where a2x fails while trying to xmllint by explicitly
			# exporting the expected catalog file path.
			message( STATUS "Workaround for Homebrew-installed A2X" )
			set(_xml_catalog_files_env_var "XML_CATALOG_FILES=${_BREW_PREFIX}/etc/xml/catalog")
		endif()
	endif()

	set(_cmd_prefix_set_env)
	if (_xml_catalog_files_env_var)
		set(_cmd_prefix_set_env "${CMAKE_COMMAND}" -E env "${_xml_catalog_files_env_var}")
	endif()

	set(_quickstart_asciidoc "${CMAKE_CURRENT_SOURCE_DIR}/quickstartguide.asciidoc")
	# Quickstart Guide - HTML format
	add_custom_command(
		TARGET wz2100_doc
		POST_BUILD
		COMMAND ${_cmd_prefix_set_env} ${A2X_COMMAND} -f xhtml -D ${CMAKE_CURRENT_BINARY_DIR} ${_quickstart_asciidoc}
		VERBATIM
	)

	set(_warzone2100_6_asciidoc "${CMAKE_CURRENT_SOURCE_DIR}/warzone2100.6.asciidoc")
	# warzone2100.6 - HTML format
	add_custom_command(
		TARGET wz2100_doc
		POST_BUILD
		COMMAND ${_cmd_prefix_set_env} ${A2X_COMMAND} -f xhtml -D ${CMAKE_CURRENT_BINARY_DIR} ${_warzone2100_6_asciidoc}
		VERBATIM
	)
	# warzone2100.6 - man-page format
	add_custom_command(
		TARGET wz2100_doc
		POST_BUILD
		COMMAND ${_cmd_prefix_set_env} ${A2X_COMMAND} -f manpage -D ${CMAKE_CURRENT_BINARY_DIR} ${_warzone2100_6_asciidoc}
		VERBATIM
	)

	if (${WKHTMLTOPDF_AVAILABLE})

		add_custom_command(
			TARGET wz2100_doc
			POST_BUILD
			COMMAND "wkhtmltopdf"
			ARGS
				"${CMAKE_CURRENT_SOURCE_DIR}/quickstartguide.htm"
				"${CMAKE_CURRENT_BINARY_DIR}/quickstartguide.pdf"
			)

	endif()

endif()

if(NOT SKIPPED_DOC_GENERATION)
	set(wz2100_doc_IMAGES_FILES
		${doc_IMAGES}
	)

	list(APPEND wz2100_doc_FILES
		"${CMAKE_CURRENT_SOURCE_DIR}/docbook-xsl.css"
		"${CMAKE_CURRENT_BINARY_DIR}/quickstartguide.html"
		"${CMAKE_CURRENT_BINARY_DIR}/warzone2100.6.html"
	)
else()
	unset(wz2100_doc_IMAGES_FILES)
endif()

include(GNUInstallDirs)
if(wz2100_doc_IMAGES_FILES)
	install(FILES ${wz2100_doc_IMAGES_FILES} DESTINATION "${CMAKE_INSTALL_DOCDIR}${WZ_OUTPUT_NAME_SUFFIX}/images" COMPONENT Docs)
endif()
install(FILES ${wz2100_doc_FILES} DESTINATION "${CMAKE_INSTALL_DOCDIR}${WZ_OUTPUT_NAME_SUFFIX}" COMPONENT Docs)
install(FILES ${host_doc_FILES} DESTINATION "${CMAKE_INSTALL_DOCDIR}${WZ_OUTPUT_NAME_SUFFIX}/hosting" COMPONENT Docs)
install(FILES ${host_doc_linux_scripts_FILES} DESTINATION "${CMAKE_INSTALL_DOCDIR}${WZ_OUTPUT_NAME_SUFFIX}/hosting/linux_scripts" COMPONENT Docs)

if(UNIX AND NOT EMSCRIPTEN AND NOT SKIPPED_DOC_GENERATION)
	# Man-page gzip and installation

	find_program(GZIP_BIN NAMES gzip PATHS /bin /usr/bin /usr/local/bin)
	if (GZIP_BIN)
		set(_warzone2100_6_uncompressed_orig "${CMAKE_CURRENT_BINARY_DIR}/warzone2100.6")
		set(_warzone2100_6_uncompressed "${CMAKE_CURRENT_BINARY_DIR}/warzone2100${WZ_OUTPUT_NAME_SUFFIX}.6")
		set(_warzone2100_6_compressed "${_warzone2100_6_uncompressed}.gz")

		add_custom_target(wz2100_manpages ALL)
		set_property(TARGET wz2100_manpages PROPERTY FOLDER "doc")
		add_dependencies(wz2100_manpages wz2100_doc)

		if (NOT "${_warzone2100_6_uncompressed_orig}" STREQUAL "${_warzone2100_6_uncompressed}")
			add_custom_command(
				TARGET wz2100_manpages
				POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy "${_warzone2100_6_uncompressed_orig}" "${_warzone2100_6_uncompressed}"
				COMMAND ${GZIP_BIN} -n -c ${_warzone2100_6_uncompressed} > ${_warzone2100_6_compressed}
				VERBATIM
			)
		else()
			add_custom_command(
				TARGET wz2100_manpages
				POST_BUILD
				COMMAND ${GZIP_BIN} -n -c ${_warzone2100_6_uncompressed} > ${_warzone2100_6_compressed}
				VERBATIM
			)
		endif()

		install(FILES "${_warzone2100_6_compressed}" DESTINATION "${CMAKE_INSTALL_MANDIR}/man6" COMPONENT Manpages)
	else()
		message( WARNING "Unable to find gzip tool; skipping Unix man page generation" )
	endif()
endif()

set(wz2100_doc_IMAGES_FILES ${wz2100_doc_IMAGES_FILES} PARENT_SCOPE)
set(wz2100_doc_FILES ${wz2100_doc_FILES} PARENT_SCOPE)
