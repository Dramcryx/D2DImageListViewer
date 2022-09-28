#ifndef DOCUMENT_VIEW_PRIVATE_H
#define DOCUMENT_VIEW_PRIVATE_H

#include <Defines.h>
#include <ComPtrOwner.h>
#include <DocumentViewParams.h>

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>

#include <optional>
#include <string>
#include <tuple>
#include <vector>

struct IDocumentModel;
struct IDocumentPage;

bool operator==(const D2D_SIZE_F& lhs, const D2D_SIZE_F& rhs);
bool operator!=(const D2D_SIZE_F& lhs, const D2D_SIZE_F& rhs);

bool operator==(const D2D1_RECT_F& lhs, const D2D1_RECT_F& rhs);
bool operator!=(const D2D1_RECT_F& lhs, const D2D1_RECT_F& rhs);

namespace DocumentViewPrivate {
/// @brief Resulting layout
struct CDocumentPagesLayout {
    D2D1_SIZE_F totalSurfaceSize = {0.0f, 0.0f};
    D2D1_SIZE_F viewportOffset = {0.0f, 0.0f};
    struct CPageLayout {
        CComPtrOwner<IDWriteTextLayout> textLayout = nullptr;
        D2D1_RECT_F textRect;

        const IDocumentPage* page = nullptr;
        D2D1_RECT_F pageRect;
    };
    std::vector<CPageLayout> pageRects;
};

/// @brief Where to draw scrolls
struct CScrollBarRects {
    std::optional<D2D1_ROUNDED_RECT> hScrollBar;
    std::optional<D2D1_ROUNDED_RECT> vScrollBar;
};

class CDocumentLayoutHelper {
public:
    CDocumentLayoutHelper() = default;
    ~CDocumentLayoutHelper() = default;

    D2D1_SIZE_F GetRenderTargetSize() const { return this->renderTargetSize; }
    void SetRenderTargetSize(const D2D1_SIZE_F& renderTargetSize);

    int GetPageMargin() const { return this->pageMargin; }
    void SetPageMargin(int margin);

    int GetPageSpacing() const { return this->pagesSpacing; }
    void SetPageSpacing(int spacing);

    TImagesViewAlignment GetAlignment() const { return this->strategy; }
    void SetAlignment(TImagesViewAlignment alignment);

    float GetVScroll() const { return this->vScroll; }
    void SetVScroll(float vScroll);
    void AddVScroll(float delta);

    float GetHScroll() const { return this->hScroll; }
    void SetHScroll(float hScroll);
    void AddHScroll(float delta);

    float GetZoom() const { return this->zoom; }
    void SetZoom(float zoom);
    void AddZoom(float delta);

    const IDocumentModel* GetModel() const{ return this->model; }
    void SetModel(const IDocumentModel* model);

    const CDocumentPagesLayout& GetLayout() const;
    const CScrollBarRects& GetRelativeScrollBarRects();

private:
    D2D1_SIZE_F renderTargetSize{0, 0};
    int pageMargin = 0;
    int pagesSpacing = 0;
    TImagesViewAlignment strategy = TImagesViewAlignment::AlignLeft;
    float vScroll = 0.0f;
    float hScroll = 0.0f;
    float zoom = 1.0f;
    const IDocumentModel* model = nullptr;

    mutable std::optional<CDocumentPagesLayout> cachedLayout;
    mutable std::optional<CScrollBarRects> cachedRelativeScrollRects;

    void resetCaches();
};

}

#endif
