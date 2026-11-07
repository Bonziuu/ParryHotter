// Bounded per-projectile identity tracking, independent of Gothic/Union.
// Effect tokens must be actual engine effect identities, not spell IDs.
#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace ParryHotter {

class CastLedger final {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Token = std::uintptr_t;
    using Id = std::uint64_t;
    static constexpr std::size_t Capacity = 32;
    static constexpr auto Lifetime = std::chrono::seconds(7);
    static constexpr Id Invalid = 0;

    enum class MatchState { NoMatch, Matched, Ambiguous };
    struct Match {
        MatchState state = MatchState::NoMatch;
        Id id = Invalid;
        int slot = -1; // zero-based slot; -1 is never an identified spell
        bool confirmed = false;
    };

    // Stage immediately before native casting so synchronous damage callbacks
    // can resolve the real effect. Never invent a token when none is available.
    // Confirm after engine success, or Revoke if the cast was rejected.
    Id Stage(Token effect, Token target, int nativeSpellId, int slot,
             TimePoint now) {
        Expire(now);
        if (effect == 0 || target == 0 || nativeSpellId < 0 || slot < 0)
            return Invalid;
        for (auto& entry : entries_) {
            if (entry.id != Invalid) continue;
            ++nextId_;
            if (nextId_ == Invalid) ++nextId_;
            entry = {nextId_, effect, target, nativeSpellId, slot,
                     now + Lifetime, false, false};
            return entry.id;
        }
        // Do not evict a still-live projectile to classify a newer one.
        return Invalid;
    }

    bool Confirm(Id id, TimePoint now) {
        Expire(now);
        auto* entry = Find(id);
        if (!entry) return false;
        entry->confirmed = true;
        return true;
    }

    // Native casting can replace the charging FX with a projectile. Both
    // identities belong to one cast and share one physics-attempt flag.
    bool AddEffectAlias(Id id, Token effect, TimePoint now) {
        Expire(now);
        auto* entry = Find(id);
        if (!entry || effect == 0) return false;
        if (entry->effect == effect) return true;
        for (const auto alias : entry->aliases)
            if (alias == effect) return true;
        for (auto& alias : entry->aliases) {
            if (alias == 0) { alias = effect; return true; }
        }
        return false;
    }

    bool Revoke(Id id) {
        auto* entry = Find(id);
        if (!entry) return false;
        *entry = {};
        return true;
    }

    Match Resolve(Token effect, Token victim, int nativeSpellId,
                  TimePoint now) const {
        Match result{};
        if (effect == 0 || victim == 0 || nativeSpellId < 0) return result;
        for (const auto& entry : entries_) {
            bool sameEffect = entry.effect == effect;
            for (const auto alias : entry.aliases) sameEffect |= alias == effect;
            if (entry.id == Invalid || now >= entry.expires ||
                !sameEffect || entry.target != victim ||
                entry.nativeSpellId != nativeSpellId) continue;
            if (result.state == MatchState::Matched) {
                // Even duplicate keys are not assumed to mean the latest cast.
                return {MatchState::Ambiguous, Invalid, -1, false};
            }
            result = {MatchState::Matched, entry.id, entry.slot,
                      entry.confirmed};
        }
        return result;
    }

    // Lets an engine adapter release retained effect/NPC references after
    // expiry, rejection or a world reset without exposing mutable entries.
    Match FindById(Id id, TimePoint now) const {
        if (id == Invalid) return {};
        for (const auto& entry : entries_) {
            if (entry.id == id && now < entry.expires)
                return {MatchState::Matched, entry.id, entry.slot,
                        entry.confirmed};
        }
        return {};
    }

    // Reserve the single physics/telemetry attempt before testing NPC physics.
    // Repeated damage callbacks cannot reapply an impulse or inflate counters.
    bool TryMarkImpact(Id id, TimePoint now) {
        Expire(now);
        auto* entry = Find(id);
        if (!entry || entry->impactAttempted) return false;
        bool unique = false;
        for (const auto effect : {entry->effect, entry->aliases[0],
                                  entry->aliases[1], entry->aliases[2]}) {
            if (!effect) continue;
            const auto match = Resolve(effect, entry->target, entry->nativeSpellId, now);
            unique |= match.state == MatchState::Matched && match.id == id;
        }
        if (!unique) return false;
        entry->impactAttempted = true;
        return true;
    }

    void Expire(TimePoint now) {
        for (auto& entry : entries_) {
            if (entry.id != Invalid && now >= entry.expires) entry = {};
        }
    }

    void Clear() {
        entries_ = {};
        // Keep generation increasing so pre-reset handles never address a new
        // projectile after load/world changes.
    }

    std::size_t Size(TimePoint now) const {
        std::size_t count = 0;
        for (const auto& entry : entries_) {
            if (entry.id != Invalid && now < entry.expires) ++count;
        }
        return count;
    }

private:
    struct Entry {
        Id id = Invalid;
        Token effect = 0;
        Token target = 0;
        int nativeSpellId = -1;
        int slot = -1;
        TimePoint expires{};
        bool confirmed = false;
        bool impactAttempted = false;
        std::array<Token, 3> aliases{};
    };

    Entry* Find(Id id) {
        if (id == Invalid) return nullptr;
        for (auto& entry : entries_) {
            if (entry.id == id) return &entry;
        }
        return nullptr;
    }

    std::array<Entry, Capacity> entries_{};
    Id nextId_ = Invalid;
};

} // namespace ParryHotter
