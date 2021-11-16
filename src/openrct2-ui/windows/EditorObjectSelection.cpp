/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <cctype>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Context.h>
#include <openrct2/Editor.h>
#include <openrct2/EditorObjectSelectionSession.h>
#include <openrct2/Game.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/LoadOrQuitAction.h>
#include <openrct2/audio/audio.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/String.hpp>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/object/MusicObject.h>
#include <openrct2/object/ObjectList.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/ObjectRepository.h>
#include <openrct2/object/RideObject.h>
#include <openrct2/object/SceneryGroupObject.h>
#include <openrct2/platform/platform.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/sprites.h>
#include <openrct2/title/TitleScreen.h>
#include <openrct2/util/Util.h>
#include <openrct2/windows/Intent.h>
#include <string>
#include <vector>

// clang-format off
enum
{
    FILTER_RCT1 = (1 << 0),
    FILTER_AA = (1 << 1),
    FILTER_LL = (1 << 2),
    FILTER_RCT2 = (1 << 3),
    FILTER_WW = (1 << 4),
    FILTER_TT = (1 << 5),
    FILTER_OO = (1 << 6),
    FILTER_CUSTOM = (1 << 7),

    FILTER_RIDE_TRANSPORT = (1 << 8),
    FILTER_RIDE_GENTLE = (1 << 9),
    FILTER_RIDE_COASTER = (1 << 10),
    FILTER_RIDE_THRILL = (1 << 11),
    FILTER_RIDE_WATER = (1 << 12),
    FILTER_RIDE_STALL = (1 << 13),

    FILTER_SELECTED = (1 << 14),
    FILTER_NONSELECTED = (1 << 15),

    FILTER_RIDES = FILTER_RIDE_TRANSPORT | FILTER_RIDE_GENTLE | FILTER_RIDE_COASTER | FILTER_RIDE_THRILL | FILTER_RIDE_WATER | FILTER_RIDE_STALL,
    FILTER_ALL = FILTER_RIDES | FILTER_RCT1 | FILTER_AA | FILTER_LL | FILTER_RCT2 | FILTER_WW | FILTER_TT | FILTER_OO | FILTER_CUSTOM | FILTER_SELECTED | FILTER_NONSELECTED,
};

static constexpr uint8_t _numSourceGameItems = 8;

static uint32_t _filter_flags;
static uint16_t _filter_object_counts[EnumValue(ObjectType::Count)];

static char _filter_string[MAX_PATH];

#define _FILTER_ALL ((_filter_flags & FILTER_ALL) == FILTER_ALL)
#define _FILTER_RCT1 (_filter_flags & FILTER_RCT1)
#define _FILTER_AA (_filter_flags & FILTER_AA)
#define _FILTER_LL (_filter_flags & FILTER_LL)
#define _FILTER_RCT2 (_filter_flags & FILTER_RCT2)
#define _FILTER_WW (_filter_flags & FILTER_WW)
#define _FILTER_TT (_filter_flags & FILTER_TT)
#define _FILTER_OO (_filter_flags & FILTER_OO)
#define _FILTER_CUSTOM (_filter_flags & FILTER_CUSTOM)
#define _FILTER_SELECTED (_filter_flags & FILTER_SELECTED)
#define _FILTER_NONSELECTED (_filter_flags & FILTER_NONSELECTED)

static constexpr const rct_string_id WINDOW_TITLE = STR_OBJECT_SELECTION;
static constexpr const int32_t WH = 400;
static constexpr const int32_t WW = 600;

struct ObjectPageDesc
{
    rct_string_id Caption;
    uint32_t Image;
    bool IsAdvanced;
};

static constexpr const ObjectPageDesc ObjectSelectionPages[] = {
    { STR_OBJECT_SELECTION_RIDE_VEHICLES_ATTRACTIONS,   SPR_TAB_RIDE_16,            false },
    { STR_OBJECT_SELECTION_SMALL_SCENERY,               SPR_TAB_SCENERY_TREES,      true  },
    { STR_OBJECT_SELECTION_LARGE_SCENERY,               SPR_TAB_SCENERY_URBAN,      true  },
    { STR_OBJECT_SELECTION_WALLS_FENCES,                SPR_TAB_SCENERY_WALLS,      true  },
    { STR_OBJECT_SELECTION_PATH_SIGNS,                  SPR_TAB_SCENERY_SIGNAGE,    true  },
    { STR_OBJECT_SELECTION_FOOTPATHS,                   SPR_TAB_SCENERY_PATHS,      false },
    { STR_OBJECT_SELECTION_PATH_EXTRAS,                 SPR_TAB_SCENERY_PATH_ITEMS, false },
    { STR_OBJECT_SELECTION_SCENERY_GROUPS,              SPR_TAB_SCENERY_STATUES,    false },
    { STR_OBJECT_SELECTION_PARK_ENTRANCE,               SPR_TAB_PARK,               false },
    { STR_OBJECT_SELECTION_WATER,                       SPR_TAB_WATER,              false },

    // Currently hidden until new save format arrives:
    // { STR_OBJECT_SELECTION_TERRAIN_SURFACES,            SPR_G2_TAB_LAND,            true  },
    // { STR_OBJECT_SELECTION_TERRAIN_EDGES,               SPR_G2_TAB_LAND,            true  },
    // { STR_OBJECT_SELECTION_STATIONS,                    SPR_TAB_PARK,               true  },
    // { STR_OBJECT_SELECTION_MUSIC,                       SPR_TAB_MUSIC_0,            false },
    // { STR_OBJECT_SELECTION_FOOTPATH_SURFACES,           SPR_TAB_SCENERY_PATHS,      false },
    // { STR_OBJECT_SELECTION_FOOTPATH_RAILINGS,           SPR_G2_PATH_RAILINGS_TAB,   false },
};

#pragma region Widgets

enum WINDOW_EDITOR_OBJECT_SELECTION_WIDGET_IDX {
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_TAB_CONTENT_PANEL,
    WIDX_ADVANCED,
    WIDX_LIST,
    WIDX_PREVIEW,
    WIDX_INSTALL_TRACK,
    WIDX_FILTER_DROPDOWN,
    WIDX_FILTER_TEXT_BOX,
    WIDX_FILTER_CLEAR_BUTTON,
    WIDX_FILTER_RIDE_TAB_FRAME,
    WIDX_FILTER_RIDE_TAB_ALL,
    WIDX_FILTER_RIDE_TAB_TRANSPORT,
    WIDX_FILTER_RIDE_TAB_GENTLE,
    WIDX_FILTER_RIDE_TAB_COASTER,
    WIDX_FILTER_RIDE_TAB_THRILL,
    WIDX_FILTER_RIDE_TAB_WATER,
    WIDX_FILTER_RIDE_TAB_STALL,
    WIDX_LIST_SORT_TYPE,
    WIDX_LIST_SORT_RIDE,
    WIDX_TAB_1,
};

validate_global_widx(WC_EDITOR_OBJECT_SELECTION, WIDX_TAB_1);

static bool _window_editor_object_selection_widgets_initialised;
static std::vector<rct_widget> _window_editor_object_selection_widgets = {
    WINDOW_SHIM(WINDOW_TITLE, WW, WH),
    MakeWidget({  0, 43}, {WW,  357}, WindowWidgetType::Resize,       WindowColour::Secondary                                                                  ),
    MakeWidget({470, 22}, {122,  14}, WindowWidgetType::Button,       WindowColour::Primary,   STR_OBJECT_SELECTION_ADVANCED, STR_OBJECT_SELECTION_ADVANCED_TIP),
    MakeWidget({  4, 60}, {288, 327}, WindowWidgetType::Scroll,       WindowColour::Secondary, SCROLL_VERTICAL                                                 ),
    MakeWidget({391, 45}, {114, 115}, WindowWidgetType::FlatBtn,      WindowColour::Secondary                                                                  ),
    MakeWidget({470, 22}, {122,  14}, WindowWidgetType::Button,       WindowColour::Primary,   STR_INSTALL_NEW_TRACK_DESIGN,  STR_INSTALL_NEW_TRACK_DESIGN_TIP ),
    MakeWidget({350, 22}, {114,  14}, WindowWidgetType::Button,       WindowColour::Primary,   STR_OBJECT_FILTER,             STR_OBJECT_FILTER_TIP            ),
    MakeWidget({  4, 45}, {211,  14}, WindowWidgetType::TextBox,     WindowColour::Secondary                                                                  ),
    MakeWidget({218, 45}, { 70,  14}, WindowWidgetType::Button,       WindowColour::Secondary, STR_OBJECT_SEARCH_CLEAR                                         ),
    MakeWidget({  3, 73}, {285,   4}, WindowWidgetType::ImgBtn,       WindowColour::Secondary                                                                  ),
    MakeTab   ({  3, 47},                                                                                       STR_OBJECT_FILTER_ALL_RIDES_TIP  ),
    MakeTab   ({ 34, 47},                                                                                       STR_TRANSPORT_RIDES_TIP          ),
    MakeTab   ({ 65, 47},                                                                                       STR_GENTLE_RIDES_TIP             ),
    MakeTab   ({ 96, 47},                                                                                       STR_ROLLER_COASTERS_TIP          ),
    MakeTab   ({127, 47},                                                                                       STR_THRILL_RIDES_TIP             ),
    MakeTab   ({158, 47},                                                                                       STR_WATER_RIDES_TIP              ),
    MakeTab   ({189, 47},                                                                                       STR_SHOPS_STALLS_TIP             ),
    MakeWidget({  4, 80}, {145,  14}, WindowWidgetType::TableHeader, WindowColour::Secondary                                                                  ),
    MakeWidget({149, 80}, {143,  14}, WindowWidgetType::TableHeader, WindowColour::Secondary                                                                  ),

    MakeTab   ({  3, 17},                                                                                       STR_STRING_DEFINED_TOOLTIP       ),
    // Copied object type times...

    WIDGETS_END,
};

