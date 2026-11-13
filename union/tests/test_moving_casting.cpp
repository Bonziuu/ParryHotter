// SDK contract simulation: per-instance leg corrections never own AI or physics.
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>

namespace Gothic_I_Classic {
using zSTRING = std::string;
struct zMAT4 {
    float m[4][4]{};
    zMAT4() { for (int i = 0; i < 4; ++i) m[i][i] = 1.0f; }
    float* operator[](int row) { return m[row]; }
    const float* operator[](int row) const { return m[row]; }
};
struct zCModelNodeInst { zCModelNodeInst* parentNode = nullptr; zMAT4 trafo; };
inline zMAT4 Multiply(const zMAT4& a, const zMAT4& b) {
    zMAT4 result;
    for (int row = 0; row < 4; ++row) for (int column = 0; column < 4; ++column) {
        result[row][column] = 0.0f;
        for (int k = 0; k < 4; ++k) result[row][column] += a[row][k] * b[k][column];
    }
    return result;
}
struct zCModel {
    std::map<std::string, zCModelNodeInst*> nodes;
    zCModelNodeInst* SearchNode(const zSTRING& name) {
        const auto found = nodes.find(name);
        return found == nodes.end() ? nullptr : found->second;
    }
    zMAT4 GetTrafoNodeToModel(zCModelNodeInst* node) {
        if (!node) return {};
        return Multiply(GetTrafoNodeToModel(node->parentNode), node->trafo);
    }
};
} // namespace Gothic_I_Classic

#include "../Plugin/MovingCasting.hpp"
using namespace Gothic_I_Classic;

struct Rig {
    zCModel model;
    zCModelNodeInst root, pelvis, upper, hand;
    std::array<zCModelNodeInst, 6> legs;
    Rig() {
        pelvis.parentNode = &root;
        upper.parentNode = &pelvis;
        hand.parentNode = &upper;
        model.nodes["Bip01"] = &root;
        model.nodes["Bip01 Pelvis"] = &pelvis;
        model.nodes["Bip01 Spine"] = &upper;
        model.nodes["Bip01 R Hand"] = &hand;
        for (int i = 0; i < 6; ++i) {
            legs[i].parentNode = i < 2 ? &pelvis : &legs[i - 2];
            legs[i].trafo[0][3] = 1.0f + float(i);
            legs[i].trafo[1][3] = -40.0f;
            model.nodes[ParryMovingCasting::Detail::kNames[i]] = &legs[i];
        }
    }
};

bool Same(const zMAT4& a, const zMAT4& b) { return ParryMovingCasting::Detail::Same(a, b); }
void assert_offsets(const zMAT4& a, const zMAT4& b) {
    for (int i = 0; i < 4; ++i) {
        assert(a[i][3] == b[i][3]);
        assert(a[3][i] == b[3][i]);
    }
}

