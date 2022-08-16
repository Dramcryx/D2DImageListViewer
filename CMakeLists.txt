project (DocumentViewer)

set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable (DocumentViewer src/main.cpp src/DocumentView.cpp src/DocumentModel.cpp src/DocumentPage.cpp)

target_include_directories (DocumentViewer PUBLIC inc)
target_compile_options (DocumentViewer PUBLIC -mwindows -municode)
target_link_libraries (DocumentViewer PUBLIC gdiplus comctl32 d2d1 ole32 windowscodecs)
target_link_options (DocumentViewer PUBLIC -mwindows -municode -Wl,--subsystem,windows)
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)