#pragma endregion

#pragma region Events

static void WindowEditorObjectSelectionClose(rct_window *w);
static void WindowEditorObjectSelectionMouseup(rct_window *w, rct_widgetindex widgetIndex);
static void WindowEditorObjectSelectionResize(rct_window *w);
static void WindowEditorObjectSelectionMousedown(rct_window *w, rct_widgetindex widgetIndex, rct_widget* widget);
static void WindowEditorObjectSelectionDropdown(rct_window *w, rct_widgetindex widgetIndex, int32_t dropdownIndex);
static void WindowEditorObjectSelectionUpdate(rct_window *w);
static void WindowEditorObjectSelectionScrollgetsize(rct_window *w, int32_t scrollIndex, int32_t *width, int32_t *height);
static void WindowEditorObjectSelectionScrollMousedown(rct_window *w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords);
static void WindowEditorObjectSelectionScrollMouseover(rct_window *w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords);
static OpenRCT2String WindowEditorObjectSelectionTooltip(rct_window* w, const rct_widgetindex widgetIndex, const rct_string_id fallback);
static void WindowEditorObjectSelectionInvalidate(rct_window *w);
static void WindowEditorObjectSelectionPaint(rct_window *w, rct_drawpixelinfo *dpi);
static void WindowEditorObjectSelectionScrollpaint(rct_window *w, rct_drawpixelinfo *dpi, int32_t scrollIndex);
static void WindowEditorObjectSelectionTextinput(rct_window *w, rct_widgetindex widgetIndex, char *text);

static rct_window_event_list window_editor_object_selection_events([](auto& events)
{
    events.close = &WindowEditorObjectSelectionClose;
    events.mouse_up = &WindowEditorObjectSelectionMouseup;
    events.resize = &WindowEditorObjectSelectionResize;
    events.mouse_down = &WindowEditorObjectSelectionMousedown;
    events.dropdown = &WindowEditorObjectSelectionDropdown;
    events.update = &WindowEditorObjectSelectionUpdate;
    events.get_scroll_size = &WindowEditorObjectSelectionScrollgetsize;
    events.scroll_mousedown = &WindowEditorObjectSelectionScrollMousedown;
    events.scroll_mouseover = &WindowEditorObjectSelectionScrollMouseover;
    events.text_input = &WindowEditorObjectSelectionTextinput;
    events.tooltip = &WindowEditorObjectSelectionTooltip;
    events.invalidate = &WindowEditorObjectSelectionInvalidate;
    events.paint = &WindowEditorObjectSelectionPaint;
    events.scroll_paint = &WindowEditorObjectSelectionScrollpaint;
});
// clang-format on

#pragma endregion

static constexpr const int32_t window_editor_object_selection_animation_loops[] = {
    20, 32, 10, 72, 24, 28, 16,
};
static constexpr const int32_t window_editor_object_selection_animation_divisor[] = {
    4, 8, 2, 4, 4, 4, 2,
};

static void WindowEditorObjectSetPage(rct_window* w, int32_t page);
static void WindowEditorObjectSelectionSetPressedTab(rct_window* w);
static int32_t GetObjectFromObjectSelection(ObjectType object_type, int32_t y);
static void WindowEditorObjectSelectionManageTracks();
static void EditorLoadSelectedObjects();
static bool FilterSelected(uint8_t objectFlags);
static bool FilterString(const ObjectRepositoryItem* item);
static bool FilterSource(const ObjectRepositoryItem* item);
static bool FilterChunks(const ObjectRepositoryItem* item);
static void FilterUpdateCounts();

static std::string ObjectGetDescription(const Object* object);
static ObjectType GetSelectedObjectType(rct_window* w);

enum
{
    RIDE_SORT_TYPE,
    RIDE_SORT_RIDE
};

enum
{
    DDIX_FILTER_RCT1,
    DDIX_FILTER_AA,
    DDIX_FILTER_LL,
    DDIX_FILTER_RCT2,
    DDIX_FILTER_WW,
    DDIX_FILTER_TT,
    DDIX_FILTER_OO,
    DDIX_FILTER_CUSTOM,
    DDIX_FILTER_SEPARATOR,
    DDIX_FILTER_SELECTED,
    DDIX_FILTER_NONSELECTED,
};

struct list_item
{
    const ObjectRepositoryItem* repositoryItem;
    rct_object_entry* entry;
    std::unique_ptr<rct_object_filters> filter;
    uint8_t* flags;
};

static rct_string_id GetRideTypeStringId(const ObjectRepositoryItem* item);

using sortFunc_t = bool (*)(const list_item&, const list_item&);

static std::vector<list_item> _listItems;
static int32_t _listSortType = RIDE_SORT_TYPE;
static bool _listSortDescending = false;
static std::unique_ptr<Object> _loadedObject;

static void VisibleListDispose()
{
    _listItems.clear();
    _listItems.shrink_to_fit();
}

static bool VisibleListSortRideName(const list_item& a, const list_item& b)
{
    auto nameA = a.repositoryItem->Name.c_str();
    auto nameB = b.repositoryItem->Name.c_str();
    return strcmp(nameA, nameB) < 0;
}

static bool VisibleListSortRideType(const list_item& a, const list_item& b)
{
    auto rideTypeA = language_get_string(GetRideTypeStringId(a.repositoryItem));
    auto rideTypeB = language_get_string(GetRideTypeStringId(b.repositoryItem));
    int32_t result = String::Compare(rideTypeA, rideTypeB);
    return result != 0 ? result < 0 : VisibleListSortRideName(a, b);
}

static void VisibleListRefresh(rct_window* w)
{
    int32_t numObjects = static_cast<int32_t>(object_repository_get_items_count());

    VisibleListDispose();
    w->selected_list_item = -1;

    const ObjectRepositoryItem* items = object_repository_get_items();
    for (int32_t i = 0; i < numObjects; i++)
    {
        uint8_t selectionFlags = _objectSelectionFlags[i];
        const ObjectRepositoryItem* item = &items[i];
        ObjectType objectType = item->ObjectEntry.GetType();
        if (objectType == GetSelectedObjectType(w) && !(selectionFlags & OBJECT_SELECTION_FLAG_6) && FilterSource(item)
            && FilterString(item) && FilterChunks(item) && FilterSelected(selectionFlags))
        {
            auto filter = std::make_unique<rct_object_filters>();
            filter->ride.category[0] = 0;
            filter->ride.category[1] = 0;
            filter->ride.ride_type = 0;

            list_item currentListItem;
            currentListItem.repositoryItem = item;
            currentListItem.entry = const_cast<rct_object_entry*>(&item->ObjectEntry);
            currentListItem.filter = std::move(filter);
            currentListItem.flags = &_objectSelectionFlags[i];
            _listItems.push_back(std::move(currentListItem));
        }
    }

    if (_listItems.empty())
    {
        VisibleListDispose();
    }
    else
    {
        sortFunc_t sortFunc = nullptr;
        switch (_listSortType)
        {
            case RIDE_SORT_TYPE:
                sortFunc = VisibleListSortRideType;
                break;
            case RIDE_SORT_RIDE:
                sortFunc = VisibleListSortRideName;
                break;
            default:
                log_warning("Wrong sort type %d, leaving list as-is.", _listSortType);
                break;
        }
        if (sortFunc != nullptr)
        {
            std::sort(_listItems.begin(), _listItems.end(), sortFunc);
            if (_listSortDescending)
            {
                std::reverse(_listItems.begin(), _listItems.end());
            }
        }
    }
    w->Invalidate();
}

