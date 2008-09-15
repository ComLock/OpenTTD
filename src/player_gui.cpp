/* $Id$ */

/** @file player_gui.cpp Player related GUIs. */

#include "stdafx.h"
#include "openttd.h"
#include "gui.h"
#include "window_gui.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "player_func.h"
#include "player_base.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_gui.h"
#include "variables.h"
#include "roadveh.h"
#include "train.h"
#include "aircraft.h"
#include "newgrf.h"
#include "player_face.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "string_func.h"
#include "settings_type.h"
#include "widgets/dropdown_func.h"
#include "widgets/dropdown_type.h"
#include "tilehighlight_func.h"

#include "table/sprites.h"
#include "table/strings.h"

enum {
	FIRST_GUI_CALL = INT_MAX,  ///< default value to specify thuis is the first call of the resizable gui
};

static void DoShowPlayerFinances(PlayerID player, bool show_small, bool show_stickied, int top = FIRST_GUI_CALL, int left = FIRST_GUI_CALL);
static void DoSelectPlayerFace(Window *parent, bool show_big, int top =  FIRST_GUI_CALL, int left = FIRST_GUI_CALL);

static void DrawPlayerEconomyStats(const Player *p, bool small)
{
	int x, y, i, j, year;
	const Money (*tbl)[EXPENSES_END];
	Money sum, cost;
	StringID str;

	if (!small) { // normal sized economics window
		/* draw categories */
		DrawStringCenterUnderline(61, 15, STR_700F_EXPENDITURE_INCOME, TC_FROMSTRING);
		for (i = 0; i != EXPENSES_END; i++)
			DrawString(2, 27 + i * 10, STR_7011_CONSTRUCTION + i, TC_FROMSTRING);
		DrawStringRightAligned(111, 27 + 10 * EXPENSES_END + 2, STR_7020_TOTAL, TC_FROMSTRING);

		/* draw the price columns */
		year = _cur_year - 2;
		j = 3;
		x = 215;
		tbl = p->yearly_expenses + 2;
		do {
			if (year >= p->inaugurated_year) {
				SetDParam(0, year);
				DrawStringRightAlignedUnderline(x, 15, STR_7010, TC_FROMSTRING);
				sum = 0;
				for (i = 0; i != EXPENSES_END; i++) {
					/* draw one row in the price column */
					cost = (*tbl)[i];
					if (cost != 0) {
						sum += cost;

						str = STR_701E;
						if (cost < 0) { cost = -cost; str++; }
						SetDParam(0, cost);
						DrawStringRightAligned(x, 27 + i * 10, str, TC_FROMSTRING);
					}
				}

				str = STR_701E;
				if (sum < 0) { sum = -sum; str++; }
				SetDParam(0, sum);
				DrawStringRightAligned(x, 27 + EXPENSES_END * 10 + 2, str, TC_FROMSTRING);

				GfxFillRect(x - 75, 27 + 10 * EXPENSES_END, x, 27 + 10 * EXPENSES_END, 215);
				x += 95;
			}
			year++;
			tbl--;
		} while (--j != 0);

		y = 27 + 10 * EXPENSES_END + 14;

		/* draw max loan aligned to loan below (y += 10) */
		SetDParam(0, _economy.max_loan);
		DrawString(202, y + 10, STR_MAX_LOAN, TC_FROMSTRING);
	} else {
		y = 15;
	}

	DrawString(2, y, STR_7026_BANK_BALANCE, TC_FROMSTRING);
	SetDParam(0, p->player_money);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);

	y += 10;

	DrawString(2, y, STR_7027_LOAN, TC_FROMSTRING);
	SetDParam(0, p->current_loan);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);

	y += 12;

	GfxFillRect(182 - 75, y - 2, 182, y - 2, 215);

	SetDParam(0, p->player_money - p->current_loan);
	DrawStringRightAligned(182, y, STR_7028, TC_FROMSTRING);
}

enum PlayerFinancesWindowWidgets {
	PFW_WIDGET_TOGGLE_SIZE   = 2,
	PFW_WIDGET_INCREASE_LOAN = 6,
	PFW_WIDGET_REPAY_LOAN    = 7,
};

static const Widget _player_finances_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   379,     0,    13, STR_700E_FINANCES,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   380,   394,     0,    13, SPR_LARGE_SMALL_WINDOW, STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_GREY,   395,   406,     0,    13, 0x0,                    STR_STICKY_BUTTON},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   406,    14, 39 + 10 * EXPENSES_END, 0x0,    STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   406, 40 + 10 * EXPENSES_END, 73 + 10 * EXPENSES_END, 0x0, STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,   202, 74 + 10 * EXPENSES_END, 85 + 10 * EXPENSES_END, STR_7029_BORROW,        STR_7035_INCREASE_SIZE_OF_LOAN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   203,   406, 74 + 10 * EXPENSES_END, 85 + 10 * EXPENSES_END, STR_702A_REPAY,         STR_7036_REPAY_PART_OF_LOAN},
{   WIDGETS_END},
};

static const Widget _player_finances_small_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,               STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   253,     0,    13, STR_700E_FINANCES,      STR_018C_WINDOW_TITLE_DRAG_THIS},
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   254,   267,     0,    13, SPR_LARGE_SMALL_WINDOW, STR_7075_TOGGLE_LARGE_SMALL_WINDOW},
{  WWT_STICKYBOX,   RESIZE_NONE,  COLOUR_GREY,   268,   279,     0,    13, 0x0,                    STR_STICKY_BUTTON},
{      WWT_EMPTY,   RESIZE_NONE,  COLOUR_GREY,     0,     0,     0,     0, 0x0,                    STR_NULL},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   279,    14,    47, STR_NULL,               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,   139,    48,    59, STR_7029_BORROW,        STR_7035_INCREASE_SIZE_OF_LOAN},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   140,   279,    48,    59, STR_702A_REPAY,         STR_7036_REPAY_PART_OF_LOAN},
{   WIDGETS_END},
};

struct PlayerFinancesWindow : Window {
	bool small;

	PlayerFinancesWindow(const WindowDesc *desc, PlayerID player, bool show_small,
					bool show_stickied, int top, int left) :
			Window(desc, player),
			small(show_small)
	{
		this->caption_color = this->window_number;

		if (show_stickied) this->flags4 |= WF_STICKY;

		/* Check if repositioning from default is required */
		if (top != FIRST_GUI_CALL && left != FIRST_GUI_CALL) {
			this->top = top;
			this->left = left;
		}

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		PlayerID player = (PlayerID)this->window_number;
		const Player *p = GetPlayer(player);

		/* Recheck the size of the window as it might need to be resized due to the local player changing */
		int new_height = ((player != _local_player) ? 0 : 12) + ((this->small != 0) ? 48 : 74 + 10 * EXPENSES_END);
		if (this->height != new_height) {
			/* Make window dirty before and after resizing */
			this->SetDirty();
			this->height = new_height;
			this->SetDirty();

			this->SetWidgetHiddenState(PFW_WIDGET_INCREASE_LOAN, player != _local_player);
			this->SetWidgetHiddenState(PFW_WIDGET_REPAY_LOAN,    player != _local_player);
		}

		/* Borrow button only shows when there is any more money to loan */
		this->SetWidgetDisabledState(PFW_WIDGET_INCREASE_LOAN, p->current_loan == _economy.max_loan);

		/* Repay button only shows when there is any more money to repay */
		this->SetWidgetDisabledState(PFW_WIDGET_REPAY_LOAN, player != _local_player || p->current_loan == 0);

		SetDParam(0, p->index);
		SetDParam(1, p->index);
		SetDParam(2, LOAN_INTERVAL);
		this->DrawWidgets();

		DrawPlayerEconomyStats(p, this->small);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case PFW_WIDGET_TOGGLE_SIZE: {/* toggle size */
				bool new_mode = !this->small;
				bool stickied = !!(this->flags4 & WF_STICKY);
				int oldtop = this->top;   ///< current top position of the window before closing it
				int oldleft = this->left; ///< current left position of the window before closing it
				PlayerID player = (PlayerID)this->window_number;

				delete this;
				/* Open up the (toggled size) Finance window at the same position as the previous */
				DoShowPlayerFinances(player, new_mode, stickied, oldtop, oldleft);
			}
			break;

			case PFW_WIDGET_INCREASE_LOAN: /* increase loan */
				DoCommandP(0, 0, _ctrl_pressed, NULL, CMD_INCREASE_LOAN | CMD_MSG(STR_702C_CAN_T_BORROW_ANY_MORE_MONEY));
				break;

			case PFW_WIDGET_REPAY_LOAN: /* repay loan */
				DoCommandP(0, 0, _ctrl_pressed, NULL, CMD_DECREASE_LOAN | CMD_MSG(STR_702F_CAN_T_REPAY_LOAN));
				break;
		}
	}
};

static const WindowDesc _player_finances_desc = {
	WDP_AUTO, WDP_AUTO, 407, 86 + 10 * EXPENSES_END, 407, 86 + 10 * EXPENSES_END,
	WC_FINANCES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_player_finances_widgets,
};

static const WindowDesc _player_finances_small_desc = {
	WDP_AUTO, WDP_AUTO, 280, 60, 280, 60,
	WC_FINANCES, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS | WDF_STICKY_BUTTON,
	_player_finances_small_widgets,
};

/**
 * Open the small/large finance window of the player
 *
 * @param player         the player who's finances are requested to be seen
 * @param show_small     show large or small version opf the window
 * @param show_stickied  previous "stickyness" of the window
 * @param top            previous top position of the window
 * @param left           previous left position of the window
 *
 * @pre is player a valid player
 */
