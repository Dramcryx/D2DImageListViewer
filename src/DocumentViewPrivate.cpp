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

std::ostream& operator<<(std::ostream& out, const D2D1_RECT_F& rect);

namespace DocumentViewPrivate {

const CDocumentPagesLayout& CDocumentLayoutHelper::GetOrCreateLayout(const CDocumentPagesLayoutParams& params, const IDocumentModel* model)
{
    if (cachedLayout.has_value() && params == lastLayoutRequest.first && model == lastLayoutRequest.second) {
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
    return *cachedLayout;
}

const CScrollBarRects& CDocumentLayoutHelper::GetOrCreateScrollBarRects(const D2D1_SIZE_F& visibleSurfaceSize, const D2D1_SIZE_F& totalSurfaceSize)
{
    if (cachedScrollRects.has_value() && visibleSurfaceSize == lastScrollBarsParams.first && totalSurfaceSize == lastScrollBarsParams.second) {
        return *cachedScrollRects;
    }

    float hVisibleToTotal = static_cast<float>(visibleSurfaceSize.width) / static_cast<float>(totalSurfaceSize.width);
    float vVisibleToTotal = static_cast<float>(visibleSurfaceSize.height) / static_cast<float>(totalSurfaceSize.height);

    constexpr int scrollBarThickness = 5;

    CScrollBarRects rects;
    // hScrollRect
    if (hVisibleToTotal < 1.0f) {
        int hScrollBarWidth = visibleSurfaceSize.width * hVisibleToTotal;
        D2D1_RECT_F scrollRect{2, -2 - scrollBarThickness, hScrollBarWidth - 2, -2};
        rects.hScrollBar = {scrollRect, 4.0, 4.0};
    }
    // vScrollRect
    if (vVisibleToTotal < 1.0f) {
        int vScrollBarHeight = visibleSurfaceSize.height * vVisibleToTotal;
        D2D1_RECT_F scrollRect{-2 - scrollBarThickness, 2, -2, vScrollBarHeight - 2};
        rects.vScrollBar = {scrollRect, 4.0, 4.0};
    }
    cachedScrollRects = rects;

    return *cachedScrollRects;
}

}