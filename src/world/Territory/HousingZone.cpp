#include <Common.h>
#include <Logging/Logger.h>
#include <Util/Util.h>
#include <Util/UtilMath.h>
#include <Database/DatabaseDef.h>
#include <Exd/ExdDataGenerated.h>
#include <Network/GamePacketNew.h>
#include <Network/PacketDef/Zone/ServerZoneDef.h>

#include "Actor/Player.h"
#include "Actor/Actor.h"
#include "Actor/EventObject.h"
#include "Land.h"
#include "House.h"

#include "Forwards.h"
#include "HousingZone.h"
#include "Manager/HousingMgr.h"
#include "Framework.h"

extern Sapphire::Framework g_fw;

using namespace Sapphire::Common;
using namespace Sapphire::Network::Packets;
using namespace Sapphire::Network::Packets::Server;
using namespace Sapphire::World::Manager;

Sapphire::HousingZone::HousingZone( uint8_t wardNum,
                                uint16_t territoryTypeId,
                                uint32_t guId,
                                const std::string& internalName,
                                const std::string& contentName ) :
  Zone( territoryTypeId, guId, internalName, contentName ),
  m_wardNum( wardNum ),
  m_territoryTypeId( territoryTypeId ),
  m_landSetId( ( static_cast< uint32_t >( territoryTypeId ) << 16 ) | wardNum )
{

}

bool Sapphire::HousingZone::init()
{

  auto pDb = g_fw.get< Db::DbWorkerPool< Db::ZoneDbConnection > >();
  auto res = pDb->query( "SELECT * FROM landset WHERE landsetid = " + std::to_string( m_landSetId ) );
  if( !res->next() )
  {
    pDb->directExecute( "INSERT INTO landset ( landsetid ) VALUES ( " + std::to_string( m_landSetId ) + " );" );
  }

  int housingIndex;
  if( m_territoryTypeId == 339 )
    housingIndex = 0;
  else if( m_territoryTypeId == 340 )
    housingIndex = 1;
  else if( m_territoryTypeId == 341 )
    housingIndex = 2;
  else if( m_territoryTypeId == 641 )
    housingIndex = 3;

  auto pExdData = g_fw.get< Data::ExdDataGenerated >();
  auto info = pExdData->get< Sapphire::Data::HousingLandSet >( housingIndex );

  uint32_t landId;
  for( landId = 0; landId < 60; landId++ )
  {
    auto pLand = make_Land( m_territoryTypeId, getWardNum(), landId, m_landSetId, info );
    m_landPtrMap[ landId ] = pLand;

    if( auto house = pLand->getHouse() )
    {
      registerHouseEntranceEObj( landId << 8 );
    }
  }

  return true;
}

Sapphire::HousingZone::~HousingZone()
{

}

void Sapphire::HousingZone::onPlayerZoneIn( Entity::Player& player )
{
  auto pLog = g_fw.get< Logger >();
  pLog->debug(
    "HousingZone::onPlayerZoneIn: Zone#" + std::to_string( getGuId() ) + "|" + std::to_string( getTerritoryTypeId() ) +
    ", Entity#" + std::to_string( player.getId() ) );

  uint32_t yardPacketNum;
  uint32_t yardPacketTotal = 8;

  sendLandSet( player );

  for( yardPacketNum = 0; yardPacketNum < yardPacketTotal; yardPacketNum++ )
  {
    auto housingObjectInitializPacket = makeZonePacket< FFXIVIpcHousingObjectInitialize >( player.getId() );
    memset( &housingObjectInitializPacket->data().landIdent, 0xFF, sizeof( Common::LandIdent ) );
    housingObjectInitializPacket->data().u1 = 0xFF;
    housingObjectInitializPacket->data().packetNum = yardPacketNum;
    housingObjectInitializPacket->data().packetTotal = yardPacketTotal;

    //TODO: Add Objects here

    player.queuePacket( housingObjectInitializPacket );
  }

  auto landSetMap = makeZonePacket< FFXIVIpcLandSetMap >( player.getId() );
  landSetMap->data().subdivision = isPlayerSubInstance( player ) == false ? 2 : 1;
  uint8_t startIndex = isPlayerSubInstance( player ) == false ? 0 : 30;
  for( uint8_t i = startIndex, count = 0; i < ( startIndex + 30 ); i++, count++ )
  {
    landSetMap->data().landInfo[ count ].status = 1;
  }

  player.queuePacket( landSetMap );

}

