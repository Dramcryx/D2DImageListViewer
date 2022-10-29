#include <DocumentFromDisk.h>

#include <Defines.h>
#include <ComPtr.h>

#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>

#include <iostream>

class CWICImage : public IPage
{
public:
    CWICImage(IDocument* parent, IWICBitmapFrameDecode* frame);
    ~CWICImage() override;

    const IDocument* GetDocument() const override { return parent; }
    TPageState GetPageState() const override;
    SIZE GetPageSize() const override;
    ID2D1Bitmap* GetPageBitmap() const override;
    void PrepareBitmapForTarget(ID2D1RenderTarget* renderTarget) override;

private:
    IDocument* parent;
    bool isFailedToLoad = false;
    CComPtr<IWICBitmapSource> imageSource;
    CComPtr<ID2D1Bitmap> bitmap;
};

const IPage* CDocumentFromDisk::GetPage(int index) const
{
    return images.at(index).get();
}

static auto GetWICFactory()
{
    static CComPtr<IWICImagingFactory> wicFactory = nullptr;
    if (wicFactory == nullptr) {
        OK(CoCreateInstance(
                CLSID_WICImagingFactory,
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&wicFactory.ptr)
            )
        );
    }

    return wicFactory;
}

CWICImage::CWICImage(IDocument* _parent, IWICBitmapFrameDecode* frame) :
    parent{_parent}
{
    TRACE()

    NOTNULL(parent);
    NOTNULL(frame);

    IWICFormatConverter* converter = NULL;
    OK(GetWICFactory()->CreateFormatConverter(&converter));
    OK(converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            NULL,
            0.f,
            WICBitmapPaletteTypeMedianCut
        )
    );
    imageSource = converter;
}

CWICImage::~CWICImage()
{
    TRACE()
}

TPageState CWICImage::GetPageState() const
{
    TRACE()

    if (isFailedToLoad) {
        return TPageState::FAILED;
    }

    if (bitmap.ptr != nullptr) {
        return TPageState::READY;
    }
    return TPageState::LOADING;
}

SIZE CWICImage::GetPageSize() const
{
    TRACE()

    if (bitmap.ptr != nullptr) {
        auto size = const_cast<ID2D1Bitmap*>(bitmap.ptr)->GetSize(); 
        return {size.width, size.height};
    }
    return {200, 200};
}

ID2D1Bitmap* CWICImage::GetPageBitmap() const
{
    return bitmap.ptr;
}

void CWICImage::PrepareBitmapForTarget(ID2D1RenderTarget* target)
{
    TRACE()
    if (imageSource == nullptr) {
        return;
    }
    NOTNULL(target);
    if (bitmap != nullptr) {
        bitmap.Reset();
    }
    OK(target->CreateBitmapFromWicBitmap(imageSource, nullptr, &bitmap.ptr));
}

CDocumentFromDisk::CDocumentFromDisk(const wchar_t* _fileName) :
    fileName{_fileName}
{
    TRACE()
    CComPtr<IWICBitmapDecoder> imageDecoder = nullptr;
    if(GetWICFactory()->CreateDecoderFromFilename(
                fileName.c_str(),
                NULL,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &imageDecoder.ptr) != S_OK)
    {
        std::wcerr << fileName.c_str() << " - format not supported\n";
        return;
    }

    UINT framesCount = 0;
    OK(imageDecoder->GetFrameCount(&framesCount));
    for (UINT i = 0; i < framesCount; ++i) {
        CComPtr<IWICBitmapFrameDecode> imageSource = nullptr;
        assert(imageDecoder->GetFrame(0, &imageSource.ptr) == S_OK);

        auto page = std::make_unique<CWICImage>(this, imageSource.ptr);
        page->Subscribe(this);
        images.push_back(std::move(page));
    }
}

CDocumentFromDisk::~CDocumentFromDisk()
{
    TRACE()
}

int CDocumentFromDisk::GetIndexOf(const IPage* page) const
{
    TRACE()

    auto findRes = std::find_if(images.begin(), images.end(), [page](const auto& pagePtr) {
        return pagePtr.get() == page;
    });
    if (findRes == images.end()) {
        return -1;
    }
    return findRes - images.begin();
}
