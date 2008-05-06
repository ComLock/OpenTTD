/* $Id$ */

/** @file window_gui.h Functions, definitions and such used only by the GUI. */

#ifndef WINDOW_GUI_H
#define WINDOW_GUI_H

#include "core/bitmath_func.hpp"
#include "vehicle_type.h"
#include "viewport_type.h"
#include "player_type.h"
#include "strings_type.h"
#include "core/alloc_type.hpp"

/**
 * The maximum number of windows that can be opened.
 */
static const int MAX_NUMBER_OF_WINDOWS = 25;

typedef void WindowProc(Window *w, WindowEvent *e);

/* How the resize system works:
    First, you need to add a WWT_RESIZEBOX to the widgets, and you need
     to add the flag WDF_RESIZABLE to the window. Now the window is ready
     to resize itself.
    As you may have noticed, all widgets have a RESIZE_XXX in their line.
     This lines controls how the widgets behave on resize. RESIZE_NONE means
     it doesn't do anything. Any other option let's one of the borders
     move with the changed width/height. So if a widget has
     RESIZE_RIGHT, and the window is made 5 pixels wider by the user,
     the right of the window will also be made 5 pixels wider.
    Now, what if you want to clamp a widget to the bottom? Give it the flag
     RESIZE_TB. This is RESIZE_TOP + RESIZE_BOTTOM. Now if the window gets
     5 pixels bigger, both the top and bottom gets 5 bigger, so the whole
     widgets moves downwards without resizing, and appears to be clamped
     to the bottom. Nice aint it?
   You should know one more thing about this system. Most windows can't
    handle an increase of 1 pixel. So there is a step function, which
    let the windowsize only be changed by X pixels. You configure this
    after making the window, like this:
      w->resize.step_height = 10;
    Now the window will only change in height in steps of 10.
   You can also give a minimum width and height. The default value is
    the default height/width of the window itself. You can change this
    AFTER window - creation, with:
     w->resize.width or w->resize.height.
   That was all.. good luck, and enjoy :) -- TrueLight */

enum ResizeFlag {
	RESIZE_NONE   = 0,  ///< no resize required

	RESIZE_LEFT   = 1,  ///< left resize flag
	RESIZE_RIGHT  = 2,  ///< rigth resize flag
	RESIZE_TOP    = 4,  ///< top resize flag
	RESIZE_BOTTOM = 8,  ///< bottom resize flag

	RESIZE_LR     = RESIZE_LEFT  | RESIZE_RIGHT,   ///<  combination of left and right resize flags
	RESIZE_RB     = RESIZE_RIGHT | RESIZE_BOTTOM,  ///<  combination of right and bottom resize flags
	RESIZE_TB     = RESIZE_TOP   | RESIZE_BOTTOM,  ///<  combination of top and bottom resize flags
	RESIZE_LRB    = RESIZE_LEFT  | RESIZE_RIGHT  | RESIZE_BOTTOM, ///< combination of left, right and bottom resize flags
	RESIZE_LRTB   = RESIZE_LEFT  | RESIZE_RIGHT  | RESIZE_TOP | RESIZE_BOTTOM,  ///<  combination of all resize flags
	RESIZE_RTB    = RESIZE_RIGHT | RESIZE_TOP    | RESIZE_BOTTOM, ///<  combination of right, top and bottom resize flag

	/* The following flags are used by the system to specify what is disabled, hidden, or clicked
	 * They are used in the same place as the above RESIZE_x flags, Widget visual_flags.
	 * These states are used in exceptions. If nothing is specified, they will indicate
	 * Enabled, visible or unclicked widgets*/
	WIDG_DISABLED = 4,  ///< widget is greyed out, not available
	WIDG_HIDDEN   = 5,  ///< widget is made invisible
	WIDG_LOWERED  = 6,  ///< widget is paint lowered, a pressed button in fact
};

enum {
	WIDGET_LIST_END = -1, ///< indicate the end of widgets' list for vararg functions
};

