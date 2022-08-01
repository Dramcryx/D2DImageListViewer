#ifndef INTERFACE_DOCUMENT_MODEL_H
#define INTERFACE_DOCUMENT_MODEL_H

#include <vector>

/// Fordward declarations ///
class IDWriteTextFormat;
/////////////////////////////

/// @brief Коллбэк, посылаемый моделью
struct IDocumentModelCallback
{
    /// @brief Посылается, если изменился документ целиком
    virtual void OnDocumentChanged() = 0;
    /// @brief Посылается, если изменились отедльные страницы документа
    virtual void OnPagesChanged(const std::vector<int>& page) = 0;
};

/// @brief Роли данных страницы
enum class TDocumentModelRoles
{
    HeaderFontRole, // IDWriteTextFormat
    ToolbarRole, // Toolbar resources
    PageRole // IDocumentPage
};

/// @brief Модель документа.
/// Не подразумевает прямого доступа к страницам, только через роли
struct IDocumentModel : public IDocumentModelCallback
{
    virtual ~IDocumentModel() = 0;

    virtual int GetPageCount() const = 0;

    virtual void* GetData(int index, TDocumentModelRoles role) const;
    virtual void SetData(int index, TDocumentModelRoles role);
};


#endif
