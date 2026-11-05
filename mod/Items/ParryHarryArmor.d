// Local-only Harry costume. The actual ASC and textures are NEVER stored
// in this repository. This instance is only injected by tools/visual_install.py
// after a complete local conversion has passed validation.
INSTANCE PH_ARMOR_HARRY(C_Item)
{
    name = "Szata Parry'ego";
    mainflag = ITEM_KAT_ARMOR;
    flags = 0;
    protection[PROT_EDGE] = 0;
    protection[PROT_BLUNT] = 0;
    protection[PROT_POINT] = 0;
    protection[PROT_FIRE] = 0;
    protection[PROT_MAGIC] = 0;
    wear = WEAR_TORSO;
    value = 1;
    // Inventory item uses an EXISTING Gothic I robe visual.
    visual = "kdfl.3ds";
    visual_change = "PH_HARRY.ASC";
    visual_skin = 0;
    material = MAT_LEATHER;
    description = name;
};
