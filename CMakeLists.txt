project (DocumentViewer)

set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(imageviewer src/DocumentView.cpp src/DocumentViewPrivate.cpp)

target_include_directories (imageviewer PUBLIC inc src)
target_compile_options (imageviewer PUBLIC -mwindows -municode -g -O2)
target_link_libraries (imageviewer PUBLIC -static gcc stdc++ winpthread -dynamic gdiplus comctl32 d2d1 ole32 windowscodecs dwrite)


add_executable (DocumentViewer example/main.cpp example/DocumentModel.cpp example/DocumentPage.cpp example/MainWindow.cpp)

target_include_directories (DocumentViewer PUBLIC inc example)
target_compile_options (DocumentViewer PUBLIC -mwindows -municode -g -O2)
target_link_libraries (DocumentViewer PUBLIC -static gcc stdc++ imageviewer winpthread)
target_link_options (DocumentViewer PUBLIC -mwindows -municode -Wl,--subsystem,windows)
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)