static void WindowEditorObjectSelectionInitWidgets()
{
    auto& widgets = _window_editor_object_selection_widgets;
    if (!_window_editor_object_selection_widgets_initialised)
    {
        _window_editor_object_selection_widgets_initialised = true;
        auto tabWidget = widgets[widgets.size() - 2];
        for (size_t i = 1; i < std::size(ObjectSelectionPages); i++)
        {
            widgets.insert(widgets.end() - 1, tabWidget);
        }
    }
}

/**
 *
 *  rct2: 0x006AA64E
 */
rct_window* WindowEditorObjectSelectionOpen()
{
    WindowEditorObjectSelectionInitWidgets();

    auto window = window_bring_to_front_by_class(WC_EDITOR_OBJECT_SELECTION);
    if (window != nullptr)
        return window;

    sub_6AB211();
    reset_selected_object_count_and_size();

    window = WindowCreateCentred(
        WW, WH, &window_editor_object_selection_events, WC_EDITOR_OBJECT_SELECTION, WF_10 | WF_RESIZABLE);
    window->widgets = _window_editor_object_selection_widgets.data();
    window->widgets[WIDX_FILTER_TEXT_BOX].string = _filter_string;

    window->enabled_widgets = (1ULL << WIDX_ADVANCED) | (1ULL << WIDX_INSTALL_TRACK) | (1ULL << WIDX_FILTER_DROPDOWN)
        | (1ULL << WIDX_FILTER_TEXT_BOX) | (1ULL << WIDX_FILTER_CLEAR_BUTTON) | (1ULL << WIDX_CLOSE)
        | (1ULL << WIDX_LIST_SORT_TYPE) | (1UL << WIDX_LIST_SORT_RIDE);

    _filter_flags = gConfigInterface.object_selection_filter_flags;
    std::fill_n(_filter_string, sizeof(_filter_string), 0x00);

    for (size_t i = WIDX_TAB_1; i < WIDX_TAB_1 + std::size(ObjectSelectionPages); i++)
    {
        window->enabled_widgets |= (1LL << i);
    }
    WindowInitScrollWidgets(window);

    window->selected_tab = 0;
    window->selected_list_item = -1;
    window->object_entry = nullptr;
    window->min_width = WW;
    window->min_height = WH;
    window->max_width = 1200;
    window->max_height = 1000;

    VisibleListRefresh(window);

    return window;
}

/**
 *
 *  rct2: 0x006AB199
 */
static void WindowEditorObjectSelectionClose(rct_window* w)
{
    unload_unselected_objects();
    EditorLoadSelectedObjects();
    editor_object_flags_free();

    if (_loadedObject != nullptr)
        _loadedObject->Unload();

    if (gScreenFlags & SCREEN_FLAGS_EDITOR)
    {
        research_populate_list_random();
    }
    else
    {
        // Used for in-game object selection cheat
        // This resets the ride selection list and resets research to 0 on current item
        gSilentResearch = true;
        research_reset_current_item();
        gSilentResearch = false;
    }

    auto intent = Intent(INTENT_ACTION_REFRESH_NEW_RIDES);
    context_broadcast_intent(&intent);

    VisibleListDispose();

    intent = Intent(INTENT_ACTION_REFRESH_SCENERY);
    context_broadcast_intent(&intent);
}

/**
 *
 *  rct2: 0x006AAFAB
 */
static void WindowEditorObjectSelectionMouseup(rct_window* w, rct_widgetindex widgetIndex)
{
    switch (widgetIndex)
    {
        case WIDX_CLOSE:
            window_close(w);
            if (gScreenFlags & SCREEN_FLAGS_EDITOR)
            {
                finish_object_selection();
            }
            if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)
            {
                game_unload_scripts();
                title_load();
            }
            break;
        case WIDX_FILTER_RIDE_TAB_ALL:
            _filter_flags |= FILTER_RIDES;
            gConfigInterface.object_selection_filter_flags = _filter_flags;
            config_save_default();

            FilterUpdateCounts();
            VisibleListRefresh(w);

            w->selected_list_item = -1;
            w->object_entry = nullptr;
            w->scrolls[0].v_top = 0;
            w->Invalidate();
            break;
        case WIDX_FILTER_RIDE_TAB_TRANSPORT:
        case WIDX_FILTER_RIDE_TAB_GENTLE:
        case WIDX_FILTER_RIDE_TAB_COASTER:
        case WIDX_FILTER_RIDE_TAB_THRILL:
        case WIDX_FILTER_RIDE_TAB_WATER:
        case WIDX_FILTER_RIDE_TAB_STALL:
            _filter_flags &= ~FILTER_RIDES;
            _filter_flags |= (1 << (widgetIndex - WIDX_FILTER_RIDE_TAB_TRANSPORT + _numSourceGameItems));
            gConfigInterface.object_selection_filter_flags = _filter_flags;
            config_save_default();

            FilterUpdateCounts();
            VisibleListRefresh(w);

            w->selected_list_item = -1;
            w->object_entry = nullptr;
            w->scrolls[0].v_top = 0;
            w->frame_no = 0;
            w->Invalidate();
            break;

        case WIDX_ADVANCED:
            w->list_information_type ^= 1;
            w->Invalidate();
            break;

        case WIDX_INSTALL_TRACK:
        {
            if (w->selected_list_item != -1)
            {
                w->selected_list_item = -1;
            }
            w->Invalidate();

            auto intent = Intent(WC_LOADSAVE);
            intent.putExtra(INTENT_EXTRA_LOADSAVE_TYPE, LOADSAVETYPE_LOAD | LOADSAVETYPE_TRACK);
            context_open_intent(&intent);
            break;
        }
        case WIDX_FILTER_TEXT_BOX:
            window_start_textbox(w, widgetIndex, STR_STRING, _filter_string, sizeof(_filter_string));
            break;
        case WIDX_FILTER_CLEAR_BUTTON:
            std::fill_n(_filter_string, sizeof(_filter_string), 0x00);
            FilterUpdateCounts();
            w->scrolls->v_top = 0;
            VisibleListRefresh(w);
            w->Invalidate();
            break;
        case WIDX_LIST_SORT_TYPE:
            if (_listSortType == RIDE_SORT_TYPE)
            {
                _listSortDescending = !_listSortDescending;
            }
            else
            {
                _listSortType = RIDE_SORT_TYPE;
                _listSortDescending = false;
            }
            VisibleListRefresh(w);
            break;
        case WIDX_LIST_SORT_RIDE:
            if (_listSortType == RIDE_SORT_RIDE)
            {
                _listSortDescending = !_listSortDescending;
            }
            else
            {
                _listSortType = RIDE_SORT_RIDE;
                _listSortDescending = false;
            }
            VisibleListRefresh(w);
            break;
        default:
            if (widgetIndex >= WIDX_TAB_1 && static_cast<size_t>(widgetIndex) < WIDX_TAB_1 + std::size(ObjectSelectionPages))
            {
                WindowEditorObjectSetPage(w, widgetIndex - WIDX_TAB_1);
            }
            break;
    }
}

static void WindowEditorObjectSelectionResize(rct_window* w)
{
    window_set_resize(w, WW, WH, 1200, 1000);
}

void WindowEditorObjectSelectionMousedown(rct_window* w, rct_widgetindex widgetIndex, rct_widget* widget)
{
    int32_t numSelectionItems = 0;

    switch (widgetIndex)
    {
        case WIDX_FILTER_DROPDOWN:

            gDropdownItemsFormat[DDIX_FILTER_RCT1] = STR_TOGGLE_OPTION;
            gDropdownItemsFormat[DDIX_FILTER_AA] = STR_TOGGLE_OPTION;
            gDropdownItemsFormat[DDIX_FILTER_LL] = STR_TOGGLE_OPTION;
            gDropdownItemsFormat[DDIX_FILTER_RCT2] = STR_TOGGLE_OPTION;
            gDropdownItemsFormat[DDIX_FILTER_WW] = STR_TOGGLE_OPTION;
            gDropdownItemsFormat[DDIX_FILTER_TT] = STR_TOGGLE_OPTION;
            gDropdownItemsFormat[DDIX_FILTER_OO] = STR_TOGGLE_OPTION;
            gDropdownItemsFormat[DDIX_FILTER_CUSTOM] = STR_TOGGLE_OPTION;

            gDropdownItemsArgs[DDIX_FILTER_RCT1] = STR_SCENARIO_CATEGORY_RCT1;
            gDropdownItemsArgs[DDIX_FILTER_AA] = STR_SCENARIO_CATEGORY_RCT1_AA;
            gDropdownItemsArgs[DDIX_FILTER_LL] = STR_SCENARIO_CATEGORY_RCT1_LL;
            gDropdownItemsArgs[DDIX_FILTER_RCT2] = STR_ROLLERCOASTER_TYCOON_2_DROPDOWN;
            gDropdownItemsArgs[DDIX_FILTER_WW] = STR_OBJECT_FILTER_WW;
            gDropdownItemsArgs[DDIX_FILTER_TT] = STR_OBJECT_FILTER_TT;
            gDropdownItemsArgs[DDIX_FILTER_OO] = STR_OBJECT_FILTER_OPENRCT2_OFFICIAL;
            gDropdownItemsArgs[DDIX_FILTER_CUSTOM] = STR_OBJECT_FILTER_CUSTOM;

            // Track manager cannot select multiple, so only show selection filters if not in track manager
            if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER))
            {
                numSelectionItems = 3;
                gDropdownItemsFormat[DDIX_FILTER_SEPARATOR] = 0;
                gDropdownItemsFormat[DDIX_FILTER_SELECTED] = STR_TOGGLE_OPTION;
                gDropdownItemsFormat[DDIX_FILTER_NONSELECTED] = STR_TOGGLE_OPTION;
                gDropdownItemsArgs[DDIX_FILTER_SEPARATOR] = STR_NONE;
                gDropdownItemsArgs[DDIX_FILTER_SELECTED] = STR_SELECTED_ONLY;
                gDropdownItemsArgs[DDIX_FILTER_NONSELECTED] = STR_NON_SELECTED_ONLY;
            }

            WindowDropdownShowText(
                { w->windowPos.x + widget->left, w->windowPos.y + widget->top }, widget->height() + 1,
                w->colours[widget->colour], Dropdown::Flag::StayOpen, _numSourceGameItems + numSelectionItems);

            for (int32_t i = 0; i < _numSourceGameItems; i++)
            {
                if (_filter_flags & (1 << i))
                {
                    Dropdown::SetChecked(i, true);
                }
            }

            if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER))
            {
                Dropdown::SetChecked(DDIX_FILTER_SELECTED, _FILTER_SELECTED != 0);
                Dropdown::SetChecked(DDIX_FILTER_NONSELECTED, _FILTER_NONSELECTED != 0);
            }
            break;
    }
}

