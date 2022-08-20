#ifndef DOCUMENT_VIEW_PRIVATE_H
#define DOCUMENT_VIEW_PRIVATE_H

#include <Defines.h>

#include <d2d1.h>
#include <d2d1helper.h>

#include <vector>
#include <optional>
#include <tuple>

struct IDocumentModel;
struct IDocumentPage;

bool operator==(const D2D_SIZE_F& lhs, const D2D_SIZE_F& rhs);
bool operator!=(const D2D_SIZE_F& lhs, const D2D_SIZE_F& rhs);

bool operator==(const D2D1_RECT_F& lhs, const D2D1_RECT_F& rhs);
bool operator!=(const D2D1_RECT_F& lhs, const D2D1_RECT_F& rhs);

namespace DocumentViewPrivate {
/// @brief Layout descriptor
struct CDocumentPagesLayoutParams {
    D2D1_SIZE_F drawSurfaceSize{0, 0};
    int pageMargin = 0;
    int pagesSpacing = 0;
    
    enum TStrategy {
        AlignLeft,
        AlignRight,
        AlignHCenter,
        HorizontalFlow
    } strategy = AlignLeft;

    bool operator==(const CDocumentPagesLayoutParams& rhs) const
    {
        return std::tie(drawSurfaceSize, pageMargin, pagesSpacing, strategy)
                    == std::tie(rhs.drawSurfaceSize, rhs.pageMargin, rhs.pagesSpacing, rhs.strategy);
    }
};

/// @brief Resulting layout
struct CDocumentPagesLayout {
    D2D1_SIZE_F totalSurfaceSize = {0.0f, 0.0f};
    std::vector<std::pair<const IDocumentPage*, D2D1_RECT_F>> pageRects;
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

    const CDocumentPagesLayout& GetOrCreateLayout(const CDocumentPagesLayoutParams& params, const IDocumentModel* model);
    const CScrollBarRects& GetOrCreateRelativeScrollBarRects(
        const D2D1_SIZE_F& zoomedTotalSurfaceSize, const D2D1_SIZE_F& visibleSurfaceSize, float vScroll, float hScroll);

private:
    std::pair<CDocumentPagesLayoutParams, const IDocumentModel*> lastLayoutRequest;
    std::optional<CDocumentPagesLayout> cachedLayout;

    struct CScrollBarsRequest {
        D2D1_SIZE_F zoomedTotalSurfaceSize;
        D2D1_SIZE_F visibleSurfaceSize;
        float hScroll;
        float vScroll;
    };

    CScrollBarsRequest lastRelativeScrollBarsRequest;
    std::optional<CScrollBarRects> cachedRelativeScrollRects;
};

}

#endif