/**
 * Window widget data structure
 */
struct Widget {
	byte type;                        ///< Widget type, see WindowWidgetTypes
	byte display_flags;               ///< Resize direction, alignment, etc. during resizing, see ResizeFlags
	byte color;                       ///< Widget colour, see docs/ottd-colourtext-palette.png
	int16 left, right, top, bottom;   ///< The position offsets inside the window
	uint16 data;                      ///< The String/Image or special code (list-matrixes) of a widget
	StringID tooltips;                ///< Tooltips that are shown when rightclicking on a widget
};

/**
 * Flags to describe the look of the frame
 */
enum FrameFlags {
	FR_NONE         =  0,
	FR_TRANSPARENT  =  1 << 0,  ///< Makes the background transparent if set
	FR_BORDERONLY   =  1 << 4,  ///< Draw border only, no background
	FR_LOWERED      =  1 << 5,  ///< If set the frame is lowered and the background color brighter (ie. buttons when pressed)
	FR_DARKENED     =  1 << 6,  ///< If set the background is darker, allows for lowered frames with normal background color when used with FR_LOWERED (ie. dropdown boxes)
};

DECLARE_ENUM_AS_BIT_SET(FrameFlags);

/* wiget.cpp */
void DrawFrameRect(int left, int top, int right, int bottom, int color, FrameFlags flags);

/**
 * Available window events
 */
enum WindowEventCodes {
	WE_CREATE,       ///< Initialize the Window
	WE_DESTROY,      ///< Prepare for deletion of the window
	WE_PAINT,        ///< Repaint the window contents
	WE_KEYPRESS,     ///< Key pressed
	WE_CLICK,        ///< Left mouse button click
	WE_DOUBLE_CLICK, ///< Left mouse button double click
	WE_RCLICK,       ///< Right mouse button click
	WE_MOUSEOVER,
	WE_MOUSELOOP,
	WE_MOUSEWHEEL,
	WE_TICK,         ///< Regularly occurring event (about once every 20 seconds orso, 10 days) for slowly changing content (typically list sorting)
	WE_4,            ///< Regularly occurring event for updating continuously changing window content (other than view ports), or timer expiring
	WE_TIMEOUT,
	WE_PLACE_OBJ,
	WE_ABORT_PLACE_OBJ,
	WE_ON_EDIT_TEXT,
	WE_ON_EDIT_TEXT_CANCEL,
	WE_POPUPMENU_SELECT,
	WE_POPUPMENU_OVER,
	WE_DRAGDROP,
	WE_PLACE_DRAG,
	WE_PLACE_MOUSEUP,
	WE_PLACE_PRESIZE,
	WE_DROPDOWN_SELECT,
	WE_RESIZE,          ///< Request to resize the window, @see WindowEvent.we.resize
	WE_MESSAGE,         ///< Receipt of a message from another window. @see WindowEvent.we.message, SendWindowMessage(), SendWindowMessageClass()
	WE_SCROLL,
	WE_INVALIDATE_DATA, ///< Notification that data displayed by the window is obsolete
	WE_CTRL_CHANGED,    ///< CTRL key has changed state
};

/**
 * Data structures for additional data associated with a window event
 * @see WindowEventCodes
 */
struct WindowEvent {
	byte event;
	union {
		struct {
			void *data;
		} create;

		struct {
			Point pt;
			int widget;
		} click;

		struct {
			Point pt;
			TileIndex tile;
			TileIndex starttile;
			ViewportPlaceMethod select_method;
			byte select_proc;
		} place;

		struct {
			Point pt;
			int widget;
		} dragdrop;

		struct {
			Point size;
			Point diff;
		} sizing;

		struct {
			char *str;
		} edittext;

		struct {
			Point pt;
		} popupmenu;

		struct {
			int button;
			int index;
		} dropdown;

		struct {
			Point pt;
			int widget;
		} mouseover;

