static const SpriteID _bridge_land_below[] = {
	0xF8D, 0xFDD,0x11C6, 0xFDD,
};

static const SpriteID _bridge_transp_below[] = {
	0x3F3, 0x3F4, 0xF8D, 0xF8D, // without ice
	0x40D, 0x40E, 0x11C6, 0x11C6, // with ice
};

static const SpriteID _bridge_transp_overlay[] = {
	0, 0, 0x534, 0x535, // without ice
	0, 0, 0x547, 0x548, // with ice
};

static const PalSpriteID _bridge_sprite_table_2_0[] = {
	    0x9C3,     0x9C7,     0x9C9,       0x0,     0x9C4,     0x9C8,     0x9CA,       0x0,
	    0x9C5,     0x9C7,     0x9C9,       0x0,     0x9C6,     0x9C8,     0x9CA,       0x0,
	   0x10E4,     0x9C7,     0x9C9,       0x0,    0x10E5,     0x9C8,     0x9CA,       0x0,
	   0x110C,     0x9C7,     0x9C9,       0x0,    0x110D,     0x9C8,     0x9CA,       0x0,
};

static const PalSpriteID _bridge_sprite_table_2_1[] = {
	    0x986,     0x988,     0x985,     0x987,     0x98A,     0x98C,     0x989,     0x98B,
	0x31D898E, 0x31D8990, 0x31D898D, 0x31D898F, 0x31D8992, 0x31D8994, 0x31D8991, 0x31D8993,
	0x31D90E7, 0x31D90E9, 0x31D90E6, 0x31D90E8, 0x31D90EB, 0x31D90ED, 0x31D90EA, 0x31D90EC,
	0x31D910F, 0x31D9111, 0x31D910E, 0x31D9110, 0x31D9113, 0x31D9115, 0x31D9112, 0x31D9114,
};

static const PalSpriteID _bridge_sprite_table_2_poles[] = {
	SPR_OPENTTD_BASE + 36 + 6*3,
	SPR_OPENTTD_BASE + 36 + 6*3,
	SPR_OPENTTD_BASE + 36 + 6*3,
	SPR_OPENTTD_BASE + 36 + 6*3,
	SPR_OPENTTD_BASE + 38 + 6*3,
	0x0,

	SPR_OPENTTD_BASE + 33 + 6*3,
	SPR_OPENTTD_BASE + 33 + 6*3,
	SPR_OPENTTD_BASE + 33 + 6*3,
	SPR_OPENTTD_BASE + 33 + 6*3,
	SPR_OPENTTD_BASE + 35 + 6*3,
	0x0,

	0x0,
	0x0,
};

static const PalSpriteID _bridge_sprite_table_4_0[] = {
	    0x9A9,     0x99F,     0x9B1,       0x0,     0x9A5,     0x997,     0x9AD,       0x0,
	    0x99D,     0x99F,     0x9B1,       0x0,     0x995,     0x997,     0x9AD,       0x0,
	   0x10F2,     0x99F,     0x9B1,       0x0,    0x10EE,     0x997,     0x9AD,       0x0,
	   0x111A,     0x99F,     0x9B1,       0x0,    0x1116,     0x997,     0x9AD,       0x0,
};

static const PalSpriteID _bridge_sprite_table_4_1[] = {
	    0x9AA,     0x9A0,     0x9B2,       0x0,     0x9A6,     0x998,     0x9AE,       0x0,
	    0x99E,     0x9A0,     0x9B2,       0x0,     0x996,     0x998,     0x9AE,       0x0,
	   0x10F3,     0x9A0,     0x9B2,       0x0,    0x10EF,     0x998,     0x9AE,       0x0,
	   0x111B,     0x9A0,     0x9B2,       0x0,    0x1117,     0x998,     0x9AE,       0x0,
};

