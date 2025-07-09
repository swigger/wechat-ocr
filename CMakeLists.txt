cmake_minimum_required(VERSION 3.20)

# 项目名称
project(wechat-ocr)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加头文件搜索路径
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vs.proj")

if (WIN32)
	# add _CRT_SECURE_NO_WARNINGS
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D_UNICODE -DUNICODE -EHsc)
	# find_package(Protobuf REQUIRED)
	link_directories("${CMAKE_CURRENT_SOURCE_DIR}/spt/x64")
	set(Protobuf_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spt" CACHE INTERNAL "")
	set(Protobuf_LIBRARIES "libprotobuf-lite.lib" CACHE INTERNAL "")
else()
	# 查找或下载并编译 Google Protobuf
	include(FetchContent)
	FetchContent_Declare(protobuf
		GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
		GIT_TAG v3.21.2
		)
	set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(protobuf_BUILD_PROTOC_BINARIES OFF CACHE BOOL "" FORCE)
	set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
	set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
	set(ABSL_PROPAGATE_CXX_STD ON)
	FetchContent_MakeAvailable(protobuf)
	target_compile_options(libprotobuf-lite PRIVATE -fPIC)
	set(Protobuf_INCLUDE_DIR "${protobuf_SOURCE_DIR}/src" CACHE INTERNAL "")
	set(Protobuf_LIBRARIES libprotobuf-lite CACHE INTERNAL "")
endif()

# 查找所有源文件，排除 src/main.cpp
file(GLOB SRC_FILES
    "src/*.cpp"
    "src/*.cc"
)
list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/pyspt.cpp")

include(export_names.cmake)
# 添加共享库目标
add_library(wcocr SHARED ${SRC_FILES})
target_include_directories(wcocr SYSTEM PRIVATE ${Protobuf_INCLUDE_DIR})
target_link_libraries(wcocr ${Protobuf_LIBRARIES})
configure_export_symbols("L" wcocr "wechat_ocr" "stop_ocr" "_ZN10CWeChatOCR*" "_ZN9CMojoCall*" "_ZT?10CWeChatOCR")

find_package(Python 3.8 COMPONENTS Interpreter Development REQUIRED)
add_library(pywcocr MODULE ${SRC_FILES} src/pyspt.cpp)
target_include_directories(pywcocr PRIVATE ${Protobuf_INCLUDE_DIR} ${Python_INCLUDE_DIRS})
target_compile_definitions(pywcocr PRIVATE EXPORTED_API=)
configure_export_symbols("LW" pywcocr "PyInit_wcocr")
target_link_libraries(pywcocr PRIVATE ${Protobuf_LIBRARIES} ${Python_LIBRARIES})
target_link_directories(pywcocr PRIVATE ${Python_LIBRARY_DIRS})
if(WIN32)
	set(PYTHON_MODULE_EXTENSION ".pyd")
else()
	set(PYTHON_MODULE_EXTENSION "${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()
set_target_properties(pywcocr PROPERTIES 
    PREFIX ""  # 移除 lib 前缀
    OUTPUT_NAME "wcocr"  # 设置基本名称
    SUFFIX ".${Python_SOABI}${PYTHON_MODULE_EXTENSION}"  # 添加 Python 特定的后缀
)


# 添加可执行目标 test_cli
add_executable(test_cli src/main.cpp)
target_link_libraries(test_cli wcocr)
target_compile_definitions(test_cli PRIVATE TEST_CLI)

# 设置输出名称为 wcocr.so
set_target_properties(wcocr PROPERTIES
    OUTPUT_NAME "wcocr"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# 设置安装规则
install(TARGETS wcocr
    LIBRARY DESTINATION lib
)