static void WindowEditorObjectSelectionDropdown(rct_window* w, rct_widgetindex widgetIndex, int32_t dropdownIndex)
{
    if (dropdownIndex == -1)
        return;

    switch (widgetIndex)
    {
        case WIDX_FILTER_DROPDOWN:
            if (dropdownIndex == DDIX_FILTER_SELECTED)
            {
                _filter_flags ^= FILTER_SELECTED;
                _filter_flags &= ~FILTER_NONSELECTED;
            }
            else if (dropdownIndex == DDIX_FILTER_NONSELECTED)
            {
                _filter_flags ^= FILTER_NONSELECTED;
                _filter_flags &= ~FILTER_SELECTED;
            }
            else
            {
                _filter_flags ^= (1 << dropdownIndex);
            }
            gConfigInterface.object_selection_filter_flags = _filter_flags;
            config_save_default();

            FilterUpdateCounts();
            w->scrolls->v_top = 0;

            VisibleListRefresh(w);
            w->Invalidate();
            break;
    }
}

/**
 *
 *  rct2: 0x006AB031
 */
static void WindowEditorObjectSelectionScrollgetsize(rct_window* w, int32_t scrollIndex, int32_t* width, int32_t* height)
{
    *height = static_cast<int32_t>(_listItems.size() * SCROLLABLE_ROW_HEIGHT);
}

/**
 *
 *  rct2: 0x006AB0B6
 */
static void WindowEditorObjectSelectionScrollMousedown(
    rct_window* w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
{
    // Used for in-game object selection cheat to prevent crashing the game
    // when windows attempt to draw objects that don't exist any more
    window_close_all_except_class(WC_EDITOR_OBJECT_SELECTION);

    int32_t selected_object = GetObjectFromObjectSelection(GetSelectedObjectType(w), screenCoords.y);
    if (selected_object == -1)
        return;

    list_item* listItem = &_listItems[selected_object];
    uint8_t object_selection_flags = *listItem->flags;
    if (object_selection_flags & OBJECT_SELECTION_FLAG_6)
        return;

    w->Invalidate();

    const CursorState* state = context_get_cursor_state();
    OpenRCT2::Audio::Play(OpenRCT2::Audio::SoundId::Click1, 0, state->position.x);

    if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)
    {
        if (!window_editor_object_selection_select_object(0, INPUT_FLAG_EDITOR_OBJECT_SELECT, listItem->repositoryItem))
            return;

        // Close any other open windows such as options/colour schemes to prevent a crash.
        window_close_all();
        // window_close(w);

        // This function calls window_track_list_open
        WindowEditorObjectSelectionManageTracks();
        return;
    }

    int32_t flags = INPUT_FLAG_EDITOR_OBJECT_1 | INPUT_FLAG_EDITOR_OBJECT_SELECT_OBJECTS_IN_SCENERY_GROUP;
    // If already selected
    if (!(object_selection_flags & OBJECT_SELECTION_FLAG_SELECTED))
        flags |= INPUT_FLAG_EDITOR_OBJECT_SELECT;

    _maxObjectsWasHit = false;
    if (!window_editor_object_selection_select_object(0, flags, listItem->repositoryItem))
    {
        rct_string_id error_title = (flags & INPUT_FLAG_EDITOR_OBJECT_SELECT) ? STR_UNABLE_TO_SELECT_THIS_OBJECT
                                                                              : STR_UNABLE_TO_DE_SELECT_THIS_OBJECT;

        context_show_error(error_title, gGameCommandErrorText, {});
        return;
    }

    if (_FILTER_SELECTED || _FILTER_NONSELECTED)
    {
        FilterUpdateCounts();
        VisibleListRefresh(w);
        w->Invalidate();
    }

    if (_maxObjectsWasHit)
    {
        context_show_error(
            STR_WARNING_TOO_MANY_OBJECTS_SELECTED, STR_NOT_ALL_OBJECTS_IN_THIS_SCENERY_GROUP_COULD_BE_SELECTED, {});
    }
}

/**
 *
 *  rct2: 0x006AB079
 */
