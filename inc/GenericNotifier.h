#ifndef D2DILV_GENERIC_NOTIFIER_H
#define D2DILV_GENERIC_NOTIFIER_H


#ifdef DEBUG
#include <unordered_set>
#include <cassert>
#endif

#include <algorithm>
#include <vector>

/// @brief Basic notifications manager
/// @tparam TCallbackInterface Notificaions interface
template <typename TCallbackInterface>
class CSimpleNotifier {
public:
    void Subscribe(TCallbackInterface* callback)
    {
        this->callbacks.push_back(callback);
#ifdef DEBUG
        assert(this->callbacksHashTable.find(callback) == this->callbacksHashTable.end());
        this->callbacksHashTable.insert(callback);
#endif
    }

    void Unsubscribe(TCallbackInterface* callback)
    {
        callbacks.erase(std::find(callbacks.begin(), callbacks.end(), callback));
#ifdef DEBUG
        assert(this->callbacksHashTable.find(callback) != this->callbacksHashTable.end());
        this->callbacksHashTable.erase(callback);
#endif
    }

    template <auto CallbackMember, typename ... TArgs>
    void Notify(TArgs&& ... args)
    {
        for (auto& callback : callbacks) {
            (callback->*CallbackMember)(std::forward<TArgs>(args)...);
        }
    }

private:
    std::vector<TCallbackInterface*> callbacks;
#ifdef DEBUG
    std::unordered_set<TCallbackInterface*> callbacksHashTable;
#endif
};

#endif
