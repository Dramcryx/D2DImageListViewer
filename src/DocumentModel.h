#ifndef DOCUMENT_MODEL_H
#define DOCUMENT_MODEL_H

#include <vector>

#include <IDocumentModel.h>
#include <IDocumentPage.h>

class CDocumentModel : public IDocumentModel
{
public:
    CDocumentModel();
    ~CDocumentModel() override;

    bool CreateObjects(ID2D1RenderTarget* renderTarget) override;

    int GetPageCount() const override;
    void* GetData(int index, TDocumentModelRoles role) const override;
    void SetData(int index, TDocumentModelRoles role) override;

    virtual void OnDocumentChanged() {}
    virtual void OnPagesChanged(const std::vector<int>& page) {}

private:
    std::vector<IDocumentPage*> pages;
};


#endif
