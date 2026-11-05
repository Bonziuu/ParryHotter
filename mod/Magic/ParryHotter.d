// Parry Hotter - Gothic 1 Daedalus prototype
// Keep this file after Spells_Params.d and before B_AssessMagic.d in Gothic.src.
// Uses built-in spell IDs and built-in rune models. No assets from Harry Potter games.

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
    TEXT[0] = "Prototyp: zamrozenie";
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
    TEXT[0] = "Silny odrzut";
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

// A simple custom AI state for the comic death.
// Gothic 1 has no ragdoll physics; this uses a known lightning-victim animation.
FUNC INT ZS_PH_AvadaDeath()
{
    Npc_ClearAIQueue(self);
    AI_PlayAni(self, "T_STAND_2_LIGHTNING_VICTIM");
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

// Called in B_AssessMagic_Breathofdeath when Parry is the caster.
// Low default Breath of Death damage (configured by installer) prevents
// early normal death before the comedy animation can finish.
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