static const PalSpriteID _bridge_sprite_table_4_2[] = {
	    0x9AC,     0x9A4,     0x9B4,       0x0,     0x9A8,     0x99C,     0x9B0,       0x0,
	    0x9A2,     0x9A4,     0x9B4,       0x0,     0x99A,     0x99C,     0x9B0,       0x0,
	   0x10F5,     0x9A4,     0x9B4,       0x0,    0x10F1,     0x99C,     0x9B0,       0x0,
	   0x111D,     0x9A4,     0x9B4,       0x0,    0x1119,     0x99C,     0x9B0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_4_3[] = {
	    0x9AB,     0x9A3,     0x9B3,       0x0,     0x9A7,     0x99B,     0x9AF,       0x0,
	    0x9A1,     0x9A3,     0x9B3,       0x0,     0x999,     0x99B,     0x9AF,       0x0,
	   0x10F4,     0x9A3,     0x9B3,       0x0,    0x10F0,     0x99B,     0x9AF,       0x0,
	   0x111C,     0x9A3,     0x9B3,       0x0,    0x1118,     0x99B,     0x9AF,       0x0,
};

static const PalSpriteID _bridge_sprite_table_4_4[] = {
	    0x9B6,     0x9BA,     0x9BC,       0x0,     0x9B5,     0x9B9,     0x9BB,       0x0,
	    0x9B8,     0x9BA,     0x9BC,       0x0,     0x9B7,     0x9B9,     0x9BB,       0x0,
	   0x10F7,     0x9BA,     0x9BC,       0x0,    0x10F6,     0x9B9,     0x9BB,       0x0,
	   0x111F,     0x9BA,     0x9BC,       0x0,    0x111E,     0x9B9,     0x9BB,       0x0,
};

static const PalSpriteID _bridge_sprite_table_4_5[] = {
	    0x9BD,     0x9C1,       0x0,       0x0,     0x9BE,     0x9C2,       0x0,       0x0,
	    0x9BF,     0x9C1,       0x0,       0x0,     0x9C0,     0x9C2,       0x0,       0x0,
	   0x10F8,     0x9C1,       0x0,       0x0,    0x10F9,     0x9C2,       0x0,       0x0,
	   0x1120,     0x9C1,       0x0,       0x0,    0x1121,     0x9C2,       0x0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_4_6[] = {
	    0x986,     0x988,     0x985,     0x987,     0x98A,     0x98C,     0x989,     0x98B,
	    0x98E,     0x990,     0x98D,     0x98F,     0x992,     0x994,     0x991,     0x993,
	   0x10E7,    0x10E9,    0x10E6,    0x10E8,    0x10EB,    0x10ED,    0x10EA,    0x10EC,
	   0x110F,    0x1111,    0x110E,    0x1110,    0x1113,    0x1115,    0x1112,    0x1114,
};

static const PalSpriteID _bridge_sprite_table_4_poles[] = {
	SPR_OPENTTD_BASE + 36,
	SPR_OPENTTD_BASE + 37,
	SPR_OPENTTD_BASE + 37,
	SPR_OPENTTD_BASE + 36,
	SPR_OPENTTD_BASE + 38,
	0x0,

	SPR_OPENTTD_BASE + 33,
	SPR_OPENTTD_BASE + 34,
	SPR_OPENTTD_BASE + 34,
	SPR_OPENTTD_BASE + 33,
	SPR_OPENTTD_BASE + 35,
	0x0,

	0x0,
	0x0,
};

static const PalSpriteID _bridge_sprite_table_5_0[] = {
	0x32189A9, 0x321899F, 0x32189B1,       0x0, 0x32189A5, 0x3218997, 0x32189AD,       0x0,
	0x321899D, 0x321899F, 0x32189B1,       0x0, 0x3218995, 0x3218997, 0x32189AD,       0x0,
	0x32190F2, 0x321899F, 0x32189B1,       0x0, 0x32190EE, 0x3218997, 0x32189AD,       0x0,
	0x321911A, 0x321899F, 0x32189B1,       0x0, 0x3219116, 0x3218997, 0x32189AD,       0x0,
	SPR_OPENTTD_BASE + 35,
};

static const PalSpriteID _bridge_sprite_table_5_1[] = {
	0x32189AA, 0x32189A0, 0x32189B2,       0x0, 0x32189A6, 0x3218998, 0x32189AE,       0x0,
	0x321899E, 0x32189A0, 0x32189B2,       0x0, 0x3218996, 0x3218998, 0x32189AE,       0x0,
	0x32190F3, 0x32189A0, 0x32189B2,       0x0, 0x32190EF, 0x3218998, 0x32189AE,       0x0,
	0x321911B, 0x32189A0, 0x32189B2,       0x0, 0x3219117, 0x3218998, 0x32189AE,       0x0,
	SPR_OPENTTD_BASE + 36,
};

static const PalSpriteID _bridge_sprite_table_5_2[] = {
	0x32189AC, 0x32189A4, 0x32189B4,       0x0, 0x32189A8, 0x321899C, 0x32189B0,       0x0,
	0x32189A2, 0x32189A4, 0x32189B4,       0x0, 0x321899A, 0x321899C, 0x32189B0,       0x0,
	0x32190F5, 0x32189A4, 0x32189B4,       0x0, 0x32190F1, 0x321899C, 0x32189B0,       0x0,
	0x321911D, 0x32189A4, 0x32189B4,       0x0, 0x3219119, 0x321899C, 0x32189B0,       0x0,
	SPR_OPENTTD_BASE + 36,
};

static const PalSpriteID _bridge_sprite_table_5_3[] = {
	0x32189AB, 0x32189A3, 0x32189B3,       0x0, 0x32189A7, 0x321899B, 0x32189AF,       0x0,
	0x32189A1, 0x32189A3, 0x32189B3,       0x0, 0x3218999, 0x321899B, 0x32189AF,       0x0,
	0x32190F4, 0x32189A3, 0x32189B3,       0x0, 0x32190F0, 0x321899B, 0x32189AF,       0x0,
	0x321911C, 0x32189A3, 0x32189B3,       0x0, 0x3219118, 0x321899B, 0x32189AF,       0x0,
	SPR_OPENTTD_BASE + 35,
};

static const PalSpriteID _bridge_sprite_table_5_4[] = {
	0x32189B6, 0x32189BA, 0x32189BC,       0x0, 0x32189B5, 0x32189B9, 0x32189BB,       0x0,
	0x32189B8, 0x32189BA, 0x32189BC,       0x0, 0x32189B7, 0x32189B9, 0x32189BB,       0x0,
	0x32190F7, 0x32189BA, 0x32189BC,       0x0, 0x32190F6, 0x32189B9, 0x32189BB,       0x0,
	0x321911F, 0x32189BA, 0x32189BC,       0x0, 0x321911E, 0x32189B9, 0x32189BB,       0x0,
	SPR_OPENTTD_BASE+38,  0x0, 0x0,		   0x0, SPR_OPENTTD_BASE + 37,
};

static const PalSpriteID _bridge_sprite_table_5_5[] = {
	0x32189BD, 0x32189C1,       0x0,       0x0, 0x32189BE, 0x32189C2,       0x0,       0x0,
	0x32189BF, 0x32189C1,       0x0,       0x0, 0x32189C0, 0x32189C2,       0x0,       0x0,
	0x32190F8, 0x32189C1,       0x0,       0x0, 0x32190F9, 0x32189C2,       0x0,       0x0,
	0x3219120, 0x32189C1,       0x0,       0x0, 0x3219121, 0x32189C2,       0x0,       0x0,
	0x0, SPR_OPENTTD_BASE + 35,
};

static const PalSpriteID _bridge_sprite_table_5_6[] = {
	    0x986,     0x988,     0x985,     0x987,     0x98A,     0x98C,     0x989,     0x98B,
	0x321898E, 0x3218990, 0x321898D, 0x321898F, 0x3218992, 0x3218994, 0x3218991, 0x3218993,
	0x32190E7, 0x32190E9, 0x32190E6, 0x32190E8, 0x32190EB, 0x32190ED, 0x32190EA, 0x32190EC,
	0x321910F, 0x3219111, 0x321910E, 0x3219110, 0x3219113, 0x3219115, 0x3219112, 0x3219114,
	0x0, SPR_OPENTTD_BASE + 35,
};

static const PalSpriteID _bridge_sprite_table_5_poles[] = {
	SPR_OPENTTD_BASE + 36 + 0x3218000,
	SPR_OPENTTD_BASE + 37 + 0x3218000,
	SPR_OPENTTD_BASE + 37 + 0x3218000,
	SPR_OPENTTD_BASE + 36 + 0x3218000,
	SPR_OPENTTD_BASE + 38 + 0x3218000,
	0x0,

	SPR_OPENTTD_BASE + 33 + 0x3218000,
	SPR_OPENTTD_BASE + 34 + 0x3218000,
	SPR_OPENTTD_BASE + 34 + 0x3218000,
	SPR_OPENTTD_BASE + 33 + 0x3218000,
	SPR_OPENTTD_BASE + 35 + 0x3218000,
	0x0,

	0x0,
	0x0,
};

static const PalSpriteID _bridge_sprite_table_3_0[] = {
	0x32089A9, 0x320899F, 0x32089B1,       0x0, 0x32089A5, 0x3208997, 0x32089AD,       0x0,
	0x320899D, 0x320899F, 0x32089B1,       0x0, 0x3208995, 0x3208997, 0x32089AD,       0x0,
	0x32090F2, 0x320899F, 0x32089B1,       0x0, 0x32090EE, 0x3208997, 0x32089AD,       0x0,
	0x320911A, 0x320899F, 0x32089B1,       0x0, 0x3209116, 0x3208997, 0x32089AD,       0x0,
};

static const PalSpriteID _bridge_sprite_table_3_1[] = {
	0x32089AA, 0x32089A0, 0x32089B2,       0x0, 0x32089A6, 0x3208998, 0x32089AE,       0x0,
	0x320899E, 0x32089A0, 0x32089B2,       0x0, 0x3208996, 0x3208998, 0x32089AE,       0x0,
	0x32090F3, 0x32089A0, 0x32089B2,       0x0, 0x32090EF, 0x3208998, 0x32089AE,       0x0,
	0x320911B, 0x32089A0, 0x32089B2,       0x0, 0x3209117, 0x3208998, 0x32089AE,       0x0,
};

static const PalSpriteID _bridge_sprite_table_3_2[] = {
	0x32089AC, 0x32089A4, 0x32089B4,       0x0, 0x32089A8, 0x320899C, 0x32089B0,       0x0,
	0x32089A2, 0x32089A4, 0x32089B4,       0x0, 0x320899A, 0x320899C, 0x32089B0,       0x0,
	0x32090F5, 0x32089A4, 0x32089B4,       0x0, 0x32090F1, 0x320899C, 0x32089B0,       0x0,
	0x320911D, 0x32089A4, 0x32089B4,       0x0, 0x3209119, 0x320899C, 0x32089B0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_3_3[] = {
	0x32089AB, 0x32089A3, 0x32089B3,       0x0, 0x32089A7, 0x320899B, 0x32089AF,       0x0,
	0x32089A1, 0x32089A3, 0x32089B3,       0x0, 0x3208999, 0x320899B, 0x32089AF,       0x0,
	0x32090F4, 0x32089A3, 0x32089B3,       0x0, 0x32090F0, 0x320899B, 0x32089AF,       0x0,
	0x320911C, 0x32089A3, 0x32089B3,       0x0, 0x3209118, 0x320899B, 0x32089AF,       0x0,
};

static const PalSpriteID _bridge_sprite_table_3_4[] = {
	0x32089B6, 0x32089BA, 0x32089BC,       0x0, 0x32089B5, 0x32089B9, 0x32089BB,       0x0,
	0x32089B8, 0x32089BA, 0x32089BC,       0x0, 0x32089B7, 0x32089B9, 0x32089BB,       0x0,
	0x32090F7, 0x32089BA, 0x32089BC,       0x0, 0x32090F6, 0x32089B9, 0x32089BB,       0x0,
	0x320911F, 0x32089BA, 0x32089BC,       0x0, 0x320911E, 0x32089B9, 0x32089BB,       0x0,
};

static const PalSpriteID _bridge_sprite_table_3_5[] = {
	0x32089BD, 0x32089C1,       0x0,       0x0, 0x32089BE, 0x32089C2,       0x0,       0x0,
	0x32089BF, 0x32089C1,       0x0,       0x0, 0x32089C0, 0x32089C2,       0x0,       0x0,
	0x32090F8, 0x32089C1,       0x0,       0x0, 0x32090F9, 0x32089C2,       0x0,       0x0,
	0x3209120, 0x32089C1,       0x0,       0x0, 0x3209121, 0x32089C2,       0x0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_3_6[] = {
	    0x986,     0x988,     0x985,     0x987,     0x98A,     0x98C,     0x989,     0x98B,
	0x320898E, 0x3208990, 0x320898D, 0x320898F, 0x3208992, 0x3208994, 0x3208991, 0x3208993,
	0x32090E7, 0x32090E9, 0x32090E6, 0x32090E8, 0x32090EB, 0x32090ED, 0x32090EA, 0x32090EC,
	0x320910F, 0x3209111, 0x320910E, 0x3209110, 0x3209113, 0x3209115, 0x3209112, 0x3209114,
	0x0, SPR_OPENTTD_BASE + 35,
};

static const PalSpriteID _bridge_sprite_table_3_poles[] = {
	SPR_OPENTTD_BASE + 36 + 0x3208000,
	SPR_OPENTTD_BASE + 37 + 0x3208000,
	SPR_OPENTTD_BASE + 37 + 0x3208000,
	SPR_OPENTTD_BASE + 36 + 0x3208000,
	SPR_OPENTTD_BASE + 38 + 0x3208000,
	0x0,

	SPR_OPENTTD_BASE + 33 + 0x3208000,
	SPR_OPENTTD_BASE + 34 + 0x3208000,
	SPR_OPENTTD_BASE + 34 + 0x3208000,
	SPR_OPENTTD_BASE + 33 + 0x3208000,
	SPR_OPENTTD_BASE + 35 + 0x3208000,
	0x0,

	0x0,
	0x0,
};

static const PalSpriteID _bridge_sprite_table_1_1[] = {
	    0x986,     0x988,     0x985,     0x987,     0x98A,     0x98C,     0x989,     0x98B,
	0x31E898E, 0x31E8990, 0x31E898D, 0x31E898F, 0x31E8992, 0x31E8994, 0x31E8991, 0x31E8993,
	0x31E90E7, 0x31E90E9, 0x31E90E6, 0x31E90E8, 0x31E90EB, 0x31E90ED, 0x31E90EA, 0x31E90EC,
	0x31E910F, 0x31E9111, 0x31E910E, 0x31E9110, 0x31E9113, 0x31E9115, 0x31E9112, 0x31E9114,
};

static const PalSpriteID _bridge_sprite_table_1_poles[] = {
	SPR_OPENTTD_BASE + 36 + 6*3,
	SPR_OPENTTD_BASE + 37 + 6*3,
	SPR_OPENTTD_BASE + 37 + 6*3,
	SPR_OPENTTD_BASE + 36 + 6*3,
	SPR_OPENTTD_BASE + 38 + 6*3,
	0x0,

	SPR_OPENTTD_BASE + 33 + 6*3,
	SPR_OPENTTD_BASE + 34 + 6*3,
	SPR_OPENTTD_BASE + 34 + 6*3,
	SPR_OPENTTD_BASE + 33 + 6*3,
	SPR_OPENTTD_BASE + 35 + 6*3,
	0x0,

	0x0,
	0x0,
};


static const PalSpriteID _bridge_sprite_table_6_0[] = {
	    0x9CD,     0x9D9,       0x0,       0x0,     0x9CE,     0x9DA,       0x0,       0x0,
	    0x9D3,     0x9D9,       0x0,       0x0,     0x9D4,     0x9DA,       0x0,       0x0,
	   0x10FC,     0x9D9,       0x0,       0x0,    0x10FD,     0x9DA,       0x0,       0x0,
	   0x1124,     0x9D9,       0x0,       0x0,    0x1125,     0x9DA,       0x0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_6_1[] = {
	    0x9CB,     0x9D7,     0x9DD,       0x0,     0x9D0,     0x9DC,     0x9E0,       0x0,
	    0x9D1,     0x9D7,     0x9DD,       0x0,     0x9D6,     0x9DC,     0x9E0,       0x0,
	   0x10FA,     0x9D7,     0x9DD,       0x0,    0x10FF,     0x9DC,     0x9E0,       0x0,
	   0x1122,     0x9D7,     0x9DD,       0x0,    0x1127,     0x9DC,     0x9E0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_6_2[] = {
	    0x9CC,     0x9D8,     0x9DE,       0x0,     0x9CF,     0x9DB,     0x9DF,       0x0,
	    0x9D2,     0x9D8,     0x9DE,       0x0,     0x9D5,     0x9DB,     0x9DF,       0x0,
	   0x10FB,     0x9D8,     0x9DE,       0x0,    0x10FE,     0x9DB,     0x9DF,       0x0,
	   0x1123,     0x9D8,     0x9DE,       0x0,    0x1126,     0x9DB,     0x9DF,       0x0,
};

static const PalSpriteID _bridge_sprite_table_6_3[] = {
	    0x986,     0x988,     0x985,     0x987,     0x98A,     0x98C,     0x989,     0x98B,
	    0x98E,     0x990,     0x98D,     0x98F,     0x992,     0x994,     0x991,     0x993,
	   0x10E7,    0x10E9,    0x10E6,    0x10E8,    0x10EB,    0x10ED,    0x10EA,    0x10EC,
	   0x110F,    0x1111,    0x110E,    0x1110,    0x1113,    0x1115,    0x1112,    0x1114,
};

static const PalSpriteID _bridge_sprite_table_6_poles[] = {
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,

	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,

	2526,
	2528,
};


static const PalSpriteID _bridge_sprite_table_7_0[] = {
	0x31C89CD, 0x31C89D9,       0x0,       0x0, 0x31C89CE, 0x31C89DA,       0x0,       0x0,
	0x31C89D3, 0x31C89D9,       0x0,       0x0, 0x31C89D4, 0x31C89DA,       0x0,       0x0,
	0x31C90FC, 0x31C89D9,       0x0,       0x0, 0x31C90FD, 0x31C89DA,       0x0,       0x0,
	0x31C9124, 0x31C89D9,       0x0,       0x0, 0x31C9125, 0x31C89DA,       0x0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_7_1[] = {
	0x31C89CB, 0x31C89D7, 0x31C89DD,       0x0, 0x31C89D0, 0x31C89DC, 0x31C89E0,       0x0,
	0x31C89D1, 0x31C89D7, 0x31C89DD,       0x0, 0x31C89D6, 0x31C89DC, 0x31C89E0,       0x0,
	0x31C90FA, 0x31C89D7, 0x31C89DD,       0x0, 0x31C90FF, 0x31C89DC, 0x31C89E0,       0x0,
	0x31C9122, 0x31C89D7, 0x31C89DD,       0x0, 0x31C9127, 0x31C89DC, 0x31C89E0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_7_2[] = {
	0x31C89CC, 0x31C89D8, 0x31C89DE,       0x0, 0x31C89CF, 0x31C89DB, 0x31C89DF,       0x0,
	0x31C89D2, 0x31C89D8, 0x31C89DE,       0x0, 0x31C89D5, 0x31C89DB, 0x31C89DF,       0x0,
	0x31C90FB, 0x31C89D8, 0x31C89DE,       0x0, 0x31C90FE, 0x31C89DB, 0x31C89DF,       0x0,
	0x31C9123, 0x31C89D8, 0x31C89DE,       0x0, 0x31C9126, 0x31C89DB, 0x31C89DF,       0x0,
};

static const PalSpriteID _bridge_sprite_table_7_3[] = {
	    0x986,     0x988,     0x985,     0x987,     0x98A,     0x98C,     0x989,     0x98B,
	0x31C898E, 0x31C8990, 0x31C898D, 0x31C898F, 0x31C8992, 0x31C8994, 0x31C8991, 0x31C8993,
	0x31C90E7, 0x31C90E9, 0x31C90E6, 0x31C90E8, 0x31C90EB, 0x31C90ED, 0x31C90EA, 0x31C90EC,
	0x31C910F, 0x31C9111, 0x31C910E, 0x31C9110, 0x31C9113, 0x31C9115, 0x31C9112, 0x31C9114,
};

static const PalSpriteID _bridge_sprite_table_7_poles[] = {
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,

	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,

	2526,
	2528,
};

static const PalSpriteID _bridge_sprite_table_8_0[] = {
	0x31E89CD, 0x31E89D9,       0x0,       0x0, 0x31E89CE, 0x31E89DA,       0x0,       0x0,
	0x31E89D3, 0x31E89D9,       0x0,       0x0, 0x31E89D4, 0x31E89DA,       0x0,       0x0,
	0x31E90FC, 0x31E89D9,       0x0,       0x0, 0x31E90FD, 0x31E89DA,       0x0,       0x0,
	0x31E9124, 0x31E89D9,       0x0,       0x0, 0x31E9125, 0x31E89DA,       0x0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_8_1[] = {
	0x31E89CB, 0x31E89D7, 0x31E89DD,       0x0, 0x31E89D0, 0x31E89DC, 0x31E89E0,       0x0,
	0x31E89D1, 0x31E89D7, 0x31E89DD,       0x0, 0x31E89D6, 0x31E89DC, 0x31E89E0,       0x0,
	0x31E90FA, 0x31E89D7, 0x31E89DD,       0x0, 0x31E90FF, 0x31E89DC, 0x31E89E0,       0x0,
	0x31E9122, 0x31E89D7, 0x31E89DD,       0x0, 0x31E9127, 0x31E89DC, 0x31E89E0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_8_2[] = {
	0x31E89CC, 0x31E89D8, 0x31E89DE,       0x0, 0x31E89CF, 0x31E89DB, 0x31E89DF,       0x0,
	0x31E89D2, 0x31E89D8, 0x31E89DE,       0x0, 0x31E89D5, 0x31E89DB, 0x31E89DF,       0x0,
	0x31E90FB, 0x31E89D8, 0x31E89DE,       0x0, 0x31E90FE, 0x31E89DB, 0x31E89DF,       0x0,
	0x31E9123, 0x31E89D8, 0x31E89DE,       0x0, 0x31E9126, 0x31E89DB, 0x31E89DF,       0x0,
};

static const PalSpriteID _bridge_sprite_table_8_3[] = {
	    0x986,     0x988,     0x985,     0x987,     0x98A,     0x98C,     0x989,     0x98B,
	0x31E898E, 0x31E8990, 0x31E898D, 0x31E898F, 0x31E8992, 0x31E8994, 0x31E8991, 0x31E8993,
	0x31E90E7, 0x31E90E9, 0x31E90E6, 0x31E90E8, 0x31E90EB, 0x31E90ED, 0x31E90EA, 0x31E90EC,
	0x31E910F, 0x31E9111, 0x31E910E, 0x31E9110, 0x31E9113, 0x31E9115, 0x31E9112, 0x31E9114,
};

static const PalSpriteID _bridge_sprite_table_8_poles[] = {
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,

	0x0,
	0x0,
	0x0,
	0x0,
	0x0,
	0x0,

	2526,
	2528,
};

static const PalSpriteID _bridge_sprite_table_0_0[] = {
	    0x9F2,     0x9F6,     0x9F8,       0x0,     0x9F1,     0x9F5,     0x9F7,       0x0,
	    0x9F4,     0x9F6,     0x9F8,       0x0,     0x9F3,     0x9F5,     0x9F7,       0x0,
	   0x1109,     0x9F6,     0x9F8,       0x0,    0x1108,     0x9F5,     0x9F7,       0x0,
	   0x1131,     0x9F6,     0x9F8,       0x0,    0x1130,     0x9F5,     0x9F7,       0x0,
};

static const PalSpriteID _bridge_sprite_table_0_1[] = {
	    0x9EE,     0x9ED,     0x9F0,     0x9EF,     0x9EA,     0x9E9,     0x9EB,     0x9EC,
	    0x9E6,     0x9E5,     0x9E8,     0x9E7,     0x9E2,     0x9E1,     0x9E3,     0x9E4,
	   0x1105,    0x1104,    0x1107,    0x1106,    0x1101,    0x1100,    0x1102,    0x1103,
	   0x112D,    0x112C,    0x112F,    0x112E,    0x1129,    0x1128,    0x112A,    0x112B,
};

static const PalSpriteID _bridge_sprite_table_0_poles[] = {
	SPR_OPENTTD_BASE + 42,
	SPR_OPENTTD_BASE + 42,
	SPR_OPENTTD_BASE + 42,
	SPR_OPENTTD_BASE + 42,
	SPR_OPENTTD_BASE + 44,
	0x0,

	SPR_OPENTTD_BASE + 39,
	SPR_OPENTTD_BASE + 39,
	SPR_OPENTTD_BASE + 39,
	SPR_OPENTTD_BASE + 39,
	SPR_OPENTTD_BASE + 41,
	0x0,

	0x0,
	0x0,
};


static const PalSpriteID _bridge_sprite_table_1_0[] = {
	0x31E89BD, 0x31E89C1,     0x9C9,       0x0, 0x31E89BE, 0x31E89C2,     0x9CA,       0x0,
	0x31E89BF, 0x31E89C1,     0x9C9,       0x0, 0x31E89C0, 0x31E89C2,     0x9CA,       0x0,
	0x31E90F8, 0x31E89C1,     0x9C9,       0x0, 0x31E90F9, 0x31E89C2,     0x9CA,       0x0,
	0x31E9120, 0x31E89C1,     0x9C9,       0x0, 0x31E9121, 0x31E89C2,     0x9CA,       0x0,
};

static const PalSpriteID _bridge_sprite_table_9_0[] = {
	    0x9F9,     0x9FD,     0x9C9,       0x0,     0x9FA,     0x9FE,     0x9CA,       0x0,
	    0x9FB,     0x9FD,     0x9C9,       0x0,     0x9FC,     0x9FE,     0x9CA,       0x0,
	   0x110A,     0x9FD,     0x9C9,       0x0,    0x110B,     0x9FE,     0x9CA,       0x0,
	   0x1132,     0x9FD,     0x9C9,       0x0,    0x1133,     0x9FE,     0x9CA,       0x0,
};

static const PalSpriteID _bridge_sprite_table_10_0[] = {
	    0xA0B,     0xA01,       0x0,       0x0,     0xA0C,     0xA02,       0x0,       0x0,
	    0xA11,     0xA01,       0x0,       0x0,     0xA12,     0xA02,       0x0,       0x0,
	    0xA17,     0xA01,       0x0,       0x0,     0xA18,     0xA02,       0x0,       0x0,
	    0xA1D,     0xA01,       0x0,       0x0,     0xA1E,     0xA02,       0x0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_10_1[] = {
	    0xA09,     0x9FF,     0xA05,       0x0,     0xA0E,     0xA04,     0xA08,       0x0,
	    0xA0F,     0x9FF,     0xA05,       0x0,     0xA14,     0xA04,     0xA08,       0x0,
	    0xA15,     0x9FF,     0xA05,       0x0,     0xA1A,     0xA04,     0xA08,       0x0,
	    0xA1B,     0x9FF,     0xA05,       0x0,     0xA20,     0xA04,     0xA08,       0x0,
};

static const PalSpriteID _bridge_sprite_table_10_2[] = {
	    0xA0A,     0xA00,     0xA06,       0x0,     0xA0D,     0xA03,     0xA07,       0x0,
	    0xA10,     0xA00,     0xA06,       0x0,     0xA13,     0xA03,     0xA07,       0x0,
	    0xA16,     0xA00,     0xA06,       0x0,     0xA19,     0xA03,     0xA07,       0x0,
	    0xA1C,     0xA00,     0xA06,       0x0,     0xA1F,     0xA03,     0xA07,       0x0,
};

static const PalSpriteID _bridge_sprite_table_10_poles[] = {
	SPR_OPENTTD_BASE + 36 + 2*6,
	SPR_OPENTTD_BASE + 36 + 2*6,
	SPR_OPENTTD_BASE + 36 + 2*6,
	SPR_OPENTTD_BASE + 36 + 2*6,
	SPR_OPENTTD_BASE + 38 + 2*6,
	0x0,

	SPR_OPENTTD_BASE + 33 + 2*6,
	SPR_OPENTTD_BASE + 33 + 2*6,
	SPR_OPENTTD_BASE + 33 + 2*6,
	SPR_OPENTTD_BASE + 33 + 2*6,
	SPR_OPENTTD_BASE + 35 + 2*6,
	0x0,

	0x0,
	0x0,
};

static const PalSpriteID _bridge_sprite_table_11_0[] = {
    0x3218A0B,     0x3218A01,       0x0,       0x0,     0x3218A0C,     0x3218A02,       0x0,       0x0,
    0x3218A11,     0x3218A01,       0x0,       0x0,     0x3218A12,     0x3218A02,       0x0,       0x0,
    0x3218A17,     0x3218A01,       0x0,       0x0,     0x3218A18,     0x3218A02,       0x0,       0x0,
    0x3218A1D,     0x3218A01,       0x0,       0x0,     0x3218A1E,     0x3218A02,       0x0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_11_1[] = {
    0x3218A09,     0x32189FF,     0x3218A05,       0x0,     0x3218A0E,     0x3218A04,     0x3218A08,       0x0,
    0x3218A0F,     0x32189FF,     0x3218A05,       0x0,     0x3218A14,     0x3218A04,     0x3218A08,       0x0,
    0x3218A15,     0x32189FF,     0x3218A05,       0x0,     0x3218A1A,     0x3218A04,     0x3218A08,       0x0,
    0x3218A1B,     0x32189FF,     0x3218A05,       0x0,     0x3218A20,     0x3218A04,     0x3218A08,       0x0,
};

static const PalSpriteID _bridge_sprite_table_11_2[] = {
    0x3218A0A,     0x3218A00,     0x3218A06,       0x0,     0x3218A0D,     0x3218A03,     0x3218A07,       0x0,
    0x3218A10,     0x3218A00,     0x3218A06,       0x0,     0x3218A13,     0x3218A03,     0x3218A07,       0x0,
    0x3218A16,     0x3218A00,     0x3218A06,       0x0,     0x3218A19,     0x3218A03,     0x3218A07,       0x0,
    0x3218A1C,     0x3218A00,     0x3218A06,       0x0,     0x3218A1F,     0x3218A03,     0x3218A07,       0x0,
};

static const PalSpriteID _bridge_sprite_table_11_poles[] = {
	SPR_OPENTTD_BASE + 36 + 2*6 + 0x3218000,
	SPR_OPENTTD_BASE + 36 + 2*6 + 0x3218000,
	SPR_OPENTTD_BASE + 36 + 2*6 + 0x3218000,
	SPR_OPENTTD_BASE + 36 + 2*6 + 0x3218000,
	SPR_OPENTTD_BASE + 38 + 2*6 + 0x3218000,
	0x0,

	SPR_OPENTTD_BASE + 33 + 2*6 + 0x3218000,
	SPR_OPENTTD_BASE + 33 + 2*6 + 0x3218000,
	SPR_OPENTTD_BASE + 33 + 2*6 + 0x3218000,
	SPR_OPENTTD_BASE + 33 + 2*6 + 0x3218000,
	SPR_OPENTTD_BASE + 35 + 2*6 + 0x3218000,
	0x0,

	0x0,
	0x0,
};

static const PalSpriteID _bridge_sprite_table_12_0[] = {
    0x3238A0B,     0x3238A01,       0x0,       0x0,     0x3238A0C,     0x3238A02,       0x0,       0x0,
    0x3238A11,     0x3238A01,       0x0,       0x0,     0x3238A12,     0x3238A02,       0x0,       0x0,
    0x3238A17,     0x3238A01,       0x0,       0x0,     0x3238A18,     0x3238A02,       0x0,       0x0,
    0x3238A1D,     0x3238A01,       0x0,       0x0,     0x3238A1E,     0x3238A02,       0x0,       0x0,
};

static const PalSpriteID _bridge_sprite_table_12_1[] = {
    0x3238A09,     0x32389FF,     0x3238A05,       0x0,     0x3238A0E,     0x3238A04,     0x3238A08,       0x0,
    0x3238A0F,     0x32389FF,     0x3238A05,       0x0,     0x3238A14,     0x3238A04,     0x3238A08,       0x0,
    0x3238A15,     0x32389FF,     0x3238A05,       0x0,     0x3238A1A,     0x3238A04,     0x3238A08,       0x0,
    0x3238A1B,     0x32389FF,     0x3238A05,       0x0,     0x3238A20,     0x3238A04,     0x3238A08,       0x0,
};

static const PalSpriteID _bridge_sprite_table_12_2[] = {
    0x3238A0A,     0x3238A00,     0x3238A06,       0x0,     0x3238A0D,     0x3238A03,     0x3238A07,       0x0,
    0x3238A10,     0x3238A00,     0x3238A06,       0x0,     0x3238A13,     0x3238A03,     0x3238A07,       0x0,
    0x3238A16,     0x3238A00,     0x3238A06,       0x0,     0x3238A19,     0x3238A03,     0x3238A07,       0x0,
    0x3238A1C,     0x3238A00,     0x3238A06,       0x0,     0x3238A1F,     0x3238A03,     0x3238A07,       0x0,
};

static const PalSpriteID _bridge_sprite_table_12_poles[] = {
	SPR_OPENTTD_BASE + 36 + 2*6 + 0x3238000,
	SPR_OPENTTD_BASE + 36 + 2*6 + 0x3238000,
	SPR_OPENTTD_BASE + 36 + 2*6 + 0x3238000,
	SPR_OPENTTD_BASE + 36 + 2*6 + 0x3238000,
	SPR_OPENTTD_BASE + 38 + 2*6 + 0x3238000,
	0x0,

	SPR_OPENTTD_BASE + 33 + 2*6 + 0x3238000,
	SPR_OPENTTD_BASE + 33 + 2*6 + 0x3238000,
	SPR_OPENTTD_BASE + 33 + 2*6 + 0x3238000,
	SPR_OPENTTD_BASE + 33 + 2*6 + 0x3238000,
	SPR_OPENTTD_BASE + 35 + 2*6 + 0x3238000,
	0x0,

	0x0,
	0x0,
};

static const uint32 * const _bridge_sprite_table_2[] = {
	_bridge_sprite_table_2_0,
	_bridge_sprite_table_2_0,
	_bridge_sprite_table_2_0,
	_bridge_sprite_table_2_0,
	_bridge_sprite_table_2_0,
	_bridge_sprite_table_2_0,
	_bridge_sprite_table_2_1,
};

static const uint32 * const _bridge_sprite_table_4[] = {
	_bridge_sprite_table_4_0,
	_bridge_sprite_table_4_1,
	_bridge_sprite_table_4_2,
	_bridge_sprite_table_4_3,
	_bridge_sprite_table_4_4,
	_bridge_sprite_table_4_5,
	_bridge_sprite_table_4_6,
};

static const uint32 * const _bridge_sprite_table_5[] = {
	_bridge_sprite_table_5_0,
	_bridge_sprite_table_5_1,
	_bridge_sprite_table_5_2,
	_bridge_sprite_table_5_3,
	_bridge_sprite_table_5_4,
	_bridge_sprite_table_5_5,
	_bridge_sprite_table_5_6,
};

static const uint32 * const _bridge_sprite_table_3[] = {
	_bridge_sprite_table_3_0,
	_bridge_sprite_table_3_1,
	_bridge_sprite_table_3_2,
	_bridge_sprite_table_3_3,
	_bridge_sprite_table_3_4,
	_bridge_sprite_table_3_5,
	_bridge_sprite_table_3_6,
};

static const uint32 * const _bridge_sprite_table_6[] = {
	_bridge_sprite_table_6_0,
	_bridge_sprite_table_6_1,
	_bridge_sprite_table_6_2,
	_bridge_sprite_table_6_2,
	_bridge_sprite_table_6_2,
	_bridge_sprite_table_6_2,
	_bridge_sprite_table_6_3,
};

static const uint32 * const _bridge_sprite_table_7[] = {
	_bridge_sprite_table_7_0,
	_bridge_sprite_table_7_1,
	_bridge_sprite_table_7_2,
	_bridge_sprite_table_7_2,
	_bridge_sprite_table_7_2,
	_bridge_sprite_table_7_2,
	_bridge_sprite_table_7_3,
};

static const uint32 * const _bridge_sprite_table_8[] = {
	_bridge_sprite_table_8_0,
	_bridge_sprite_table_8_1,
	_bridge_sprite_table_8_2,
	_bridge_sprite_table_8_2,
	_bridge_sprite_table_8_2,
	_bridge_sprite_table_8_2,
	_bridge_sprite_table_8_3,
};

static const uint32 * const _bridge_sprite_table_0[] = {
	_bridge_sprite_table_0_0,
	_bridge_sprite_table_0_0,
	_bridge_sprite_table_0_0,
	_bridge_sprite_table_0_0,
	_bridge_sprite_table_0_0,
	_bridge_sprite_table_0_0,
	_bridge_sprite_table_0_1,
};

static const uint32 * const _bridge_sprite_table_1[] = {
	_bridge_sprite_table_1_0,
	_bridge_sprite_table_1_0,
	_bridge_sprite_table_1_0,
	_bridge_sprite_table_1_0,
	_bridge_sprite_table_1_0,
	_bridge_sprite_table_1_0,
	_bridge_sprite_table_1_1,
};

static const uint32 * const _bridge_sprite_table_9[] = {
	_bridge_sprite_table_9_0,
	_bridge_sprite_table_9_0,
	_bridge_sprite_table_9_0,
	_bridge_sprite_table_9_0,
	_bridge_sprite_table_9_0,
	_bridge_sprite_table_9_0,
	_bridge_sprite_table_4_6,
};

static const uint32 * const _bridge_sprite_table_10[] = {
	_bridge_sprite_table_10_0,
	_bridge_sprite_table_10_1,
	_bridge_sprite_table_10_2,
	_bridge_sprite_table_10_2,
	_bridge_sprite_table_10_2,
	_bridge_sprite_table_10_2,
	_bridge_sprite_table_4_6,
};

static const uint32 * const _bridge_sprite_table_11[] = {
	_bridge_sprite_table_11_0,
	_bridge_sprite_table_11_1,
	_bridge_sprite_table_11_2,
	_bridge_sprite_table_11_2,
	_bridge_sprite_table_11_2,
	_bridge_sprite_table_11_2,
	_bridge_sprite_table_5_6,
};

static const uint32 * const _bridge_sprite_table_12[] = {
	_bridge_sprite_table_12_0,
	_bridge_sprite_table_12_1,
	_bridge_sprite_table_12_2,
	_bridge_sprite_table_12_2,
	_bridge_sprite_table_12_2,
	_bridge_sprite_table_12_2,
	_bridge_sprite_table_3_6,
};

static const uint32 * const * const _bridge_sprite_table[MAX_BRIDGES] = {
	_bridge_sprite_table_0,
	_bridge_sprite_table_1,
	_bridge_sprite_table_2,
	_bridge_sprite_table_3,
	_bridge_sprite_table_4,
	_bridge_sprite_table_5,
	_bridge_sprite_table_6,
	_bridge_sprite_table_7,
	_bridge_sprite_table_8,
	_bridge_sprite_table_9,
	_bridge_sprite_table_10,
	_bridge_sprite_table_11,
	_bridge_sprite_table_12
};

static const uint32 * const _bridge_poles_table[] = {
	_bridge_sprite_table_0_poles,
	_bridge_sprite_table_1_poles,
	_bridge_sprite_table_2_poles,
	_bridge_sprite_table_3_poles,
	_bridge_sprite_table_4_poles,
	_bridge_sprite_table_5_poles,
	_bridge_sprite_table_6_poles,
	_bridge_sprite_table_7_poles,
	_bridge_sprite_table_8_poles,
	_bridge_sprite_table_2_poles,
	_bridge_sprite_table_10_poles,
	_bridge_sprite_table_11_poles,
	_bridge_sprite_table_12_poles
};


