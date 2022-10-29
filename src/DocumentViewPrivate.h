#ifndef DOCUMENT_VIEW_PRIVATE_H
#define DOCUMENT_VIEW_PRIVATE_H

#include <Defines.h>
#include <ComPtr.h>
#include <DocumentViewParams.h>

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>

#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

struct IDocumentsModel;
struct IPage;

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
        CComPtr<IDWriteTextLayout> textLayout = nullptr;
        D2D1_RECT_F textRect;

        const IPage* page = nullptr;
        D2D1_RECT_F pageRect;
    };
    std::vector<CPageLayout> pageRects;

private:
    /// Offsets or other values that allow to modify existing layout
    friend class CDocumentLayoutHelper;
    float alignmentContextValue1 = 0.f;
    float alignmentContextValue2 = 0.f;
    float alignmentContextValue3 = 0.f;
    float alignmentContextValue4 = 0.f;
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

    const CDocumentPagesLayout& GetLayout() const;
    const CScrollBarRects& GetRelativeScrollBarRects() const;

    void AddPage(const IPage* page, IDWriteTextFormat* format, std::wstring headerText);
    void DeletePage(const std::variant<const IPage*, int>& page);
    void ClearPages();
    void RefreshLayout();

private:
    D2D1_SIZE_F renderTargetSize{0, 0};
    int pageMargin = 0;
    int pagesSpacing = 0;
    TImagesViewAlignment strategy = TImagesViewAlignment::AlignLeft;
    float vScroll = 0.0f;
    float hScroll = 0.0f;
    float zoom = 1.0f;

    CDocumentPagesLayout layout;
    CScrollBarRects relativeScrollRects;

    CDocumentPagesLayout::CPageLayout createAbsolutePageLayout(const IPage* page, IDWriteTextFormat* format, std::wstring text) const;
    void adjustLayoutForCurrentAlignment(CDocumentPagesLayout::CPageLayout& absoluteLayout);
    void adjustPage(CDocumentPagesLayout::CPageLayout& absoluteLayout, float topOffset, float leftOffset) const;
    void calcScrollBars();
};

}

#endif
