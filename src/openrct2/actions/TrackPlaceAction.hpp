/*****************************************************************************
 * Copyright (c) 2014-2018 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "GameAction.h"

DEFINE_GAME_ACTION(TrackPlaceAction, GAME_COMMAND_PLACE_TRACK, GameActionResult)
{
private:
    NetworkRideId_t _rideIndex;
    int32_t _trackType;
    CoordsXYZD _origin;
    int32_t _brakeSpeed;
    int32_t _colour;
    int32_t _seatRotation;
    int32_t _trackPlaceFlags;

public:
    TrackPlaceAction()
    {
    }

    TrackPlaceAction(
        NetworkRideId_t rideIndex, int32_t trackType, CoordsXYZD origin, int32_t brakeSpeed, int32_t colour,
        int32_t seatRotation, int32_t liftHillAndAlternativeState)
        : _rideIndex(_rideIndex)
        , _trackType(_trackType)
        , _origin(origin)
        , _brakeSpeed(_brakeSpeed)
        , _colour(colour)
        , _seatRotation(_seatRotation)
        , _trackPlaceFlags(liftHillAndAlternativeState)
    {
    }

    uint16_t GetActionFlags() const override
    {
        return GameAction::GetActionFlags();
    }

    void Serialise(DataSerialiser & stream) override
    {
        GameAction::Serialise(stream);

        stream << DS_TAG(_rideIndex) << DS_TAG(_trackType) << DS_TAG(_origin) << DS_TAG(_brakeSpeed) << DS_TAG(_colour)
               << DS_TAG(_seatRotation) << DS_TAG(_trackPlaceFlags);
    }

    GameActionResult::Ptr Query() const override
    {
        Ride* ride = get_ride(_rideIndex);
        if (ride == nullptr)
        {
            log_warning("Invalid ride for track placement, rideIndex = %d", _rideIndex);
            return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_NONE);
        }
        if (ride->type == RIDE_TYPE_NULL)
        {
            log_warning("Invalid ride type, rideIndex = %d", _rideIndex);
            return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_NONE);
        }
        rct_ride_entry* rideEntry = get_ride_entry(ride->subtype);
        if (rideEntry == nullptr)
        {
            log_warning("Invalid ride subtype for track placement, rideIndex = %d", _rideIndex);
            return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_NONE);
        }

        if (!direction_valid(_origin.direction))
        {
            log_warning("Invalid direction for track placement, direction = %d", _origin.direction);
            return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_NONE);
        }

        auto res = std::make_unique<GameActionResult>();
        res->ExpenditureType = RCT_EXPENDITURE_TYPE_RIDE_CONSTRUCTION;
        res->Position.x = _origin.x + 16;
        res->Position.y = _origin.y + 16;
        res->Position.z = _origin.z;
        gCommandExpenditureType = RCT_EXPENDITURE_TYPE_RIDE_CONSTRUCTION;
        gCommandPosition.x = res->Position.x;
        gCommandPosition.y = res->Position.y;
        gCommandPosition.z = res->Position.z;

        int16_t trackpieceZ = _origin.z;
        gTrackGroundFlags = 0;

        uint32_t rideTypeFlags = RideProperties[ride->type].flags;

        if ((ride->lifecycle_flags & RIDE_LIFECYCLE_INDESTRUCTIBLE_TRACK) && _trackType == TRACK_ELEM_END_STATION)
        {
            return std::make_unique<GameActionResult>(GA_ERROR::DISALLOWED, STR_NOT_ALLOWED_TO_MODIFY_STATION);
        }

        if (!(GetActionFlags() & GA_FLAGS::ALLOW_WHILE_PAUSED))
        {
            if (game_is_paused() && !gCheatsBuildInPauseMode)
            {
                return std::make_unique<GameActionResult>(
                    GA_ERROR::DISALLOWED, STR_CONSTRUCTION_NOT_POSSIBLE_WHILE_GAME_IS_PAUSED);
            }
        }

        const uint8_t(*wallEdges)[16];
        if (rideTypeFlags & RIDE_TYPE_FLAG_FLAT_RIDE)
        {
            wallEdges = &FlatRideTrackSequenceElementAllowedWallEdges[_trackType];
        }
        else
        {
            if (_trackType == TRACK_ELEM_ON_RIDE_PHOTO)
            {
                if (ride->lifecycle_flags & RIDE_LIFECYCLE_ON_RIDE_PHOTO)
                {
                    return std::make_unique<GameActionResult>(GA_ERROR::DISALLOWED, STR_ONLY_ONE_ON_RIDE_PHOTO_PER_RIDE);
                }
            }
            else if (_trackType == TRACK_ELEM_CABLE_LIFT_HILL)
            {
                if (ride->lifecycle_flags & RIDE_LIFECYCLE_CABLE_LIFT_HILL_COMPONENT_USED)
                {
                    return std::make_unique<GameActionResult>(GA_ERROR::DISALLOWED, STR_ONLY_ONE_CABLE_LIFT_HILL_PER_RIDE);
                }
            }
            // Backwards steep lift hills are allowed, even on roller coasters that do not support forwards steep lift hills.
            if ((_trackPlaceFlags & CONSTRUCTION_LIFT_HILL_SELECTED)
                && !(RideTypePossibleTrackConfigurations[ride->type] & (1ULL << TRACK_LIFT_HILL_STEEP))
                && !gCheatsEnableChainLiftOnAllTrack)
            {
                if (TrackFlags[_trackType] & TRACK_ELEM_FLAG_IS_STEEP_UP)
                {
                    return std::make_unique<GameActionResult>(GA_ERROR::DISALLOWED, STR_TOO_STEEP_FOR_LIFT_HILL);
                }
            }

            wallEdges = &TrackSequenceElementAllowedWallEdges[_trackType];
        }

        money32 cost = 0;
        TileElement* tileElement;
        const rct_preview_track* trackBlock = get_track_def_from_ride(ride, _trackType);
        uint32_t num_elements = 0;
        // First check if any of the track pieces are outside the park
        for (; trackBlock->index != 0xFF; trackBlock++)
        {
            CoordsXYZ tileCoords{ _origin.x, _origin.y, _origin.z };
            
            switch (_origin.direction)
            {
                case 0:
                    tileCoords.x += trackBlock->x;
                    tileCoords.y += trackBlock->y;
                    break;
                case 1:
                    tileCoords.x += trackBlock->y;
                    tileCoords.y -= trackBlock->x;
                    break;
                case 2:
                    tileCoords.x -= trackBlock->x;
                    tileCoords.y -= trackBlock->y;
                    break;
                case 3:
                    tileCoords.x -= trackBlock->y;
                    tileCoords.y += trackBlock->x;
                    break;
            }

            if (!map_is_location_owned(tileCoords.x, tileCoords.y, tileCoords.z) && !gCheatsSandboxMode)
            {
                return std::make_unique<GameActionResult>(GA_ERROR::DISALLOWED, STR_LAND_NOT_OWNED_BY_PARK);
            }
            num_elements++;
        }

        if (!map_check_free_elements_and_reorganise(num_elements))
        {
            log_warning("Not enough free map elments to place track.");
            return std::make_unique<GameActionResult>(GA_ERROR::NO_FREE_ELEMENTS, STR_TILE_ELEMENT_LIMIT_REACHED);
        }
        const uint16_t* trackFlags = (rideTypeFlags & RIDE_TYPE_FLAG_FLAT_RIDE) ? FlatTrackFlags : TrackFlags;
        if (trackFlags[_trackType] & TRACK_ELEM_FLAG_STARTS_AT_HALF_HEIGHT)
        {
            if ((_origin.z & 0x0F) != 8)
            {
                return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_CONSTRUCTION_ERR_UNKNOWN);
            }
        }
        else
        {
            if ((_origin.z & 0x0F) != 0)
            {
                return std::make_unique<GameActionResult>(GA_ERROR::INVALID_PARAMETERS, STR_CONSTRUCTION_ERR_UNKNOWN);
            }
        }

        // If that is not the case, then perform the remaining checks
        trackBlock = get_track_def_from_ride(ride, _trackType);

        for (int32_t blockIndex = 0; trackBlock->index != 0xFF; trackBlock++, blockIndex++)
        {
            CoordsXYZ tileCoords{ _origin.x, _origin.y, _origin.z };
            int32_t bl = trackBlock->var_08;
            int32_t bh;
            switch (_origin.direction)
            {
                case 0:
                    tileCoords.x += trackBlock->x;
                    tileCoords.y += trackBlock->y;
                    break;
                case 1:
                    tileCoords.x += trackBlock->y;
                    tileCoords.y -= trackBlock->x;
                    bl = rol8(bl, 1);
                    bh = bl;
                    bh = ror8(bh, 4);
                    bl &= 0xEE;
                    bh &= 0x11;
                    bl |= bh;
                    break;
                case 2:
                    tileCoords.x -= trackBlock->x;
                    tileCoords.y -= trackBlock->y;
                    bl = rol8(bl, 2);
                    bh = bl;
                    bh = ror8(bh, 4);
                    bl &= 0xCC;
                    bh &= 0x33;
                    bl |= bh;
                    break;
                case 3:
                    tileCoords.x -= trackBlock->y;
                    tileCoords.y += trackBlock->x;
                    bl = rol8(bl, 3);
                    bh = bl;
                    bh = ror8(bh, 4);
                    bl &= 0x88;
                    bh &= 0x77;
                    bl |= bh;
                    break;
            }
            
            tileCoords.z += trackBlock->z;

            trackpieceZ = tileCoords.z;

            if (tileCoords.z < 16)
            {
                gGameCommandErrorText = STR_TOO_LOW;
                return MONEY32_UNDEFINED;
            }

            int32_t baseZ = tileCoords.z / 8;

            int32_t clearanceZ = trackBlock->var_07;
            if (trackBlock->var_09 & (1 << 2) && RideData5[ride->type].clearance_height > 24)
            {
                clearanceZ += 24;
            }
            else
            {
                clearanceZ += RideData5[ride->type].clearance_height;
            }

            clearanceZ = (clearanceZ / 8) + baseZ;

            if (clearanceZ >= 255)
            {
                gGameCommandErrorText = STR_TOO_HIGH;
                return MONEY32_UNDEFINED;
            }

            _currentTrackEndX = tileCoords.x;
            _currentTrackEndY = tileCoords.y;

            if (!gCheatsDisableClearanceChecks || flags & GAME_COMMAND_FLAG_GHOST)
            {
                uint8_t crossingMode = (ride->type == RIDE_TYPE_MINIATURE_RAILWAY && _trackType == TRACK_ELEM_FLAT)
                    ? CREATE_CROSSING_MODE_TRACK_OVER_PATH
                    : CREATE_CROSSING_MODE_NONE;
                if (!map_can_construct_with_clear_at(
                        tileCoords.x, tileCoords.y, baseZ, clearanceZ, &map_place_non_scenery_clear_func, bl, flags, &cost,
                        crossingMode))
                    return MONEY32_UNDEFINED;
            }

            // 6c53dc

            if ((flags & GAME_COMMAND_FLAG_APPLY) && !(flags & GAME_COMMAND_FLAG_GHOST) && !gCheatsDisableClearanceChecks)
            {
                footpath_remove_litter(tileCoords.x, tileCoords.y, tileCoords.z);
                if (rideTypeFlags & RIDE_TYPE_FLAG_TRACK_NO_WALLS)
                {
                    wall_remove_at(tileCoords.x, tileCoords.y, baseZ * 8, clearanceZ * 8);
                }
                else
                {
                    // Remove walls in the directions this track intersects
                    uint8_t intersectingDirections = (*wallEdges)[blockIndex];
                    intersectingDirections ^= 0x0F;
                    for (int32_t i = 0; i < 4; i++)
                    {
                        if (intersectingDirections & (1 << i))
                        {
                            wall_remove_intersecting_walls(tileCoords.x, tileCoords.y, baseZ, clearanceZ, i);
                        }
                    }
                }
            }

            bh = gMapGroundFlags & (ELEMENT_IS_ABOVE_GROUND | ELEMENT_IS_UNDERGROUND);
            if (gTrackGroundFlags != 0 && (gTrackGroundFlags & bh) == 0)
            {
                gGameCommandErrorText = STR_CANT_BUILD_PARTLY_ABOVE_AND_PARTLY_BELOW_GROUND;
                return MONEY32_UNDEFINED;
            }

            gTrackGroundFlags = bh;
            if (rideTypeFlags & RIDE_TYPE_FLAG_FLAT_RIDE)
            {
                if (FlatTrackFlags[_trackType] & TRACK_ELEM_FLAG_ONLY_ABOVE_GROUND)
                {
                    if (gTrackGroundFlags & TRACK_ELEMENT_LOCATION_IS_UNDERGROUND)
                    {
                        gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ABOVE_GROUND;
                        return MONEY32_UNDEFINED;
                    }
                }
            }
            else
            {
                if (TrackFlags[_trackType] & TRACK_ELEM_FLAG_ONLY_ABOVE_GROUND)
                {
                    if (gTrackGroundFlags & TRACK_ELEMENT_LOCATION_IS_UNDERGROUND)
                    {
                        gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ABOVE_GROUND;
                        return MONEY32_UNDEFINED;
                    }
                }
            }

            if (rideTypeFlags & RIDE_TYPE_FLAG_FLAT_RIDE)
            {
                if (FlatTrackFlags[_trackType] & TRACK_ELEM_FLAG_ONLY_UNDERWATER)
                {
                    if (!(gMapGroundFlags & ELEMENT_IS_UNDERWATER))
                    {
                        gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_UNDERWATER;
                        return MONEY32_UNDEFINED;
                    }
                }
            }
            else
            {
                if (TrackFlags[_trackType] & TRACK_ELEM_FLAG_ONLY_UNDERWATER)
                { // No element has this flag
                    if (gMapGroundFlags & ELEMENT_IS_UNDERWATER)
                    {
                        gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_UNDERWATER;
                        return MONEY32_UNDEFINED;
                    }
                }
            }

            if (gMapGroundFlags & ELEMENT_IS_UNDERWATER && !gCheatsDisableClearanceChecks)
            {
                gGameCommandErrorText = STR_RIDE_CANT_BUILD_THIS_UNDERWATER;
                return MONEY32_UNDEFINED;
            }

            if ((rideTypeFlags & RIDE_TYPE_FLAG_TRACK_MUST_BE_ON_WATER) && !byte_9D8150)
            {
                tileElement = map_get_surface_element_at({ x, y });

                uint8_t water_height = tileElement->AsSurface()->GetWaterHeight() * 2;
                if (water_height == 0)
                {
                    gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ON_WATER;
                    return MONEY32_UNDEFINED;
                }

                if (water_height != baseZ)
                {
                    gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ON_WATER;
                    return MONEY32_UNDEFINED;
                }
                water_height -= 2;
                if (water_height == tileElement->base_height)
                {
                    bh = tileElement->AsSurface()->GetSlope() & TILE_ELEMENT_SLOPE_ALL_CORNERS_UP;
                    if (bh == TILE_ELEMENT_SLOPE_W_CORNER_DN || bh == TILE_ELEMENT_SLOPE_S_CORNER_DN
                        || bh == TILE_ELEMENT_SLOPE_E_CORNER_DN || bh == TILE_ELEMENT_SLOPE_N_CORNER_DN)
                    {
                        gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ON_WATER;
                        return MONEY32_UNDEFINED;
                    }
                }
            }

            int32_t entranceDirections;
            if (ride_type_has_flag(ride->type, RIDE_TYPE_FLAG_FLAT_RIDE))
            {
                entranceDirections = FlatRideTrackSequenceProperties[_trackType][0];
            }
            else
            {
                entranceDirections = TrackSequenceProperties[_trackType][0];
            }
            if ((entranceDirections & TRACK_SEQUENCE_FLAG_ORIGIN) && trackBlock->index == 0)
            {
                if (!track_add_station_element(tileCoords.x, tileCoords.y, baseZ, direction, rideIndex, 0))
                {
                    return MONEY32_UNDEFINED;
                }
            }
            // 6c55be
            if (entranceDirections & TRACK_SEQUENCE_FLAG_CONNECTS_TO_PATH)
            {
                entranceDirections &= 0x0F;
                if (entranceDirections != 0)
                {
                    if (!(flags & GAME_COMMAND_FLAG_APPLY) && !(flags & GAME_COMMAND_FLAG_GHOST)
                        && !gCheatsDisableClearanceChecks)
                    {
                        uint8_t _bl = entranceDirections;
                        for (int32_t dl = bitscanforward(_bl); dl != -1; dl = bitscanforward(_bl))
                        {
                            _bl &= ~(1 << dl);
                            int32_t temp_x = tileCoords.x, temp_y = tileCoords.y;
                            int32_t temp_direction = (direction + dl) & 3;
                            temp_x += CoordsDirectionDelta[temp_direction].x;
                            temp_y += CoordsDirectionDelta[temp_direction].y;
                            temp_direction = direction_reverse(temp_direction);
                            wall_remove_intersecting_walls(temp_x, temp_y, baseZ, clearanceZ, temp_direction & 3);
                        }
                    }
                }
            }
            // 6c5648 12 push
            tileElement = map_get_surface_element_at({ tileCoords.x, tileCoords.y });
            if (!gCheatsDisableSupportLimits)
            {
                int32_t ride_height = clearanceZ - tileElement->base_height;
                if (ride_height >= 0)
                {
                    uint16_t maxHeight;

                    if (RideGroupManager::RideTypeIsIndependent(ride->type) && rideEntry->max_height != 0)
                    {
                        maxHeight = rideEntry->max_height;
                    }
                    else if (RideGroupManager::RideTypeHasRideGroups(ride->type))
                    {
                        const RideGroup* rideGroup = RideGroupManager::GetRideGroup(ride->type, rideEntry);
                        maxHeight = rideGroup->MaximumHeight;
                    }
                    else
                    {
                        maxHeight = RideData5[ride->type].max_height;
                    }

                    ride_height /= 2;
                    if (ride_height > maxHeight && !byte_9D8150)
                    {
                        gGameCommandErrorText = STR_TOO_HIGH_FOR_SUPPORTS;
                        return MONEY32_UNDEFINED;
                    }
                }
            }

            int32_t _support_height = baseZ - tileElement->base_height;
            if (_support_height < 0)
            {
                _support_height = 10;
            }

            cost += ((_support_height / 2) * RideTrackCosts[ride->type].support_price) * 5;

            // 6c56d3

            if (!(flags & GAME_COMMAND_FLAG_APPLY))
                continue;

            invalidate_test_results(ride);
            switch (_trackType)
            {
                case TRACK_ELEM_ON_RIDE_PHOTO:
                    ride->lifecycle_flags |= RIDE_LIFECYCLE_ON_RIDE_PHOTO;
                    break;
                case TRACK_ELEM_CABLE_LIFT_HILL:
                    if (trackBlock->index != 0)
                        break;
                    ride->lifecycle_flags |= RIDE_LIFECYCLE_CABLE_LIFT_HILL_COMPONENT_USED;
                    ride->cable_lift_x = tileCoords.x;
                    ride->cable_lift_y = tileCoords.y;
                    ride->cable_lift_z = baseZ;
                    break;
                case TRACK_ELEM_BLOCK_BRAKES:
                    ride->num_block_brakes++;
                    ride->window_invalidate_flags |= RIDE_INVALIDATE_RIDE_OPERATING;

                    ride->mode = RIDE_MODE_CONTINUOUS_CIRCUIT_BLOCK_SECTIONED;
                    if (ride->type == RIDE_TYPE_LIM_LAUNCHED_ROLLER_COASTER)
                        ride->mode = RIDE_MODE_POWERED_LAUNCH_BLOCK_SECTIONED;

                    break;
            }

            if (trackBlock->index == 0)
            {
                switch (_trackType)
                {
                    case TRACK_ELEM_25_DEG_UP_TO_FLAT:
                    case TRACK_ELEM_60_DEG_UP_TO_FLAT:
                    case TRACK_ELEM_DIAG_25_DEG_UP_TO_FLAT:
                    case TRACK_ELEM_DIAG_60_DEG_UP_TO_FLAT:
                        if (!(liftHillAndAlternativeState & CONSTRUCTION_LIFT_HILL_SELECTED))
                            break;
                        [[fallthrough]];
                    case TRACK_ELEM_CABLE_LIFT_HILL:
                        ride->num_block_brakes++;
                        break;
                }
            }

            entranceDirections = 0;
            if (ride->overall_view.xy != RCT_XY8_UNDEFINED)
            {
                if (!(flags & GAME_COMMAND_FLAG_5))
                {
                    if (ride_type_has_flag(ride->type, RIDE_TYPE_FLAG_FLAT_RIDE))
                    {
                        entranceDirections = FlatRideTrackSequenceProperties[_trackType][0];
                    }
                    else
                    {
                        entranceDirections = TrackSequenceProperties[_trackType][0];
                    }
                }
            }
            if (entranceDirections & TRACK_SEQUENCE_FLAG_ORIGIN || ride->overall_view.xy == RCT_XY8_UNDEFINED)
            {
                ride->overall_view.x = tileCoords.x / 32;
                ride->overall_view.y = tileCoords.y / 32;
            }

            tileElement = tile_element_insert(tileCoords.x / 32, tileCoords.y / 32, baseZ, bl & 0xF);
            assert(tileElement != nullptr);
            tileElement->clearance_height = clearanceZ;
            tileElement->SetType(TILE_ELEMENT_TYPE_TRACK);
            tileElement->SetDirection(direction);
            if (liftHillAndAlternativeState & CONSTRUCTION_LIFT_HILL_SELECTED)
            {
                tileElement->AsTrack()->SetHasChain(true);
            }

            tileElement->AsTrack()->SetSequenceIndex(trackBlock->index);
            tileElement->AsTrack()->SetRideIndex(_rideIndex);
            tileElement->AsTrack()->SetTrackType(_trackType);

            if (flags & GAME_COMMAND_FLAG_GHOST)
            {
                tileElement->flags |= TILE_ELEMENT_FLAG_GHOST;
            }

            switch (_trackType)
            {
                case TRACK_ELEM_WATERFALL:
                    map_animation_create(
                        MAP_ANIMATION_TYPE_TRACK_WATERFALL, tileCoords.x, tileCoords.y, tileElement->base_height);
                    break;
                case TRACK_ELEM_RAPIDS:
                    map_animation_create(MAP_ANIMATION_TYPE_TRACK_RAPIDS, tileCoords.x, tileCoords.y, tileElement->base_height);
                    break;
                case TRACK_ELEM_WHIRLPOOL:
                    map_animation_create(
                        MAP_ANIMATION_TYPE_TRACK_WHIRLPOOL, tileCoords.x, tileCoords.y, tileElement->base_height);
                    break;
                case TRACK_ELEM_SPINNING_TUNNEL:
                    map_animation_create(
                        MAP_ANIMATION_TYPE_TRACK_SPINNINGTUNNEL, tileCoords.x, tileCoords.y, tileElement->base_height);
                    break;
            }
            if (track_element_has_speed_setting(_trackType))
            {
                tileElement->AsTrack()->SetBrakeBoosterSpeed(brakeSpeed);
            }
            else
            {
                tileElement->AsTrack()->SetSeatRotation(seatRotation);
            }

            if (liftHillAndAlternativeState & RIDE_TYPE_ALTERNATIVE_TRACK_TYPE)
            {
                tileElement->AsTrack()->SetInverted(true);
            }
            tileElement->AsTrack()->SetColourScheme(colour);

            if (ride_type_has_flag(ride->type, RIDE_TYPE_FLAG_FLAT_RIDE))
            {
                entranceDirections = FlatRideTrackSequenceProperties[_trackType][0];
            }
            else
            {
                entranceDirections = TrackSequenceProperties[_trackType][0];
            }

            if (entranceDirections & TRACK_SEQUENCE_FLAG_ORIGIN)
            {
                if (trackBlock->index == 0)
                {
                    track_add_station_element(tileCoords.x, tileCoords.y, baseZ, direction, rideIndex, GAME_COMMAND_FLAG_APPLY);
                }
                sub_6CB945(rideIndex);
                ride_update_max_vehicles(ride);
            }

            if (rideTypeFlags & RIDE_TYPE_FLAG_TRACK_MUST_BE_ON_WATER)
            {
                TileElement* surfaceElement = map_get_surface_element_at({ tileCoords.x, tileCoords.y });
                surfaceElement->AsSurface()->SetHasTrackThatNeedsWater(true);
                tileElement = surfaceElement;
            }

            if (!gCheatsDisableClearanceChecks || !(flags & GAME_COMMAND_FLAG_GHOST))
            {
                footpath_connect_edges(tileCoords.x, tileCoords.y, tileElement, flags);
            }
            map_invalidate_tile_full(tileCoords.x, tileCoords.y);
        }

        if (gGameCommandNestLevel == 1)
        {
            LocationXYZ16 coord;
            coord.x = res->Position.x;
            coord.y = res->Position.y;
            coord.z = trackpieceZ;
            network_set_player_last_action_coord(network_get_player_index(game_command_playerid), coord);
        }

        money32 price = RideTrackCosts[ride->type].track_price;
        price *= (rideTypeFlags & RIDE_TYPE_FLAG_FLAT_RIDE) ? FlatRideTrackPricing[_trackType] : TrackPricing[_trackType];

        price >>= 16;
        price = cost + ((price / 2) * 10);

        if (gParkFlags & PARK_FLAGS_NO_MONEY)
        {
            return 0;
        }
        else
        {
            return price;
        }
    }

    GameActionResult::Ptr Execute() const override
    {
    }
};