static void WindowEditorObjectSelectionScrollMouseover(
    rct_window* w, int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
{
    int32_t selectedObject = GetObjectFromObjectSelection(GetSelectedObjectType(w), screenCoords.y);
    if (selectedObject != -1)
    {
        list_item* listItem = &_listItems[selectedObject];
        uint8_t objectSelectionFlags = *listItem->flags;
        if (objectSelectionFlags & OBJECT_SELECTION_FLAG_6)
        {
            selectedObject = -1;
        }
    }
    if (selectedObject != w->selected_list_item)
    {
        w->selected_list_item = selectedObject;

        if (_loadedObject != nullptr)
        {
            _loadedObject->Unload();
            _loadedObject = nullptr;
        }

        if (selectedObject == -1)
        {
            w->object_entry = nullptr;
        }
        else
        {
            auto listItem = &_listItems[selectedObject];
            w->object_entry = listItem->entry;
            _loadedObject = object_repository_load_object(listItem->entry);
        }

        w->Invalidate();
    }
}

/**
 *
 *  rct2: 0x006AB058
 */
static OpenRCT2String WindowEditorObjectSelectionTooltip(
    rct_window* w, const rct_widgetindex widgetIndex, const rct_string_id fallback)
{
    if (widgetIndex >= WIDX_TAB_1 && static_cast<size_t>(widgetIndex) < WIDX_TAB_1 + std::size(ObjectSelectionPages))
    {
        auto ft = Formatter();
        ft.Add<rct_string_id>(ObjectSelectionPages[(widgetIndex - WIDX_TAB_1)].Caption);
        return { fallback, ft };
    }
    return { fallback, {} };
}

/**
 *
 *  rct2: 0x006AA9FD
 */
static void WindowEditorObjectSelectionInvalidate(rct_window* w)
{
    // Resize widgets
    w->widgets[WIDX_BACKGROUND].right = w->width - 1;
    w->widgets[WIDX_BACKGROUND].bottom = w->height - 1;
    w->widgets[WIDX_TITLE].right = w->width - 2;
    w->widgets[WIDX_CLOSE].left = w->width - 13;
    w->widgets[WIDX_CLOSE].right = w->width - 3;
    w->widgets[WIDX_TAB_CONTENT_PANEL].right = w->width - 1;
    w->widgets[WIDX_TAB_CONTENT_PANEL].bottom = w->height - 1;
    w->widgets[WIDX_ADVANCED].left = w->width - 130;
    w->widgets[WIDX_ADVANCED].right = w->width - 9;
    w->widgets[WIDX_LIST].right = w->width - 309;
    w->widgets[WIDX_LIST].bottom = w->height - 14;
    w->widgets[WIDX_PREVIEW].left = w->width - 209;
    w->widgets[WIDX_PREVIEW].right = w->width - 96;
    w->widgets[WIDX_INSTALL_TRACK].left = w->width - 130;
    w->widgets[WIDX_INSTALL_TRACK].right = w->width - 9;
    w->widgets[WIDX_FILTER_DROPDOWN].left = w->width - 250;
    w->widgets[WIDX_FILTER_DROPDOWN].right = w->width - 137;

    // Set pressed widgets
    w->pressed_widgets |= 1ULL << WIDX_PREVIEW;
    WindowEditorObjectSelectionSetPressedTab(w);
    if (w->list_information_type & 1)
        w->pressed_widgets |= (1ULL << WIDX_ADVANCED);
    else
        w->pressed_widgets &= ~(1ULL << WIDX_ADVANCED);

    // Set window title and buttons
    auto ft = Formatter::Common();
    ft.Add<rct_string_id>(ObjectSelectionPages[w->selected_tab].Caption);
    auto& titleWidget = w->widgets[WIDX_TITLE];
    auto& installTrackWidget = w->widgets[WIDX_INSTALL_TRACK];
    if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)
    {
        titleWidget.text = STR_TRACK_DESIGNS_MANAGER_SELECT_RIDE_TYPE;
        installTrackWidget.type = WindowWidgetType::Button;
    }
    else if (gScreenFlags & SCREEN_FLAGS_TRACK_DESIGNER)
    {
        titleWidget.text = STR_ROLLER_COASTER_DESIGNER_SELECT_RIDE_TYPES_VEHICLES;
        installTrackWidget.type = WindowWidgetType::Empty;
    }
    else
    {
        titleWidget.text = STR_OBJECT_SELECTION;
        installTrackWidget.type = WindowWidgetType::Empty;
    }

    // Align tabs, hide advanced ones
    bool advancedMode = (w->list_information_type & 1) != 0;
    int32_t x = 3;
    for (size_t i = 0; i < std::size(ObjectSelectionPages); i++)
    {
        auto& widget = w->widgets[WIDX_TAB_1 + i];
        if (!advancedMode && ObjectSelectionPages[i].IsAdvanced)
        {
            widget.type = WindowWidgetType::Empty;
        }
        else
        {
            widget.type = WindowWidgetType::Tab;
            widget.left = x;
            widget.right = x + 30;
            x += 31;
        }
    }

    if (gScreenFlags & (SCREEN_FLAGS_TRACK_MANAGER | SCREEN_FLAGS_TRACK_DESIGNER))
    {
        w->widgets[WIDX_ADVANCED].type = WindowWidgetType::Empty;
        for (size_t i = 1; i < std::size(ObjectSelectionPages); i++)
        {
            w->widgets[WIDX_TAB_1 + i].type = WindowWidgetType::Empty;
        }
        x = 150;
    }
    else
    {
        w->widgets[WIDX_ADVANCED].type = WindowWidgetType::Button;
        x = 300;
    }

    w->widgets[WIDX_FILTER_DROPDOWN].type = WindowWidgetType::Button;
    w->widgets[WIDX_LIST].right = w->width - (WW - 587) - x;
    w->widgets[WIDX_PREVIEW].left = w->width - (WW - 537) - (x / 2);
    w->widgets[WIDX_PREVIEW].right = w->widgets[WIDX_PREVIEW].left + 113;
    w->widgets[WIDX_FILTER_RIDE_TAB_FRAME].right = w->widgets[WIDX_LIST].right;

    bool ridePage = (GetSelectedObjectType(w) == ObjectType::Ride);
    w->widgets[WIDX_LIST].top = (ridePage ? 118 : 60);
    w->widgets[WIDX_FILTER_TEXT_BOX].right = w->widgets[WIDX_LIST].right - 77;
    w->widgets[WIDX_FILTER_TEXT_BOX].top = (ridePage ? 79 : 45);
    w->widgets[WIDX_FILTER_TEXT_BOX].bottom = (ridePage ? 92 : 58);
    w->widgets[WIDX_FILTER_CLEAR_BUTTON].left = w->widgets[WIDX_LIST].right - 73;
    w->widgets[WIDX_FILTER_CLEAR_BUTTON].right = w->widgets[WIDX_LIST].right;
    w->widgets[WIDX_FILTER_CLEAR_BUTTON].top = (ridePage ? 79 : 45);
    w->widgets[WIDX_FILTER_CLEAR_BUTTON].bottom = (ridePage ? 92 : 58);

    if (ridePage)
    {
        w->enabled_widgets |= (1ULL << WIDX_FILTER_RIDE_TAB_ALL) | (1ULL << WIDX_FILTER_RIDE_TAB_TRANSPORT)
            | (1ULL << WIDX_FILTER_RIDE_TAB_GENTLE) | (1ULL << WIDX_FILTER_RIDE_TAB_COASTER)
            | (1ULL << WIDX_FILTER_RIDE_TAB_THRILL) | (1ULL << WIDX_FILTER_RIDE_TAB_WATER)
            | (1ULL << WIDX_FILTER_RIDE_TAB_STALL);

        for (int32_t i = 0; i < 7; i++)
            w->pressed_widgets &= ~(1 << (WIDX_FILTER_RIDE_TAB_ALL + i));

        if ((_filter_flags & FILTER_RIDES) == FILTER_RIDES)
            w->pressed_widgets |= (1ULL << WIDX_FILTER_RIDE_TAB_ALL);
        else
        {
            for (int32_t i = 0; i < 6; i++)
            {
                if (_filter_flags & (1 << (_numSourceGameItems + i)))
                    w->pressed_widgets |= 1ULL << (WIDX_FILTER_RIDE_TAB_TRANSPORT + i);
            }
        }

        w->widgets[WIDX_FILTER_RIDE_TAB_FRAME].type = WindowWidgetType::ImgBtn;
        for (int32_t i = WIDX_FILTER_RIDE_TAB_ALL; i <= WIDX_FILTER_RIDE_TAB_STALL; i++)
            w->widgets[i].type = WindowWidgetType::Tab;

        int32_t width_limit = (w->widgets[WIDX_LIST].width() - 15) / 2;

        w->widgets[WIDX_LIST_SORT_TYPE].type = WindowWidgetType::TableHeader;
        w->widgets[WIDX_LIST_SORT_TYPE].top = w->widgets[WIDX_FILTER_TEXT_BOX].bottom + 3;
        w->widgets[WIDX_LIST_SORT_TYPE].bottom = w->widgets[WIDX_LIST_SORT_TYPE].top + 13;
        w->widgets[WIDX_LIST_SORT_TYPE].left = 4;
        w->widgets[WIDX_LIST_SORT_TYPE].right = w->widgets[WIDX_LIST_SORT_TYPE].left + width_limit;

        w->widgets[WIDX_LIST_SORT_RIDE].type = WindowWidgetType::TableHeader;
        w->widgets[WIDX_LIST_SORT_RIDE].top = w->widgets[WIDX_LIST_SORT_TYPE].top;
        w->widgets[WIDX_LIST_SORT_RIDE].bottom = w->widgets[WIDX_LIST_SORT_TYPE].bottom;
        w->widgets[WIDX_LIST_SORT_RIDE].left = w->widgets[WIDX_LIST_SORT_TYPE].right + 1;
        w->widgets[WIDX_LIST_SORT_RIDE].right = w->widgets[WIDX_LIST].right;

        w->widgets[WIDX_LIST].top = w->widgets[WIDX_LIST_SORT_TYPE].bottom + 2;
    }
    else
    {
        w->enabled_widgets &= ~(
            (1ULL << WIDX_FILTER_RIDE_TAB_ALL) | (1ULL << WIDX_FILTER_RIDE_TAB_TRANSPORT)
            | (1ULL << WIDX_FILTER_RIDE_TAB_GENTLE) | (1ULL << WIDX_FILTER_RIDE_TAB_COASTER)
            | (1ULL << WIDX_FILTER_RIDE_TAB_THRILL) | (1ULL << WIDX_FILTER_RIDE_TAB_WATER)
            | (1ULL << WIDX_FILTER_RIDE_TAB_STALL));
        for (int32_t i = WIDX_FILTER_RIDE_TAB_FRAME; i <= WIDX_FILTER_RIDE_TAB_STALL; i++)
            w->widgets[i].type = WindowWidgetType::Empty;

        w->widgets[WIDX_LIST_SORT_TYPE].type = WindowWidgetType::Empty;
        w->widgets[WIDX_LIST_SORT_RIDE].type = WindowWidgetType::Empty;
    }
}

