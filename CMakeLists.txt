cmake_minimum_required(VERSION 3.10)
project(sakura)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finput-charset=UTF-8 -fexec-charset=UTF-8")

find_package(OpenSSL REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio)
find_package(cpr REQUIRED)

add_library(Sakura STATIC
  sakura.cpp
)

target_include_directories(Sakura PUBLIC
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(Sakura PRIVATE
  cpr::cpr
  ${OpenCV_LIBS}
)

add_executable(sakura
  main.cpp sakura.cpp
)

target_include_directories(sakura PRIVATE
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(sakura PRIVATE
  Sakura
  opencv_core
  opencv_imgcodecs
  opencv_imgproc
)