static void DoShowPlayerFinances(PlayerID player, bool show_small, bool show_stickied, int top, int left)
{
	if (!IsValidPlayerID(player)) return;

	if (BringWindowToFrontById(WC_FINANCES, player)) return;
	new PlayerFinancesWindow(show_small ? &_player_finances_small_desc : &_player_finances_desc, player, show_small, show_stickied, top, left);
}

void ShowPlayerFinances(PlayerID player)
{
	DoShowPlayerFinances(player, false, false);
}

/* List of colours for the livery window */
static const StringID _colour_dropdown[] = {
	STR_00D1_DARK_BLUE,
	STR_00D2_PALE_GREEN,
	STR_00D3_PINK,
	STR_00D4_YELLOW,
	STR_00D5_RED,
	STR_00D6_LIGHT_BLUE,
	STR_00D7_GREEN,
	STR_00D8_DARK_GREEN,
	STR_00D9_BLUE,
	STR_00DA_CREAM,
	STR_00DB_MAUVE,
	STR_00DC_PURPLE,
	STR_00DD_ORANGE,
	STR_00DE_BROWN,
	STR_00DF_GREY,
	STR_00E0_WHITE,
};

/* Association of liveries to livery classes */
static const LiveryClass _livery_class[LS_END] = {
	LC_OTHER,
	LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL, LC_RAIL,
	LC_ROAD, LC_ROAD,
	LC_SHIP, LC_SHIP,
	LC_AIRCRAFT, LC_AIRCRAFT, LC_AIRCRAFT,
	LC_ROAD, LC_ROAD,
};

class DropDownListColourItem : public DropDownListItem {
public:
	DropDownListColourItem(int result, bool masked) : DropDownListItem(result, masked) {}

	virtual ~DropDownListColourItem() {}

	StringID String() const
	{
		return _colour_dropdown[this->result];
	}

	uint Height(uint width) const
	{
		return 14;
	}

	bool Selectable() const
	{
		return true;
	}

	void Draw(int x, int y, uint width, uint height, bool sel, int bg_colour) const
	{
		DrawSprite(SPR_VEH_BUS_SIDE_VIEW, PALETTE_RECOLOR_START + this->result, x + 16, y + 7);
		DrawStringTruncated(x + 32, y + 3, this->String(), sel ? TC_WHITE : TC_BLACK, width - 30);
	}
};

struct SelectPlayerLiveryWindow : public Window {
private:
	uint32 sel;
	LiveryClass livery_class;

	enum PlayerLiveryWindowWidgets {
		PLW_WIDGET_CLOSE,
		PLW_WIDGET_CAPTION,
		PLW_WIDGET_CLASS_GENERAL,
		PLW_WIDGET_CLASS_RAIL,
		PLW_WIDGET_CLASS_ROAD,
		PLW_WIDGET_CLASS_SHIP,
		PLW_WIDGET_CLASS_AIRCRAFT,
		PLW_WIDGET_SPACER_CLASS,
		PLW_WIDGET_SPACER_DROPDOWN,
		PLW_WIDGET_PRI_COL_DROPDOWN,
		PLW_WIDGET_SEC_COL_DROPDOWN,
		PLW_WIDGET_MATRIX,
	};

	void ShowColourDropDownMenu(uint32 widget)
	{
		uint32 used_colours = 0;
		const Livery *livery;
		LiveryScheme scheme;

		/* Disallow other player colours for the primary colour */
		if (HasBit(this->sel, LS_DEFAULT) && widget == PLW_WIDGET_PRI_COL_DROPDOWN) {
			const Player *p;
			FOR_ALL_PLAYERS(p) {
				if (p->index != _local_player) SetBit(used_colours, p->player_color);
			}
		}

		/* Get the first selected livery to use as the default dropdown item */
		for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
			if (HasBit(this->sel, scheme)) break;
		}
		if (scheme == LS_END) scheme = LS_DEFAULT;
		livery = &GetPlayer((PlayerID)this->window_number)->livery[scheme];

		DropDownList *list = new DropDownList();
		for (uint i = 0; i < lengthof(_colour_dropdown); i++) {
			list->push_back(new DropDownListColourItem(i, HasBit(used_colours, i)));
		}

		ShowDropDownList(this, list, widget == PLW_WIDGET_PRI_COL_DROPDOWN ? livery->colour1 : livery->colour2, widget);
	}

public:
	SelectPlayerLiveryWindow(const WindowDesc *desc, PlayerID player) : Window(desc, player)
	{
		this->caption_color = player;
		this->livery_class = LC_OTHER;
		this->sel = 1;
		this->LowerWidget(PLW_WIDGET_CLASS_GENERAL);
		this->OnInvalidateData(_loaded_newgrf_features.has_2CC);
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const Player *p = GetPlayer((PlayerID)this->window_number);
		LiveryScheme scheme = LS_DEFAULT;
		int y = 51;

		/* Disable dropdown controls if no scheme is selected */
		this->SetWidgetDisabledState(PLW_WIDGET_PRI_COL_DROPDOWN, this->sel == 0);
		this->SetWidgetDisabledState(PLW_WIDGET_SEC_COL_DROPDOWN, this->sel == 0);

		if (this->sel != 0) {
			for (scheme = LS_BEGIN; scheme < LS_END; scheme++) {
				if (HasBit(this->sel, scheme)) break;
			}
			if (scheme == LS_END) scheme = LS_DEFAULT;
		}

		SetDParam(0, STR_00D1_DARK_BLUE + p->livery[scheme].colour1);
		SetDParam(1, STR_00D1_DARK_BLUE + p->livery[scheme].colour2);

		this->DrawWidgets();

		for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
			if (_livery_class[scheme] == this->livery_class) {
				bool sel = HasBit(this->sel, scheme) != 0;

				if (scheme != LS_DEFAULT) {
					DrawSprite(p->livery[scheme].in_use ? SPR_BOX_CHECKED : SPR_BOX_EMPTY, PAL_NONE, 2, y);
				}

				DrawString(15, y, STR_LIVERY_DEFAULT + scheme, sel ? TC_WHITE : TC_BLACK);

				DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOR(p->livery[scheme].colour1), 152, y);
				DrawString(165, y, STR_00D1_DARK_BLUE + p->livery[scheme].colour1, sel ? TC_WHITE : TC_GOLD);

				if (!this->IsWidgetHidden(PLW_WIDGET_SEC_COL_DROPDOWN)) {
					DrawSprite(SPR_SQUARE, GENERAL_SPRITE_COLOR(p->livery[scheme].colour2), 277, y);
					DrawString(290, y, STR_00D1_DARK_BLUE + p->livery[scheme].colour2, sel ? TC_WHITE : TC_GOLD);
				}

				y += 14;
			}
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		/* Number of liveries in each class, used to determine the height of the livery window */
		static const byte livery_height[] = {
			1,
			13,
			4,
			2,
			3,
		};

		switch (widget) {
			/* Livery Class buttons */
			case PLW_WIDGET_CLASS_GENERAL:
			case PLW_WIDGET_CLASS_RAIL:
			case PLW_WIDGET_CLASS_ROAD:
			case PLW_WIDGET_CLASS_SHIP:
			case PLW_WIDGET_CLASS_AIRCRAFT: {
				LiveryScheme scheme;

				this->RaiseWidget(this->livery_class + PLW_WIDGET_CLASS_GENERAL);
				this->livery_class = (LiveryClass)(widget - PLW_WIDGET_CLASS_GENERAL);
				this->sel = 0;
				this->LowerWidget(this->livery_class + PLW_WIDGET_CLASS_GENERAL);

				/* Select the first item in the list */
				for (scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
					if (_livery_class[scheme] == this->livery_class) {
						this->sel = 1 << scheme;
						break;
					}
				}
				this->height = 49 + livery_height[this->livery_class] * 14;
				this->widget[PLW_WIDGET_MATRIX].bottom = this->height - 1;
				this->widget[PLW_WIDGET_MATRIX].data = livery_height[this->livery_class] << 8 | 1;
				MarkWholeScreenDirty();
				break;
			}

			case PLW_WIDGET_PRI_COL_DROPDOWN: /* First colour dropdown */
				ShowColourDropDownMenu(PLW_WIDGET_PRI_COL_DROPDOWN);
				break;

			case PLW_WIDGET_SEC_COL_DROPDOWN: /* Second colour dropdown */
				ShowColourDropDownMenu(PLW_WIDGET_SEC_COL_DROPDOWN);
				break;

			case PLW_WIDGET_MATRIX: {
				LiveryScheme scheme;
				LiveryScheme j = (LiveryScheme)((pt.y - 48) / 14);

				for (scheme = LS_BEGIN; scheme <= j; scheme++) {
					if (_livery_class[scheme] != this->livery_class) j++;
					if (scheme >= LS_END) return;
				}
				if (j >= LS_END) return;

				/* If clicking on the left edge, toggle using the livery */
				if (pt.x < 10) {
					DoCommandP(0, j | (2 << 8), !GetPlayer((PlayerID)this->window_number)->livery[j].in_use, NULL, CMD_SET_PLAYER_COLOR);
				}

				if (_ctrl_pressed) {
					ToggleBit(this->sel, j);
				} else {
					this->sel = 1 << j;
				}
				this->SetDirty();
				break;
			}
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		for (LiveryScheme scheme = LS_DEFAULT; scheme < LS_END; scheme++) {
			if (HasBit(this->sel, scheme)) {
				DoCommandP(0, scheme | (widget == PLW_WIDGET_PRI_COL_DROPDOWN ? 0 : 256), index, NULL, CMD_SET_PLAYER_COLOR);
			}
		}
	}

	virtual void OnInvalidateData(int data = 0)
	{
		static bool has2cc = true;

		if (has2cc == !!data) return;

		has2cc = !!data;

		int r = this->widget[has2cc ? PLW_WIDGET_SEC_COL_DROPDOWN : PLW_WIDGET_PRI_COL_DROPDOWN].right;
		this->SetWidgetHiddenState(PLW_WIDGET_SEC_COL_DROPDOWN, !has2cc);
		this->widget[PLW_WIDGET_CAPTION].right = r;
		this->widget[PLW_WIDGET_SPACER_CLASS].right = r;
		this->widget[PLW_WIDGET_MATRIX].right = r;
		this->width = r + 1;
	}
};