static void WindowEditorObjectSelectionPaintDescriptions(rct_window* w, rct_drawpixelinfo* dpi)
{
    const auto& widget = w->widgets[WIDX_PREVIEW];
    auto screenPos = w->windowPos + ScreenCoordsXY{ w->widgets[WIDX_LIST].right + 4, widget.bottom + 23 };
    auto width = w->windowPos.x + w->width - screenPos.x - 4;

    auto description = ObjectGetDescription(_loadedObject.get());
    if (!description.empty())
    {
        auto ft = Formatter();
        ft.Add<rct_string_id>(STR_STRING);
        ft.Add<const char*>(description.c_str());

        screenPos.y += DrawTextWrapped(dpi, screenPos, width, STR_WINDOW_COLOUR_2_STRINGID, ft) + LIST_ROW_HEIGHT;
    }
    if (GetSelectedObjectType(w) == ObjectType::Ride)
    {
        auto* rideObject = reinterpret_cast<RideObject*>(_loadedObject.get());
        const auto* rideEntry = reinterpret_cast<rct_ride_entry*>(rideObject->GetLegacyData());
        if (rideEntry->shop_item[0] != ShopItem::None)
        {
            std::string sells = "";
            for (size_t i = 0; i < std::size(rideEntry->shop_item); i++)
            {
                if (rideEntry->shop_item[i] == ShopItem::None)
                    continue;

                if (!sells.empty())
                    sells += ", ";

                sells += language_get_string(GetShopItemDescriptor(rideEntry->shop_item[i]).Naming.Plural);
            }
            auto ft = Formatter();
            ft.Add<const char*>(sells.c_str());
            screenPos.y += DrawTextWrapped(dpi, screenPos, width, STR_RIDE_OBJECT_SHOP_SELLS, ft) + 2;
        }
    }
    else if (GetSelectedObjectType(w) == ObjectType::SceneryGroup)
    {
        const auto* sceneryGroupObject = reinterpret_cast<SceneryGroupObject*>(_loadedObject.get());
        auto ft = Formatter();
        ft.Add<uint16_t>(sceneryGroupObject->GetNumIncludedObjects());
        screenPos.y += DrawTextWrapped(dpi, screenPos, width, STR_INCLUDES_X_OBJECTS, ft) + 2;
    }
    else if (GetSelectedObjectType(w) == ObjectType::Music)
    {
        screenPos.y += DrawTextWrapped(dpi, screenPos, width, STR_MUSIC_OBJECT_TRACK_HEADER) + 2;
        const auto* musicObject = reinterpret_cast<MusicObject*>(_loadedObject.get());
        for (size_t i = 0; i < musicObject->GetTrackCount(); i++)
        {
            const auto* track = musicObject->GetTrack(i);
            if (track->Name.empty())
                continue;

            auto stringId = track->Composer.empty() ? STR_MUSIC_OBJECT_TRACK_LIST_ITEM
                                                    : STR_MUSIC_OBJECT_TRACK_LIST_ITEM_WITH_COMPOSER;
            auto ft = Formatter();
            ft.Add<const char*>(track->Name.c_str());
            ft.Add<const char*>(track->Composer.c_str());
            screenPos.y += DrawTextWrapped(dpi, screenPos + ScreenCoordsXY{ 10, 0 }, width, stringId, ft);
        }
    }
}

static void WindowEditorObjectSelectionPaintDebugData(rct_window* w, rct_drawpixelinfo* dpi)
{
    list_item* listItem = &_listItems[w->selected_list_item];
    auto screenPos = w->windowPos + ScreenCoordsXY{ w->width - 5, w->height - (LIST_ROW_HEIGHT * 5) };
    // Draw ride type.
    if (GetSelectedObjectType(w) == ObjectType::Ride)
    {
        auto stringId = GetRideTypeStringId(listItem->repositoryItem);
        DrawTextBasic(dpi, screenPos, stringId, {}, { COLOUR_WHITE, TextAlignment::RIGHT });
    }

    screenPos.y += LIST_ROW_HEIGHT;

    // Draw object source
    auto stringId = object_manager_get_source_game_string(listItem->repositoryItem->GetFirstSourceGame());
    DrawTextBasic(dpi, screenPos, stringId, {}, { COLOUR_WHITE, TextAlignment::RIGHT });
    screenPos.y += LIST_ROW_HEIGHT;

    // Draw object dat name
    {
        const char* path = path_get_filename(listItem->repositoryItem->Path.c_str());
        auto ft = Formatter();
        ft.Add<rct_string_id>(STR_STRING);
        ft.Add<const char*>(path);
        DrawTextBasic(
            dpi, { w->windowPos.x + w->width - 5, screenPos.y }, STR_WINDOW_COLOUR_2_STRINGID, ft,
            { COLOUR_BLACK, TextAlignment::RIGHT });
        screenPos.y += LIST_ROW_HEIGHT;
    }

    // Draw object author (will be blank space if no author in file or a non JSON object)
    {
        auto ft = Formatter();
        std::string authorsString;
        for (size_t i = 0; i < listItem->repositoryItem->Authors.size(); i++)
        {
            if (i > 0)
            {
                authorsString.append(", ");
            }
            authorsString.append(listItem->repositoryItem->Authors[i]);
        }
        ft.Add<rct_string_id>(STR_STRING);
        ft.Add<const char*>(authorsString.c_str());
        DrawTextEllipsised(
            dpi, { w->windowPos.x + w->width - 5, screenPos.y }, w->width - w->widgets[WIDX_LIST].right - 4,
            STR_WINDOW_COLOUR_2_STRINGID, ft, { TextAlignment::RIGHT });
    }
}

/**
 *
 *  rct2: 0x006AAB56
 */
static void WindowEditorObjectSelectionPaint(rct_window* w, rct_drawpixelinfo* dpi)
{
    int32_t width;

    WindowDrawWidgets(w, dpi);

    // Draw tabs
    for (size_t i = 0; i < std::size(ObjectSelectionPages); i++)
    {
        const auto& widget = w->widgets[WIDX_TAB_1 + i];
        if (widget.type != WindowWidgetType::Empty)
        {
            auto image = ImageId(ObjectSelectionPages[i].Image);
            auto screenPos = w->windowPos + ScreenCoordsXY{ widget.left, widget.top };
            gfx_draw_sprite(dpi, image, screenPos);
        }
    }

    const int32_t ride_tabs[] = {
        SPR_TAB_RIDE_16,        IMAGE_TYPE_REMAP | SPR_TAB_RIDES_TRANSPORT_0,
        SPR_TAB_RIDES_GENTLE_0, IMAGE_TYPE_REMAP | SPR_TAB_RIDES_ROLLER_COASTERS_0,
        SPR_TAB_RIDES_THRILL_0, SPR_TAB_RIDES_WATER_0,
        SPR_TAB_RIDES_SHOP_0,   SPR_TAB_FINANCES_RESEARCH_0,
    };
    const int32_t ThrillRidesTabAnimationSequence[] = {
        5, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 0, 0, 0,
    };

    // Draw ride tabs
    if (GetSelectedObjectType(w) == ObjectType::Ride)
    {
        for (int32_t i = 0; i < 7; i++)
        {
            const auto& widget = w->widgets[WIDX_FILTER_RIDE_TAB_ALL + i];
            if (widget.type == WindowWidgetType::Empty)
                continue;

            int32_t spriteIndex = ride_tabs[i];
            int32_t frame = 0;
            if (i != 0 && w->pressed_widgets & (1ULL << (WIDX_FILTER_RIDE_TAB_ALL + i)))
            {
                frame = w->frame_no / window_editor_object_selection_animation_divisor[i - 1];
            }
            spriteIndex += (i == 4 ? ThrillRidesTabAnimationSequence[frame] : frame);

            auto screenPos = w->windowPos + ScreenCoordsXY{ widget.left, widget.top };
            gfx_draw_sprite(dpi, ImageId(spriteIndex, w->colours[1]), screenPos);
        }
    }

    // Preview background
    const auto& previewWidget = w->widgets[WIDX_PREVIEW];
    gfx_fill_rect(
        dpi,
        { w->windowPos + ScreenCoordsXY{ previewWidget.left + 1, previewWidget.top + 1 },
          w->windowPos + ScreenCoordsXY{ previewWidget.right - 1, previewWidget.bottom - 1 } },
        ColourMapA[w->colours[1]].darkest);

    // Draw number of selected items
    if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER))
    {
        auto screenPos = w->windowPos + ScreenCoordsXY{ 3, w->height - 13 };

        int32_t numSelected = _numSelectedObjectsForType[EnumValue(GetSelectedObjectType(w))];
        int32_t totalSelectable = object_entry_group_counts[EnumValue(GetSelectedObjectType(w))];

        auto ft = Formatter();
        ft.Add<uint16_t>(numSelected);
        ft.Add<uint16_t>(totalSelectable);
        DrawTextBasic(dpi, screenPos, STR_OBJECT_SELECTION_SELECTION_SIZE, ft);
    }

    // Draw sort button text
    const auto& listSortTypeWidget = w->widgets[WIDX_LIST_SORT_TYPE];
    if (listSortTypeWidget.type != WindowWidgetType::Empty)
    {
        auto ft = Formatter();
        auto stringId = _listSortType == RIDE_SORT_TYPE ? static_cast<rct_string_id>(_listSortDescending ? STR_DOWN : STR_UP)
                                                        : STR_NONE;
        ft.Add<rct_string_id>(stringId);
        auto screenPos = w->windowPos + ScreenCoordsXY{ listSortTypeWidget.left + 1, listSortTypeWidget.top + 1 };
        DrawTextEllipsised(dpi, screenPos, listSortTypeWidget.width(), STR_OBJECTS_SORT_TYPE, ft, { w->colours[1] });
    }
    const auto& listSortRideWidget = w->widgets[WIDX_LIST_SORT_RIDE];
    if (listSortRideWidget.type != WindowWidgetType::Empty)
    {
        auto ft = Formatter();
        auto stringId = _listSortType == RIDE_SORT_RIDE ? static_cast<rct_string_id>(_listSortDescending ? STR_DOWN : STR_UP)
                                                        : STR_NONE;
        ft.Add<rct_string_id>(stringId);
        auto screenPos = w->windowPos + ScreenCoordsXY{ listSortRideWidget.left + 1, listSortRideWidget.top + 1 };
        DrawTextEllipsised(dpi, screenPos, listSortRideWidget.width(), STR_OBJECTS_SORT_RIDE, ft, { w->colours[1] });
    }

    if (w->selected_list_item == -1 || _loadedObject == nullptr)
        return;

    list_item* listItem = &_listItems[w->selected_list_item];

    // Draw preview
    {
        rct_drawpixelinfo clipDPI;
        auto screenPos = w->windowPos + ScreenCoordsXY{ previewWidget.left + 1, previewWidget.top + 1 };
        width = previewWidget.width() - 1;
        int32_t height = previewWidget.height() - 1;
        if (clip_drawpixelinfo(&clipDPI, dpi, screenPos, width, height))
        {
            _loadedObject->DrawPreview(&clipDPI, width, height);
        }
    }

    // Draw name of object
    {
        auto screenPos = w->windowPos + ScreenCoordsXY{ previewWidget.midX() + 1, previewWidget.bottom + 3 };
        width = w->width - w->widgets[WIDX_LIST].right - 6;
        auto ft = Formatter();
        ft.Add<rct_string_id>(STR_STRING);
        ft.Add<const char*>(listItem->repositoryItem->Name.c_str());
        DrawTextEllipsised(dpi, screenPos, width, STR_WINDOW_COLOUR_2_STRINGID, ft, { TextAlignment::CENTRE });
    }

    WindowEditorObjectSelectionPaintDescriptions(w, dpi);
    WindowEditorObjectSelectionPaintDebugData(w, dpi);
}

