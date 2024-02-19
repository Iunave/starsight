#ifndef STARSIGHT_RESOURCE_HPP
#define STARSIGHT_RESOURCE_HPP

#include <cstdint>
#include <atomic>
#include <filesystem>
#include "assertion.hpp"

class SharedAsset
{
private:
    std::atomic_uint32_t RefCount;

public:

    SharedAsset()
        : RefCount(0)
    {
    }

    SharedAsset(const SharedAsset& Other)
        : RefCount(Other.GetRefCount())
    {
    }

    uint32_t GetRefCount() const
    {
        return RefCount.load(std::memory_order_relaxed);
    }

    void AddReference()
    {
        RefCount.fetch_add(1, std::memory_order_relaxed);
    }

    uint32_t RemoveReference()
    {
        return RefCount.fetch_sub(1, std::memory_order_relaxed);
    }
};

template<typename T>
concept IsSharedAsset = requires(T obj)
{
    std::is_base_of_v<SharedAsset, T>;

    { T::LoadAsset(std::filesystem::path{}) } -> std::same_as<T*>;

    { obj.IsLoaded() } -> std::same_as<bool>;
};

template<IsSharedAsset T>
class TAssetPtr
{
private:

    std::filesystem::path AssetPath;
    T* Object;

public:

    ~TAssetPtr()
    {
        Reset();
    }

    TAssetPtr(std::filesystem::path Path = "", T* Object_ = nullptr)
        : AssetPath(std::move(Path))
        , Object(Object_)
    {
        if(Object)
        {
            ASSERT(!AssetPath.empty());
            Object->AddReference();
        }
    }

    TAssetPtr(const TAssetPtr& Other)
        : AssetPath(Other.AssetPath)
        , Object(Other.Object)
    {
        if(Object)
        {
            ASSERT(!AssetPath.empty());
            Object->AddReference();
        }
    }

    TAssetPtr(TAssetPtr&& Other)
        : AssetPath(std::move(Other.AssetPath))
        , Object(Other.Object)
    {
        Other.AssetPath.clear();
        Other.Object = nullptr;
    }

    TAssetPtr& operator=(std::filesystem::path Path)
    {
        Reset();
        AssetPath = std::move(Path);

        return *this;
    }

    TAssetPtr& operator=(const TAssetPtr& Other)
    {
        Reset();

        AssetPath = Other.AssetPath;
        Object = Other.Object;

        if(Object)
        {
            ASSERT(!AssetPath.empty());
            Object->AddReference();
        }

        return *this;
    }

    TAssetPtr& operator=(TAssetPtr&& Other)
    {
        AssetPath = std::move(Other.AssetPath);
        Object = Other.Object;

        Other.AssetPath.clear();
        Other.Object = nullptr;

        return *this;
    }

    void Reset()
    {
        if(!AssetPath.empty() && Object && Object->RemoveReference() == 1)
        {
            //should be handled by the garbage collector
            void();
        }

        AssetPath.clear();
        Object = nullptr;
    }

    void Load()
    {
        ASSERT(!AssetPath.empty());

        if(Object == nullptr)
        {
            Object = T::LoadAsset(AssetPath);
            Object->AddReference();
        }
    }

    bool IsLoaded() const
    {
        return Object && Object->IsLoaded();
    }

    const std::filesystem::path& GetPath() const
    {
        return AssetPath;
    }

    T* GetPtr()
    {
        return Object;
    }

    T* operator->()
    {
        ASSERT(IsLoaded());
        return Object;
    }

    const T* operator->() const
    {
        ASSERT(IsLoaded());
        return Object;
    }
};

#endif //STARSIGHT_RESOURCE_HPP
