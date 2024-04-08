////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/MoveOnlyAny.h>

namespace Ovito {

/**
 * \brief A tagged (=strongly-typed) tuple, which can be used as key for the RendererResourceCache class.
 */
template<typename TagType, typename... TupleFields>
struct RendererResourceKey : public std::tuple<TupleFields...>
{
    /// Inherit constructors of tuple type.
    using std::tuple<TupleFields...>::tuple;
};

/**
 * \brief A cache data structure that accepts keys with arbitrary type and which handles resource lifetime.
 */
class RendererResourceCache : public std::enable_shared_from_this<RendererResourceCache>
{
public:

    /// Data type used by the resource cache to refer to a frame being in flight.
    using ResourceFrameHandle = int;

    /// A frame being in flight.
    class ResourceFrame
    {
    public:
        /// Default constructor.
        ResourceFrame() = default;

        /// Destructor.
        ~ResourceFrame() {
            if(_cache)
                _cache->releaseResourceFrame(_frameNumber);
        }

        // A frame cannot be copied.
        ResourceFrame(const ResourceFrame& other) = delete;
        ResourceFrame& operator=(const ResourceFrame& other) = delete;

        // A frame can be moved.
        ResourceFrame(ResourceFrame&& rhs) noexcept : _cache(std::exchange(rhs._cache, {})), _frameNumber(std::exchange(rhs._frameNumber, 0)) { OVITO_ASSERT(!rhs._cache); }
        ResourceFrame& operator=(ResourceFrame&& rhs) noexcept {
            ResourceFrame(std::move(rhs)).swap(*this);
            return *this;
        }
        inline void swap(ResourceFrame& rhs) noexcept {
            _cache.swap(rhs._cache);
            std::swap(_frameNumber, rhs._frameNumber);
        }

        /// Returns a reference to the cached value for the given key.
        /// Creates a new cache entry with a default-initialized value if the key doesn't exist.
        template<typename Value, typename Key>
        Value& lookup(Key&& key) const {
            OVITO_ASSERT(_cache && _frameNumber != 0);
            return _cache->lookup<Value>(std::forward<Key>(key), _frameNumber);
        }

        /// Returns true if the frame is valid.
        inline operator bool() const noexcept { return (bool)_cache; }

    private:
        /// Constructor.
        ResourceFrame(std::shared_ptr<RendererResourceCache> cache, ResourceFrameHandle frameNumber) noexcept : _cache(std::move(cache)), _frameNumber(frameNumber) {}

        std::shared_ptr<RendererResourceCache> _cache;
        ResourceFrameHandle _frameNumber = 0;

        friend class RendererResourceCache;
    };

public:

    /// Constructor.
    RendererResourceCache() = default;

#ifdef OVITO_DEBUG
    /// Destructor.
    ~RendererResourceCache() {
        OVITO_ASSERT(QThread::currentThread() == _owningThread);
        // The cache should be completely empty at the time it is destroyed.
        OVITO_ASSERT(_activeResourceFrames.empty());
        OVITO_ASSERT(empty());
    }
#endif

    // A cache cannot be copied or moved.
    RendererResourceCache(const RendererResourceCache& other) = delete;
    RendererResourceCache& operator=(const RendererResourceCache& other) = delete;
    RendererResourceCache(RendererResourceCache&& other) = delete;
    RendererResourceCache& operator=(RendererResourceCache&& other) = delete;

    /// Returns a reference to the value for the given key.
    /// Creates a new cache entry with a default-initialized value if the key doesn't exist.
    template<typename Value, typename Key>
    Value& lookup(Key&& key, ResourceFrameHandle resourceFrame) {
        OVITO_ASSERT(std::find(_activeResourceFrames.begin(), _activeResourceFrames.end(), resourceFrame) != _activeResourceFrames.end());

        // Note: The following check has been disabled, because we now allow lookups to be made from worker threads.
        // The user is reponsible for ensuring that no two threads access the cache concurrently.

        // OVITO_ASSERT(QThread::currentThread() == _owningThread);

        // Check if the key exists in the cache.
        for(CacheEntry& entry : _entries) {
            if(entry.key.type() == typeid(Key) && entry.value.type() == typeid(Value) && key == any_cast<const Key&>(entry.key)) {
                // Register the frame in which the resource was actively used.
                if(std::find(entry.frames.begin(), entry.frames.end(), resourceFrame) == entry.frames.end())
                    entry.frames.push_back(resourceFrame);
                // Return reference to the value.
                return any_cast<Value&>(entry.value);
            }
        }
        // Create a new key-value pair with a default-constructed value.
        _entries.emplace_back(std::forward<Key>(key), resourceFrame);
        any_moveonly& value = _entries.back().value;
        value.emplace<Value>();
        OVITO_ASSERT(_entries.back().key.type() == typeid(Key));
        OVITO_ASSERT(value.type() == typeid(Value));
        return any_cast<Value&>(value);
    }

