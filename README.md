# Direct2D image list viewer

## Brief

A small Direct2D playground project that demonstrates some basics of the API usage (C++17).

The view uses `ID2D1DeviceContext` to render with DXGI swapchain attached to a window based on [this](https://learn.microsoft.com/en-us/archive/msdn-magazine/2013/may/windows-with-c-introducing-direct2d-1-1) article.

There are 4 image layous implemented:
- aligned to left
- aligned to right of the widest image in the list
- aligned to the center of the widest image in the list
- horizontal flow - the images are composed by rows dynamically based on the window size

Basic zooming with `Ctrl+Mousewheel` is implemented. The scrolls are custom since only `ID2D1DCRenderTarget` supports Windows-provided scrolling.

## Model
Initially the view was supposed to draw multi-page documents such as TIFF. For this purpose, there is an `IDocument` interface that represents a document itslef and an `IPage` interface representing individual `IDocument`'s pages bitmaps.

The `IDocumentModel` hides this multi-level mess for the view. It operates with individual `IPage`s using `IDocumentModel::GetData` when calculating a layout and rendering bitmaps. The view also subscribes to `IDocumentModelCallback` notifications that the model should send when some events occur (e.g. document added/deleted).

There are so-called model 'roles', like in Qt models. This approach helps to represent different parts for one index - the text on top of the page, the page itself and the font for the text. Toolbar role on view's side is not implemented yet, its' purpose is to provide some tool to operate on individual pages.

A generic model implementation is represented by `CBasicDocumentModel`. It can store `IDocument`s correctly, caching their `IPage`s to quickly reply on `IDocumentModel::GetData`, and it also notifies the view on the changes.

`CDocumentFromDisk` and `CWICImage` are also two very basic implementations of the `IDocument` and the `IPage` respectively for a document that can be loaded from path and is supported by Windows Imaging Component (WIC). Based on them, it is possible to implement any other document type such as PDF, for example (if you have some renderer).

## Example
The example application that uses the view as it's child in the main window is implemented in `example` directory. It includes the view creation and populating it with images from `bin` and radio buttons to switch between layouts.

## Build
The project implementation began on Linux using MinGW to compile and Wine to run, so the `CMakeLists.txt` is for building on Linux (ccurrently) but later I introduced an MSVC `.sln` as well. The build system is not mature yet, I will work on that.

### Linux
#### Prerequesties
 - MinGW
 - Wine

```
mkdir build; cd build; cmake ../; cmake --build . -- j$(nproc)
```
The output is the example binary called `DocumentViewer.exe` and the static library `libimageviewer.a`. You can run like `./DocumentViewer.exe`.

### Windows
#### Prerequesties
- Visual Studio with Windows SDK installed

Just open the solution in Visual Studio and build it. Use the `example` project to run.