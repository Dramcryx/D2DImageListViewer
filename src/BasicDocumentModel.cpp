#include <BasicDocumentModel.h>

#include <Defines.h>

#include <wincodec.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <shlwapi.h>

#include <cassert>
#include <iostream>

static auto DirectWriteFactory()
{
    static CComPtr<IDWriteFactory> factory = nullptr;
    if (factory == nullptr)
    {
        OK(DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&factory.ptr)
            )
        );
    }
    return factory.ptr;
}

CBasicDocumentModel::CBasicDocumentModel()
{
    TRACE()

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

CBasicDocumentModel::~CBasicDocumentModel()
{
    TRACE()
}

void CBasicDocumentModel::CreateImages(ID2D1RenderTarget* renderTarget)
{
    TRACE()

    for (auto& page : images) {
        page->PrepareBitmapForTarget(renderTarget);
    }
    currentTarget = renderTarget;
}

void* CBasicDocumentModel::GetData(int index, TDocumentModelRoles role) const
{
    TRACE()

    switch (role)
    {
    case TDocumentModelRoles::HeaderFontRole:
        return headerFont.ptr;
    case TDocumentModelRoles::HeaderTextRole:
    {
        const auto& image = images.at(index);
        auto document = image->GetDocument();
        auto documentName = ::PathFindFileNameW(document->GetName());
        int indexInDocument = document->GetIndexOf(image);
        
        wchar_t pageTitleBuffer[4096] = {0};
        wsprintf(pageTitleBuffer, L"%s %d of %d", documentName, indexInDocument + 1, document->GetPagesCount());
#ifdef __MINGW32__
        return wcsdup(pageTitleBuffer);
#else
        return _wcsdup( pageTitleBuffer );
#endif
    }
    case TDocumentModelRoles::ToolbarRole:
        return nullptr;
    case TDocumentModelRoles::PageRole:
        return images.at(index);
    default:
        break;
    }
    return nullptr;
}

void CBasicDocumentModel::AddDocument(IDocument* document)
{
    TRACE()

    documents.emplace_back(document);
    auto& lastAdded = *documents.back();
    lastAdded.Subscribe(this);
    for (int i = 0; i < lastAdded.GetPagesCount(); ++i) {
        this->images.push_back(const_cast<IPage*>(lastAdded.GetPage(i)));
        if (currentTarget != nullptr) {
            this->images.back()->PrepareBitmapForTarget(currentTarget);
        }
    }
    Notify<&IDocumentsModelCallback::OnDocumentAdded>(document);
}

void CBasicDocumentModel::DeleteDocument(int index)
{
    TRACE()

    assert(0 <= index && index < (int)documents.size());
    std::unique_ptr<IDocument> released{documents[index].release()};
    released->Unsubscribe(this);
    documents.erase(documents.begin() + index);
    images.erase(std::remove_if(images.begin(), images.end(), [&released](const auto& imagePtr) {
        return imagePtr->GetDocument() == released.get();
    }), images.end());
    Notify<&IDocumentsModelCallback::OnDocumentDeleted>(released.get());
}

void CBasicDocumentModel::DeleteDocument(const IDocument* document)
{
    TRACE()

    auto findRes = std::find_if(documents.begin(), documents.end(),
        [document](const auto& docPtr) {
            return docPtr.get() == document;
        }
    );
    if (findRes != documents.end()) {
        std::unique_ptr<IDocument> released{findRes->release()};
        released->Unsubscribe(this);
        documents.erase(findRes);
        images.erase(std::remove_if(images.begin(), images.end(), [&released](const auto& imagePtr) {
            return imagePtr->GetDocument() == released.get();
        }), images.end());
        Notify<&IDocumentsModelCallback::OnDocumentDeleted>(released.get());
    }
}