/**
 *
 *  rct2: 0x006AADA3
 */
static void WindowEditorObjectSelectionScrollpaint(rct_window* w, rct_drawpixelinfo* dpi, int32_t scrollIndex)
{
    ScreenCoordsXY screenCoords;

    bool ridePage = (GetSelectedObjectType(w) == ObjectType::Ride);

    uint8_t paletteIndex = ColourMapA[w->colours[1]].mid_light;
    gfx_clear(dpi, paletteIndex);

    screenCoords.y = 0;
    for (const auto& listItem : _listItems)
    {
        if (screenCoords.y + SCROLLABLE_ROW_HEIGHT >= dpi->y && screenCoords.y <= dpi->y + dpi->height)
        {
            // Draw checkbox
            if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) && !(*listItem.flags & 0x20))
                gfx_fill_rect_inset(
                    dpi, { { 2, screenCoords.y }, { 11, screenCoords.y + 10 } }, w->colours[1], INSET_RECT_F_E0);

            // Highlight background
            auto highlighted = listItem.entry == w->object_entry && !(*listItem.flags & OBJECT_SELECTION_FLAG_6);
            if (highlighted)
            {
                auto bottom = screenCoords.y + (SCROLLABLE_ROW_HEIGHT - 1);
                gfx_filter_rect(dpi, { 0, screenCoords.y, w->width, bottom }, FilterPaletteID::PaletteDarken1);
            }

            // Draw checkmark
            if (!(gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER) && (*listItem.flags & OBJECT_SELECTION_FLAG_SELECTED))
            {
                screenCoords.x = 2;
                FontSpriteBase fontSpriteBase = highlighted ? FontSpriteBase::MEDIUM_EXTRA_DARK : FontSpriteBase::MEDIUM_DARK;
                colour_t colour2 = NOT_TRANSLUCENT(w->colours[1]);
                if (*listItem.flags & (OBJECT_SELECTION_FLAG_IN_USE | OBJECT_SELECTION_FLAG_ALWAYS_REQUIRED))
                    colour2 |= COLOUR_FLAG_INSET;

                gfx_draw_string(
                    dpi, screenCoords, static_cast<const char*>(CheckBoxMarkString),
                    { static_cast<colour_t>(colour2), fontSpriteBase });
            }

            screenCoords.x = gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER ? 0 : 15;

            auto bufferWithColour = strcpy(gCommonStringFormatBuffer, highlighted ? "{WINDOW_COLOUR_2}" : "{BLACK}");
            auto buffer = strchr(bufferWithColour, '\0');

            colour_t colour = COLOUR_BLACK;
            FontSpriteBase fontSpriteBase = FontSpriteBase::MEDIUM;
            if (*listItem.flags & OBJECT_SELECTION_FLAG_6)
            {
                colour = w->colours[1] & 0x7F;
                fontSpriteBase = FontSpriteBase::MEDIUM_DARK;
            }

            int32_t width_limit = w->widgets[WIDX_LIST].width() - screenCoords.x;

            if (ridePage)
            {
                width_limit /= 2;
                // Draw ride type
                rct_string_id rideTypeStringId = GetRideTypeStringId(listItem.repositoryItem);
                safe_strcpy(buffer, language_get_string(rideTypeStringId), 256 - (buffer - bufferWithColour));
                auto ft = Formatter();
                ft.Add<const char*>(gCommonStringFormatBuffer);
                DrawTextEllipsised(dpi, screenCoords, width_limit - 15, STR_STRING, ft, { colour, fontSpriteBase });
                screenCoords.x = w->widgets[WIDX_LIST_SORT_RIDE].left - w->widgets[WIDX_LIST].left;
            }

            // Draw text
            safe_strcpy(buffer, listItem.repositoryItem->Name.c_str(), 256 - (buffer - bufferWithColour));
            if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)
            {
                while (*buffer != 0 && *buffer != 9)
                    buffer++;

                *buffer = 0;
            }
            auto ft = Formatter();
            ft.Add<const char*>(gCommonStringFormatBuffer);
            DrawTextEllipsised(dpi, screenCoords, width_limit, STR_STRING, ft, { colour, fontSpriteBase });
        }
        screenCoords.y += SCROLLABLE_ROW_HEIGHT;
    }
}

static void WindowEditorObjectSetPage(rct_window* w, int32_t page)
{
    if (w->selected_tab == page)
        return;

    w->selected_tab = page;
    w->selected_list_item = -1;
    w->object_entry = nullptr;
    w->scrolls[0].v_top = 0;
    w->frame_no = 0;

    if (page == EnumValue(ObjectType::Ride))
    {
        _listSortType = RIDE_SORT_TYPE;
        _listSortDescending = false;
    }
    else
    {
        _listSortType = RIDE_SORT_RIDE;
        _listSortDescending = false;
    }

    VisibleListRefresh(w);
    w->Invalidate();
}

static void WindowEditorObjectSelectionSetPressedTab(rct_window* w)
{
    for (size_t i = 0; i < std::size(ObjectSelectionPages); i++)
    {
        w->pressed_widgets &= ~(1 << (WIDX_TAB_1 + i));
    }
    w->pressed_widgets |= 1LL << (WIDX_TAB_1 + w->selected_tab);
}

/**
 * Takes the y coordinate of the clicked on scroll list
 * and converts this into an object selection.
 * Returns the position in the list.
 * Object_selection_flags, installed_entry also populated
 *
 *  rct2: 0x006AA703
 */
static int32_t GetObjectFromObjectSelection(ObjectType object_type, int32_t y)
{
    int32_t listItemIndex = y / SCROLLABLE_ROW_HEIGHT;
    if (listItemIndex < 0 || static_cast<size_t>(listItemIndex) >= _listItems.size())
        return -1;

    return listItemIndex;
}

/**
 *
 *  rct2: 0x006D33E2
 */
static void WindowEditorObjectSelectionManageTracks()
{
    set_every_ride_type_invented();
    set_every_ride_entry_invented();

    gEditorStep = EditorStep::DesignsManager;

    int32_t entry_index = 0;
    for (; object_entry_get_chunk(ObjectType::Ride, entry_index) == nullptr; entry_index++)
        ;

    rct_ride_entry* ride_entry = get_ride_entry(entry_index);
    uint8_t ride_type = ride_entry_get_first_non_null_ride_type(ride_entry);

    auto intent = Intent(WC_TRACK_DESIGN_LIST);
    intent.putExtra(INTENT_EXTRA_RIDE_TYPE, ride_type);
    intent.putExtra(INTENT_EXTRA_RIDE_ENTRY_INDEX, entry_index);
    context_open_intent(&intent);
}

