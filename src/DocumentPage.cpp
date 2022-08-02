#include "DocumentPage.h"

#include <cassert>

#include <combaseapi.h>
#include <d2d1.h>
#include <wincodec.h>

CDocumentPage::CDocumentPage(const wchar_t* fileName,
    IWICImagingFactory* wicFactory, ID2D1RenderTarget* target)
{
    CComPtrOwner<IWICBitmapDecoder> imageDecoder = nullptr;
    assert(wicFactory->CreateDecoderFromFilename(
                fileName,
                NULL,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &imageDecoder.ptr) == S_OK);

    CComPtrOwner<IWICBitmapFrameDecode> imageSource = nullptr;
    assert(imageDecoder->GetFrame(0, &imageSource.ptr) == S_OK);

    CComPtrOwner<IWICFormatConverter> converter = NULL;
    assert(wicFactory->CreateFormatConverter(&converter.ptr) == S_OK);
    assert(converter->Initialize(
                imageSource.ptr,
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,
                NULL,
                0.f,
                WICBitmapPaletteTypeMedianCut) == S_OK);

    assert(target->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap.ptr) == S_OK);
}

IDocumentPage::TPageState CDocumentPage::GetPageState() const
{
    if (bitmap.ptr == nullptr)
    {
        return TPageState::LOADING;
    }
    return TPageState::LOADED;
}

SIZE CDocumentPage::GetPageSize() const
{
    if (bitmap.ptr == nullptr)
    {
        return SIZE{0, 0};
    }
    auto size = const_cast<CDocumentPage*>(this)->bitmap->GetSize();
    return SIZE{(int)size.width, (int)size.height};
}

ID2D1Bitmap* CDocumentPage::GetPageBitmap() const
{
    return bitmap.ptr;
}