		struct {
			bool cont;      ///< continue the search? (default true)
			uint16 key;     ///< 16-bit Unicode value of the key
			uint16 keycode; ///< untranslated key (including shift-state)
		} keypress;

		struct {
			int msg;      ///< message to be sent
			int wparam;   ///< additional message-specific information
			int lparam;   ///< additional message-specific information
		} message;

		struct {
			Point delta;   ///< delta position against position of last call
		} scroll;

		struct {
			int wheel;     ///< how much was 'wheel'd'
		} wheel;

		struct {
			bool cont;     ///< continue the search? (default true)
		} ctrl;
	} we;
};

/**
 * High level window description
 */
struct WindowDesc {
	int16 left;             ///< Prefered x position of left edge of the window, @see WindowDefaultPosition()
	int16 top;              ///< Prefered y position of the top of the window, @see WindowDefaultPosition()
	int16 minimum_width;    ///< Minimal width of the window
	int16 minimum_height;   ///< Minimal height of the window
	int16 default_width;    ///< Prefered initial width of the window
	int16 default_height;   ///< Prefered initial height of the window
	WindowClass cls;        ///< Class of the window, @see WindowClass
	WindowClass parent_cls; ///< Class of the parent window, @see WindowClass
	uint32 flags;           ///< Flags, @see WindowDefaultFlags
	const Widget *widgets;  ///< List of widgets with their position and size for the window
	WindowProc *proc;       ///< Window event handler function for the window
};

/**
 * Window default widget/window handling flags
 */
enum WindowDefaultFlag {
	WDF_STD_TOOLTIPS    =   1 << 0, ///< use standard routine when displaying tooltips
	WDF_DEF_WIDGET      =   1 << 1, ///< Default widget control for some widgets in the on click event, @see DispatchLeftClickEvent()
	WDF_STD_BTN         =   1 << 2, ///< Default handling for close and titlebar widgets (widget no 0 and 1)

	WDF_UNCLICK_BUTTONS =   1 << 4, ///< Unclick buttons when the window event times out
	WDF_STICKY_BUTTON   =   1 << 5, ///< Set window to sticky mode; they are not closed unless closed with 'X' (widget 2)
	WDF_RESIZABLE       =   1 << 6, ///< Window can be resized
	WDF_MODAL           =   1 << 7, ///< The window is a modal child of some other window, meaning the parent is 'inactive'
};

/**
 * Special values for 'left' and 'top' to cause a specific placement
 */
enum WindowDefaultPosition {
	WDP_AUTO      = -1, ///< Find a place automatically
	WDP_CENTER    = -2, ///< Center the window (left/right or top/bottom)
	WDP_ALIGN_TBR = -3, ///< Align the right side of the window with the right side of the main toolbar
	WDP_ALIGN_TBL = -4, ///< Align the left side of the window with the left side of the main toolbar
};

#define WP(ptr, str) (*(str*)(ptr)->custom)

/**
 * Scrollbar data structure
 */
struct Scrollbar {
	uint16 count;  ///< Number of elements in the list
	uint16 cap;    ///< Number of visible elements of the scroll bar
	uint16 pos;
};

/**
 * Data structure for resizing a window
 */
struct ResizeInfo {
	uint width;       ///< Minimum allowed width of the window
	uint height;      ///< Minimum allowed height of the window
	uint step_width;  ///< Step-size of width resize changes
	uint step_height; ///< Step-size of height resize changes
};

/**
 * Message data structure for messages sent between winodows
 * @see SendWindowMessageW()
 */
struct WindowMessage {
	int msg;
	int wparam;
	int lparam;
};

/**
 * Data structure for an opened window
 */
struct Window : ZeroedMemoryAllocator {
private:
	WindowProc *wndproc;   ///< Event handler function for the window. Do not use directly, call HandleWindowEvent() instead.

public:
	Window(WindowProc *proc) : wndproc(proc) {}
	virtual ~Window();

	uint16 flags4;              ///< Window flags, @see WindowFlags
	WindowClass window_class;   ///< Window class
	WindowNumber window_number; ///< Window number within the window class

