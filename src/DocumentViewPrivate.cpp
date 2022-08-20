#include "DocumentViewPrivate.h"

#include <IDocumentModel.h>
#include <IDocumentPage.h>

#include <algorithm>
#include <iostream>

bool operator==(const D2D_SIZE_F& lhs, const D2D_SIZE_F& rhs)
{
    return std::tie(lhs.height, lhs.width) == std::tie(rhs.height, rhs.width);
}

bool operator!=(const D2D_SIZE_F& lhs, const D2D_SIZE_F& rhs)
{
    return !(lhs == rhs);
}

bool operator==(const D2D1_RECT_F& lhs, const D2D1_RECT_F& rhs)
{
    return std::tie(lhs.left, lhs.top, lhs.right, lhs.bottom)
                == std::tie(rhs.left, rhs.top, rhs.right, rhs.bottom);
}

bool operator!=(const D2D1_RECT_F& lhs, const D2D1_RECT_F& rhs)
{
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& out, const D2D1_RECT_F& rect);

namespace DocumentViewPrivate {

const CDocumentPagesLayout& CDocumentLayoutHelper::GetOrCreateLayout(const CDocumentPagesLayoutParams& params, const IDocumentModel* model)
{
    // If fully matched, return cached layout
    if (cachedLayout.has_value() && params == lastLayoutRequest.first && model == lastLayoutRequest.second) {
        return *cachedLayout;
    }

    // otherwise, if size changed, only flow layout requires recalculations, others are not
    if (cachedLayout.has_value()
            && params.drawSurfaceSize != lastLayoutRequest.first.drawSurfaceSize
            && model == lastLayoutRequest.second
            && params.strategy != CDocumentPagesLayoutParams::HorizontalFlow
            && lastLayoutRequest.first.strategy != CDocumentPagesLayoutParams::HorizontalFlow)
    {
        return *cachedLayout;
    }

    CDocumentPagesLayout retval;

    if (model->GetPageCount() == 0) {
        return *(cachedLayout = retval);
    }

    const float pageMargin = float(params.pageMargin);

    switch (params.strategy)
    {
    case CDocumentPagesLayoutParams::AlignLeft:
    {
        float topOffset = 0.0;
        float maxWidth = 0.0;
        for (int i = 0; i < model->GetPageCount(); ++i) {
            auto page = reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole));
            auto pageSize = page->GetPageSize();

            retval.pageRects.emplace_back(
                page,
                D2D1_RECT_F{
                    pageMargin,
                    topOffset + pageMargin,
                    (float)pageSize.cx + pageMargin,
                    topOffset + (float)pageSize.cy + pageMargin
                }
            );

            maxWidth = std::max(maxWidth, (float)pageSize.cx + pageMargin * 2);
            topOffset += pageSize.cy + pageMargin * 2;
            topOffset += params.pagesSpacing;
        }
        retval.totalSurfaceSize = {maxWidth, topOffset - params.pagesSpacing};
        break;
    }
    case CDocumentPagesLayoutParams::AlignRight:
    {
        std::vector<const IDocumentPage*> pages;
        pages.reserve(model->GetPageCount());
        
        for (int i = 0; i < model->GetPageCount(); ++i) {
            pages.push_back(reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole)));
        }

        float maxPageWidth = (*std::max_element(pages.begin(), pages.end(), [](const auto& lhs, const auto& rhs) {
            return lhs->GetPageSize().cx < rhs->GetPageSize().cx;
        }))->GetPageSize().cx;

        float topOffset = 0.0;

        for (const auto& page : pages) {
            auto pageSize = page->GetPageSize();

            retval.pageRects.emplace_back(
                page,
                D2D1_RECT_F{
                    maxPageWidth + pageMargin - pageSize.cx,
                    topOffset + pageMargin,
                    maxPageWidth + pageMargin,
                    topOffset + (float)pageSize.cy + pageMargin
                }
            );

            topOffset += pageSize.cy + pageMargin * 2;
            topOffset += params.pagesSpacing;
        }
        retval.totalSurfaceSize = {maxPageWidth + pageMargin * 2, topOffset - params.pagesSpacing};
        break;
    }
    case CDocumentPagesLayoutParams::AlignHCenter:
    {
        std::vector<const IDocumentPage*> pages;
        pages.reserve(model->GetPageCount());
        
        for (int i = 0; i < model->GetPageCount(); ++i) {
            pages.push_back(reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole)));
        }

        float maxPageWidth = (*std::max_element(pages.begin(), pages.end(), [](const auto& lhs, const auto& rhs) {
            return lhs->GetPageSize().cx < rhs->GetPageSize().cx;
        }))->GetPageSize().cx;
        float topOffset = 0.0;

        for (const auto& page : pages) {
            auto pageSize = page->GetPageSize();

            retval.pageRects.emplace_back(
                page,
                D2D1_RECT_F{
                    maxPageWidth / 2 + pageMargin - pageSize.cx / 2,
                    topOffset + pageMargin,
                    maxPageWidth / 2 + pageMargin + pageSize.cx / 2,
                    topOffset + (float)pageSize.cy + pageMargin
                }
            );

            topOffset += pageSize.cy + pageMargin * 2;
            topOffset += params.pagesSpacing;
        }
        retval.totalSurfaceSize = {maxPageWidth + pageMargin * 2, topOffset - params.pagesSpacing};
        break;
    }
    case CDocumentPagesLayoutParams::HorizontalFlow:
    {
        float totalLeftOffset = 0.0;

        float topOffset = 0.0;
        float leftOffset = 0.0;
        float maxHeight = 0.0;
        for (int i = 0; i < model->GetPageCount(); ++i) {
            auto page = reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole));
            auto pageSize = page->GetPageSize();

            if (leftOffset != 0.0 && (pageSize.cx + leftOffset + pageMargin * 2 > params.drawSurfaceSize.width)) {
                totalLeftOffset = std::max(totalLeftOffset, leftOffset);
                leftOffset = 0.0;

                topOffset += maxHeight + pageMargin * 2;
                topOffset += params.pagesSpacing;
                maxHeight = 0.0;
            }

            retval.pageRects.emplace_back(
                page,
                D2D1_RECT_F{
                    leftOffset + pageMargin,
                    topOffset + pageMargin,
                    leftOffset + (float)pageSize.cx + pageMargin,
                    topOffset + (float)pageSize.cy + pageMargin
                }
            );

            maxHeight = std::max(maxHeight, (float)pageSize.cy + pageMargin * 2);
            leftOffset += pageSize.cx + pageMargin * 2;
            leftOffset += params.pagesSpacing;
        }
        totalLeftOffset = std::max(totalLeftOffset, leftOffset);
        retval.totalSurfaceSize = {totalLeftOffset - params.pagesSpacing, topOffset + maxHeight - params.pagesSpacing};
        break;
    }
    default:
        break;
    }
    cachedLayout.emplace(std::move(retval));

    lastLayoutRequest.first = params;
    lastLayoutRequest.second = model;

    return *cachedLayout;
}