/**
 *
 *  rct2: 0x006ABBBE
 */
static void EditorLoadSelectedObjects()
{
    int32_t numItems = static_cast<int32_t>(object_repository_get_items_count());
    const ObjectRepositoryItem* items = object_repository_get_items();
    for (int32_t i = 0; i < numItems; i++)
    {
        if (_objectSelectionFlags[i] & OBJECT_SELECTION_FLAG_SELECTED)
        {
            const ObjectRepositoryItem* item = &items[i];
            const rct_object_entry* entry = &item->ObjectEntry;
            const auto* loadedObject = object_manager_get_loaded_object(ObjectEntryDescriptor(*item));
            if (loadedObject == nullptr)
            {
                loadedObject = object_manager_load_object(entry);
                if (loadedObject == nullptr)
                {
                    log_error("Failed to load entry %.8s", entry->name);
                }
                else if (!(gScreenFlags & SCREEN_FLAGS_EDITOR))
                {
                    // Defaults selected items to researched (if in-game)
                    ObjectType objectType = entry->GetType();
                    auto entryIndex = object_manager_get_loaded_object_entry_index(loadedObject);
                    if (objectType == ObjectType::Ride)
                    {
                        rct_ride_entry* rideEntry = get_ride_entry(entryIndex);
                        uint8_t rideType = ride_entry_get_first_non_null_ride_type(rideEntry);
                        ResearchCategory category = static_cast<ResearchCategory>(GetRideTypeDescriptor(rideType).Category);
                        research_insert_ride_entry(rideType, entryIndex, category, true);
                    }
                    else if (objectType == ObjectType::SceneryGroup)
                    {
                        research_insert_scenery_group_entry(entryIndex, true);
                    }
                }
            }
        }
    }
    if (_numSelectedObjectsForType[EnumValue(ObjectType::Water)] == 0)
    {
        // Reloads the default cyan water palette if no palette was selected.
        load_palette();
    }
}

static void WindowEditorObjectSelectionUpdate(rct_window* w)
{
    if (gCurrentTextBox.window.classification == w->classification && gCurrentTextBox.window.number == w->number)
    {
        window_update_textbox_caret();
        widget_invalidate(w, WIDX_FILTER_TEXT_BOX);
    }

    for (rct_widgetindex i = WIDX_FILTER_RIDE_TAB_TRANSPORT; i <= WIDX_FILTER_RIDE_TAB_STALL; i++)
    {
        if (!(w->pressed_widgets & (1ULL << i)))
            continue;

        w->frame_no++;
        if (w->frame_no >= window_editor_object_selection_animation_loops[i - WIDX_FILTER_RIDE_TAB_TRANSPORT])
            w->frame_no = 0;

        widget_invalidate(w, i);
        break;
    }
}

static void WindowEditorObjectSelectionTextinput(rct_window* w, rct_widgetindex widgetIndex, char* text)
{
    if (widgetIndex != WIDX_FILTER_TEXT_BOX || text == nullptr)
        return;

    if (strcmp(_filter_string, text) == 0)
        return;

    safe_strcpy(_filter_string, text, sizeof(_filter_string));

    FilterUpdateCounts();

    w->scrolls->v_top = 0;

    VisibleListRefresh(w);
    w->Invalidate();
}

static bool FilterSelected(uint8_t objectFlag)
{
    if (_FILTER_SELECTED == _FILTER_NONSELECTED)
    {
        return true;
    }
    if (_FILTER_SELECTED && objectFlag & OBJECT_SELECTION_FLAG_SELECTED)
    {
        return true;
    }
    if (_FILTER_NONSELECTED && !(objectFlag & OBJECT_SELECTION_FLAG_SELECTED))
    {
        return true;
    }

    return false;
}

static bool FilterString(const ObjectRepositoryItem* item)
{
    // Nothing to search for
    if (_filter_string[0] == '\0')
        return true;

    // Object doesn't have a name
    if (item->Name.empty())
        return false;

    // Get ride type
    const char* rideTypeName = language_get_string(GetRideTypeStringId(item));

    // Get object name (ride/vehicle for rides) and type name (rides only) in uppercase
    const auto nameUpper = String::ToUpper(item->Name);
    const auto typeUpper = String::ToUpper(rideTypeName);
    const auto pathUpper = String::ToUpper(item->Path);
    const auto filterUpper = String::ToUpper(_filter_string);

    // Check if the searched string exists in the name, ride type, or filename
    bool inName = nameUpper.find(filterUpper) != std::string::npos;
    bool inRideType = (item->ObjectEntry.GetType() == ObjectType::Ride) && typeUpper.find(filterUpper) != std::string::npos;
    bool inPath = pathUpper.find(filterUpper) != std::string::npos;

    return inName || inRideType || inPath;
}

static bool SourcesMatch(ObjectSourceGame source)
{
    // clang-format off
    return (_FILTER_RCT1 && source == ObjectSourceGame::RCT1) ||
           (_FILTER_AA && source == ObjectSourceGame::AddedAttractions) ||
           (_FILTER_LL && source == ObjectSourceGame::LoopyLandscapes) ||
           (_FILTER_RCT2 && source == ObjectSourceGame::RCT2) ||
           (_FILTER_WW && source == ObjectSourceGame::WackyWorlds) ||
           (_FILTER_TT && source == ObjectSourceGame::TimeTwister) ||
           (_FILTER_OO && source == ObjectSourceGame::OpenRCT2Official) ||
           (_FILTER_CUSTOM &&
            source != ObjectSourceGame::RCT1 &&
            source != ObjectSourceGame::AddedAttractions &&
            source != ObjectSourceGame::LoopyLandscapes &&
            source != ObjectSourceGame::RCT2 &&
            source != ObjectSourceGame::WackyWorlds &&
            source != ObjectSourceGame::TimeTwister &&
            source != ObjectSourceGame::OpenRCT2Official);
    // clang-format on
}

static bool FilterSource(const ObjectRepositoryItem* item)
{
    if (_FILTER_ALL)
        return true;

    for (auto source : item->Sources)
    {
        if (SourcesMatch(source))
            return true;
    }

    return false;
}

static bool FilterChunks(const ObjectRepositoryItem* item)
{
    if (item->ObjectEntry.GetType() == ObjectType::Ride)
    {
        uint8_t rideType = 0;
        for (int32_t i = 0; i < MAX_RIDE_TYPES_PER_RIDE_ENTRY; i++)
        {
            if (item->RideInfo.RideType[i] != RIDE_TYPE_NULL)
            {
                rideType = item->RideInfo.RideType[i];
                break;
            }
        }
        return (_filter_flags & (1 << (GetRideTypeDescriptor(rideType).Category + _numSourceGameItems))) != 0;
    }
    return true;
}

static void FilterUpdateCounts()
{
    if (!_FILTER_ALL || strlen(_filter_string) > 0)
    {
        const auto& selectionFlags = _objectSelectionFlags;
        std::fill(std::begin(_filter_object_counts), std::end(_filter_object_counts), 0);

        size_t numObjects = object_repository_get_items_count();
        const ObjectRepositoryItem* items = object_repository_get_items();
        for (size_t i = 0; i < numObjects; i++)
        {
            const ObjectRepositoryItem* item = &items[i];
            if (FilterSource(item) && FilterString(item) && FilterChunks(item) && FilterSelected(selectionFlags[i]))
            {
                ObjectType objectType = item->ObjectEntry.GetType();
                _filter_object_counts[EnumValue(objectType)]++;
            }
        }
    }
}

static rct_string_id GetRideTypeStringId(const ObjectRepositoryItem* item)
{
    rct_string_id result = STR_NONE;
    for (int32_t i = 0; i < MAX_RIDE_TYPES_PER_RIDE_ENTRY; i++)
    {
        uint8_t rideType = item->RideInfo.RideType[i];
        if (rideType != RIDE_TYPE_NULL)
        {
            result = GetRideTypeDescriptor(rideType).Naming.Name;
            break;
        }
    }
    return result;
}

static std::string ObjectGetDescription(const Object* object)
{
    switch (object->GetObjectType())
    {
        case ObjectType::Ride:
        {
            const RideObject* rideObject = static_cast<const RideObject*>(object);
            return rideObject->GetDescription();
        }
        default:
            return "";
    }
}

static ObjectType GetSelectedObjectType(rct_window* w)
{
    auto tab = w->selected_tab;
    if (tab >= EnumValue(ObjectType::ScenarioText))
        return static_cast<ObjectType>(tab + 1);

    return static_cast<ObjectType>(tab);
}
