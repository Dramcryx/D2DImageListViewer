#ifndef D2DILV_COMPTR_H
#define D2DILV_COMPTR_H

#include <utility>

template<typename T>
struct CComPtr
{
    T* ptr = nullptr;

    CComPtr() : ptr{nullptr}
    {}

    CComPtr(T* _ptr) : ptr{_ptr}
    {
    }

    CComPtr(const CComPtr& rhs)
    {
        Reset();
        ptr = rhs.ptr;
        ptr->AddRef();
    }

    CComPtr(CComPtr&& rhs) noexcept
    {
        Reset();
        ptr = std::exchange(rhs.ptr, nullptr);
    }

    CComPtr& operator=(const CComPtr& rhs)
    {
        Reset();
        ptr = rhs.ptr;
        ptr->AddRef();
        return *this;
    }

    CComPtr& operator=(CComPtr&& rhs) noexcept
    {
        Reset();
        ptr = std::exchange(rhs.ptr, nullptr);
        return *this;
    }

    ~CComPtr()
    {
        Reset();
    }

    operator T*()
    {
        return ptr;
    }

    T* operator->()
    {
        return ptr;
    }

    void Reset(T* newPtr = nullptr)
    {
        if (ptr != nullptr)
        {
            std::exchange(ptr, newPtr)->Release();
        }
    }
};

#endif
