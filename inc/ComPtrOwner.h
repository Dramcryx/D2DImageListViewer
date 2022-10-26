#ifndef COMPTR_OWNER_H
#define COMPTR_OWNER_H

#include <iostream>
#include <utility>

template<typename T>
struct CComPtrOwner
{
    T* ptr = nullptr;

    CComPtrOwner() : ptr{nullptr}
    {}

    CComPtrOwner(T* _ptr) : ptr{_ptr}
    {
#if 0
        std::cout << __PRETTY_FUNCTION__ << "\n";
#endif
    }

    CComPtrOwner(const CComPtrOwner& rhs)
    {
#if 0
        std::cout << __PRETTY_FUNCTION__ << "\n";
#endif
        Reset();
        ptr = rhs.ptr;
        ptr->AddRef();
    }

    CComPtrOwner(CComPtrOwner&& rhs) noexcept
    {
#if 0
        std::cout << __PRETTY_FUNCTION__ << "\n";
#endif
        Reset();
        ptr = std::exchange(rhs.ptr, nullptr);
    }

    CComPtrOwner& operator=(const CComPtrOwner& rhs)
    {
#if 0
        std::cout << __PRETTY_FUNCTION__ << "\n";
#endif
        Reset();
        ptr = rhs.ptr;
        ptr->AddRef();
        return *this;
    }

    CComPtrOwner& operator=(CComPtrOwner&& rhs) noexcept
    {
#if 0
        std::cout << __PRETTY_FUNCTION__ << "\n";
#endif
        Reset();
        ptr = std::exchange(rhs.ptr, nullptr);
        return *this;
    }

    ~CComPtrOwner()
    {
#if 0
        std::cout << __PRETTY_FUNCTION__ << "\n";
#endif
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
#if 0
        std::cout << __PRETTY_FUNCTION__ << "\n";
#endif
        if (ptr != nullptr)
        {
            std::exchange(ptr, newPtr)->Release();
        }
    }
};

#endif
