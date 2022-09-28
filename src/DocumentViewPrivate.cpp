#include "DocumentViewPrivate.h"

#include <IDocumentModel.h>
#include <IDocumentPage.h>

#include <algorithm>
#include <cassert>
#include <iostream>

#if 1
#define DEBUG_VAR(x) std::cout << #x << '=' << x << "\n";
#else
#define DEBUG_VAR(x)
#endif

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

IDWriteFactory* DirectWriteFactory()
{
    static CComPtrOwner<IDWriteFactory> factory = nullptr;
    if (factory == nullptr)
    {
        OK(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&factory.ptr)
        ));
    }
    return factory.ptr;
}

std::ostream& operator<<(std::ostream& out, const D2D1_RECT_F& rect);
std::ostream& operator<<(std::ostream& out, const D2D1_SIZE_F& size);

namespace DocumentViewPrivate {

void CDocumentLayoutHelper::SetRenderTargetSize(const D2D1_SIZE_F& renderTargetSize)
{
    this->renderTargetSize = renderTargetSize;
    this->resetCaches();
}

void CDocumentLayoutHelper::SetPageMargin(int margin)
{
    this->pageMargin = margin;
    this->resetCaches();
}

void CDocumentLayoutHelper::SetPageSpacing(int spacing)
{
    this->pagesSpacing = spacing;
    this->resetCaches();
}

void CDocumentLayoutHelper::SetAlignment(TImagesViewAlignment alignment)
{
    this->strategy = alignment;
    this->resetCaches();
}

void CDocumentLayoutHelper::SetVScroll(float vScroll)
{
    this->vScroll = vScroll;
    this->resetCaches();
}

void CDocumentLayoutHelper::AddVScroll(float delta)
{
    this->vScroll += delta;
    this->resetCaches();
}

void CDocumentLayoutHelper::SetHScroll(float hScroll)
{
    this->hScroll = hScroll;
    this->resetCaches();
}

void CDocumentLayoutHelper::AddHScroll(float delta)
{
    this->hScroll += delta;
    this->resetCaches();
}

void CDocumentLayoutHelper::SetZoom(float zoom)
{
    this->zoom = zoom;
    this->resetCaches();
}

void CDocumentLayoutHelper::AddZoom(float delta)
{
    this->zoom += delta;
    this->resetCaches();
}

void CDocumentLayoutHelper::SetModel(const IDocumentModel* model)
{
    this->model = model;
    this->resetCaches();
}

const CDocumentPagesLayout& CDocumentLayoutHelper::GetLayout() const
{
    if (cachedLayout.has_value())
    {
        return *cachedLayout;
    }

    CDocumentPagesLayout retval;

    const float pageMargin = this->pageMargin;

    auto generateTextLayout = [](IDWriteTextFormat* textFormat, const std::wstring& text, float maxWidth, IDWriteTextLayout** output) {
        OK(DirectWriteFactory()->CreateTextLayout(
                text.c_str(),
                text.length(),
                textFormat,
                maxWidth,
                0.0f,
                output
        ));

        DWRITE_TEXT_METRICS textMetrics;
        OK((*output)->GetMetrics(&textMetrics));
        return std::make_pair(textMetrics.widthIncludingTrailingWhitespace, textMetrics.height * 11.f / 10.f);
    };

    retval.pageRects.reserve(model->GetPageCount());
    switch (strategy)
    {
    case TImagesViewAlignment::AlignLeft:
    {
        float topOffset = 0.0;
        float maxWidth = 0.0;
        for (int i = 0; i < model->GetPageCount(); ++i) {
            auto page = reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole));
            auto pageSize = page->GetPageSize();

            CDocumentPagesLayout::CPageLayout pageLayout;

            auto format = reinterpret_cast<IDWriteTextFormat*>(model->GetData(i, TDocumentModelRoles::HeaderFontRole));
            auto [textWidth, textHeight] = generateTextLayout(
                format,
                std::wstring{(wchar_t*)model->GetData(i, TDocumentModelRoles::HeaderTextRole)},
                pageSize.cx,
                &pageLayout.textLayout.ptr
            );

            pageLayout.textRect = {
                pageMargin,
                topOffset + pageMargin,
                textWidth + pageMargin,
                topOffset + pageMargin + textHeight};

            pageLayout.page = page;
            pageLayout.pageRect = {
                pageMargin,
                pageLayout.textRect.bottom,
                pageMargin + pageSize.cx,
                pageLayout.textRect.bottom + pageSize.cy + pageMargin
            };

            retval.pageRects.push_back(std::move(pageLayout));

            maxWidth = std::max(maxWidth, (float)pageSize.cx + pageMargin * 2);
            topOffset += pageSize.cy + pageMargin * 2 + textHeight;
            topOffset += pagesSpacing;
        }
        retval.totalSurfaceSize = {maxWidth, topOffset - pagesSpacing};
        break;
    }
    case TImagesViewAlignment::AlignRight:
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

