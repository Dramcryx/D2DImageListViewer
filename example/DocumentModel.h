#ifndef DOCUMENT_MODEL_H
#define DOCUMENT_MODEL_H

#include <ComPtrOwner.h>
#include <IDocumentModel.h>
#include <IDocumentPage.h>

#include <vector>

class IDWriteTextFormat;

class CDocumentModel : public IDocumentModel
{
public:
    CDocumentModel();
    ~CDocumentModel() override;

    // IDocumentModel
    bool CreateObjects(ID2D1RenderTarget* renderTarget) override;

    int GetPageCount() const override;
    void* GetData(int index, TDocumentModelRoles role) const override;
    void SetData(int index, TDocumentModelRoles role) override;

    void OnDocumentChanged() override {}
    void OnPagesChanged(const std::vector<int>& page) override {}

    void AddImageFromFile(const wchar_t* filename);

private:
    std::vector<IDocumentPage*> pages;
    CComPtrOwner<IDWriteTextFormat> headerFont;
    ID2D1RenderTarget* currentTarget;
};


#endif
