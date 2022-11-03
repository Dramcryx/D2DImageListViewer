#ifndef D2DILV_DOCUMENT_FROM_DISK_H
#define D2DILV_DOCUMENT_FROM_DISK_H

#include <ComPtr.h>
#include <IDocumentModel.h>

#include <memory>
#include <string>

class CWICImage;

struct IWICImagingFactory;

class CDocumentFromDisk : public IDocument
{
public:
    CDocumentFromDisk(const wchar_t* fileName);
    ~CDocumentFromDisk() override;
    
    const wchar_t* GetName() const override { return fileName.c_str(); }
    int GetPagesCount() const override { return images.size();}
    const IPage* GetPage(int index) const override;
    int GetIndexOf(const IPage* page) const override;

private:
    std::wstring fileName;
    CComPtr<IWICImagingFactory> wicFactory;
    std::vector<std::unique_ptr<CWICImage>> images;
};

#endif
