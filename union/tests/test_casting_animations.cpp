// Gothic adapter contract simulation, not an SDK ABI or game execution test.
#include <array>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Gothic_I_Classic {
struct zSTRING {
    std::string value;
    explicit zSTRING(const char* text) : value(text) {}
};
struct zCModelAni {
    int id = -1;
    int GetAniID() const { return id; }
};
struct zCModel {
    bool live = true;
    std::map<std::string, int> names;
    std::map<int, zCModelAni> animations;
    std::map<int, bool> active;
    std::vector<int> stopped;
    std::function<void()> onStop;
    int GetAniIDFromAniName(const zSTRING& name) const {
        assert(live);
        const auto found = names.find(name.value);
        return found == names.end() ? -1 : found->second;
    }
    zCModelAni* GetAniFromAniID(int id) {
        assert(live);
        auto found = animations.find(id);
        return found == animations.end() ? nullptr : &found->second;
    }
    int IsAniActive(zCModelAni* animation) { assert(live); return active[animation->id]; }
    void StopAni(int id) {
        assert(live);
        active[id] = false;
        stopped.push_back(id);
        if (onStop) onStop();
    }
};
struct oCAniCtrl_Human {
    int t_stand_2_cast = 100;
    int s_cast = 101;
    int t_cast_2_shoot = 102;
    int t_cast_2_stand = 103;
    int s_shoot = 104;
    int t_shoot_2_stand = 105;
};
struct oCAIHuman : oCAniCtrl_Human {};
} // namespace Gothic_I_Classic

#include "../Plugin/CastingAnimations.hpp"
using namespace Gothic_I_Classic;

namespace {
constexpr std::array<const char*, 6> names{{
    "T_PH_PREPARE", "S_PH_CHARGE", "T_PH_RELEASE",
    "T_PH_CANCEL", "S_PH_RELEASE", "T_PH_RECOVER",
}};
using IDs = std::array<int, 6>;
constexpr IDs originals{{100, 101, 102, 103, 104, 105}};
IDs Read(const oCAIHuman& ai) {
    return {{ai.t_stand_2_cast, ai.s_cast, ai.t_cast_2_shoot,
             ai.t_cast_2_stand, ai.s_shoot, ai.t_shoot_2_stand}};
}
IDs Populate(zCModel& model, int first = 0) {
    IDs result{};
    for (std::size_t i = 0; i < names.size(); ++i) {
        const int id = first + static_cast<int>(i);
        model.names[names[i]] = id;
        model.animations[id] = {id};
        result[i] = id;
    }
    return result;
}
} // namespace