        for (int i = 0; i < model->GetPageCount(); ++i) {
            auto page = reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole));
            auto pageSize = page->GetPageSize();

            CDocumentPagesLayout::CPageLayout pageLayout;

            auto format = reinterpret_cast<IDWriteTextFormat*>(model->GetData(i, TDocumentModelRoles::HeaderFontRole));
            auto [textWidth, textHeight] = generateTextLayout(
                format,
                std::wstring{(wchar_t*)model->GetData(i, TDocumentModelRoles::HeaderTextRole)},
                pageSize.cx,
                &pageLayout.textLayout.ptr
            );

            pageLayout.textRect = {
                maxPageWidth + pageMargin - textWidth,
                topOffset + pageMargin,
                maxPageWidth + pageMargin,
                topOffset + textHeight};

            pageLayout.page = page;
            pageLayout.pageRect = {
                maxPageWidth + pageMargin - pageSize.cx,
                pageLayout.textRect.bottom,
                maxPageWidth + pageMargin,
                pageLayout.textRect.bottom + pageSize.cy + pageMargin
            };

            retval.pageRects.push_back(std::move(pageLayout));

            topOffset += pageSize.cy + pageMargin * 2 + textHeight;
            topOffset += pagesSpacing;
        }
        retval.totalSurfaceSize = {maxPageWidth + pageMargin * 2, topOffset - pagesSpacing};
        break;
    }
    case TImagesViewAlignment::AlignHCenter:
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

        for (int i = 0; i < model->GetPageCount(); ++i) {
            auto page = reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole));
            auto pageSize = page->GetPageSize();

            CDocumentPagesLayout::CPageLayout pageLayout;

            auto format = reinterpret_cast<IDWriteTextFormat*>(model->GetData(i, TDocumentModelRoles::HeaderFontRole));
            auto [textWidth, textHeight] = generateTextLayout(
                format,
                std::wstring{(wchar_t*)model->GetData(i, TDocumentModelRoles::HeaderTextRole)},
                pageSize.cx,
                &pageLayout.textLayout.ptr
            );

            pageLayout.textRect = {
                maxPageWidth / 2 + pageMargin - pageSize.cx / 2,
                topOffset + pageMargin,
                maxPageWidth / 2 + pageMargin - pageSize.cx / 2 + textWidth,
                topOffset + textHeight};
            pageLayout.page = page;
            pageLayout.pageRect = {
                maxPageWidth / 2 + pageMargin - pageSize.cx / 2,
                pageLayout.textRect.bottom,
                maxPageWidth / 2 + pageMargin + pageSize.cx / 2,
                pageLayout.textRect.bottom + pageSize.cy + pageMargin
            };

            retval.pageRects.push_back(std::move(pageLayout));

            topOffset += pageSize.cy + pageMargin * 2 + textHeight;
            topOffset += pagesSpacing;
        }
        retval.totalSurfaceSize = {maxPageWidth + pageMargin * 2, topOffset - pagesSpacing};
        break;
    }
    case TImagesViewAlignment::HorizontalFlow:
    {
        float totalLeftOffset = 0.0;

        float topOffset = 0.0;
        float leftOffset = 0.0;
        float maxHeight = 0.0;

        const D2D1_SIZE_F drawSurfaceSize{renderTargetSize.width / this->zoom, renderTargetSize.height / this->zoom};
        for (int i = 0; i < model->GetPageCount(); ++i) {
            auto page = reinterpret_cast<IDocumentPage*>(model->GetData(i, TDocumentModelRoles::PageRole));
            auto pageSize = page->GetPageSize();

            if (leftOffset != 0.0 && (pageSize.cx + leftOffset + pageMargin * 2 > drawSurfaceSize.width)) {
                totalLeftOffset = std::max(totalLeftOffset, leftOffset);
                leftOffset = 0.0;

                topOffset += maxHeight + pageMargin * 2;
                topOffset += pagesSpacing;
                maxHeight = 0.0;
            }

            CDocumentPagesLayout::CPageLayout pageLayout;

            auto format = reinterpret_cast<IDWriteTextFormat*>(model->GetData(i, TDocumentModelRoles::HeaderFontRole));
            auto [textWidth, textHeight] = generateTextLayout(
                format,
                std::wstring{(wchar_t*)model->GetData(i, TDocumentModelRoles::HeaderTextRole)},
                pageSize.cx,
                &pageLayout.textLayout.ptr
            );

            pageLayout.textRect = {
                leftOffset + pageMargin,
                topOffset + pageMargin,
                leftOffset + (float)pageSize.cx + pageMargin + textWidth,
                topOffset + pageMargin + textHeight};
            pageLayout.page = page;

            pageLayout.pageRect = {
                pageLayout.textRect.left,
                pageLayout.textRect.bottom,
                pageLayout.textRect.left + (float)pageSize.cx,
                pageLayout.textRect.bottom + pageSize.cy + pageMargin
            };

            retval.pageRects.push_back(std::move(pageLayout));

            maxHeight = std::max(maxHeight, (float)pageSize.cy + pageMargin * 2 + textHeight);
            leftOffset += pageSize.cx + pageMargin * 2;
            leftOffset += pagesSpacing;
        }
        totalLeftOffset = std::max(totalLeftOffset, leftOffset);
        retval.totalSurfaceSize = {totalLeftOffset - pagesSpacing, topOffset + maxHeight - pagesSpacing};
        break;
    }
    default:
        break;
    }

    retval.viewportOffset = {
        retval.totalSurfaceSize.width * this->zoom * this->hScroll,
        retval.totalSurfaceSize.height * this->zoom * this->vScroll
    };

    cachedLayout.emplace(std::move(retval));
    return *cachedLayout;
}

