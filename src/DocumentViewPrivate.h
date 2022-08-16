#ifndef DOCUMENT_VIEW_PRIVATE_H
#define DOCUMENT_VIEW_PRIVATE_H

class CDocumentView;

class CDocumentViewPrivate {
public:
    CDocumentViewPrivate(CDocumentView* view);
    ~CDocumentViewPrivate() = default;

    void GeneratePagesLayout();
    void GenerateScrollBars();
};

#endif