void Sapphire::HousingZone::sendLandSet( Entity::Player& player )
{
  auto landsetInitializePacket = makeZonePacket< FFXIVIpcLandSetInitialize >( player.getId() );

  landsetInitializePacket->data().landIdent.wardNum = m_wardNum;
  //landsetInitializePacket->data().landIdent.landSetId = m_landSetId;
  landsetInitializePacket->data().landIdent.territoryTypeId = m_territoryTypeId;
  //TODO: get current WorldId
  landsetInitializePacket->data().landIdent.worldId = 67;
  landsetInitializePacket->data().subInstance = isPlayerSubInstance( player ) == false ? 1 : 2;

  uint8_t startIndex = isPlayerSubInstance( player ) == false ? 0 : 30;

  for( uint8_t i = startIndex, count = 0; i < ( startIndex + 30 ); ++i, ++count )
  {
    auto pLand = getLand( i );

    // todo: move this and sendLandUpdate building logic to its own function
    auto& landData = landsetInitializePacket->data().land[ count ];

    landData.plotSize = pLand->getSize();
    landData.houseState = pLand->getState();
    landData.iconAddIcon = pLand->getSharing();
    landData.fcId = pLand->getFcId();
    landData.fcIcon = pLand->getFcIcon();
    landData.fcIconColor = pLand->getFcColor();

    if( auto house = pLand->getHouse() )
    {
      landData.flags = 1;

      auto& parts = house->getHouseParts();

      for( auto i = 0; i != parts.size(); i++ )
      {
        landData.housePart[ i ] = parts[ i ].first;
        landData.houseColour[ i ] = parts[ i ].second;
      }
    }
  }

  player.queuePacket( landsetInitializePacket );
}

void Sapphire::HousingZone::sendLandUpdate( uint8_t landId )
{
  auto pLand = getLand( landId );
  for( const auto& playerIt : m_playerMap )
  {
    auto pPlayer = playerIt.second;

    auto landUpdatePacket = makeZonePacket< FFXIVIpcLandUpdate >( pPlayer->getId() );
    landUpdatePacket->data().landIdent.landId = landId;

    auto& landData = landUpdatePacket->data().land;

    landData.plotSize = pLand->getSize();
    landData.houseState = pLand->getState();
    landData.iconAddIcon = pLand->getSharing();
    landData.fcId = pLand->getFcId();
    landData.fcIcon = pLand->getFcIcon();
    landData.fcIconColor = pLand->getFcColor();


    if( auto house = pLand->getHouse() )
    {
      landData.flags = 1;

      auto& parts = house->getHouseParts();

      for( auto i = 0; i != parts.size(); i++ )
      {
        landData.housePart[ i ] = parts[ i ].first;
        landData.houseColour[ i ] = parts[ i ].second;
      }
    }

    pPlayer->queuePacket( landUpdatePacket );
  }
}

bool Sapphire::HousingZone::isPlayerSubInstance( Entity::Player& player )
{
  return player.getPos().x < -15000.0f; //ToDo: get correct pos
}

void Sapphire::HousingZone::onUpdate( uint32_t currTime )
{
  for( auto pLandItr : m_landPtrMap )
  {
    pLandItr.second->update( currTime );
  }
}

uint8_t Sapphire::HousingZone::getWardNum() const
{
  return m_wardNum;
}

uint32_t Sapphire::HousingZone::getLandSetId() const
{
  return m_landSetId;
}

Sapphire::LandPtr Sapphire::HousingZone::getLand( uint8_t id )
{
  auto it = m_landPtrMap.find( id );
  if( it == m_landPtrMap.end() )
    return nullptr;

  return it->second;
}

Sapphire::Entity::EventObjectPtr Sapphire::HousingZone::registerHouseEntranceEObj( uint8_t plotId )
{
  auto land = getLand( plotId );
  assert( land );

  auto eObj = Entity::make_EventObject( getNextEObjId(), 2002737, 0, 4, land->getMapMarkerPosition(), 0.f, "entrance" );
  eObj->setHousingLink( plotId << 8 );
  eObj->setScale( 1.f );

  registerEObj( eObj );

  return eObj;
}