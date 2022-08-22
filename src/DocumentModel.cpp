#include <Defines.h>

#include "DocumentModel.h"

#include "DocumentPage.h"

#include <ComPtrOwner.h>

#include <combaseapi.h>
#include <dwrite.h>
#include <wincodec.h>

#include <cassert>

namespace {
IDWriteFactory* DirectWriteFactory()
{
    static CComPtrOwner<IDWriteFactory> factory = nullptr;
    if (factory == nullptr)
    {
        OK(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&factory.ptr)
        ));
    }
    return factory.ptr;
}
}


CDocumentModel::CDocumentModel() : headerFont{nullptr}
{
    auto factory = DirectWriteFactory();
    OK(factory->CreateTextFormat(
        L"DejaVu Serif",
        nullptr,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_ULTRA_CONDENSED,
        28,
        L"en-us",
        &headerFont.ptr
    ));
}

CDocumentModel::~CDocumentModel()
{
    for (auto page : pages)
    {
        delete page;
    }
}

bool CDocumentModel::CreateObjects(ID2D1RenderTarget* renderTarget)
{
    for (auto page : pages)
    {
        delete page;
    }
    pages.clear();

    const wchar_t *files[] = {
        L"pic1.jpg",
        L"pic2.jpg",
        L"pic3.jpg",
        L"pic4.jpg"
    };

    CComPtrOwner<IWICImagingFactory> wicFactory = nullptr;

    assert(CoCreateInstance(CLSID_WICImagingFactory,
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&wicFactory.ptr)) == S_OK);

    for (auto file : files) {
        pages.push_back(new CDocumentPage{file, wicFactory, renderTarget});        
    }
    return true;
}

int CDocumentModel::GetPageCount() const
{
    return pages.size();
}

void* CDocumentModel::GetData(int index, TDocumentModelRoles role) const
{
    switch (role)
    {
    case TDocumentModelRoles::HeaderFontRole:
        return headerFont.ptr;
    case TDocumentModelRoles::HeaderTextRole:
    {
        wchar_t pageTitleBuffer[128] = {0};
        wsprintf(pageTitleBuffer, L"Page %d of %d", index + 1, pages.size());
        return wcsdup(pageTitleBuffer);
    }
    case TDocumentModelRoles::ToolbarRole:
        return nullptr;
    case TDocumentModelRoles::PageRole:
        return pages.at(index);
    default:
        break;
    }
    return nullptr;
}

void CDocumentModel::SetData(int index, TDocumentModelRoles role)
{

}