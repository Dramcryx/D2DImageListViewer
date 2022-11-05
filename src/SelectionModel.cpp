#include "SelectionModel.h"

#include <Defines.h>

#include <unordered_set>

CSelectionModel::CSelectionModel(IDocumentsModel* _model) : model{_model}
{
    if (model != nullptr) {
        model->Subscribe(this);
    }
}

CSelectionModel::~CSelectionModel()
{
    if (model != nullptr) {
        model->Unsubscribe(this);
        model = nullptr;
    }
}

void CSelectionModel::SetModel(IDocumentsModel* _model)
{
    TRACE()

    auto oldModel = this->model;

    if (this->model != nullptr) {
        this->model->Unsubscribe(this);
    }

    this->model = _model;

    if (this->model != nullptr) {
        this->model->Subscribe(this);
    }

    if (oldModel != this->model) {
        this->ClearSelection();
    }
}

std::vector<int> CSelectionModel::GetSelectedPages() const
{
    std::vector<int> retval;
    std::transform(indexToPage.begin(), indexToPage.end(), std::back_inserter(retval), [](const auto& kv) {
        return kv.first;
    });
    return retval;
}

void CSelectionModel::Select(int index, TSelectionMode mode)
{
    TRACE()

    switch (mode)
    {
    case TSelectionMode::SelectOne:
    {
        this->selectOneActive(index);
        break;
    }
    case TSelectionMode::SelectRange:
    {
        if (activeIndex == -1) {
            this->selectOneActive(index);
            break;
        }
        auto [begin, end] = std::minmax({index, activeIndex});
        ++end;
        for (int i = begin; i < end; ++i) {
            this->indexToPage[i] = model->GetData(index, TDocumentModelRoles::PageRole);
        }
        break;
    }
    case TSelectionMode::SelectAppend:
    {
        if (activeIndex == -1) {
            this->selectOneActive(index);
            break;
        }
        this->indexToPage[index] = model->GetData(index, TDocumentModelRoles::PageRole);
    }
    default:
        break;
    }
    Notify<&ISelectionModelCallback::OnSelectionChanged>(GetSelectedPages());
}

void CSelectionModel::Deselect(int index, TSelectionMode mode)
{
    TRACE()

    if (this->indexToPage.empty()) {
        return;
    }
    switch (mode)
    {
    case TSelectionMode::SelectOne:
    {
        this->ClearSelection();
        return;
    }
    case TSelectionMode::SelectRange:
    {
        if (activeIndex == -1) {
            this->ClearSelection();
            return;
        }
        auto [begin, end] = std::minmax({index, activeIndex});
        ++end;
        for (int i = begin; i < end; ++i) {
            indexToPage.erase(i);
        }
        break;
    }
    case TSelectionMode::SelectAppend:
    {
        if (activeIndex == -1) {
            this->ClearSelection();
            return;
        }
        indexToPage.erase(index);
    }
    default:
        break;
    }
    Notify<&ISelectionModelCallback::OnSelectionChanged>(GetSelectedPages());
}

void CSelectionModel::ClearSelection()
{
    TRACE()

    this->indexToPage.clear();
    this->activeIndex = -1;
    Notify<&ISelectionModelCallback::OnSelectionChanged>(GetSelectedPages());
}

void CSelectionModel::OnDocumentDeleted(IDocument* doc)
{
    TRACE()

    if (indexToPage.empty()) {
        return;
    }

    for (auto it = indexToPage.begin(); it != indexToPage.end();)
    {
        if (doc->GetIndexOf(reinterpret_cast<const IPage*>(it->second)) != -1) {
            it = indexToPage.erase(it);
        } else {
            ++it;
        }
    }

    if (indexToPage.empty()) {
        this->activeIndex = -1;
    }

    Notify<&ISelectionModelCallback::OnSelectionChanged>(GetSelectedPages());
}

void CSelectionModel::selectOneActive(int index)
{
    TRACE()

    this->indexToPage.clear();
    this->indexToPage[index] = model->GetData(index, TDocumentModelRoles::PageRole);
    this->activeIndex = index;
}