int main() {
    using namespace ParryMovingCasting;
    Rig rig;
    const auto root = rig.root.trafo, pelvis = rig.pelvis.trafo;
    const auto upper = rig.upper.trafo, hand = rig.hand.trafo;
    std::array<zMAT4, 6> original;
    for (int i = 0; i < 6; ++i) original[i] = rig.legs[i].trafo;

    // Native state stays in its casting animation: no native run animation,
    // root displacement, weapon fields or physics exist in this adapter.
    for (int i = 0; i < 20; ++i) Update(&rig.model, true, 0.0f, 540.0f, 0.0f, 1.0f/60.0f);
    BeforeBlend(&rig.model);
    AfterBlend(&rig.model);
    assert(Applied() && Supported());
    assert(!Same(rig.legs[0].trafo, original[0]));
    assert(Same(rig.root.trafo, root) && Same(rig.pelvis.trafo, pelvis));
    assert(Same(rig.upper.trafo, upper) && Same(rig.hand.trafo, hand));
    for (int i = 0; i < 6; ++i) assert_offsets(rig.legs[i].trafo, original[i]);
    const auto once = rig.legs[0].trafo;
    AfterBlend(&rig.model); // Even a repeated callback cannot accumulate tilt.
    assert(Same(once, rig.legs[0].trafo));
    BeforeBlend(&rig.model);
    for (int i = 0; i < 6; ++i) assert(Same(rig.legs[i].trafo, original[i]));
    AfterBlend(&rig.model);
    Restore(&rig.model);
    for (int i = 0; i < 6; ++i) assert(Same(rig.legs[i].trafo, original[i]));
    assert(!Applied());
    AfterBlend(&rig.model);
    assert(!Applied()); // Restore also disables further correction until Update.

    // Native animation replaced a joint while the correction was installed.
    // Preserve that newer pose, but undo all the other still-owned joints.
    Update(&rig.model, true, 0.0f, 540.0f, 0.0f, 1.0f/60.0f);
    AfterBlend(&rig.model);
    rig.legs[0].trafo[0][0] = 0.1234f;
    const auto foreign = rig.legs[0].trafo;
    BeforeBlend(&rig.model);
    assert(Same(rig.legs[0].trafo, foreign));
    for (int i = 1; i < 6; ++i) assert(Same(rig.legs[i].trafo, original[i]));
    rig.legs[0].trafo = original[0];

    // A newer translation does not transfer ownership of our rotation: undo
    // only the rotation, keeping the independently updated local offset.
    AfterBlend(&rig.model);
    rig.legs[0].trafo[1][3] = -42.0f;
    BeforeBlend(&rig.model);
    assert(Detail::SameRotation(rig.legs[0].trafo, original[0]));
    assert(rig.legs[0].trafo[1][3] == -42.0f);
    rig.legs[0].trafo = original[0];

    // A missing joint is all-or-nothing; never invent a skeleton or edit only
    // half the character. Unrelated and replacement models are untouched.
    rig.model.nodes.erase("Bip01 R Foot");
    AfterBlend(&rig.model);
    assert(!Applied() && !Supported());
    for (int i = 0; i < 6; ++i) assert(Same(rig.legs[i].trafo, original[i]));
    rig.model.nodes["Bip01 R Foot"] = &rig.legs[5];
    Rig other;
    AfterBlend(&other.model);
    assert(Same(other.legs[0].trafo, original[0]));
    AfterBlend(&rig.model);
    assert(Applied());
    Forget(); // World teardown: never dereference the remembered old model.
    Update(&other.model, false, 0, 0, 0, 0.016f);
    AfterBlend(&rig.model);
    assert(!Applied());
    assert(Same(other.legs[0].trafo, original[0]));

    // Replacing a node inside the same model does not prove the saved node is
    // alive. Compare pointers, then only touch the node found in the live rig.
    Forget();
    Rig replaced;
    Update(&replaced.model, true, 0, 500, 0, 0.05f);
    AfterBlend(&replaced.model);
    zCModelNodeInst replacement;
    replacement.parentNode = &replaced.pelvis;
    replacement.trafo[1][3] = -77.0f;
    const auto replacementPose = replacement.trafo;
    replaced.model.nodes["Bip01 L Thigh"] = &replacement;
    Restore(&replaced.model);
    assert(Same(replacement.trafo, replacementPose));

    // An invalid parent transform aborts and rolls back earlier corrections.
    Forget();
    Rig invalid;
    zCModelNodeInst invalidParent;
    invalidParent.parentNode = &invalid.pelvis;
    invalidParent.trafo[1][1] = std::numeric_limits<float>::quiet_NaN();
    invalid.legs[3].parentNode = &invalidParent;
    Update(&invalid.model, true, 0, 500, 0, 0.05f);
    AfterBlend(&invalid.model);
    assert(!Applied());
    assert(Same(invalid.legs[0].trafo, original[0]));

    // Case-sensitive native node lookup also accepts compiled uppercase rigs.
    Forget();
    Rig uppercase;
    for (int i = 0; i < 6; ++i) {
        std::string name = Detail::kNames[i];
        uppercase.model.nodes.erase(name);
        for (auto& letter : name) if (letter >= 'a' && letter <= 'z') letter = char(letter-'a'+'A');
        uppercase.model.nodes[name] = &uppercase.legs[i];
    }
    Update(&uppercase.model, true, 0, 500, 0, 0.05f);
    AfterBlend(&uppercase.model);
    assert(Applied() && Supported());
    Restore(&uppercase.model);
    for (int i = 0; i < 6; ++i) assert(Same(uppercase.legs[i].trafo, original[i]));

    // A rotated parent changes the local hinge axis, while the resulting
    // model-space swing still follows the requested model-space direction.
    zMAT4 identity, parent, rotated, modelRotation;
    assert(Detail::Rotate(parent, identity, {{0,1,0}}, 90));
    assert(Detail::Rotate(rotated, parent, {{1,0,0}}, 16));
    assert(Detail::Rotate(modelRotation, identity, {{1,0,0}}, 16));
    const auto actualModel = Multiply(parent, rotated);
    const auto expectedModel = Multiply(modelRotation, parent);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            assert(std::fabs(actualModel[row][column]-expectedModel[row][column]) < 0.00001f);

    // Measured movement determines stride, including backwards and strafing.
    ParryHotter::CastingGait gait;
    auto stopped = gait.Step(true, 0, 0, 0, 0.016f);
    assert(!stopped.active && stopped.speed == 0);
    auto forward = gait.Step(true, 0, 540, 0, 0.05f);
    assert(forward.active && std::fabs(forward.hipAxis[0] - 1) < 0.0001f);
    auto strafe = gait.Step(true, 540, 0, 0, 0.05f);
    assert(std::fabs(strafe.hipAxis[2] + 1) < 0.0001f);
    auto backward = gait.Step(true, 0, -540, 0, 0.05f);
    assert(std::fabs(backward.hipAxis[0] + 1) < 0.0001f);
    for (int i = 0; i < 300; ++i) stopped = gait.Step(true, 0, 0, 0, 1.0f/60);
    assert(!stopped.active);
    auto disabled = gait.Step(false, 540, 0, 0, 0.016f);
    assert(!disabled.active && disabled.weight == 0);
    auto bad = gait.Step(true, std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(), 0, 0.016f);
    assert(!bad.active && bad.speed == 0);

    // Same measured travel/time produces the same cycle at common frame rates.
    std::array<ParryHotter::CastingGaitPose, 3> fps;
    const std::array<int, 3> rates{{30, 60, 120}};
    for (int i = 0; i < 3; ++i) {
        ParryHotter::CastingGait sample;
        for (int frame = 0; frame < rates[i]; ++frame)
            fps[i] = sample.Step(true, 0, 320, 0, 1.0f / float(rates[i]));
    }
    for (int i = 1; i < 3; ++i) {
        const float phaseDistance = std::fabs(std::sin(fps[i].phase) - std::sin(fps[0].phase));
        assert(phaseDistance < 0.0001f);
        assert(std::fabs(fps[i].weight - fps[0].weight) < 0.0001f);
        for (int joint = 0; joint < 6; ++joint)
            assert(std::fabs(fps[i].angles[joint] - fps[0].angles[joint]) < 0.01f);
    }
    std::cout << "Player casting gait: measured stride, isolated legs, ownership and lifecycle OK\n";
}
