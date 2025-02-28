# 这个模块提供了控制共享库导出符号的功能
# 对Linux生成版本脚本，对Windows生成DEF文件

# 函数：配置导出符号
# 参数：
#   TARGET_NAME - 目标库名称
#   HOSTS - 目标平台 LWA for Linux, Windows, Apple
#   SYMBOLS - 要导出的符号列表
function(configure_export_symbols HOSTS TARGET_NAME)
  # 解析参数
  set(SYMBOLS ${ARGN})
  
  if(NOT SYMBOLS)
    message(WARNING "没有指定导出符号")
    return()
  endif()
  
  # 为不同平台创建不同的导出文件
  if (UNIX AND NOT APPLE)
    string(FIND "${HOSTS}" "L" FOUND_INDEX)
        if (${FOUND_INDEX} GREATER -1)
        # Linux平台: 生成版本脚本
        set(EXPORT_FILE "${CMAKE_BINARY_DIR}/${TARGET_NAME}_symbols.def")
        file(WRITE ${EXPORT_FILE} "{\n")
        file(APPEND ${EXPORT_FILE} "  global:\n")
        
        foreach(SYMBOL ${SYMBOLS})
        file(APPEND ${EXPORT_FILE} "    ${SYMBOL};\n")
        endforeach()
        
        file(APPEND ${EXPORT_FILE} "\n")
        file(APPEND ${EXPORT_FILE} "  local: *;\n")
        file(APPEND ${EXPORT_FILE} "};\n")
        
        # 添加链接器标志
        target_link_options(${TARGET_NAME} PRIVATE "-Wl,--version-script=${EXPORT_FILE}")
        message(STATUS "为 ${TARGET_NAME} 配置了符号导出 (${SYMBOLS})")
    endif()
  elseif(WIN32)
    string(FIND "${HOSTS}" "W" FOUND_INDEX)
    if (${FOUND_INDEX} GREATER -1)
        # Windows平台: 生成DEF文件
        set(EXPORT_FILE "${CMAKE_BINARY_DIR}/${TARGET_NAME}_exports.def")
        file(WRITE ${EXPORT_FILE} "EXPORTS\n")
        
        foreach(SYMBOL ${SYMBOLS})
        file(APPEND ${EXPORT_FILE} "    ${SYMBOL}\n")
        endforeach()
        
        # 添加DEF文件到链接器
        set_property(TARGET ${TARGET_NAME} APPEND_STRING PROPERTY LINK_FLAGS " /DEF:\"${EXPORT_FILE}\"")
        message(STATUS "为 ${TARGET_NAME} 配置了符号导出 (${SYMBOLS})")
    endif()
  elseif(APPLE)
    string(FIND "${HOSTS}" "A" FOUND_INDEX)
    if (${FOUND_INDEX} GREATER -1)
        # macOS平台处理（可选）
        message(STATUS "TODO: macOS平台暂时不支持")
    endif()
  endif()
  
  # 设置属性记录已应用导出配置
  # set_target_properties(${TARGET_NAME} PROPERTIES HAS_SYMBOL_EXPORTS TRUE)
endfunction()