int main() {
    oCAIHuman ai;
    zCModel model;
    const auto custom = Populate(model); // Animation ID zero is valid.
    assert(ParryCasting::Bind(&ai, &model));
    assert(Read(ai) == custom && model.stopped.empty());
    assert(ParryCasting::Bind(&ai, &model)); // Rebinding cannot save our IDs.
    model.active[0] = model.active[2] = true;
    model.active[999] = true; // Unrelated active animations remain untouched.
    ParryCasting::Restore(&ai, &model);
    assert(Read(ai) == originals);
    assert((model.stopped == std::vector<int>{0, 2}) && model.active[999]);
    ParryCasting::Restore(&ai, &model);
    assert(model.stopped.size() == 2); // Repeated restoration is harmless.

    // Missing final state cannot partially bind the five states found first.
    for (const int invalid : {0, 1, 2}) {
        zCModel incomplete;
        Populate(incomplete);
        if (invalid == 0) incomplete.names.erase(names.back());
        if (invalid == 1) incomplete.animations.erase(5);
        if (invalid == 2) incomplete.animations[5].id = 999;
        assert(!ParryCasting::Bind(&ai, &incomplete));
        assert(Read(ai) == originals && incomplete.stopped.empty());
    }
    assert(!ParryCasting::Bind(nullptr, &model));
    assert(!ParryCasting::Bind(&ai, nullptr));
    assert(Read(ai) == originals);

    // Native handle changes while bound become the next restore values.
    assert(ParryCasting::Bind(&ai, &model));
    ai.t_cast_2_shoot = 777;
    assert(ParryCasting::Bind(&ai, &model));
    assert(Read(ai) == custom);
    ai.s_cast = 888; // Another plugin takes ownership after our last Bind.
    ParryCasting::Restore(&ai, &model);
    auto expected = originals;
    expected[1] = 888;
    expected[2] = 777;
    assert(Read(ai) == expected);

    // If overlay loading changes IDs on the same model, preserve originals
    // and do not stop foreign animations occupying the former ID range.
    ai = {};
    model.stopped.clear();
    assert(ParryCasting::Bind(&ai, &model));
    const auto remapped = Populate(model, 20);
    model.active[0] = true;
    assert(ParryCasting::Bind(&ai, &model));
    assert(Read(ai) == remapped);
    model.active[20] = true;
    ParryCasting::Restore(&ai, &model);
    assert(Read(ai) == originals && model.active[0]);
    assert((model.stopped == std::vector<int>{20}));

    // Losing one optional state while bound restores the native state; the
    // now-unmapped old ID must not stop somebody else's active animation.
    assert(ParryCasting::Bind(&ai, &model));
    model.names.erase(names.front());
    model.active[20] = true;
    const auto stoppedCount = model.stopped.size();
    assert(!ParryCasting::Bind(&ai, &model));
    assert(Read(ai) == originals && model.active[20]);
    assert(model.stopped.size() == stoppedCount);

    // A model/world swap only forgets pointers to the old, destroyed pair.
    auto oldAI = std::make_unique<oCAIHuman>();
    auto oldModel = std::make_unique<zCModel>();
    Populate(*oldModel);
    assert(ParryCasting::Bind(oldAI.get(), oldModel.get()));
    oldAI.reset();
    oldModel.reset();
    zCModel replacement;
    const auto replacementIDs = Populate(replacement, 50);
    ParryCasting::Restore(&ai, &replacement);
    assert(Read(ai) == originals && replacement.stopped.empty());
    assert(ParryCasting::Bind(&ai, &replacement));
    assert(Read(ai) == replacementIDs);
    ParryCasting::Forget();
    ParryCasting::Restore(&ai, &replacement);
    assert(Read(ai) == replacementIDs && replacement.stopped.empty());

    oldAI = std::make_unique<oCAIHuman>();
    oldModel = std::make_unique<zCModel>();
    Populate(*oldModel);
    assert(ParryCasting::Bind(oldAI.get(), oldModel.get()));
    oldAI.reset();
    oldModel.reset();
    ai = {};
    assert(ParryCasting::Bind(&ai, &replacement)); // Direct rebind also forgets dead pointers.
    ParryCasting::Restore(&ai, &replacement);
    assert(Read(ai) == originals);

    // A cancellation callback can tear down the model and only Forget. No
    // further native lookup/cancellation may dereference that model afterwards.
    ai = {};
    assert(ParryCasting::Bind(&ai, &replacement));
    replacement.active[50] = replacement.active[51] = true;
    replacement.onStop = [&] {
        assert(Read(ai) == originals);
        replacement.live = false;
        ParryCasting::Forget();
    };
    ParryCasting::Restore(&ai, &replacement);
    assert(Read(ai) == originals && replacement.stopped.size() == 1);
    replacement.live = true;

    // A nested callback can bind a new pair; outer restoration must preserve
    // that newer ownership instead of cancelling more animations afterwards.
    oCAIHuman nestedAI;
    zCModel nestedModel;
    const auto nestedIDs = Populate(nestedModel, 80);
    assert(ParryCasting::Bind(&ai, &replacement));
    replacement.active[50] = replacement.active[51] = true;
    const auto stoppedBeforeNested = replacement.stopped.size();
    replacement.onStop = [&] { assert(ParryCasting::Bind(&nestedAI, &nestedModel)); };
    ParryCasting::Restore(&ai, &replacement);
    assert(Read(ai) == originals && Read(nestedAI) == nestedIDs);
    assert(replacement.stopped.size() == stoppedBeforeNested + 1);
    ParryCasting::Restore(&nestedAI, &nestedModel);
    assert(Read(nestedAI) == originals);

    std::cout << "Gothic casting adapter simulation: optional handles, ownership and lifecycle OK\n";
}
