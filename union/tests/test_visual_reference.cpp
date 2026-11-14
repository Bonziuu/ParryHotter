#include "../Plugin/VisualReference.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>

namespace {
struct Counts {
    int created = 0;
    int destroyed = 0;
    int alive = 0;
    int added = 0;
    int released = 0;
};

struct Visual {
    Counts& counts;
    int refs = 1; // The reference returned by LoadVisual.
    explicit Visual(Counts& value) : counts(value) {
        ++counts.created;
        ++counts.alive;
    }
    ~Visual() { ++counts.destroyed; --counts.alive; }
    void AddRef() { assert(refs > 0); ++refs; ++counts.added; }
    void Release() {
        assert(refs > 0);
        ++counts.released;
        if (--refs == 0) delete this;
    }
};

struct Node {
    Visual* visual = nullptr;
    ~Node() { SetVisual(nullptr); }
    // Matches the native SetNodeVisualS reference ownership: the node adds
    // its own reference and drops the old one when detached or replaced.
    void SetVisual(Visual* value) {
        if (visual == value) return;
        if (visual) visual->Release();
        visual = value;
        if (visual) visual->AddRef();
    }
};

bool TryAttach(Node& node, Counts& counts, bool nativeAccepts) {
    Visual* visual = new Visual(counts);
    ParryVisual::LoadedReference<Visual> loaded(visual);
    assert(loaded.Get() == visual && visual->refs == 1); // Guard never AddRef's.
    if (!nativeAccepts || node.visual) return false;
    node.SetVisual(visual);
    assert(visual->refs == 2); // Loader + node until this scope returns.
    return node.visual == visual;
}
} // namespace

int main() {
    using Guard = ParryVisual::LoadedReference<Visual>;
    static_assert(!std::is_copy_constructible<Guard>::value);
    static_assert(!std::is_copy_assignable<Guard>::value);
    static_assert(!std::is_move_constructible<Guard>::value);
    static_assert(!std::is_move_assignable<Guard>::value);

    Counts counts;
    Node hand;
    // Repeated load/attach/detach must not retain one loader reference per aim.
    for (int i = 0; i < 100; ++i) {
        assert(TryAttach(hand, counts, true));
        assert(hand.visual && hand.visual->refs == 1 && counts.alive == 1);
        hand.SetVisual(nullptr);
        assert(counts.alive == 0 && counts.created == counts.destroyed);
    }
    assert(counts.added == 100 && counts.released == 200);

    // An unsuccessful native attachment still releases the loaded resource.
    assert(!TryAttach(hand, counts, false));
    assert(!hand.visual && counts.alive == 0 && counts.created == counts.destroyed);

    // Early return for an occupied socket must leave the foreign visual alone.
    Visual* foreign = new Visual(counts);
    hand.SetVisual(foreign);
    foreign->Release(); // Only the node owns the foreign visual now.
    const int liveBefore = counts.alive;
    assert(!TryAttach(hand, counts, true));
    assert(hand.visual == foreign && foreign->refs == 1 && counts.alive == liveBefore);
    hand.SetVisual(nullptr);
    assert(counts.alive == 0 && counts.created == counts.destroyed);

    // A failed LoadVisual can be guarded safely as well.
    const int releases = counts.released;
    { Guard empty(nullptr); assert(!empty.Get()); }
    assert(counts.released == releases);
    assert(counts.created == 103 && counts.destroyed == 103);
    std::cout << "Loaded visual references: repeated attach/detach, failures and foreign ownership OK\n";
}