	int left;   ///< x position of left edge of the window
	int top;    ///< y position of top edge of the window
	int width;  ///< width of the window (number of pixels to the right in x direction)
	int height; ///< Height of the window (number of pixels down in y direction)

	Scrollbar hscroll;  ///< Horizontal scroll bar
	Scrollbar vscroll;  ///< First vertical scroll bar
	Scrollbar vscroll2; ///< Second vertical scroll bar
	ResizeInfo resize;  ///< Resize information

	byte caption_color; ///< Background color of the window caption, contains PlayerID

	ViewPort *viewport;    ///< Pointer to viewport, if present
	const Widget *original_widget; ///< Original widget layout, copied from WindowDesc
	Widget *widget;        ///< Widgets of the window
	uint widget_count;     ///< Number of widgets of the window
	uint32 desc_flags;     ///< Window/widgets default flags setting, @see WindowDefaultFlag

	WindowMessage message; ///< Buffer for storing received messages (for communication between different window events)
	Window *parent;        ///< Parent window
	byte custom[WINDOW_CUSTOM_SIZE]; ///< Additional data depending on window type

	void HandleButtonClick(byte widget);

	void SetWidgetDisabledState(byte widget_index, bool disab_stat);
	void DisableWidget(byte widget_index);
	void EnableWidget(byte widget_index);
	bool IsWidgetDisabled(byte widget_index) const;
	void SetWidgetHiddenState(byte widget_index, bool hidden_stat);
	void HideWidget(byte widget_index);
	void ShowWidget(byte widget_index);
	bool IsWidgetHidden(byte widget_index) const;
	void SetWidgetLoweredState(byte widget_index, bool lowered_stat);
	void ToggleWidgetLoweredState(byte widget_index);
	void LowerWidget(byte widget_index);
	void RaiseWidget(byte widget_index);
	bool IsWidgetLowered(byte widget_index) const;

	void RaiseButtons();
	void CDECL SetWidgetsDisabledState(bool disab_stat, int widgets, ...);
	void CDECL SetWidgetsHiddenState(bool hidden_stat, int widgets, ...);
	void CDECL SetWidgetsLoweredState(bool lowered_stat, int widgets, ...);
	void InvalidateWidget(byte widget_index) const;

	void SetDirty() const;

	virtual void HandleWindowEvent(WindowEvent *e);
};

struct menu_d {
	byte item_count;      ///< follow_vehicle
	byte sel_index;       ///< scrollpos_x
	byte main_button;     ///< scrollpos_y
	byte action_id;
	StringID string_id;   ///< unk30
	uint16 checked_items; ///< unk32
	byte disabled_items;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(menu_d));

struct def_d {
	int16 data_1, data_2, data_3;
	int16 data_4, data_5;
	bool close;
	byte byte_1;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(def_d));

struct void_d {
	void *data;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(void_d));

struct tree_d {
	uint16 base;
	uint16 count;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(tree_d));

struct tooltips_d {
	StringID string_id;
	byte paramcount;
	uint64 params[5];
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(tooltips_d));

struct depot_d {
	VehicleID sel;
	VehicleType type;
	bool generate_list;
	uint16 engine_list_length;
	uint16 wagon_list_length;
	uint16 engine_count;
	uint16 wagon_count;
	Vehicle **vehicle_list;
	Vehicle **wagon_list;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(depot_d));

struct vehicledetails_d {
	byte tab;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(vehicledetails_d));

struct smallmap_d {
	int32 scroll_x;
	int32 scroll_y;
	int32 subscroll;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(smallmap_d));

struct vp_d {
	VehicleID follow_vehicle;
	int32 scrollpos_x;
	int32 scrollpos_y;
	int32 dest_scrollpos_x;
	int32 dest_scrollpos_y;
	ViewPort vp_data;          ///< Screen position and zoom of the viewport
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(vp_d));

struct highscore_d {
	uint32 background_img;
	int8 rank;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(highscore_d));