static const Widget _select_player_livery_widgets[] = {
{ WWT_CLOSEBOX, RESIZE_NONE,  COLOUR_GREY,   0,  10,   0,  13, STR_00C5,                  STR_018B_CLOSE_WINDOW },
{  WWT_CAPTION, RESIZE_NONE,  COLOUR_GREY,  11, 399,   0,  13, STR_7007_NEW_COLOR_SCHEME, STR_018C_WINDOW_TITLE_DRAG_THIS },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,   0,  21,  14,  35, SPR_IMG_COMPANY_GENERAL,   STR_LIVERY_GENERAL_TIP },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,  22,  43,  14,  35, SPR_IMG_TRAINLIST,         STR_LIVERY_TRAIN_TIP },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,  44,  65,  14,  35, SPR_IMG_TRUCKLIST,         STR_LIVERY_ROADVEH_TIP },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,  66,  87,  14,  35, SPR_IMG_SHIPLIST,          STR_LIVERY_SHIP_TIP },
{   WWT_IMGBTN, RESIZE_NONE,  COLOUR_GREY,  88, 109,  14,  35, SPR_IMG_AIRPLANESLIST,     STR_LIVERY_AIRCRAFT_TIP },
{    WWT_PANEL, RESIZE_NONE,  COLOUR_GREY, 110, 399,  14,  35, 0x0,                       STR_NULL },
{    WWT_PANEL, RESIZE_NONE,  COLOUR_GREY,   0, 149,  36,  47, 0x0,                       STR_NULL },
{ WWT_DROPDOWN, RESIZE_NONE,  COLOUR_GREY, 150, 274,  36,  47, STR_02BD,                  STR_LIVERY_PRIMARY_TIP },
{ WWT_DROPDOWN, RESIZE_NONE,  COLOUR_GREY, 275, 399,  36,  47, STR_02E1,                  STR_LIVERY_SECONDARY_TIP },
{   WWT_MATRIX, RESIZE_NONE,  COLOUR_GREY,   0, 399,  48,  48 + 1 * 14, (1 << 8) | 1,     STR_LIVERY_PANEL_TIP },
{ WIDGETS_END },
};

static const WindowDesc _select_player_livery_desc = {
	WDP_AUTO, WDP_AUTO, 400, 49 + 1 * 14, 400, 49 + 1 * 14,
	WC_PLAYER_COLOR, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_select_player_livery_widgets,
};

/**
 * Draws the face of a player.
 * @param pf    the player's face
 * @param color the (background) color of the gradient
 * @param x     x-position to draw the face
 * @param y     y-position to draw the face
 */
void DrawPlayerFace(PlayerFace pf, int color, int x, int y)
{
	GenderEthnicity ge = (GenderEthnicity)GetPlayerFaceBits(pf, PFV_GEN_ETHN, GE_WM);

	bool has_moustache   = !HasBit(ge, GENDER_FEMALE) && GetPlayerFaceBits(pf, PFV_HAS_MOUSTACHE,   ge) != 0;
	bool has_tie_earring = !HasBit(ge, GENDER_FEMALE) || GetPlayerFaceBits(pf, PFV_HAS_TIE_EARRING, ge) != 0;
	bool has_glasses     = GetPlayerFaceBits(pf, PFV_HAS_GLASSES, ge) != 0;
	SpriteID pal;

	/* Modify eye colour palette only if 2 or more valid values exist */
	if (_pf_info[PFV_EYE_COLOUR].valid_values[ge] < 2) {
		pal = PAL_NONE;
	} else {
		switch (GetPlayerFaceBits(pf, PFV_EYE_COLOUR, ge)) {
			default: NOT_REACHED();
			case 0: pal = PALETTE_TO_BROWN; break;
			case 1: pal = PALETTE_TO_BLUE;  break;
			case 2: pal = PALETTE_TO_GREEN; break;
		}
	}

	/* Draw the gradient (background) */
	DrawSprite(SPR_GRADIENT, GENERAL_SPRITE_COLOR(color), x, y);

	for (PlayerFaceVariable pfv = PFV_CHEEKS; pfv < PFV_END; pfv++) {
		switch (pfv) {
			case PFV_MOUSTACHE:   if (!has_moustache)   continue; break;
			case PFV_LIPS:        /* FALL THROUGH */
			case PFV_NOSE:        if (has_moustache)    continue; break;
			case PFV_TIE_EARRING: if (!has_tie_earring) continue; break;
			case PFV_GLASSES:     if (!has_glasses)     continue; break;
			default: break;
		}
		DrawSprite(GetPlayerFaceSprite(pf, pfv, ge), (pfv == PFV_EYEBROWS) ? pal : PAL_NONE, x, y);
	}
}

/** Widget description for the normal/simple player face selection dialog */
static const Widget _select_player_face_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},              // PFW_WIDGET_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   174,     0,    13, STR_7043_FACE_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},    // PFW_WIDGET_CAPTION
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   189,     0,    13, SPR_LARGE_SMALL_WINDOW,  STR_FACE_ADVANCED_TIP},              // PFW_WIDGET_TOGGLE_LARGE_SMALL
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   189,    14,   150, 0x0,                     STR_NULL},                           // PFW_WIDGET_SELECT_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    94,   151,   162, STR_012E_CANCEL,         STR_7047_CANCEL_NEW_FACE_SELECTION}, // PFW_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   189,   151,   162, STR_012F_OK,             STR_7048_ACCEPT_NEW_FACE_SELECTION}, // PFW_WIDGET_ACCEPT
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    75,    86, STR_7044_MALE,           STR_7049_SELECT_MALE_FACES},         // PFW_WIDGET_MALE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    87,    98, STR_7045_FEMALE,         STR_704A_SELECT_FEMALE_FACES},       // PFW_WIDGET_FEMALE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   137,   148, STR_7046_NEW_FACE,       STR_704B_GENERATE_RANDOM_NEW_FACE},  // PFW_WIDGET_RANDOM_NEW_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   187,    16,    27, STR_FACE_ADVANCED,       STR_FACE_ADVANCED_TIP},              // PFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON
{   WIDGETS_END},
};

