#ifndef DOCUMENT_MODEL_H
#define DOCUMENT_MODEL_H

#include <vector>

#include <IDocumentModel.h>
#include <IDocumentPage.h>

class CDocumentModel : public IDocumentModel
{
public:
    CDocumentModel() = default;
    ~CDocumentModel() override = default;

    int GetPageCount() const override;
    void* GetData(int index, TDocumentModelRoles role) const override;
    void SetData(int index, TDocumentModelRoles role) override;

private:
    std::vector<IDocumentPage*> pages;
};


#endif