struct scroller_d {
	int height;
	uint16 counter;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(scroller_d));

enum SortListFlags {
	VL_NONE    = 0,      ///< no sort
	VL_DESC    = 1 << 0, ///< sort descending or ascending
	VL_RESORT  = 1 << 1, ///< instruct the code to resort the list in the next loop
	VL_REBUILD = 1 << 2, ///< create sort-listing to use for qsort and friends
	VL_END     = 1 << 3,
};

DECLARE_ENUM_AS_BIT_SET(SortListFlags);

struct Listing {
	bool order;    ///< Ascending/descending
	byte criteria; ///< Sorting criteria
};

struct list_d {
	uint16 list_length;  ///< length of the list being sorted
	byte sort_type;      ///< what criteria to sort on
	SortListFlags flags; ///< used to control sorting/resorting/etc.
	uint16 resort_timer; ///< resort list after a given amount of ticks if set
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(list_d));

struct message_d {
	int msg;
	int wparam;
	int lparam;
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(message_d));

struct vehiclelist_d {
	const Vehicle** sort_list;  // List of vehicles (sorted)
	Listing *_sorting;          // pointer to the appropiate subcategory of _sorting
	uint16 length_of_sort_list; // Keeps track of how many vehicle pointers sort list got space for
	VehicleType vehicle_type;   // The vehicle type that is sorted
	list_d l;                   // General list struct
};
assert_compile(WINDOW_CUSTOM_SIZE >= sizeof(vehiclelist_d));

/****************** THESE ARE NOT WIDGET TYPES!!!!! *******************/
enum WindowWidgetBehaviours {
	WWB_PUSHBUTTON  = 1 << 5,

	WWB_MASK        = 0xE0,
};


/**
 * Window widget types
 */
enum WindowWidgetTypes {
	WWT_EMPTY,      ///< Empty widget, place holder to reserve space in widget array

	WWT_PANEL,      ///< Simple depressed panel
	WWT_INSET,      ///< Pressed (inset) panel, most commonly used as combo box _text_ area
	WWT_IMGBTN,     ///< Button with image
	WWT_IMGBTN_2,   ///< Button with diff image when clicked

	WWT_TEXTBTN,    ///< Button with text
	WWT_TEXTBTN_2,  ///< Button with diff text when clicked
	WWT_LABEL,      ///< Centered label
	WWT_TEXT,       ///< Pure simple text
	WWT_MATRIX,     ///< List of items underneath each other
	WWT_SCROLLBAR,  ///< Vertical scrollbar
	WWT_FRAME,      ///< Frame
	WWT_CAPTION,    ///< Window caption (window title between closebox and stickybox)

	WWT_HSCROLLBAR, ///< Horizontal scrollbar
	WWT_STICKYBOX,  ///< Sticky box (normally at top-right of a window)
	WWT_SCROLL2BAR, ///< 2nd vertical scrollbar
	WWT_RESIZEBOX,  ///< Resize box (normally at bottom-right of a window)
	WWT_CLOSEBOX,   ///< Close box (at top-left of a window)
	WWT_DROPDOWN,   ///< Raised drop down list (regular)
	WWT_DROPDOWNIN, ///< Inset drop down list (used on game options only)
	WWT_EDITBOX,    ///< a textbox for typing (don't forget to call ShowOnScreenKeyboard() when clicked)
	WWT_LAST,       ///< Last Item. use WIDGETS_END to fill up padding!!

	WWT_MASK = 0x1F,

	WWT_PUSHBTN     = WWT_PANEL   | WWB_PUSHBUTTON,
	WWT_PUSHTXTBTN  = WWT_TEXTBTN | WWB_PUSHBUTTON,
	WWT_PUSHIMGBTN  = WWT_IMGBTN  | WWB_PUSHBUTTON,
};

#define WIDGETS_END WWT_LAST,   RESIZE_NONE,     0,     0,     0,     0,     0, 0, STR_NULL

/**
 * Window flags
 */