/** Widget description for the advanced player face selection dialog */
static const Widget _select_player_face_adv_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                STR_018B_CLOSE_WINDOW},              // PFW_WIDGET_CLOSEBOX
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   204,     0,    13, STR_7043_FACE_SELECTION, STR_018C_WINDOW_TITLE_DRAG_THIS},    // PFW_WIDGET_CAPTION
{     WWT_IMGBTN,   RESIZE_NONE,  COLOUR_GREY,   205,   219,     0,    13, SPR_LARGE_SMALL_WINDOW,  STR_FACE_SIMPLE_TIP},                // PFW_WIDGET_TOGGLE_LARGE_SMALL
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   219,    14,   207, 0x0,                     STR_NULL},                           // PFW_WIDGET_SELECT_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    94,   208,   219, STR_012E_CANCEL,         STR_7047_CANCEL_NEW_FACE_SELECTION}, // PFW_WIDGET_CANCEL
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   219,   208,   219, STR_012F_OK,             STR_7048_ACCEPT_NEW_FACE_SELECTION}, // PFW_WIDGET_ACCEPT
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    96,   156,    32,    43, STR_7044_MALE,           STR_7049_SELECT_MALE_FACES},         // PFW_WIDGET_MALE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   157,   217,    32,    43, STR_7045_FEMALE,         STR_704A_SELECT_FEMALE_FACES},       // PFW_WIDGET_FEMALE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   137,   148, STR_RANDOM,              STR_704B_GENERATE_RANDOM_NEW_FACE},  // PFW_WIDGET_RANDOM_NEW_FACE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    95,   217,    16,    27, STR_FACE_SIMPLE,         STR_FACE_SIMPLE_TIP},                // PFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   158,   169, STR_FACE_LOAD,           STR_FACE_LOAD_TIP},                  // PFW_WIDGET_LOAD
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   170,   181, STR_FACE_FACECODE,       STR_FACE_FACECODE_TIP},              // PFW_WIDGET_FACECODE
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     2,    93,   182,   193, STR_FACE_SAVE,           STR_FACE_SAVE_TIP},                  // PFW_WIDGET_SAVE
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,    96,   156,    46,    57, STR_FACE_EUROPEAN,       STR_FACE_SELECT_EUROPEAN},           // PFW_WIDGET_ETHNICITY_EUR
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   157,   217,    46,    57, STR_FACE_AFRICAN,        STR_FACE_SELECT_AFRICAN},            // PFW_WIDGET_ETHNICITY_AFR
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   217,    60,    71, STR_EMPTY,               STR_FACE_MOUSTACHE_EARRING_TIP},     // PFW_WIDGET_HAS_MOUSTACHE_EARRING
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   175,   217,    72,    83, STR_EMPTY,               STR_FACE_GLASSES_TIP},               // PFW_WIDGET_HAS_GLASSES
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   110,   121, SPR_ARROW_LEFT,          STR_FACE_EYECOLOUR_TIP},             // PFW_WIDGET_EYECOLOUR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   110,   121, STR_EMPTY,               STR_FACE_EYECOLOUR_TIP},             // PFW_WIDGET_EYECOLOUR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   110,   121, SPR_ARROW_RIGHT,         STR_FACE_EYECOLOUR_TIP},             // PFW_WIDGET_EYECOLOUR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   158,   169, SPR_ARROW_LEFT,          STR_FACE_CHIN_TIP},                  // PFW_WIDGET_CHIN_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   158,   169, STR_EMPTY,               STR_FACE_CHIN_TIP},                  // PFW_WIDGET_CHIN
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   158,   169, SPR_ARROW_RIGHT,         STR_FACE_CHIN_TIP},                  // PFW_WIDGET_CHIN_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,    98,   109, SPR_ARROW_LEFT,          STR_FACE_EYEBROWS_TIP},              // PFW_WIDGET_EYEBROWS_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,    98,   109, STR_EMPTY,               STR_FACE_EYEBROWS_TIP},              // PFW_WIDGET_EYEBROWS
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,    98,   109, SPR_ARROW_RIGHT,         STR_FACE_EYEBROWS_TIP},              // PFW_WIDGET_EYEBROWS_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   146,   157, SPR_ARROW_LEFT,          STR_FACE_LIPS_MOUSTACHE_TIP},        // PFW_WIDGET_LIPS_MOUSTACHE_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   146,   157, STR_EMPTY,               STR_FACE_LIPS_MOUSTACHE_TIP},        // PFW_WIDGET_LIPS_MOUSTACHE
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   146,   157, SPR_ARROW_RIGHT,         STR_FACE_LIPS_MOUSTACHE_TIP},        // PFW_WIDGET_LIPS_MOUSTACHE_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   134,   145, SPR_ARROW_LEFT,          STR_FACE_NOSE_TIP},                  // PFW_WIDGET_NOSE_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   134,   145, STR_EMPTY,               STR_FACE_NOSE_TIP},                  // PFW_WIDGET_NOSE
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   134,   145, SPR_ARROW_RIGHT,         STR_FACE_NOSE_TIP},                  // PFW_WIDGET_NOSE_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,    86,    97, SPR_ARROW_LEFT,          STR_FACE_HAIR_TIP},                  // PFW_WIDGET_HAIR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,    86,    97, STR_EMPTY,               STR_FACE_HAIR_TIP},                  // PFW_WIDGET_HAIR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,    86,    97, SPR_ARROW_RIGHT,         STR_FACE_HAIR_TIP},                  // PFW_WIDGET_HAIR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   170,   181, SPR_ARROW_LEFT,          STR_FACE_JACKET_TIP},                // PFW_WIDGET_JACKET_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   170,   181, STR_EMPTY,               STR_FACE_JACKET_TIP},                // PFW_WIDGET_JACKET
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   170,   181, SPR_ARROW_RIGHT,         STR_FACE_JACKET_TIP},                // PFW_WIDGET_JACKET_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   182,   193, SPR_ARROW_LEFT,          STR_FACE_COLLAR_TIP},                // PFW_WIDGET_COLLAR_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   182,   193, STR_EMPTY,               STR_FACE_COLLAR_TIP},                // PFW_WIDGET_COLLAR
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   182,   193, SPR_ARROW_RIGHT,         STR_FACE_COLLAR_TIP},                // PFW_WIDGET_COLLAR_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   194,   205, SPR_ARROW_LEFT,          STR_FACE_TIE_EARRING_TIP},           // PFW_WIDGET_TIE_EARRING_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   194,   205, STR_EMPTY,               STR_FACE_TIE_EARRING_TIP},           // PFW_WIDGET_TIE_EARRING
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   194,   205, SPR_ARROW_RIGHT,         STR_FACE_TIE_EARRING_TIP},           // PFW_WIDGET_TIE_EARRING_R
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    175,  183,   122,   133, SPR_ARROW_LEFT,          STR_FACE_GLASSES_TIP_2},             // PFW_WIDGET_GLASSES_L
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    184,  208,   122,   133, STR_EMPTY,               STR_FACE_GLASSES_TIP_2},             // PFW_WIDGET_GLASSES
{ WWT_PUSHIMGBTN,   RESIZE_NONE,  COLOUR_GREY,    209,  217,   122,   133, SPR_ARROW_RIGHT,         STR_FACE_GLASSES_TIP_2},             // PFW_WIDGET_GLASSES_R
{   WIDGETS_END},
};

class SelectPlayerFaceWindow : public Window
{
	PlayerFace face; // player face bits
	bool advanced;   // advance player face selection window

	GenderEthnicity ge;
	bool is_female;
	bool is_moust_male;

	/**
	 * Names of the widgets. Keep them in the same order as in the widget array.
	 * Do not change the order of the widgets from PFW_WIDGET_HAS_MOUSTACHE_EARRING to PFW_WIDGET_GLASSES_R,
	 * this order is needed for the WE_CLICK event of DrawFaceStringLabel().
	 */
	enum PlayerFaceWindowWidgets {
		PFW_WIDGET_CLOSEBOX = 0,
		PFW_WIDGET_CAPTION,
		PFW_WIDGET_TOGGLE_LARGE_SMALL,
		PFW_WIDGET_SELECT_FACE,
		PFW_WIDGET_CANCEL,
		PFW_WIDGET_ACCEPT,
		PFW_WIDGET_MALE,
		PFW_WIDGET_FEMALE,
		PFW_WIDGET_RANDOM_NEW_FACE,
		PFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON,
		/* from here is the advanced player face selection window */
		PFW_WIDGET_LOAD,
		PFW_WIDGET_FACECODE,
		PFW_WIDGET_SAVE,
		PFW_WIDGET_ETHNICITY_EUR,
		PFW_WIDGET_ETHNICITY_AFR,
		PFW_WIDGET_HAS_MOUSTACHE_EARRING,
		PFW_WIDGET_HAS_GLASSES,
		PFW_WIDGET_EYECOLOUR_L,
		PFW_WIDGET_EYECOLOUR,
		PFW_WIDGET_EYECOLOUR_R,
		PFW_WIDGET_CHIN_L,
		PFW_WIDGET_CHIN,
		PFW_WIDGET_CHIN_R,
		PFW_WIDGET_EYEBROWS_L,
		PFW_WIDGET_EYEBROWS,
		PFW_WIDGET_EYEBROWS_R,
		PFW_WIDGET_LIPS_MOUSTACHE_L,
		PFW_WIDGET_LIPS_MOUSTACHE,
		PFW_WIDGET_LIPS_MOUSTACHE_R,
		PFW_WIDGET_NOSE_L,
		PFW_WIDGET_NOSE,
		PFW_WIDGET_NOSE_R,
		PFW_WIDGET_HAIR_L,
		PFW_WIDGET_HAIR,
		PFW_WIDGET_HAIR_R,
		PFW_WIDGET_JACKET_L,
		PFW_WIDGET_JACKET,
		PFW_WIDGET_JACKET_R,
		PFW_WIDGET_COLLAR_L,
		PFW_WIDGET_COLLAR,
		PFW_WIDGET_COLLAR_R,
		PFW_WIDGET_TIE_EARRING_L,
		PFW_WIDGET_TIE_EARRING,
		PFW_WIDGET_TIE_EARRING_R,
		PFW_WIDGET_GLASSES_L,
		PFW_WIDGET_GLASSES,
		PFW_WIDGET_GLASSES_R,
	};
	/**
	 * Draw dynamic a label to the left of the button and a value in the button
	 *
	 * @param widget_index   index of this widget in the window
	 * @param str            the label which will be draw
	 * @param val            the value which will be draw
	 * @param is_bool_widget is it a bool button
	 */
	void DrawFaceStringLabel(byte widget_index, StringID str, uint8 val, bool is_bool_widget)
	{
		/* Write the label in gold (0x2) to the left of the button. */
		DrawStringRightAligned(this->widget[widget_index].left - (is_bool_widget ? 5 : 14), this->widget[widget_index].top + 1, str, TC_GOLD);

		if (!this->IsWidgetDisabled(widget_index)) {
			if (is_bool_widget) {
				/* if it a bool button write yes or no */
				str = (val != 0) ? STR_FACE_YES : STR_FACE_NO;
			} else {
				/* else write the value + 1 */
				SetDParam(0, val + 1);
				str = STR_JUST_INT;
			}

			/* Draw the value/bool in white (0xC). If the button clicked adds 1px to x and y text coordinates (IsWindowWidgetLowered()). */
			DrawStringCentered(this->widget[widget_index].left + (this->widget[widget_index].right - this->widget[widget_index].left) / 2 +
				this->IsWidgetLowered(widget_index), this->widget[widget_index].top + 1 + this->IsWidgetLowered(widget_index), str, TC_WHITE);
		}
	}

