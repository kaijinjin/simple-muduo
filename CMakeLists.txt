# cmake最低版本要求
cmake_minimum_required(VERSION 3.28.3)
# 项目名称
project(mymuduo)

# LIBRARY_OUTPUT_DIRECTORY动态库的路径：设置为CMakeLists目录/lib
set(LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
# 设置调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# 定义参与编译的源代码：根目录下的所有源文件到变量SRC_LIST中
aux_source_directory(. SRC_LIST)
# 编译生成的动态库：mymuduo
add_library(mymuduo SHARED ${SRC_LIST})
