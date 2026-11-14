// Adopt the reference returned by zCVisual::LoadVisual. A model attachment
// acquires its own reference; this guard releases only the loader's reference.
// No Gothic/Union dependency, so the ownership contract is testable separately.
#pragma once

namespace ParryVisual {

template<class Visual>
class LoadedReference final {
public:
    explicit LoadedReference(Visual* visual) noexcept : visual_(visual) {}
    ~LoadedReference() {
        if (visual_) visual_->Release();
    }

    LoadedReference(const LoadedReference&) = delete;
    LoadedReference& operator=(const LoadedReference&) = delete;
    LoadedReference(LoadedReference&&) = delete;
    LoadedReference& operator=(LoadedReference&&) = delete;

    Visual* Get() const noexcept { return visual_; }

private:
    Visual* visual_ = nullptr;
};

} // namespace ParryVisual
