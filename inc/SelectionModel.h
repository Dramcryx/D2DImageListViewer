#ifndef D2ILV_SELECTION_MODEL_H
#define D2ILV_SELECTION_MODEL_H

#include <GenericNotifier.h>
#include <IDocumentModel.h>

#include <unordered_map>

enum class TSelectionMode {
    SelectOne,
    SelectRange,
    SelectAppend
};

struct ISelectionModelCallback {
    virtual void OnSelectionChanged(const std::vector<int>& /*newSelection*/) {}
};

class CSelectionModel : public CSimpleNotifier<ISelectionModelCallback>, private IDocumentsModelCallback {
public:
    CSelectionModel(IDocumentsModel* model = nullptr);
    ~CSelectionModel();

    inline int GetCurrentIndex() const { return activeIndex; }
    inline void SetCurrentIndex(int index) { activeIndex = index; }

    inline bool HasSelection() const { return !indexToPage.empty(); }
    inline bool IsSelected(int index) const
    {
        return indexToPage.find(index) != indexToPage.end();
    }
    
    inline const IDocumentsModel* GetModel() const { return model; }

    void SetModel(IDocumentsModel* model);
    
    std::vector<int> GetSelectedPages() const;

    void Select(int index, TSelectionMode mode);
    
    void Deselect(int index, TSelectionMode mode);
    
    void ClearSelection();

protected:
    void OnDocumentDeleted(IDocument* doc) override;

private:
    IDocumentsModel* model = nullptr;
    int activeIndex = -1;
    std::unordered_map<int, void*> indexToPage;

    void selectOneActive(int index);
};

#endif