	void UpdateData()
	{
		this->ge = (GenderEthnicity)GB(this->face, _pf_info[PFV_GEN_ETHN].offset, _pf_info[PFV_GEN_ETHN].length); // get the gender and ethnicity
		this->is_female = HasBit(this->ge, GENDER_FEMALE); // get the gender: 0 == male and 1 == female
		this->is_moust_male = !is_female && GetPlayerFaceBits(this->face, PFV_HAS_MOUSTACHE, this->ge) != 0; // is a male face with moustache
	}

public:
	SelectPlayerFaceWindow(const WindowDesc *desc, Window *parent, bool advanced, int top, int left) : Window(desc, parent->window_number)
	{
		this->parent = parent;
		this->caption_color = this->window_number;
		this->face = GetPlayer((PlayerID)this->window_number)->face;
		this->advanced = advanced;

		this->UpdateData();

		/* Check if repositioning from default is required */
		if (top != FIRST_GUI_CALL && left != FIRST_GUI_CALL) {
			this->top = top;
			this->left = left;
		}

		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		/* lower the non-selected gender button */
		this->SetWidgetLoweredState(PFW_WIDGET_MALE,  !this->is_female);
		this->SetWidgetLoweredState(PFW_WIDGET_FEMALE, this->is_female);

		/* advanced player face selection window */
		if (this->advanced) {
			/* lower the non-selected ethnicity button */
			this->SetWidgetLoweredState(PFW_WIDGET_ETHNICITY_EUR, !HasBit(this->ge, ETHNICITY_BLACK));
			this->SetWidgetLoweredState(PFW_WIDGET_ETHNICITY_AFR,  HasBit(this->ge, ETHNICITY_BLACK));


			/* Disable dynamically the widgets which PlayerFaceVariable has less than 2 options
			* (or in other words you haven't any choice).
			* If the widgets depend on a HAS-variable and this is false the widgets will be disabled, too. */

			/* Eye colour buttons */
			this->SetWidgetsDisabledState(_pf_info[PFV_EYE_COLOUR].valid_values[this->ge] < 2,
				PFW_WIDGET_EYECOLOUR, PFW_WIDGET_EYECOLOUR_L, PFW_WIDGET_EYECOLOUR_R, WIDGET_LIST_END);

			/* Chin buttons */
			this->SetWidgetsDisabledState(_pf_info[PFV_CHIN].valid_values[this->ge] < 2,
				PFW_WIDGET_CHIN, PFW_WIDGET_CHIN_L, PFW_WIDGET_CHIN_R, WIDGET_LIST_END);

			/* Eyebrows buttons */
			this->SetWidgetsDisabledState(_pf_info[PFV_EYEBROWS].valid_values[this->ge] < 2,
				PFW_WIDGET_EYEBROWS, PFW_WIDGET_EYEBROWS_L, PFW_WIDGET_EYEBROWS_R, WIDGET_LIST_END);

			/* Lips or (if it a male face with a moustache) moustache buttons */
			this->SetWidgetsDisabledState(_pf_info[this->is_moust_male ? PFV_MOUSTACHE : PFV_LIPS].valid_values[this->ge] < 2,
				PFW_WIDGET_LIPS_MOUSTACHE, PFW_WIDGET_LIPS_MOUSTACHE_L, PFW_WIDGET_LIPS_MOUSTACHE_R, WIDGET_LIST_END);

			/* Nose buttons | male faces with moustache haven't any nose options */
			this->SetWidgetsDisabledState(_pf_info[PFV_NOSE].valid_values[this->ge] < 2 || this->is_moust_male,
				PFW_WIDGET_NOSE, PFW_WIDGET_NOSE_L, PFW_WIDGET_NOSE_R, WIDGET_LIST_END);

			/* Hair buttons */
			this->SetWidgetsDisabledState(_pf_info[PFV_HAIR].valid_values[this->ge] < 2,
				PFW_WIDGET_HAIR, PFW_WIDGET_HAIR_L, PFW_WIDGET_HAIR_R, WIDGET_LIST_END);

			/* Jacket buttons */
			this->SetWidgetsDisabledState(_pf_info[PFV_JACKET].valid_values[this->ge] < 2,
				PFW_WIDGET_JACKET, PFW_WIDGET_JACKET_L, PFW_WIDGET_JACKET_R, WIDGET_LIST_END);

			/* Collar buttons */
			this->SetWidgetsDisabledState(_pf_info[PFV_COLLAR].valid_values[this->ge] < 2,
				PFW_WIDGET_COLLAR, PFW_WIDGET_COLLAR_L, PFW_WIDGET_COLLAR_R, WIDGET_LIST_END);

			/* Tie/earring buttons | female faces without earring haven't any earring options */
			this->SetWidgetsDisabledState(_pf_info[PFV_TIE_EARRING].valid_values[this->ge] < 2 ||
					(this->is_female && GetPlayerFaceBits(this->face, PFV_HAS_TIE_EARRING, this->ge) == 0),
				PFW_WIDGET_TIE_EARRING, PFW_WIDGET_TIE_EARRING_L, PFW_WIDGET_TIE_EARRING_R, WIDGET_LIST_END);

			/* Glasses buttons | faces without glasses haven't any glasses options */
			this->SetWidgetsDisabledState(_pf_info[PFV_GLASSES].valid_values[this->ge] < 2 || GetPlayerFaceBits(this->face, PFV_HAS_GLASSES, this->ge) == 0,
				PFW_WIDGET_GLASSES, PFW_WIDGET_GLASSES_L, PFW_WIDGET_GLASSES_R, WIDGET_LIST_END);
		}

		this->DrawWidgets();

		/* Draw dynamic button value and labels for the advanced player face selection window */
		if (this->advanced) {
			if (this->is_female) {
				/* Only for female faces */
				this->DrawFaceStringLabel(PFW_WIDGET_HAS_MOUSTACHE_EARRING, STR_FACE_EARRING,   GetPlayerFaceBits(this->face, PFV_HAS_TIE_EARRING, this->ge), true );
				this->DrawFaceStringLabel(PFW_WIDGET_TIE_EARRING,           STR_FACE_EARRING,   GetPlayerFaceBits(this->face, PFV_TIE_EARRING,     this->ge), false);
			} else {
				/* Only for male faces */
				this->DrawFaceStringLabel(PFW_WIDGET_HAS_MOUSTACHE_EARRING, STR_FACE_MOUSTACHE, GetPlayerFaceBits(this->face, PFV_HAS_MOUSTACHE,   this->ge), true );
				this->DrawFaceStringLabel(PFW_WIDGET_TIE_EARRING,           STR_FACE_TIE,       GetPlayerFaceBits(this->face, PFV_TIE_EARRING,     this->ge), false);
			}
			if (this->is_moust_male) {
				/* Only for male faces with moustache */
				this->DrawFaceStringLabel(PFW_WIDGET_LIPS_MOUSTACHE,        STR_FACE_MOUSTACHE, GetPlayerFaceBits(this->face, PFV_MOUSTACHE,       this->ge), false);
			} else {
				/* Only for female faces or male faces without moustache */
				this->DrawFaceStringLabel(PFW_WIDGET_LIPS_MOUSTACHE,        STR_FACE_LIPS,      GetPlayerFaceBits(this->face, PFV_LIPS,            this->ge), false);
			}
			/* For all faces */
			this->DrawFaceStringLabel(PFW_WIDGET_HAS_GLASSES,           STR_FACE_GLASSES,     GetPlayerFaceBits(this->face, PFV_HAS_GLASSES,     this->ge), true );
			this->DrawFaceStringLabel(PFW_WIDGET_HAIR,                  STR_FACE_HAIR,        GetPlayerFaceBits(this->face, PFV_HAIR,            this->ge), false);
			this->DrawFaceStringLabel(PFW_WIDGET_EYEBROWS,              STR_FACE_EYEBROWS,    GetPlayerFaceBits(this->face, PFV_EYEBROWS,        this->ge), false);
			this->DrawFaceStringLabel(PFW_WIDGET_EYECOLOUR,             STR_FACE_EYECOLOUR,   GetPlayerFaceBits(this->face, PFV_EYE_COLOUR,      this->ge), false);
			this->DrawFaceStringLabel(PFW_WIDGET_GLASSES,               STR_FACE_GLASSES,     GetPlayerFaceBits(this->face, PFV_GLASSES,         this->ge), false);
			this->DrawFaceStringLabel(PFW_WIDGET_NOSE,                  STR_FACE_NOSE,        GetPlayerFaceBits(this->face, PFV_NOSE,            this->ge), false);
			this->DrawFaceStringLabel(PFW_WIDGET_CHIN,                  STR_FACE_CHIN,        GetPlayerFaceBits(this->face, PFV_CHIN,            this->ge), false);
			this->DrawFaceStringLabel(PFW_WIDGET_JACKET,                STR_FACE_JACKET,      GetPlayerFaceBits(this->face, PFV_JACKET,          this->ge), false);
			this->DrawFaceStringLabel(PFW_WIDGET_COLLAR,                STR_FACE_COLLAR,      GetPlayerFaceBits(this->face, PFV_COLLAR,          this->ge), false);
		}

		/* Draw the player face picture */
		DrawPlayerFace(this->face, GetPlayer((PlayerID)this->window_number)->player_color, 2, 16);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			/* Toggle size, advanced/simple face selection */
			case PFW_WIDGET_TOGGLE_LARGE_SMALL:
			case PFW_WIDGET_TOGGLE_LARGE_SMALL_BUTTON: {
				DoCommandP(0, 0, this->face, NULL, CMD_SET_PLAYER_FACE);

				/* Backup some data before deletion */
				int oldtop = this->top;     ///< current top position of the window before closing it
				int oldleft = this->left;   ///< current top position of the window before closing it
				bool adv = !this->advanced;
				Window *parent = this->parent;

				delete this;

				/* Open up the (toggled size) Face selection window at the same position as the previous */
				DoSelectPlayerFace(parent, adv, oldtop, oldleft);
			} break;


			/* OK button */
			case PFW_WIDGET_ACCEPT:
				DoCommandP(0, 0, this->face, NULL, CMD_SET_PLAYER_FACE);
				/* Fall-Through */

			/* Cancel button */
			case PFW_WIDGET_CANCEL:
				delete this;
				break;

			/* Load button */
			case PFW_WIDGET_LOAD:
				this->face = _player_face;
				ScaleAllPlayerFaceBits(this->face);
				ShowErrorMessage(INVALID_STRING_ID, STR_FACE_LOAD_DONE, 0, 0);
				this->UpdateData();
				this->SetDirty();
				break;

			/* 'Player face number' button, view and/or set player face number */
			case PFW_WIDGET_FACECODE:
				SetDParam(0, this->face);
				ShowQueryString(STR_JUST_INT, STR_FACE_FACECODE_CAPTION, 10 + 1, 0, this, CS_NUMERAL, QSF_NONE);
				break;

			/* Save button */
			case PFW_WIDGET_SAVE:
				_player_face = this->face;
				ShowErrorMessage(INVALID_STRING_ID, STR_FACE_SAVE_DONE, 0, 0);
				break;

			/* Toggle gender (male/female) button */
			case PFW_WIDGET_MALE:
			case PFW_WIDGET_FEMALE:
				SetPlayerFaceBits(this->face, PFV_GENDER, this->ge, widget - PFW_WIDGET_MALE);
				ScaleAllPlayerFaceBits(this->face);
				this->UpdateData();
				this->SetDirty();
				break;

			/* Randomize face button */
			case PFW_WIDGET_RANDOM_NEW_FACE:
				RandomPlayerFaceBits(this->face, this->ge, this->advanced);
				this->UpdateData();
				this->SetDirty();
				break;

			/* Toggle ethnicity (european/african) button */
			case PFW_WIDGET_ETHNICITY_EUR:
			case PFW_WIDGET_ETHNICITY_AFR:
				SetPlayerFaceBits(this->face, PFV_ETHNICITY, this->ge, widget - PFW_WIDGET_ETHNICITY_EUR);
				ScaleAllPlayerFaceBits(this->face);
				this->UpdateData();
				this->SetDirty();
				break;

			default:
				/* For all buttons from PFW_WIDGET_HAS_MOUSTACHE_EARRING to PFW_WIDGET_GLASSES_R is the same function.
				* Therefor is this combined function.
				* First it checks which PlayerFaceVariable will be change and then
				* a: invert the value for boolean variables
				* or b: it checks inside of IncreasePlayerFaceBits() if a left (_L) butten is pressed and then decrease else increase the variable */
				if (this->advanced && widget >= PFW_WIDGET_HAS_MOUSTACHE_EARRING && widget <= PFW_WIDGET_GLASSES_R) {
					PlayerFaceVariable pfv; // which PlayerFaceVariable shall be edited

					if (widget < PFW_WIDGET_EYECOLOUR_L) { // Bool buttons
						switch (widget - PFW_WIDGET_HAS_MOUSTACHE_EARRING) {
							default: NOT_REACHED();
							case 0: pfv = this->is_female ? PFV_HAS_TIE_EARRING : PFV_HAS_MOUSTACHE; break; // Has earring/moustache button
							case 1: pfv = PFV_HAS_GLASSES; break; // Has glasses button
						}
						SetPlayerFaceBits(this->face, pfv, this->ge, !GetPlayerFaceBits(this->face, pfv, this->ge));
						ScaleAllPlayerFaceBits(this->face);
					} else { // Value buttons
						switch ((widget - PFW_WIDGET_EYECOLOUR_L) / 3) {
							default: NOT_REACHED();
							case 0: pfv = PFV_EYE_COLOUR; break;  // Eye colour buttons
							case 1: pfv = PFV_CHIN; break;        // Chin buttons
							case 2: pfv = PFV_EYEBROWS; break;    // Eyebrows buttons
							case 3: pfv = this->is_moust_male ? PFV_MOUSTACHE : PFV_LIPS; break; // Moustache or lips buttons
							case 4: pfv = PFV_NOSE; break;        // Nose buttons
							case 5: pfv = PFV_HAIR; break;        // Hair buttons
							case 6: pfv = PFV_JACKET; break;      // Jacket buttons
							case 7: pfv = PFV_COLLAR; break;      // Collar buttons
							case 8: pfv = PFV_TIE_EARRING; break; // Tie/earring buttons
							case 9: pfv = PFV_GLASSES; break;     // Glasses buttons
						}
						/* 0 == left (_L), 1 == middle or 2 == right (_R) - button click */
						IncreasePlayerFaceBits(this->face, pfv, this->ge, (((widget - PFW_WIDGET_EYECOLOUR_L) % 3) != 0) ? 1 : -1);
					}
					this->UpdateData();
					this->SetDirty();
				}
				break;
		}
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;
		/* Set a new player face number */
		if (!StrEmpty(str)) {
			this->face = strtoul(str, NULL, 10);
			ScaleAllPlayerFaceBits(this->face);
			ShowErrorMessage(INVALID_STRING_ID, STR_FACE_FACECODE_SET, 0, 0);
			this->UpdateData();
			this->SetDirty();
		} else {
			ShowErrorMessage(INVALID_STRING_ID, STR_FACE_FACECODE_ERR, 0, 0);
		}
	}
};