enum WindowFlags {
	WF_TIMEOUT_SHL       = 0,       ///< Window timeout counter shift
	WF_TIMEOUT_MASK      = 7,       ///< Window timeout counter bit mask (3 bits), @see WF_TIMEOUT_SHL
	WF_DRAGGING          = 1 <<  3, ///< Window is being dragged
	WF_SCROLL_UP         = 1 <<  4, ///< Upper scroll button has been pressed, @see ScrollbarClickHandler()
	WF_SCROLL_DOWN       = 1 <<  5, ///< Lower scroll button has been pressed, @see ScrollbarClickHandler()
	WF_SCROLL_MIDDLE     = 1 <<  6, ///< Scrollbar scrolling, @see ScrollbarClickHandler()
	WF_HSCROLL           = 1 <<  7,
	WF_SIZING            = 1 <<  8,
	WF_STICKY            = 1 <<  9, ///< Window is made sticky by user

	WF_DISABLE_VP_SCROLL = 1 << 10, ///< Window does not do autoscroll, @see HandleAutoscroll()

	WF_WHITE_BORDER_ONE  = 1 << 11,
	WF_WHITE_BORDER_MASK = 1 << 12 | WF_WHITE_BORDER_ONE,
	WF_SCROLL2           = 1 << 13,
};

/* window.cpp */
void CallWindowEventNP(Window *w, int event);
void CallWindowTickEvent();

Window *BringWindowToFrontById(WindowClass cls, WindowNumber number);
Window *FindWindowFromPt(int x, int y);

bool IsWindowOfPrototype(const Window *w, const Widget *widget);
void AssignWidgetToWindow(Window *w, const Widget *widget);
Window *AllocateWindow(int x, int y, int width, int height,
			WindowProc *proc, WindowClass cls, const Widget *widget,
			void *data = NULL);

Window *AllocateWindowDesc(const WindowDesc *desc, void *data = NULL);
Window *AllocateWindowDescFront(const WindowDesc *desc, int window_number, void *data = NULL);

void DrawWindowViewport(const Window *w);

int GetMenuItemIndex(const Window *w, int x, int y);
void RelocateAllWindows(int neww, int newh);

/* misc_gui.cpp */
void GuiShowTooltipsWithArgs(StringID str, uint paramcount, const uint64 params[]);
static inline void GuiShowTooltips(StringID str)
{
	GuiShowTooltipsWithArgs(str, 0, NULL);
}

/* widget.cpp */
int GetWidgetFromPos(const Window *w, int x, int y);
void DrawWindowWidgets(const Window *w);

enum SortButtonState {
	SBS_OFF,
	SBS_DOWN,
	SBS_UP,
};

void DrawSortButtonState(const Window *w, int widget, SortButtonState state);



/* window.cpp */
extern Window *_z_windows[];
extern Window **_last_z_window;
#define FOR_ALL_WINDOWS(wz) for (wz = _z_windows; wz != _last_z_window; wz++)

extern Point _cursorpos_drag_start;

extern int _scrollbar_start_pos;
extern int _scrollbar_size;
extern byte _scroller_click_timeout;

extern bool _scrolling_scrollbar;
extern bool _scrolling_viewport;
extern bool _popup_menu_active;

extern byte _special_mouse_mode;
enum SpecialMouseMode {
	WSM_NONE     = 0,
	WSM_DRAGDROP = 1,
	WSM_SIZING   = 2,
	WSM_PRESIZE  = 3,
};

Window *GetCallbackWnd();
Window **FindWindowZPosition(const Window *w);

void ScrollbarClickHandler(Window *w, const Widget *wi, int x, int y);

void ResizeButtons(Window *w, byte left, byte right);

void ResizeWindowForWidget(Window *w, int widget, int delta_x, int delta_y);


/**
 * Sets the enabled/disabled status of a widget.
 * By default, widgets are enabled.
 * On certain conditions, they have to be disabled.
 * @param widget_index : index of this widget in the window
 * @param disab_stat : status to use ie: disabled = true, enabled = false
 */
