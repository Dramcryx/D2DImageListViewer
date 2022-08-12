#include "DocumentModel.h"

#include <cassert>

#include <combaseapi.h>
#include <wincodec.h>

#include "ComPtrOwner.h"
#include "DocumentPage.h"

CDocumentModel::CDocumentModel()
{
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
    case TDocumentModelRoles::ToolbarRole:
        return nullptr;
    default:
        return pages.at(index);
        break;
    }
    return nullptr;
}

void CDocumentModel::SetData(int index, TDocumentModelRoles role)
{

}