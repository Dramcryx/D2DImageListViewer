#ifndef INTERFACE_DOCUMENT_PAGE_H
#define INTERFACE_DOCUMENT_PAGE_H

#include <windows.h>

struct IDocumentModel;

struct ID2D1Bitmap;

struct IDocumentPage
{
    enum class TPageState {
        LOADED,
        LOADING,
        FAILED
    };

    virtual ~IDocumentPage() = 0;

    virtual TPageState GetPageState() const = 0;
    virtual RECT GetPageRect() const = 0;

    virtual ID2D1Bitmap& GetPageBitmap() const = 0;
};

#endif
