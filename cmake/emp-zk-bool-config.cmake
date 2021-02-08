find_package(emp-ot)

find_path(EMP-ZK-BOOL_INCLUDE_DIR emp-zk-bool/emp-zk-bool.h)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(emp-zk-bool DEFAULT_MSG EMP-ZK-BOOL_INCLUDE_DIR)

if(EMP-ZK-BOOL_FOUND)
	set(EMP-ZK-BOOL_INCLUDE_DIRS ${EMP-ZK-BOOL_INCLUDE_DIR} ${EMP-OT_INCLUDE_DIRS})
	set(EMP-ZK-BOOL_LIBRARIES ${EMP-OT_LIBRARIES})
endif()
