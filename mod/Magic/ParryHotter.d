// Parry Hotter - Gothic 1 Daedalus prototype
// Keep this file after Spells_Params.d and before B_AssessMagic.d in Gothic.src.
// Uses built-in spell IDs and built-in rune models. No assets from Harry Potter games.

// Selected slot comes from the Union HUD at the moment of casting.
// 0 preserves compatibility with the older DEV mode without Union.
// Gothic's native spells share our placeholder spell IDs, so distinguish
// Parry Hotter slots on impact whenever the Union plugin is present.
VAR INT PH_SELECTED_SPELL;
VAR INT PH_UNION_LOADED; // 1 only while the native HUD plugin owns casting


FUNC INT PH_IsSelectedSpell(VAR INT slot)
{
    if (!Npc_IsPlayer(other))
    {
        return FALSE;
    };
    // In PLAY, unrelated vanilla spells must never trigger Parry effects.
    // In DEV without Union, retain the original player-runestone fallback.
    if (PH_UNION_LOADED == 1)
    {
        if (PH_SELECTED_SPELL == slot)
        {
            return TRUE;
        };
    }
    else
    {
        if (PH_SELECTED_SPELL == 0)
        {
            return TRUE;
        };
    };
    return FALSE;
};

INSTANCE PH_Rune_Flipendo(C_Item)
{
    name = "Runa";
    mainflag = ITEM_KAT_RUNE;
    flags = 0;
    value = 50;
    visual = "ItAr_Rune_06.3ds";
    material = MAT_STONE;
    spell = SPL_WINDFIST;
    mag_circle = 1;
    description = "Flipendo";
    TEXT[0] = "Odrzut przeciwnika";
    TEXT[1] = NAME_Manakosten; COUNT[1] = SPL_SENDCAST_WINDFIST;
};

INSTANCE PH_Rune_Levicorpus(C_Item)
{
    name = "Runa";
    mainflag = ITEM_KAT_RUNE;
    flags = 0;
    value = 50;
    visual = "ItAr_Rune_06.3ds";
    material = MAT_STONE;
    spell = SPL_ICECUBE;
    mag_circle = 1;
    description = "Levicorpus";
    TEXT[0] = "Zawieszenie celu (prototyp)";
    TEXT[1] = NAME_Manakosten; COUNT[1] = SPL_SENDCAST_ICECUBE;
};

INSTANCE PH_Rune_Rictusempra(C_Item)
{
    name = "Runa";
    mainflag = ITEM_KAT_RUNE;
    flags = 0;
    value = 50;
    visual = "ItAr_Rune_06.3ds";
    material = MAT_STONE;
    spell = SPL_BERZERK;
    mag_circle = 1;
    description = "Rictusempra";
    TEXT[0] = "Prototyp: szal";
    TEXT[1] = NAME_Manakosten; COUNT[1] = SPL_SENDCAST_BERZERK;
};

INSTANCE PH_Rune_Expelliarmus(C_Item)
{
    name = "Runa";
    mainflag = ITEM_KAT_RUNE;
    flags = 0;
    value = 50;
    visual = "ItAr_Rune_06.3ds";
    material = MAT_STONE;
    spell = SPL_THUNDERBALL;
    mag_circle = 1;
    description = "Expelliarmus";
    TEXT[0] = "Ogluszenie i rozbrojenie";
    TEXT[1] = NAME_Manakosten; COUNT[1] = SPL_SENDCAST_THUNDERBALL;
};

INSTANCE PH_Rune_Bumbardo(C_Item)
{
    name = "Runa";
    mainflag = ITEM_KAT_RUNE;
    flags = 0;
    value = 50;
    visual = "ItAr_Rune_06.3ds";
    material = MAT_STONE;
    spell = SPL_STORMFIST;
    mag_circle = 1;
    description = "Bumbardo Maximus";
    TEXT[0] = "Wybuch i wstrzas celu";
    TEXT[1] = NAME_Manakosten; COUNT[1] = SPL_SENDCAST_STORMFIST;
};

INSTANCE PH_Rune_Avada(C_Item)
{
    name = "Runa";
    mainflag = ITEM_KAT_RUNE;
    flags = 0;
    value = 50;
    visual = "ItAr_Rune_06.3ds";
    material = MAT_STONE;
    spell = SPL_BREATHOFDEATH;
    mag_circle = 1;
    description = "Avada Kedavra";
    TEXT[0] = "Piorun i zabawna smierc";
    TEXT[1] = NAME_Manakosten; COUNT[1] = SPL_SENDCAST_BREATHOFDEATH;
};