/** normal/simple player face selection window description */
static const WindowDesc _select_player_face_desc = {
	WDP_AUTO, WDP_AUTO, 190, 163, 190, 163,
	WC_PLAYER_FACE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_player_face_widgets,
};

/** advanced player face selection window description */
static const WindowDesc _select_player_face_adv_desc = {
	WDP_AUTO, WDP_AUTO, 220, 220, 220, 220,
	WC_PLAYER_FACE, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_select_player_face_adv_widgets,
};

/**
 * Open the simple/advanced player face selection window
 *
 * @param player the player which face shall be edited
 * @param adv    simple or advanced player face selection window
 * @param top    previous top position of the window
 * @param left   previous left position of the window
 *
 * @pre is player a valid player
 */
static void DoSelectPlayerFace(Window *parent, bool adv, int top, int left)
{
	if (!IsValidPlayerID((PlayerID)parent->window_number)) return;

	if (BringWindowToFrontById(WC_PLAYER_FACE, parent->window_number)) return;
	new SelectPlayerFaceWindow(adv ? &_select_player_face_adv_desc : &_select_player_face_desc, parent, adv, top, left); // simple or advanced window
}


/* Names of the widgets. Keep them in the same order as in the widget array */
enum PlayerCompanyWindowWidgets {
	PCW_WIDGET_CLOSEBOX = 0,
	PCW_WIDGET_CAPTION,
	PCW_WIDGET_FACE,
	PCW_WIDGET_NEW_FACE,
	PCW_WIDGET_COLOR_SCHEME,
	PCW_WIDGET_PRESIDENT_NAME,
	PCW_WIDGET_COMPANY_NAME,
	PCW_WIDGET_BUILD_VIEW_HQ,
	PCW_WIDGET_RELOCATE_HQ,
	PCW_WIDGET_BUY_SHARE,
	PCW_WIDGET_SELL_SHARE,
	PCW_WIDGET_COMPANY_PASSWORD,
};

static const Widget _player_company_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_GREY,     0,    10,     0,    13, STR_00C5,                          STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_GREY,    11,   359,     0,    13, STR_7001,                          STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_GREY,     0,   359,    14,   157, 0x0,                               STR_NULL},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,    89,   158,   169, STR_7004_NEW_FACE,                 STR_7030_SELECT_NEW_FACE_FOR_PRESIDENT},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,    90,   179,   158,   169, STR_7005_COLOR_SCHEME,             STR_7031_CHANGE_THE_COMPANY_VEHICLE},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   180,   269,   158,   169, STR_7009_PRESIDENT_NAME,           STR_7032_CHANGE_THE_PRESIDENT_S},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   270,   359,   158,   169, STR_7008_COMPANY_NAME,             STR_7033_CHANGE_THE_COMPANY_NAME},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,    18,    29, STR_7072_VIEW_HQ,                  STR_7070_BUILD_COMPANY_HEADQUARTERS},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,    32,    43, STR_RELOCATE_HQ,                   STR_RELOCATE_COMPANY_HEADQUARTERS},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,     0,   179,   158,   169, STR_7077_BUY_25_SHARE_IN_COMPANY,  STR_7079_BUY_25_SHARE_IN_THIS_COMPANY},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   180,   359,   158,   169, STR_7078_SELL_25_SHARE_IN_COMPANY, STR_707A_SELL_25_SHARE_IN_THIS_COMPANY},
{ WWT_PUSHTXTBTN,   RESIZE_NONE,  COLOUR_GREY,   266,   355,   138,   149, STR_COMPANY_PASSWORD,              STR_COMPANY_PASSWORD_TOOLTIP},
{   WIDGETS_END},
};


/**
 * Draws text "Vehicles:" and number of all vehicle types, or "(none)"
 * @param player ID of player to print statistics of
 */
static void DrawPlayerVehiclesAmount(PlayerID player)
{
	const int x = 110;
	int y = 63;
	const Vehicle *v;
	uint train = 0;
	uint road  = 0;
	uint air   = 0;
	uint ship  = 0;

	DrawString(x, y, STR_7039_VEHICLES, TC_FROMSTRING);

	FOR_ALL_VEHICLES(v) {
		if (v->owner == player) {
			switch (v->type) {
				case VEH_TRAIN:    if (IsFrontEngine(v)) train++; break;
				case VEH_ROAD:     if (IsRoadVehFront(v)) road++; break;
				case VEH_AIRCRAFT: if (IsNormalAircraft(v)) air++; break;
				case VEH_SHIP:     ship++; break;
				default: break;
			}
		}
	}

	if (train + road + air + ship == 0) {
		DrawString(x + 70, y, STR_7042_NONE, TC_FROMSTRING);
	} else {
		if (train != 0) {
			SetDParam(0, train);
			DrawString(x + 70, y, STR_TRAINS, TC_FROMSTRING);
			y += 10;
		}

		if (road != 0) {
			SetDParam(0, road);
			DrawString(x + 70, y, STR_ROAD_VEHICLES, TC_FROMSTRING);
			y += 10;
		}

		if (air != 0) {
			SetDParam(0, air);
			DrawString(x + 70, y, STR_AIRCRAFT, TC_FROMSTRING);
			y += 10;
		}

		if (ship != 0) {
			SetDParam(0, ship);
			DrawString(x + 70, y, STR_SHIPS, TC_FROMSTRING);
		}
	}
}