    /// Indicates whether the cache is currently empty.
    bool empty() const { return _entries.empty(); }

    /// Returns the current number of cache entries.
    size_t size() const { return _entries.size(); }

    /// Informs the resource manager that a new frame is going to be rendered.
    ResourceFrame acquireResourceFrame() {
        OVITO_ASSERT(QThread::currentThread() == _owningThread);

        // On the first frame, the cache should be empty.
        OVITO_ASSERT(!_activeResourceFrames.empty() || _entries.empty());

        // Wrap around counter.
        if(_nextResourceFrame == std::numeric_limits<ResourceFrameHandle>::max())
            _nextResourceFrame = 0;

        // Add it to the list of active frames.
        _nextResourceFrame++;
        _activeResourceFrames.push_back(_nextResourceFrame);

        return ResourceFrame(shared_from_this(), _nextResourceFrame);
    }

private:

    /// Informs the resource manager that a frame has completely finished rendering and all resources associated with that frame may be released.
    void releaseResourceFrame(ResourceFrameHandle frame) {
        OVITO_ASSERT(QThread::currentThread() == _owningThread);
        OVITO_ASSERT(frame > 0);

        // Remove frame from the list of active frames.
        // There is no need to maintain the original list order.
        // We can move the last item into the erased list position.
        auto iter = std::find(_activeResourceFrames.begin(), _activeResourceFrames.end(), frame);
        OVITO_ASSERT(iter != _activeResourceFrames.end());
        *iter = _activeResourceFrames.back();
        _activeResourceFrames.pop_back();

        // Release the resources associated with the frame unless they are shared with another frame that is still in flight.
        auto end = _entries.end();
        for(auto entry = _entries.begin(); entry != end; ) {
            auto frameIter = std::find(entry->frames.begin(), entry->frames.end(), frame);
            if(frameIter != entry->frames.end()) {
                if(entry->frames.size() != 1) {
                    *frameIter = entry->frames.back();
                    entry->frames.pop_back();
                }
                else {
                    --end;
                    *entry = std::move(*end);
                    continue;
                }
            }
            ++entry;
        }
        _entries.erase(end, _entries.end());

        OVITO_ASSERT(!_activeResourceFrames.empty() || _entries.empty());
    }

    struct CacheEntry {
        template<typename Key> CacheEntry(Key&& _key, ResourceFrameHandle _frame) noexcept : key(std::forward<Key>(_key)) { frames.push_back(_frame); }
        Ovito::any_moveonly key;
        Ovito::any_moveonly value;
        QVarLengthArray<ResourceFrameHandle, 6> frames;

        // A cache entry cannot be copied.
        CacheEntry(const CacheEntry& other) = delete;
        CacheEntry& operator=(const CacheEntry& other) = delete;

        // A cache entry can be moved.
        CacheEntry(CacheEntry&& other) noexcept : key(std::move(other.key)), value(std::move(other.value)), frames(std::move(other.frames)) {}
        CacheEntry& operator=(CacheEntry&& other) noexcept {
            key = std::move(other.key);
            value = std::move(other.value);
            frames = std::move(other.frames);
            return *this;
        }
    };

    /// Stores all key-value pairs of the cache.
    /// Note we are using std::deque instead of std::vector here, because we require stability of pointers.
    /// lookup() returns references to elements in the cache, which must remain valid even when new objects are added
    /// to the cache.
    std::deque<CacheEntry> _entries;

    /// List of frames that are currently being rendered (by the CPU and/or the GPU).
    std::vector<ResourceFrameHandle> _activeResourceFrames;

    /// Counter that keeps track of how many resource frames have been acquired in total.
    ResourceFrameHandle _nextResourceFrame = 0;

#ifdef OVITO_DEBUG
    /// Keep track of the thread that owns the resource cache.
    /// Resource caches are not thread-safe and may only be used from a single thread.
    QThread* _owningThread = QThread::currentThread();
#endif
};

}   // End of namespace