// Called in B_AssessMagic_ThunderBall when Parry is the caster.
// The game will still apply its regular short electrical stun.
FUNC VOID PH_OnExpelliarmusHit()
{
    VAR C_ITEM heldWeapon;
    if (Npc_IsPlayer(other))
    {
        if (!Npc_IsDead(self))
        {
            heldWeapon = Npc_GetReadiedWeapon(self);
            if (Hlp_IsValidItem(heldWeapon))
            {
                AI_DropItem(self, Hlp_GetInstanceID(heldWeapon));
            };
        };
    };
};

// Levicorpus: a short harmless human reaction, deliberately separate from
// Gothic's damaging ZS_MagicFreeze. This is the SCRIPTED PLACEHOLDER for the
// eventual inverted floating animation. No permanent damage or trapped AI.
// Non-human targets retain normal IceCube behavior.
FUNC VOID PH_OnLevicorpusHit()
{
    if (Npc_IsPlayer(other))
    {
        if (!Npc_IsDead(self) && C_NpcIsHuman(self))
        {
            Npc_ClearAIQueue(self);
            AI_StartState(self, ZS_PH_Levicorpus, 0, "");
        };
    };
};

FUNC VOID ZS_PH_Levicorpus()
{
    B_FullStop(self);
    AI_PlayAni(self, "T_PSI_VICTIM");
    AI_Wait(self, 0.8);
};

FUNC INT ZS_PH_Levicorpus_Loop()
{
    if (Npc_IsDead(self) || (Npc_GetStateTime(self) >= 4))
    {
        return LOOP_END;
    };
    AI_Wait(self, 0.4);
    return LOOP_CONTINUE;
};

FUNC VOID ZS_PH_Levicorpus_End()
{
    if (!Npc_IsDead(self))
    {
        AI_ContinueRoutine(self);
    };
};

// Bumbardo Maximus keeps native Gothic Stormfist push physics. After a
// successful magic impact, queue a short shock reaction on human targets.
// This is a distinct experimental effect, NOT a custom ragdoll or flight arc.
FUNC VOID PH_OnBumbardoHit()
{
    if (PH_IsSelectedSpell(5))
    {
        if (!Npc_IsDead(self) && C_NpcIsHuman(self))
        {
            AI_PlayAni(self, "T_STAND_2_LIGHTNING_VICTIM");
        };
    };
};

// Rictusempra: temporary comedy animation state, not permanent Berserk.
// Using animations already shipped with Gothic 1; no extra files needed.
FUNC VOID PH_OnRictusempraHit()
{
    if (Npc_IsPlayer(other))
    {
        if (!Npc_IsDead(self))
        {
            if (C_NpcIsHuman(self))
            {
                Npc_ClearAIQueue(self);
                AI_StartState(self, ZS_PH_Rictusempra, 0, "");
            };
        };
    };
};

FUNC VOID ZS_PH_Rictusempra()
{
    B_FullStop(self);
    AI_PlayAni(self, "T_PSI_VICTIM");
    AI_Wait(self, 0.8);
    AI_PlayAni(self, "T_SEARCH");
};

FUNC INT ZS_PH_Rictusempra_Loop()
{
    if (Npc_GetStateTime(self) >= 4)
    {
        return LOOP_END;
    };
    AI_Wait(self, 0.5);
    return LOOP_CONTINUE;
};

FUNC VOID ZS_PH_Rictusempra_End()
{
    AI_ContinueRoutine(self);
};

// A simple custom AI state for the comic death.
// Gothic 1 has no ragdoll physics; this uses a known lightning-victim animation.
FUNC INT ZS_PH_AvadaDeath()
{
    Npc_ClearAIQueue(self);
    // Two existing Gothic 1 animations; future versions will add custom ones.
    if (Hlp_Random(100) < 50)
    {
        AI_PlayAni(self, "T_STAND_2_LIGHTNING_VICTIM");
    }
    else
    {
        AI_PlayAni(self, "T_PSI_VICTIM");
    };
    return LOOP_CONTINUE;
};

FUNC INT ZS_PH_AvadaDeath_Loop()
{
    if (Npc_GetStateTime(self) >= 2)
    {
        Npc_ChangeAttribute(self, ATR_HITPOINTS, -99999);
        return LOOP_END;
    };
    AI_Wait(self, 0.5);
    return LOOP_CONTINUE;
};

FUNC VOID ZS_PH_AvadaDeath_End()
{
};

// Invoked for Parry's Avada effect, guarded by the selected spell slot.
// The original Gothic BreathOfDeath damage is NOT reduced globally.
// Weak NPCs may die from the native effect before the comic animation;
// a truly independent spell implementation is needed to guarantee timing.
FUNC VOID PH_OnAvadaHit()
{
    if (Npc_IsPlayer(other))
    {
        if (!Npc_IsDead(self))
        {
            if (C_NpcIsHuman(self))
            {
                AI_StartState(self, ZS_PH_AvadaDeath, 0, "");
            }
            else
            {
                Npc_ChangeAttribute(self, ATR_HITPOINTS, -99999);
            };
        };
    };
};