int GetAmountOwnedBy(const Player *p, PlayerID owner)
{
	return (p->share_owners[0] == owner) +
				 (p->share_owners[1] == owner) +
				 (p->share_owners[2] == owner) +
				 (p->share_owners[3] == owner);
}

/**
 * Draws list of all companies with shares
 * @param p pointer to the Player structure
 */
static void DrawCompanyOwnerText(const Player *p)
{
	const Player *p2;
	uint num = 0;
	const byte height = GetCharacterHeight(FS_NORMAL);

	FOR_ALL_PLAYERS(p2) {
		uint amt = GetAmountOwnedBy(p, p2->index);
		if (amt != 0) {
			SetDParam(0, amt * 25);
			SetDParam(1, p2->index);

			DrawString(120, (num++) * height + 116, STR_707D_OWNED_BY, TC_FROMSTRING);
		}
	}
}

/**
 * Player company window event definition
 *
 * @param w window pointer
 * @param e event been triggered
 */
struct PlayerCompanyWindow : Window
{
	PlayerCompanyWindowWidgets query_widget;

	PlayerCompanyWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->caption_color = this->window_number;
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		const Player *p = GetPlayer((PlayerID)this->window_number);
		bool local = this->window_number == _local_player;

		this->SetWidgetHiddenState(PCW_WIDGET_NEW_FACE,       !local);
		this->SetWidgetHiddenState(PCW_WIDGET_COLOR_SCHEME,   !local);
		this->SetWidgetHiddenState(PCW_WIDGET_PRESIDENT_NAME, !local);
		this->SetWidgetHiddenState(PCW_WIDGET_COMPANY_NAME,   !local);
		this->widget[PCW_WIDGET_BUILD_VIEW_HQ].data = (local && p->location_of_HQ == 0) ? STR_706F_BUILD_HQ : STR_7072_VIEW_HQ;
		if (local && p->location_of_HQ != 0) this->widget[PCW_WIDGET_BUILD_VIEW_HQ].type = WWT_PUSHTXTBTN; //HQ is already built.
		this->SetWidgetDisabledState(PCW_WIDGET_BUILD_VIEW_HQ, !local && p->location_of_HQ == 0);
		this->SetWidgetHiddenState(PCW_WIDGET_RELOCATE_HQ,      !local || p->location_of_HQ == 0);
		this->SetWidgetHiddenState(PCW_WIDGET_BUY_SHARE,        local);
		this->SetWidgetHiddenState(PCW_WIDGET_SELL_SHARE,       local);
		this->SetWidgetHiddenState(PCW_WIDGET_COMPANY_PASSWORD, !local || !_networking);

		if (!local) {
			if (_settings_game.economy.allow_shares) { // Shares are allowed
				/* If all shares are owned by someone (none by nobody), disable buy button */
				this->SetWidgetDisabledState(PCW_WIDGET_BUY_SHARE, GetAmountOwnedBy(p, PLAYER_SPECTATOR) == 0 ||
						/* Only 25% left to buy. If the player is human, disable buying it up.. TODO issues! */
						(GetAmountOwnedBy(p, PLAYER_SPECTATOR) == 1 && !p->is_ai) ||
						/* Spectators cannot do anything of course */
						_local_player == PLAYER_SPECTATOR);

				/* If the player doesn't own any shares, disable sell button */
				this->SetWidgetDisabledState(PCW_WIDGET_SELL_SHARE, (GetAmountOwnedBy(p, _local_player) == 0) ||
						/* Spectators cannot do anything of course */
						_local_player == PLAYER_SPECTATOR);
			} else { // Shares are not allowed, disable buy/sell buttons
				this->DisableWidget(PCW_WIDGET_BUY_SHARE);
				this->DisableWidget(PCW_WIDGET_SELL_SHARE);
			}
		}

		SetDParam(0, p->index);
		SetDParam(1, p->index);

		this->DrawWidgets();

		/* Player face */
		DrawPlayerFace(p->face, p->player_color, 2, 16);

		/* "xxx (Manager)" */
		SetDParam(0, p->index);
		DrawStringMultiCenter(48, 141, STR_7037_PRESIDENT, MAX_LENGTH_PRESIDENT_NAME_PIXELS);

		/* "Inaugurated:" */
		SetDParam(0, p->inaugurated_year);
		DrawString(110, 23, STR_7038_INAUGURATED, TC_FROMSTRING);

		/* "Colour scheme:" */
		DrawString(110, 43, STR_7006_COLOR_SCHEME, TC_FROMSTRING);
		/* Draw company-colour bus */
		DrawSprite(SPR_VEH_BUS_SW_VIEW, PLAYER_SPRITE_COLOR(p->index), 215, 44);

		/* "Vehicles:" */
		DrawPlayerVehiclesAmount((PlayerID)this->window_number);

		/* "Company value:" */
		SetDParam(0, CalculateCompanyValue(p));
		DrawString(110, 106, STR_7076_COMPANY_VALUE, TC_FROMSTRING);

		/* Shares list */
		DrawCompanyOwnerText(p);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case PCW_WIDGET_NEW_FACE: DoSelectPlayerFace(this, false); break;

			case PCW_WIDGET_COLOR_SCHEME:
				if (BringWindowToFrontById(WC_PLAYER_COLOR, this->window_number)) break;
				new SelectPlayerLiveryWindow(&_select_player_livery_desc, (PlayerID)this->window_number);
				break;

			case PCW_WIDGET_PRESIDENT_NAME:
				this->query_widget = PCW_WIDGET_PRESIDENT_NAME;
				SetDParam(0, this->window_number);
				ShowQueryString(STR_PLAYER_NAME, STR_700B_PRESIDENT_S_NAME, MAX_LENGTH_PRESIDENT_NAME_BYTES, MAX_LENGTH_PRESIDENT_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case PCW_WIDGET_COMPANY_NAME:
				this->query_widget = PCW_WIDGET_COMPANY_NAME;
				SetDParam(0, this->window_number);
				ShowQueryString(STR_COMPANY_NAME, STR_700A_COMPANY_NAME, MAX_LENGTH_COMPANY_NAME_BYTES, MAX_LENGTH_COMPANY_NAME_PIXELS, this, CS_ALPHANUMERAL, QSF_ENABLE_DEFAULT);
				break;

			case PCW_WIDGET_BUILD_VIEW_HQ: {
				TileIndex tile = GetPlayer((PlayerID)this->window_number)->location_of_HQ;
				if (tile == 0) {
					if ((byte)this->window_number != _local_player) return;
					SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, VHM_RECT, this);
					SetTileSelectSize(2, 2);
					this->LowerWidget(PCW_WIDGET_BUILD_VIEW_HQ);
					this->InvalidateWidget(PCW_WIDGET_BUILD_VIEW_HQ);
				} else {
					if (_ctrl_pressed) {
						ShowExtraViewPortWindow(tile);
					} else {
						ScrollMainWindowToTile(tile);
					}
				}
				break;
			}

			case PCW_WIDGET_RELOCATE_HQ:
				SetObjectToPlaceWnd(SPR_CURSOR_HQ, PAL_NONE, VHM_RECT, this);
				SetTileSelectSize(2, 2);
				this->LowerWidget(PCW_WIDGET_RELOCATE_HQ);
				this->InvalidateWidget(PCW_WIDGET_RELOCATE_HQ);
				break;

			case PCW_WIDGET_BUY_SHARE:
				DoCommandP(0, this->window_number, 0, NULL, CMD_BUY_SHARE_IN_COMPANY | CMD_MSG(STR_707B_CAN_T_BUY_25_SHARE_IN_THIS));
				break;

			case PCW_WIDGET_SELL_SHARE:
				DoCommandP(0, this->window_number, 0, NULL, CMD_SELL_SHARE_IN_COMPANY | CMD_MSG(STR_707C_CAN_T_SELL_25_SHARE_IN));
				break;

#ifdef ENABLE_NETWORK
			case PCW_WIDGET_COMPANY_PASSWORD:
				if (this->window_number == _local_player) ShowNetworkCompanyPasswordWindow(this);
				break;
#endif /* ENABLE_NETWORK */
		}
	}

	virtual void OnHundredthTick()
	{
		/* redraw the window every now and then */
		this->SetDirty();
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		if (DoCommandP(tile, 0, 0, NULL, CMD_BUILD_COMPANY_HQ | CMD_NO_WATER | CMD_MSG(STR_7071_CAN_T_BUILD_COMPANY_HEADQUARTERS)))
			ResetObjectToPlace();
			this->widget[PCW_WIDGET_BUILD_VIEW_HQ].type = WWT_PUSHTXTBTN; // this button can now behave as a normal push button
			this->RaiseButtons();
	}

	virtual void OnPlaceObjectAbort()
	{
		this->RaiseButtons();
	}

	virtual void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;

		_cmd_text = str;
		switch (this->query_widget) {
			default: NOT_REACHED();

			case PCW_WIDGET_PRESIDENT_NAME:
				DoCommandP(0, 0, 0, NULL, CMD_RENAME_PRESIDENT | CMD_MSG(STR_700D_CAN_T_CHANGE_PRESIDENT));
				break;

			case PCW_WIDGET_COMPANY_NAME:
				DoCommandP(0, 0, 0, NULL, CMD_RENAME_COMPANY | CMD_MSG(STR_700C_CAN_T_CHANGE_COMPANY_NAME));
				break;
		}
	}
};

