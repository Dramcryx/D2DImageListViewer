#ifndef DOCUMENT_PAGE_H
#define DOCUMENT_PAGE_H

#include <ComPtrOwner.h>
#include <IDocumentPage.h>

#include <d2d1.h>

class CDocumentPage : public IDocumentPage
{
public:
    CDocumentPage(const wchar_t* fileName, IWICImagingFactory* wicFactory, ID2D1RenderTarget* target);
    ~CDocumentPage() override = default;

    TPageState GetPageState() const override;
    SIZE GetPageSize() const override;

    ID2D1Bitmap* GetPageBitmap() const override;

private:
    CComPtrOwner<ID2D1Bitmap> bitmap = nullptr;
};

#endif
