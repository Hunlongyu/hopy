function(hopy_add_test name)
  qt_add_executable(${name} ${name}.cpp)
  # Console subsystem so QtTest output reaches stdout/ctest logs (static Qt
  # otherwise defaults test exes to the GUI subsystem with no console).
  set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF)
  target_link_libraries(${name} PRIVATE hopy_lib Qt6::Test)
  # Tests statically link Qt too; restrict SQL drivers to SQLite so psql/odbc
  # plugins aren't auto-imported (they'd need libpq and fail to link).
  qt_import_plugins(${name} INCLUDE_BY_TYPE sqldrivers Qt6::QSQLiteDriverPlugin)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

hopy_add_test(test_smoke)
hopy_add_test(test_hash)
hopy_add_test(test_repository)
hopy_add_test(test_typefilter)
hopy_add_test(test_screenplacement)
hopy_add_test(test_settings)
hopy_add_test(test_clipboard_detect)
hopy_add_test(test_colordetect)
hopy_add_test(test_version)
hopy_add_test(test_update_parse)
hopy_add_test(test_update_checksum)
hopy_add_test(test_update_installer)
hopy_add_test(test_record_delegate)
hopy_add_test(test_open_target)
hopy_add_test(test_text_info)
