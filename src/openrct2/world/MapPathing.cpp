/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "MapPathing.h"
#include "Map.h"
#include "../util/Util.h"
#include "../ride/Ride.h"
#include "../ride/RideData.h"

struct Node;
struct NodeConnection
{
    int32_t cost;
    Node* node;
};

struct Node
{
    CoordsXYZ loc;
    std::vector<NodeConnection> directConnections;
    std::vector<NodeConnection> indirectConnections;
    uint8_t type;
    PathElement* element;
};

std::unordered_map<uint32_t, Node*> nodesMap;
std::vector<Node> nodes;

uint32_t GetHash(const CoordsXYZ& loc)
{
    return (loc.x / 32) * COORDS_Z_STEP * 32 + (loc.y / 32) * COORDS_Z_STEP + loc.z / COORDS_Z_STEP;
}

static void ResetNodes()
{
    nodes.clear();
    nodesMap.clear();
    tile_element_iterator it;

    tile_element_iterator_begin(&it);
    do
    {
        auto type = it.element->GetType();
        if (type != TILE_ELEMENT_TYPE_PATH)
        {
            continue;
        }

        Node newNode;
        newNode.loc = { it.x * 32, it.y * 32, it.element->GetBaseZ() };
        newNode.type = type;
        newNode.element = it.element->AsPath();
        nodes.push_back(newNode);
    } while (tile_element_iterator_next(&it));

    for (auto& i : nodes)
    {
        uint32_t hash = GetHash(i.loc);
        nodesMap[hash] = &i;
    }
}

static bool is_valid_path_z_and_direction(TileElement* tileElement, int32_t currentZ, int32_t currentDirection)
{
    if (tileElement->AsPath()->IsSloped())
    {
        int32_t slopeDirection = tileElement->AsPath()->GetSlopeDirection();
        if (slopeDirection == currentDirection)
        {
            if (currentZ != tileElement->GetBaseZ())
                return false;
        }
        else
        {
            slopeDirection = direction_reverse(slopeDirection);
            if (slopeDirection != currentDirection)
                return false;
            if (currentZ != tileElement->GetBaseZ() + 2 * COORDS_Z_STEP)
                return false;
        }
    }
    else
    {
        if (currentZ != tileElement->GetBaseZ())
            return false;
    }
    return true;
}

static std::optional<Node*> footpath_element_next_in_direction(Node& node, Direction chosenDirection)
{
    TileElement* nextTileElement;
    auto targetLoc = node.loc;
    if (node.element->IsSloped())
    {
        if (node.element->GetSlopeDirection() == chosenDirection)
        {
            targetLoc.z += 2 * COORDS_Z_STEP;
        }
    }

    targetLoc += CoordsDirectionDelta.at(static_cast<int64_t>(chosenDirection));
    nextTileElement = map_get_first_element_at(targetLoc);
    do
    {
        if (nextTileElement == nullptr)
            break;
        if (nextTileElement->IsGhost())
            continue;
        if (nextTileElement->GetType() != TILE_ELEMENT_TYPE_PATH)
            continue;
        if (!is_valid_path_z_and_direction(nextTileElement, targetLoc.z, chosenDirection))
            continue;

        return { nodesMap[GetHash({ targetLoc.x, targetLoc.y, nextTileElement->GetBaseZ() })] };
    } while (!(nextTileElement++)->IsLastForTile());

    return std::nullopt;
}


static void ConnectNodes()
{
    for (auto& nd : nodesMap)
    {
        uint8_t edges = nd.second->element->GetEdges();

        for (int32_t test_edge = bitscanforward(edges); test_edge != -1; test_edge = bitscanforward(edges))
        {
            edges &= ~(1 << test_edge);
            if (auto res = footpath_element_next_in_direction(*nd.second, test_edge); res)
            {
                nd.second->directConnections.push_back({ 1, *res });
            }
        }
    }
}

static void ConnectRides()
{
    for (auto& nd : nodesMap)
    {
        if (nd.second->element->IsQueue())
        {
            auto rideIndex = nd.second->element->GetRideIndex();
            if (rideIndex != RIDE_ID_NULL)
            {
                auto ride = get_ride(rideIndex);
                if (RideTypeDescriptors[ride->type].Flags & RIDE_TYPE_FLAG_TRANSPORT_RIDE)
                {
                    for (auto i = 0; i < MAX_STATIONS; ++i)
                    {
                        if (i == nd.second->element->GetStationIndex())
                            continue;

                        auto stationExit = ride->stations[i].Exit;
                        if (stationExit.isNull())
                            continue;
                        auto stationExitPath = stationExit.ToCoordsXYZ();
                        stationExitPath += CoordsDirectionDelta[direction_reverse(stationExit.direction)];
                        auto cost = abs(stationExitPath.x - nd.second->loc.x) + abs(stationExitPath.y - nd.second->loc.y)
                            + abs(stationExitPath.z - nd.second->loc.z);
                        cost /= 50; // Arbitary amount;
                        if (nodesMap.find(GetHash(stationExitPath)) != nodesMap.end())
                        {
                            auto node = nodesMap[GetHash(stationExitPath)];
                            nd.second->indirectConnections.push_back({ cost, node });
                        }
                    }
                }
            }
        }
    }
}

static void ConnectTwoEdges()
{
    for (auto & nd: nodesMap)
    {
        if (nd.second->directConnections.size() == 2)
        {
            auto& parent = nd.second;
            for (auto& child : parent->directConnections)
            {
                // Connect all 2 size... don't loop... remember parent...
            }
        }
    }
}

void InitMapPathing()
{
    ResetNodes();
    ConnectNodes();
    ConnectRides();
    ConnectTwoEdges();
}
