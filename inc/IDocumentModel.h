#ifndef D2DILV_INTERFACE_DOCUMENT_MODEL_H
#define D2DILV_INTERFACE_DOCUMENT_MODEL_H

#include <GenericNotifier.h>

#ifdef __MINGW32__
#include <windef.h> // SIZE
#else
#include <windows.h>
#endif

/// Fordward declarations ///
// Direct2D
struct ID2D1Bitmap;
struct ID2D1RenderTarget;
// Document interface
struct IDocument;
/////////////////////////////

/// @brief Page data roles
enum class TDocumentModelRoles
{
    HeaderFontRole, // IDWriteTextFormat
    HeaderTextRole, // LPCWSTR
    ToolbarRole, // Toolbar resources
    PageRole // IDocumentPage
};

/// @brief IPage notifications
struct IPageCallback
{
    /// @brief Must be sent if loading happens asynchronously on start
    virtual void OnLoadingStarted() {}

    /// @brief Must be sent if loading happens asynchronously on finish
    virtual void OnLoadingFinished() {}
};

/// @brief Page bitmap state
enum class TPageState {
    LOADING,
    READY,
    FAILED
};

/// @brief Document page interface
struct IPage : CSimpleNotifier<IPageCallback>
{
    /// @brief Default desctructor
    virtual ~IPage() = default;

    /// @brief Get document that this page belongs to
    /// @return Weak pointer to document
    virtual const IDocument* GetDocument() const = 0;

    /// @brief Get bitmap state
    /// @return Bitmap state
    virtual TPageState GetPageState() const = 0;

    /// @brief Get page size.
    /// This method exists because if you load page asynchronously,
    /// the page size might be known before it is actually decode (depending on your page impl).
    /// @return Page size
    virtual SIZE GetPageSize() const = 0;

    /// @brief Create render-target-dependent bitmap
    /// @param renderTarget Render target where the page is going to be drawn
    virtual void PrepareBitmapForTarget(ID2D1RenderTarget* renderTarget) = 0;
    virtual ID2D1Bitmap* GetPageBitmap() const = 0;
};

/// @brief IDocument notifications
struct IDocumentCallback
{
    virtual void OnChanged() {}
};

/// @brief Document interface. Implementations should subscribe to page changes.
struct IDocument : IPageCallback, CSimpleNotifier<IDocumentCallback>
{
    /// @brief Default destructor
    virtual ~IDocument() = default;

    /// @brief Get document name
    /// @return Model-owned string pointer
    virtual const wchar_t* GetName() const = 0;

    /// @brief Get document pages count. Depending on your document type,
    /// the document can have multiple pages (e.g. TIFF)
    /// @return Number of pages
    virtual int GetPagesCount() const = 0;

    /// @brief Get page of the document
    /// @param index 0...PagesCount - 1
    /// @return Model-owned pointer to page
    virtual const IPage* GetPage(int index) const = 0;

    /// @brief Get page number in the document
    /// @param page Weak pointer to page
    /// @return Index of the page on success, -1 on failure
    virtual int GetIndexOf(const IPage* page) const = 0;
};

/// @brief IDocumentsModel notifications
struct IDocumentsModelCallback
{
    virtual void OnDocumentChanged(IDocument*) {}
    virtual void OnDocumentAdded(IDocument*) {}
    virtual void OnDocumentDeleted(IDocument*) {}
};

/// @brief DocumentsModel interface. Implementations should subscribe to document changes.
struct IDocumentsModel : IDocumentCallback, CSimpleNotifier<IDocumentsModelCallback>
{
    virtual ~IDocumentsModel() = default;

    /// @brief Create bitmaps for render target
    /// @param renderTarget New render target where documents will be drawn to
    virtual void CreateImages(ID2D1RenderTarget* renderTarget) = 0;

    /// @brief Get current count of documents in model
    /// @return Count of documents
    virtual int GetDocumentsCount() const = 0;

    /// @brief Get model-owned pointer to the document
    /// @param index 0...DocumentsCount - 1
    /// @return Model-owner pointer to the document
    virtual IDocument* GetDocument(int index) const = 0;

    /// @brief Get total count of pages including all documents
    /// @return Total count of pictures
    virtual int GetTotalPageCount() const = 0;

    /// @brief Get page related model data
    /// @param index 0...TotalPageCount - 1
    /// @param role Data role
    /// @return Model-owned pointer to object
    virtual void* GetData(int index, TDocumentModelRoles role) const = 0;
};

#endif
