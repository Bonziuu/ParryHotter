// Parry Hotter camera-center targeting diagnostic state.
// Keep this independent from Gothic/Union to make first-engine testing easier.
#pragma once

namespace ParryHotter {

enum class AimTargetState {
    NoPlayer,
    NoCamera,
    NoWorld,
    NoRayHit,
    WallOrObject,
    DeadNpc,
    WrongWorld,
    OutOfRange,
    ValidNpc,
    NoSpellBook
};

inline const char* AimTargetDescription(AimTargetState reason) {
    switch (reason) {
        case AimTargetState::NoPlayer: return "brak gracza";
        case AimTargetState::NoCamera: return "brak kamery";
        case AimTargetState::NoWorld: return "brak swiata";
        case AimTargetState::NoRayHit: return "brak trafienia promieniem";
        case AimTargetState::WallOrObject: return "przeszkoda / brak NPC";
        case AimTargetState::DeadNpc: return "martwy NPC";
        case AimTargetState::WrongWorld: return "NPC z innego swiata";
        case AimTargetState::OutOfRange: return "NPC poza zasiegiem";
        case AimTargetState::ValidNpc: return "NPC w celowniku";
        case AimTargetState::NoSpellBook: return "brak ksiegi magii";
        default: return "nieznany";
    }
}

inline bool TargetCanCast(AimTargetState state) {
    return state == AimTargetState::ValidNpc;
}

} // namespace ParryHotter