inline void Window::SetWidgetDisabledState(byte widget_index, bool disab_stat)
{
	assert(widget_index < this->widget_count);
	SB(this->widget[widget_index].display_flags, WIDG_DISABLED, 1, !!disab_stat);
}

/**
 * Sets a widget to disabled.
 * @param widget_index : index of this widget in the window
 */
inline void Window::DisableWidget(byte widget_index)
{
	SetWidgetDisabledState(widget_index, true);
}

/**
 * Sets a widget to Enabled.
 * @param widget_index : index of this widget in the window
 */
inline void Window::EnableWidget(byte widget_index)
{
	SetWidgetDisabledState(widget_index, false);
}

/**
 * Gets the enabled/disabled status of a widget.
 * @param widget_index : index of this widget in the window
 * @return status of the widget ie: disabled = true, enabled = false
 */
inline bool Window::IsWidgetDisabled(byte widget_index) const
{
	assert(widget_index < this->widget_count);
	return HasBit(this->widget[widget_index].display_flags, WIDG_DISABLED);
}

/**
 * Sets the hidden/shown status of a widget.
 * By default, widgets are visible.
 * On certain conditions, they have to be hidden.
 * @param widget_index index of this widget in the window
 * @param hidden_stat status to use ie. hidden = true, visible = false
 */
inline void Window::SetWidgetHiddenState(byte widget_index, bool hidden_stat)
{
	assert(widget_index < this->widget_count);
	SB(this->widget[widget_index].display_flags, WIDG_HIDDEN, 1, !!hidden_stat);
}

/**
 * Sets a widget hidden.
 * @param widget_index : index of this widget in the window
 */
inline void Window::HideWidget(byte widget_index)
{
	SetWidgetHiddenState(widget_index, true);
}

/**
 * Sets a widget visible.
 * @param widget_index : index of this widget in the window
 */
inline void Window::ShowWidget(byte widget_index)
{
	SetWidgetHiddenState(widget_index, false);
}

/**
 * Gets the visibility of a widget.
 * @param widget_index : index of this widget in the window
 * @return status of the widget ie: hidden = true, visible = false
 */
inline bool Window::IsWidgetHidden(byte widget_index) const
{
	assert(widget_index < this->widget_count);
	return HasBit(this->widget[widget_index].display_flags, WIDG_HIDDEN);
}

/**
 * Sets the lowered/raised status of a widget.
 * @param widget_index : index of this widget in the window
 * @param lowered_stat : status to use ie: lowered = true, raised = false
 */
inline void Window::SetWidgetLoweredState(byte widget_index, bool lowered_stat)
{
	assert(widget_index < this->widget_count);
	SB(this->widget[widget_index].display_flags, WIDG_LOWERED, 1, !!lowered_stat);
}

/**
 * Invert the lowered/raised  status of a widget.
 * @param widget_index : index of this widget in the window
 */
inline void Window::ToggleWidgetLoweredState(byte widget_index)
{
	assert(widget_index < this->widget_count);
	ToggleBit(this->widget[widget_index].display_flags, WIDG_LOWERED);
}

/**
 * Marks a widget as lowered.
 * @param widget_index : index of this widget in the window
 */
inline void Window::LowerWidget(byte widget_index)
{
	SetWidgetLoweredState(widget_index, true);
}

/**
 * Marks a widget as raised.
 * @param widget_index : index of this widget in the window
 */
inline void Window::RaiseWidget(byte widget_index)
{
	SetWidgetLoweredState(widget_index, false);
}

/**
 * Gets the lowered state of a widget.
 * @param widget_index : index of this widget in the window
 * @return status of the widget ie: lowered = true, raised= false
 */
inline bool Window::IsWidgetLowered(byte widget_index) const
{
	assert(widget_index < this->widget_count);
	return HasBit(this->widget[widget_index].display_flags, WIDG_LOWERED);
}

#endif /* WINDOW_GUI_H */
