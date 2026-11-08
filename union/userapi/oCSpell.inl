// Preserve the engine's actual return value and per-effect cast identity.
int PH_Cast();
// Block investing the player's spells outside the plugin's own calls.
int PH_Invest();
// Defer automatic full-charge casting until the plugin receives LMB release.
int PH_GetSpellStatus();
