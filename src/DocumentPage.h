#ifndef DOCUMENT_PAGE_H
#define DOCUMENT_PAGE_H

#include <d2d1.h>

class CDocumentPage
{
public:
    enum class TPageState {
        LOADED,
        LOADING,
        FAILED
    };

    CDocumentPage();
    ~CDocumentPage();

    TPageState GetPageState() const;
    RECT GetPageRect() const;

    ID2D1Bitmap& GetPageBitmap() const;
};

#endif
