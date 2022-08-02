#ifndef INTERFACE_DOCUMENT_PAGE_H
#define INTERFACE_DOCUMENT_PAGE_H

#include <windef.h>

struct IDocumentModel;

struct IWICImagingFactory;
struct ID2D1Bitmap;

struct IDocumentPage
{
    enum class TPageState {
        LOADED,
        LOADING,
        FAILED
    };

    virtual ~IDocumentPage() = default;

    virtual TPageState GetPageState() const = 0;
    virtual SIZE GetPageSize() const = 0;

    virtual ID2D1Bitmap* GetPageBitmap() const = 0;
};

#endif
