#include "DocumentViewPrivate.h"

#include <IDocumentModel.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>

#ifdef DEBUG
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
    static CComPtr<IDWriteFactory> factory = nullptr;
    if (factory == nullptr)
    {
        OK(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&factory.ptr)
        ));
    }
    return factory.ptr;
}

std::ostream& operator<<(std::ostream& out, const SIZE& size)
{
    return out << size.cx << ' ' << size.cy;
}

std::ostream& operator<<(std::ostream& out, const D2D1_RECT_F& rect);
std::ostream& operator<<(std::ostream& out, const D2D1_SIZE_F& size);

namespace DocumentViewPrivate {

inline float Width(const D2D1_RECT_F& rect)
{
    return rect.right - rect.left;
}

inline float Height(const D2D1_RECT_F& rect)
{
    return rect.bottom - rect.top;
}

inline std::pair<float, float> WxH(const D2D1_RECT_F& rect)
{
    return {Width(rect), Height(rect)};
}

void CDocumentLayoutHelper::SetRenderTargetSize(const D2D1_SIZE_F& renderTargetSize)
{
    this->renderTargetSize = renderTargetSize;
     if (strategy == TImagesViewAlignment::HorizontalFlow) {
        this->RefreshLayout();
    } else {
        this->calcScrollBars();
    }
}

void CDocumentLayoutHelper::SetPageMargin(int margin)
{
    this->pageMargin = margin;
    this->RefreshLayout();
}

void CDocumentLayoutHelper::SetPageSpacing(int spacing)
{
    this->pagesSpacing = spacing;
    this->RefreshLayout();
}

void CDocumentLayoutHelper::SetAlignment(TImagesViewAlignment alignment)
{
    this->strategy = alignment;
    this->RefreshLayout();
}

void CDocumentLayoutHelper::SetVScroll(float vScroll)
{
    this->vScroll = vScroll;
    this->calcScrollBars();
}

void CDocumentLayoutHelper::AddVScroll(float delta)
{
    this->vScroll += delta;
    this->calcScrollBars();
}

void CDocumentLayoutHelper::SetHScroll(float hScroll)
{
    this->hScroll = hScroll;
    this->calcScrollBars();
}

void CDocumentLayoutHelper::AddHScroll(float delta)
{
    this->hScroll += delta;
    this->calcScrollBars();
}

void CDocumentLayoutHelper::SetZoom(float zoom)
{
    this->zoom = zoom;
    if (strategy == TImagesViewAlignment::HorizontalFlow) {
        this->RefreshLayout();
    } else {
        this->calcScrollBars();
    }
}

void CDocumentLayoutHelper::AddZoom(float delta)
{
    this->zoom += delta;
    if (strategy == TImagesViewAlignment::HorizontalFlow) {
        this->RefreshLayout();
    } else {
        this->calcScrollBars();
    }
}

const CDocumentPagesLayout& CDocumentLayoutHelper::GetLayout() const
{
    return layout;
}

const CScrollBarRects& CDocumentLayoutHelper::GetRelativeScrollBarRects() const
{
    return relativeScrollRects;
}

void CDocumentLayoutHelper::AddPage(const IPage* page, IDWriteTextFormat* format, std::wstring headerText)
{
    TRACE()

    auto absoluteLayout = createAbsolutePageLayout(page, format, headerText);
    adjustLayoutForCurrentAlignment(absoluteLayout);
    layout.pageRects.push_back(std::move(absoluteLayout));

    calcScrollBars();
}

void CDocumentLayoutHelper::DeletePage(const std::variant<const IPage*, int>& page)
{
    TRACE()

    if (std::holds_alternative<int>(page))
    {
        int index = std::get<int>(page);
        assert(0 <= index && index < layout.pageRects.size());
        layout.pageRects.erase(layout.pageRects.begin() + index);
    }
    else
    {
        auto pagePtr = std::get<const IPage*>(page);
        auto iter = std::find_if(layout.pageRects.begin(), layout.pageRects.end(),
            [pagePtr](const auto& pageLayout) {
                return pageLayout.page == pagePtr;
            }
        );
        assert(iter != layout.pageRects.end());
        layout.pageRects.erase(iter);
    }
    RefreshLayout();
}

void CDocumentLayoutHelper::ClearPages()
{
    layout = CDocumentPagesLayout{};
}

void CDocumentLayoutHelper::RefreshLayout()
{
    auto& retval = layout;
    retval.alignmentContextValue1 = 0.f;
    retval.alignmentContextValue2 = 0.f;
    retval.alignmentContextValue3 = 0.f;
    retval.alignmentContextValue4 = 0.f;

    for (auto& pageRect : retval.pageRects)
    {
        {
            auto [pageWidth, pageHeight] = pageRect.page->GetPageSize();
            auto [textWidth, textHeight] = WxH(pageRect.textRect);
            pageRect.textRect = {0.f, 0.f, textWidth, textHeight};
            pageRect.pageRect = {0.f, textHeight, (float)pageWidth, textHeight + (float)pageHeight};
        }
        auto& absoluteLayout = pageRect;
        adjustLayoutForCurrentAlignment(absoluteLayout);
    }

    calcScrollBars();
}

CDocumentPagesLayout::CPageLayout CDocumentLayoutHelper::createAbsolutePageLayout(
    const IPage* page, IDWriteTextFormat* format, std::wstring text
) const
{
    CDocumentPagesLayout::CPageLayout pageLayout;

    auto pageSize = page->GetPageSize();

    float textWidth = 0.f;
    float textHeight = 0.f;
    std::tie(pageLayout.textLayout.ptr, textWidth, textHeight) = [](IDWriteTextFormat* textFormat, const std::wstring& text, float maxWidth)
    {
        IDWriteTextLayout* output = nullptr;
        OK(DirectWriteFactory()->CreateTextLayout(text.c_str(), text.length(), textFormat, maxWidth, 0.0f, &output));

        DWRITE_TEXT_METRICS textMetrics;
        OK(output->GetMetrics(&textMetrics));
        return std::make_tuple(
            output, textMetrics.widthIncludingTrailingWhitespace, textMetrics.height * 11.f / 10.f
        );
    }(format, text, pageSize.cx);

    pageLayout.textRect = {
        (float)pageMargin,
        (float)pageMargin,
        (float)pageMargin + textWidth,
        pageMargin + textHeight};
    pageLayout.page = page;

    pageLayout.pageRect = {
        pageLayout.textRect.left,
        pageLayout.textRect.bottom,
        pageLayout.textRect.left + (float)pageSize.cx,
        pageLayout.textRect.bottom + pageSize.cy + pageMargin
    };

    return pageLayout;
}

void CDocumentLayoutHelper::adjustLayoutForCurrentAlignment(CDocumentPagesLayout::CPageLayout& absoluteLayout)
{
    auto& retval = layout;
    switch (strategy)
    {
    case TImagesViewAlignment::AlignLeft:
    {
        float& topOffset = retval.alignmentContextValue1;
        float& maxWidth = retval.alignmentContextValue2;
    
        adjustPage(absoluteLayout, topOffset, 0.f);
        DEBUG_VAR(absoluteLayout.pageRect);

        auto pageSize = absoluteLayout.page->GetPageSize();
        auto textHeight = Height(absoluteLayout.textRect);
#undef max
        maxWidth = std::max(maxWidth, (float)pageSize.cx + pageMargin * 2);
        topOffset += pageSize.cy + pageMargin * 2 + textHeight;
        topOffset += pagesSpacing;
        
        retval.totalSurfaceSize = {maxWidth, topOffset - pagesSpacing};
        break;
    }
    case TImagesViewAlignment::AlignRight:
    {
        float oldMaxPageWidth = retval.alignmentContextValue1;
        float& maxPageWidth = retval.alignmentContextValue1;
        float& topOffset = retval.alignmentContextValue2;

        maxPageWidth = std::max(maxPageWidth, Width(absoluteLayout.pageRect));
        if (maxPageWidth != oldMaxPageWidth) {
            for (auto& pageLayout : retval.pageRects) {
                auto [pageWidth, _] = pageLayout.page->GetPageSize();
                auto textWidth = Width(pageLayout.textRect);
                pageLayout.textRect.left = pageMargin;
                pageLayout.textRect.right = pageMargin + textWidth;
                pageLayout.pageRect.left = pageMargin;
                pageLayout.pageRect.right = pageMargin + pageWidth;
                adjustPage(pageLayout, 0, maxPageWidth);
            }
        }

        adjustPage(absoluteLayout, topOffset, maxPageWidth);

        auto pageHeight = Height(absoluteLayout.pageRect);
        auto textHeight = Height(absoluteLayout.textRect);

        topOffset += pageHeight + pageMargin * 2 + textHeight;
        topOffset += pagesSpacing;

        retval.totalSurfaceSize = {maxPageWidth + pageMargin * 2, topOffset - pagesSpacing};
        break;
    }
    case TImagesViewAlignment::AlignHCenter:
    {
        float oldMaxPageWidth = retval.alignmentContextValue1;
        float& maxPageWidth = retval.alignmentContextValue1;
        float& topOffset = retval.alignmentContextValue2;

        maxPageWidth = std::max(maxPageWidth, Width(absoluteLayout.pageRect));
        if (maxPageWidth != oldMaxPageWidth) {
            for (auto& pageLayout : retval.pageRects) {
                auto [pageWidth, _] = pageLayout.page->GetPageSize();
                auto textWidth = Width(pageLayout.textRect);
                pageLayout.textRect.left = pageMargin;
                pageLayout.textRect.right = pageMargin + textWidth;
                pageLayout.pageRect.left = pageMargin;
                pageLayout.pageRect.right = pageMargin + pageWidth;
                adjustPage(pageLayout, 0, maxPageWidth / 2 - Width(pageLayout.pageRect) / 2);
            }
        }

        auto pageWidth = Width(absoluteLayout.pageRect);
        auto pageHeight = Height(absoluteLayout.pageRect);
        adjustPage(absoluteLayout, topOffset, maxPageWidth / 2 - pageWidth / 2);
        
        auto textHeight = Height(absoluteLayout.textRect);

        topOffset += pageHeight + pageMargin * 2 + textHeight;
        topOffset += pagesSpacing;

        retval.totalSurfaceSize = {maxPageWidth + pageMargin * 2, topOffset - pagesSpacing};
        break;
    }
    case TImagesViewAlignment::HorizontalFlow:
    {
        float& totalLeftOffset = retval.alignmentContextValue1;

        float& topOffset = retval.alignmentContextValue2;
        float& leftOffset = retval.alignmentContextValue3;
        float& maxHeight = retval.alignmentContextValue4;

        const D2D1_SIZE_F drawSurfaceSize{renderTargetSize.width / this->zoom, renderTargetSize.height / this->zoom};

        auto [textWidth, textHeight] = WxH(absoluteLayout.textRect);
        auto [pageWidth, pageHeight] = WxH(absoluteLayout.pageRect);
        
        if (leftOffset != 0.0 && (pageWidth + leftOffset + pageMargin * 2 > drawSurfaceSize.width)) {
            totalLeftOffset = std::max(totalLeftOffset, leftOffset);
            leftOffset = 0.0;

            topOffset += maxHeight + pageMargin * 2;
            topOffset += pagesSpacing;
            maxHeight = 0.0;
        }
        
        adjustPage(absoluteLayout, topOffset, leftOffset);

        maxHeight = std::max(maxHeight, (float)pageHeight + pageMargin * 2 + textHeight);
        leftOffset += pageWidth + pageMargin * 2;
        leftOffset += pagesSpacing;

        totalLeftOffset = std::max(totalLeftOffset, leftOffset);
        retval.totalSurfaceSize = {totalLeftOffset - pagesSpacing, topOffset + maxHeight - pagesSpacing};
        break;
    }
    default:
        break;
    }
}

void CDocumentLayoutHelper::adjustPage(CDocumentPagesLayout::CPageLayout& absoluteLayout, float topOffset, float leftOffset) const
{
    switch(strategy)
    {
        case TImagesViewAlignment::AlignRight:
        {
            auto textWidth = Width(absoluteLayout.textRect);
            auto pageWidth = Width(absoluteLayout.pageRect);

            absoluteLayout.pageRect.left += leftOffset - pageWidth;
            absoluteLayout.pageRect.right += leftOffset - pageWidth;
            absoluteLayout.pageRect.top += topOffset;
            absoluteLayout.pageRect.bottom += topOffset;

            absoluteLayout.textRect.right = absoluteLayout.pageRect.right;
            absoluteLayout.textRect.left = absoluteLayout.pageRect.right - textWidth;

            absoluteLayout.textRect.top += topOffset;
            absoluteLayout.textRect.bottom += topOffset;

            break;
        }
        case TImagesViewAlignment::AlignLeft:
        case TImagesViewAlignment::AlignHCenter:
        case TImagesViewAlignment::HorizontalFlow:
        {
            absoluteLayout.textRect.left += leftOffset;
            absoluteLayout.textRect.top += topOffset;
            absoluteLayout.textRect.right += leftOffset;
            absoluteLayout.textRect.bottom += topOffset;

            absoluteLayout.pageRect.left += leftOffset;
            absoluteLayout.pageRect.top += topOffset;
            absoluteLayout.pageRect.right += leftOffset;
            absoluteLayout.pageRect.bottom += topOffset;
            break;
        }

    }
}

void CDocumentLayoutHelper::calcScrollBars()
{
    if(layout.totalSurfaceSize.height == 0.f && layout.totalSurfaceSize.width == 0.f) {
        return;
    }
    this->zoom = std::max(this->zoom, 0.1f);
    const float vVisibleToTotal = this->renderTargetSize.height / (layout.totalSurfaceSize.height * this->zoom);
    DEBUG_VAR(vVisibleToTotal)
    DEBUG_VAR(vScroll)
#undef min
    vScroll = std::clamp(vScroll, std::min(-1.0f + vVisibleToTotal, 0.f), 0.0f);
    DEBUG_VAR(vScroll)
    const float hVisibleToTotal = this->renderTargetSize.width / (layout.totalSurfaceSize.width * this->zoom);
    DEBUG_VAR(hVisibleToTotal)
    DEBUG_VAR(hScroll)
    hScroll = std::clamp(hScroll, std::min(-1.0f + hVisibleToTotal, 0.f), 0.0f);
    DEBUG_VAR(hScroll)

    layout.viewportOffset = {
        layout.totalSurfaceSize.width * this->zoom * this->hScroll,
        layout.totalSurfaceSize.height * this->zoom * this->vScroll
    };

    CScrollBarRects newRects;
    DEBUG_VAR(renderTargetSize)

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

    relativeScrollRects = newRects;
}

}