static const WindowDesc _player_company_desc = {
	WDP_AUTO, WDP_AUTO, 360, 170, 360, 170,
	WC_COMPANY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET | WDF_UNCLICK_BUTTONS,
	_player_company_widgets,
};

void ShowPlayerCompany(PlayerID player)
{
	if (!IsValidPlayerID(player)) return;

	AllocateWindowDescFront<PlayerCompanyWindow>(&_player_company_desc, player);
}



struct BuyCompanyWindow : Window {
	BuyCompanyWindow(const WindowDesc *desc, WindowNumber window_number) : Window(desc, window_number)
	{
		this->FindWindowPlacementAndResize(desc);
	}

	virtual void OnPaint()
	{
		Player *p = GetPlayer((PlayerID)this->window_number);
		SetDParam(0, STR_COMPANY_NAME);
		SetDParam(1, p->index);
		this->DrawWidgets();

		DrawPlayerFace(p->face, p->player_color, 2, 16);

		SetDParam(0, p->index);
		SetDParam(1, p->bankrupt_value);
		DrawStringMultiCenter(214, 65, STR_705B_WE_ARE_LOOKING_FOR_A_TRANSPORT, 238);
	}

	virtual void OnClick(Point pt, int widget)
	{
		switch (widget) {
			case 3:
				delete this;
				break;

			case 4:
				DoCommandP(0, this->window_number, 0, NULL, CMD_BUY_COMPANY | CMD_MSG(STR_7060_CAN_T_BUY_COMPANY));
				break;
		}
	}
};

static const Widget _buy_company_widgets[] = {
{   WWT_CLOSEBOX,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,     0,    10,     0,    13, STR_00C5,              STR_018B_CLOSE_WINDOW},
{    WWT_CAPTION,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,    11,   333,     0,    13, STR_00B3_MESSAGE_FROM, STR_018C_WINDOW_TITLE_DRAG_THIS},
{      WWT_PANEL,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,     0,   333,    14,   136, 0x0,                   STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,   148,   207,   117,   128, STR_00C9_NO,           STR_NULL},
{    WWT_TEXTBTN,   RESIZE_NONE,  COLOUR_LIGHT_BLUE,   218,   277,   117,   128, STR_00C8_YES,          STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _buy_company_desc = {
	153, 171, 334, 137, 334, 137,
	WC_BUY_COMPANY, WC_NONE,
	WDF_STD_TOOLTIPS | WDF_STD_BTN | WDF_DEF_WIDGET,
	_buy_company_widgets,
};


void ShowBuyCompanyDialog(uint player)
{
	AllocateWindowDescFront<BuyCompanyWindow>(&_buy_company_desc, player);
}

/********** HIGHSCORE and ENDGAME windows */

extern StringID EndGameGetPerformanceTitleFromValue(uint value);


struct EndGameHighScoreBaseWindow : Window
{
	uint32 background_img;
	int8 rank;

	EndGameHighScoreBaseWindow(const WindowDesc *desc) : Window(desc)
	{
	}

	/* Always draw a maximized window and within there the centered background */
	void SetupHighScoreEndWindow(uint *x, uint *y)
	{
		/* resize window to "full-screen" */
		this->width = _screen.width;
		this->height = _screen.height;
		this->widget[0].right = this->width - 1;
		this->widget[0].bottom = this->height - 1;

		this->DrawWidgets();

		/* Center Highscore/Endscreen background */
		*x = max(0, (_screen.width  / 2) - (640 / 2));
		*y = max(0, (_screen.height / 2) - (480 / 2));
		for (uint i = 0; i < 10; i++) { // the image is split into 10 50px high parts
			DrawSprite(this->background_img + i, PAL_NONE, *x, *y + (i * 50));
		}
	}

	virtual void OnClick(Point pt, int widget)
	{
		delete this;
	}
};

/** End game window shown at the end of the game */
struct EndGameWindow : EndGameHighScoreBaseWindow {
	EndGameWindow(const WindowDesc *desc) : EndGameHighScoreBaseWindow(desc)
	{
		/* Pause in single-player to have a look at the highscore at your own leisure */
		if (!_networking) DoCommandP(0, 1, 0, NULL, CMD_PAUSE);

		this->background_img = SPR_TYCOON_IMG1_BEGIN;

		if (_local_player != PLAYER_SPECTATOR) {
			const Player *p = GetPlayer(_local_player);
			if (p->old_economy[0].performance_history == SCORE_MAX) {
				this->background_img = SPR_TYCOON_IMG2_BEGIN;
			}
		}

		/* In a network game show the endscores of the custom difficulty 'network' which is the last one
		 * as well as generate a TOP5 of that game, and not an all-time top5. */
		if (_networking) {
			this->window_number = lengthof(_highscore_table) - 1;
			this->rank = SaveHighScoreValueNetwork();
		} else {
			/* in single player _local player is always valid */
			const Player *p = GetPlayer(_local_player);
			this->window_number = _settings_game.difficulty.diff_level;
			this->rank = SaveHighScoreValue(p);
		}

		MarkWholeScreenDirty();
	}

	~EndGameWindow()
	{
		if (!_networking) DoCommandP(0, 0, 0, NULL, CMD_PAUSE); // unpause
		ShowHighscoreTable(this->window_number, this->rank);
	}

	virtual void OnPaint()
	{
		const Player *p;
		uint x, y;

		this->SetupHighScoreEndWindow(&x, &y);

		if (!IsValidPlayerID(_local_player)) return;

		p = GetPlayer(_local_player);
		/* We need to get performance from last year because the image is shown
		 * at the start of the new year when these things have already been copied */
		if (this->background_img == SPR_TYCOON_IMG2_BEGIN) { // Tycoon of the century \o/
			SetDParam(0, p->index);
			SetDParam(1, p->index);
			SetDParam(2, EndGameGetPerformanceTitleFromValue(p->old_economy[0].performance_history));
			DrawStringMultiCenter(x + (640 / 2), y + 107, STR_021C_OF_ACHIEVES_STATUS, 640);
		} else {
			SetDParam(0, p->index);
			SetDParam(1, EndGameGetPerformanceTitleFromValue(p->old_economy[0].performance_history));
			DrawStringMultiCenter(x + (640 / 2), y + 157, STR_021B_ACHIEVES_STATUS, 640);
		}
	}
};

struct HighScoreWindow : EndGameHighScoreBaseWindow
{
	HighScoreWindow(const WindowDesc *desc, int difficulty, int8 ranking) : EndGameHighScoreBaseWindow(desc)
	{
		/* pause game to show the chart */
		if (!_networking) DoCommandP(0, 1, 0, NULL, CMD_PAUSE);

		/* Close all always on-top windows to get a clean screen */
		if (_game_mode != GM_MENU) HideVitalWindows();

		MarkWholeScreenDirty();
		this->window_number = difficulty; // show highscore chart for difficulty...
		this->background_img = SPR_HIGHSCORE_CHART_BEGIN; // which background to show
		this->rank = ranking;
	}

	~HighScoreWindow()
	{
		if (_game_mode != GM_MENU) ShowVitalWindows();

		if (!_networking) DoCommandP(0, 0, 0, NULL, CMD_PAUSE); // unpause
	}

	virtual void OnPaint()
	{
		const HighScore *hs = _highscore_table[this->window_number];
		uint x, y;

		this->SetupHighScoreEndWindow(&x, &y);

		SetDParam(0, _settings_client.gui.ending_year);
		SetDParam(1, this->window_number + STR_6801_EASY);
		DrawStringMultiCenter(x + (640 / 2), y + 62, !_networking ? STR_0211_TOP_COMPANIES_WHO_REACHED : STR_TOP_COMPANIES_NETWORK_GAME, 500);

		/* Draw Highscore peepz */
		for (uint8 i = 0; i < lengthof(_highscore_table[0]); i++) {
			SetDParam(0, i + 1);
			DrawString(x + 40, y + 140 + (i * 55), STR_0212, TC_BLACK);

			if (hs[i].company[0] != '\0') {
				TextColour colour = (this->rank == i) ? TC_RED : TC_BLACK; // draw new highscore in red

				DoDrawString(hs[i].company, x + 71, y + 140 + (i * 55), colour);
				SetDParam(0, hs[i].title);
				SetDParam(1, hs[i].score);
				DrawString(x + 71, y + 160 + (i * 55), STR_HIGHSCORE_STATS, colour);
			}
		}
	}
};

static const Widget _highscore_widgets[] = {
{      WWT_PANEL, RESIZE_NONE,  COLOUR_END, 0, 640, 0, 480, 0x0, STR_NULL},
{   WIDGETS_END},
};

static const WindowDesc _highscore_desc = {
	0, 0, 641, 481, 641, 481,
	WC_HIGHSCORE, WC_NONE,
	0,
	_highscore_widgets,
};

static const WindowDesc _endgame_desc = {
	0, 0, 641, 481, 641, 481,
	WC_ENDSCREEN, WC_NONE,
	0,
	_highscore_widgets,
};

/** Show the highscore table for a given difficulty. When called from
 * endgame ranking is set to the top5 element that was newly added
 * and is thus highlighted */
void ShowHighscoreTable(int difficulty, int8 ranking)
{
	DeleteWindowByClass(WC_HIGHSCORE);
	new HighScoreWindow(&_highscore_desc, difficulty, ranking);
}

/** Show the endgame victory screen in 2050. Update the new highscore
 * if it was high enough */
void ShowEndGameChart()
{
	/* Dedicated server doesn't need the highscore window */
	if (_network_dedicated) return;

	HideVitalWindows();
	DeleteWindowByClass(WC_ENDSCREEN);
	new EndGameWindow(&_endgame_desc);
}