const CScrollBarRects& CDocumentLayoutHelper::GetOrCreateRelativeScrollBarRects(
        const D2D1_SIZE_F& zoomedTotalSurfaceSize, const D2D1_SIZE_F& visibleSurfaceSize, float vScroll, float hScroll)
{
    // if surface size not changed, we can just change the position
    if (cachedRelativeScrollRects.has_value()
            && zoomedTotalSurfaceSize == lastRelativeScrollBarsRequest.zoomedTotalSurfaceSize
            && visibleSurfaceSize == lastRelativeScrollBarsRequest.visibleSurfaceSize)
    {
        // but if scroll pos is not changed, return cache
        if (vScroll == lastRelativeScrollBarsRequest.vScroll && hScroll == lastRelativeScrollBarsRequest.hScroll) {
            return *cachedRelativeScrollRects;
        }
        // finally, move scroll rects and update request cache
        {
            const float newHScrollBarLeft = -visibleSurfaceSize.width * hScroll;
            auto& hScrollBarRect = cachedRelativeScrollRects->hScrollBar->rect;
            const float hScrollBarWidth = hScrollBarRect.right - hScrollBarRect.left;
            hScrollBarRect.left = newHScrollBarLeft;
            hScrollBarRect.right = newHScrollBarLeft + hScrollBarWidth;
            lastRelativeScrollBarsRequest.hScroll = hScroll;
        }

        {
            const float newVScrollBarTop = -visibleSurfaceSize.height * vScroll;
            auto& vScrollBarRect = cachedRelativeScrollRects->vScrollBar->rect;
            const float vScrollBarHeight = vScrollBarRect.bottom - vScrollBarRect.top;
            vScrollBarRect.top = newVScrollBarTop;
            vScrollBarRect.bottom = newVScrollBarTop + vScrollBarHeight;
            lastRelativeScrollBarsRequest.vScroll = vScroll;
        }
        return *cachedRelativeScrollRects;
    }


    cachedRelativeScrollRects.reset();
    CScrollBarRects newRects;

    const float hVisibleToTotal = visibleSurfaceSize.width / zoomedTotalSurfaceSize.width;
    const float vVisibleToTotal = visibleSurfaceSize.height / zoomedTotalSurfaceSize.height;

    constexpr float scrollBarThickness = 5.0f;
    constexpr float scrollBarRadius = 3.0f;

    if (hVisibleToTotal < 1.0f)
    {
        const float hScrollBarWidth = visibleSurfaceSize.width * hVisibleToTotal;
        const float hScrollBarLeft = -visibleSurfaceSize.width * hScroll;
        const float hScrollBarTop = visibleSurfaceSize.height - scrollBarThickness;
        D2D1_RECT_F hScrollBarRect{
            hScrollBarLeft,
            hScrollBarTop,
            hScrollBarLeft + hScrollBarWidth,
            hScrollBarTop + scrollBarThickness
        };
        newRects.hScrollBar = {hScrollBarRect, scrollBarRadius, scrollBarRadius};
    }

    if (vVisibleToTotal < 1.0f)
    {
        const float vScrollBarHeight = visibleSurfaceSize.height * vVisibleToTotal;
        const float vScrollBarLeft = visibleSurfaceSize.width - scrollBarThickness;
        const float vScrollBarTop = -visibleSurfaceSize.height * vScroll;
        D2D1_RECT_F vScrollBarRect{
            vScrollBarLeft,
            vScrollBarTop,
            vScrollBarLeft + scrollBarThickness,
            vScrollBarTop + vScrollBarHeight
        };
        newRects.vScrollBar = {vScrollBarRect, scrollBarRadius, scrollBarRadius};
    }

    lastRelativeScrollBarsRequest.zoomedTotalSurfaceSize = zoomedTotalSurfaceSize;
    lastRelativeScrollBarsRequest.visibleSurfaceSize = visibleSurfaceSize;
    lastRelativeScrollBarsRequest.vScroll = vScroll;
    lastRelativeScrollBarsRequest.hScroll = hScroll;

    return *(cachedRelativeScrollRects = newRects);
}

}