const CScrollBarRects& CDocumentLayoutHelper::GetRelativeScrollBarRects()
{
    if (cachedRelativeScrollRects.has_value()) {
        return *cachedRelativeScrollRects;
    }

    CScrollBarRects newRects;
    DEBUG_VAR(renderTargetSize);

    const float hVisibleToTotal = this->renderTargetSize.width / (GetLayout().totalSurfaceSize.width * this->zoom);
    const float vVisibleToTotal = this->renderTargetSize.height / (GetLayout().totalSurfaceSize.height * this->zoom);
    DEBUG_VAR(hVisibleToTotal)
    DEBUG_VAR(vVisibleToTotal)

    constexpr float scrollBarThickness = 5.0f;
    constexpr float scrollBarRadius = 3.0f;

    if (hVisibleToTotal < 1.0f)
    {
        const float hScrollBarWidth = this->renderTargetSize.width * hVisibleToTotal;
        const float hScrollBarLeft = -this->renderTargetSize.width * hScroll;
        const float hScrollBarTop = this->renderTargetSize.height - scrollBarThickness;
        DEBUG_VAR(hScrollBarWidth)
        DEBUG_VAR(hScrollBarLeft)
        DEBUG_VAR(hScrollBarTop)
        D2D1_RECT_F hScrollBarRect{
            hScrollBarLeft,
            hScrollBarTop,
            hScrollBarLeft + hScrollBarWidth,
            hScrollBarTop + scrollBarThickness
        };
        DEBUG_VAR(hScrollBarRect)
        newRects.hScrollBar = {hScrollBarRect, scrollBarRadius, scrollBarRadius};
    }

    if (vVisibleToTotal < 1.0f)
    {
        const float vScrollBarHeight = this->renderTargetSize.height * vVisibleToTotal;
        const float vScrollBarLeft = this->renderTargetSize.width - scrollBarThickness;
        const float vScrollBarTop = -this->renderTargetSize.height * vScroll;
        DEBUG_VAR(vScrollBarHeight)
        DEBUG_VAR(vScrollBarLeft)
        DEBUG_VAR(vScrollBarTop)
        D2D1_RECT_F vScrollBarRect{
            vScrollBarLeft,
            vScrollBarTop,
            vScrollBarLeft + scrollBarThickness,
            vScrollBarTop + vScrollBarHeight
        };
        DEBUG_VAR(vScrollBarRect)
        newRects.vScrollBar = {vScrollBarRect, scrollBarRadius, scrollBarRadius};
    }
    return *(cachedRelativeScrollRects = newRects);
}

void CDocumentLayoutHelper::resetCaches()
{
    this->zoom = std::max(this->zoom, 0.1f);
    const float vVisibleToTotal = this->renderTargetSize.height / (GetLayout().totalSurfaceSize.height * this->zoom);
    vScroll = std::clamp(vScroll, -1.0f + vVisibleToTotal, 0.0f);
    const float hVisibleToTotal = this->renderTargetSize.width / (GetLayout().totalSurfaceSize.width * this->zoom);
    hScroll = std::clamp(hScroll, -1.0f + hVisibleToTotal, 0.0f);
    this->cachedLayout.reset();
    this->cachedRelativeScrollRects.reset();
}

}