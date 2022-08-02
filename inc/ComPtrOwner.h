#ifndef COMPTR_OWNER_H
#define COMPTR_OWNER_H

template<typename T>
struct CComPtrOwner
{
    T* ptr;

    CComPtrOwner(T* _ptr) : ptr{_ptr}
    {}

    ~CComPtrOwner()
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

    void Reset()
    {
        if (ptr != nullptr)
        {
            ptr->Release();
            ptr = nullptr;
        }
    }
};

#endif
