#ifndef D2DILV_BASIC_DOCUMENT_MODEL_H
#define D2DILV_BASIC_DOCUMENT_MODEL_H

#include <ComPtr.h>
#include <IDocumentModel.h>

#include <vector>
#include <memory>

class IDWriteTextFormat;

/// @brief Generic document model implementation.
/// Supports adding and deleting documents and notifying view about that.
class CBasicDocumentModel : public IDocumentsModel
{
public:
    /// @brief Default constructor
    CBasicDocumentModel();

    /// @brief Desctructor
    ~CBasicDocumentModel() override;

    /// @copydoc IDocumentsModel::CreateImages
    void CreateImages(ID2D1RenderTarget* renderTarget) override;
    
    /// @copydoc IDocumentsModel::GetDocumentsCount
    int GetDocumentsCount() const override { return documents.size(); }
    
    /// @copydoc IDocumentsModel::GetDocument
    IDocument* GetDocument(int index) const override { return documents.at(index).get(); }
    
    /// @copydoc IDocumentsModel::GetTotalPageCount
    int GetTotalPageCount() const override { return images.size(); }

    /// @copydoc IDocumentsModel::GetData
    void* GetData(int index, TDocumentModelRoles role) const override;

    /// @brief Add document to the model. Model takes ownership.
    /// @param document Pointer to the document object
    void AddDocument(IDocument* document);

    /// @brief Delete document item from model
    /// @param index Index of the document in the model
    void DeleteDocument(int index);

    /// @brief Delete document item from model
    /// @param document Weak pointer to the document
    void DeleteDocument(const IDocument* document);

private:
    CComPtr<IDWriteTextFormat> headerFont;
    ID2D1RenderTarget* currentTarget = nullptr;
    std::vector<std::unique_ptr<IDocument>> documents;
    std::vector<IPage*> images;
};

#endif