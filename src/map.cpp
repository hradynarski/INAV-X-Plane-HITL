#include "map.h"
#include "config.h"
#include "msp.h"
#include "util.h"

TMap g_map;

#define INAV_LAT_LON_SCALE 10000000.0f
#define DISTANCE_BETWEEN_TWO_LONGITUDE_POINTS_AT_EQUATOR    1.113195f     //inav lon to cm

#define LAT_LON_DIFF (2000.0f / DISTANCE_BETWEEN_TWO_LONGITUDE_POINTS_AT_EQUATOR / INAV_LAT_LON_SCALE)

//==============================================================
//==============================================================
void TMap::init()
{
  if (XPLMMapExists(XPLM_MAP_USER_INTERFACE))
  {
    createOurMapLayer(XPLM_MAP_USER_INTERFACE, NULL);
  }
  // Listen for any new map objects that get created
  XPLMRegisterMapCreationHook(&TMap::createOurMapLayerStatic, NULL);
}

//==============================================================
//==============================================================
void TMap::destroy()
{
  XPLMDestroyMapLayer(this->layer);
}

//==============================================================
//==============================================================
void TMap::setMarkingType(TMapMarkType type)
{
  this->markingType = type;
  this->pHead = this->pTail = 0;
}

//==============================================================
//==============================================================
TMapMarkType TMap::getMarkingType()
{
  return this->markingType;
}

//==============================================================
//==============================================================
void TMap::createOurMapLayerStatic(const char * mapIdentifier, void * refcon)
{
  g_map.createOurMapLayer(mapIdentifier, refcon);
}


//==============================================================
//==============================================================
void TMap::createOurMapLayer(const char * mapIdentifier, void * refcon)
{
  if (!this->layer && // Confirm we haven't created our markings layer yet (e.g., as a result of a previous callback), or if we did, it's been destroyed
    !strcmp(mapIdentifier, XPLM_MAP_USER_INTERFACE)) // we only want to create a layer in the normal user interface map (not the IOS)
  {
    XPLMCreateMapLayer_t params;
    params.structSize = sizeof(XPLMCreateMapLayer_t);
    params.mapToCreateLayerIn = XPLM_MAP_USER_INTERFACE;
    params.willBeDeletedCallback = &TMap::willBeDeletedStatic;
    params.prepCacheCallback = NULL;
    params.showUiToggle = 1;
    params.refcon = NULL;
    params.layerType = xplm_MapLayer_Markings;
    params.drawCallback = &TMap::drawMarkingsStatic;
    params.iconCallback = NULL;
    params.labelCallback = NULL;
    params.layerName = "INAV HITL";
    // Note: this could fail (return NULL) if we hadn't already confirmed that params.mapToCreateLayerIn exists in X-Plane already
    this->layer = XPLMCreateMapLayer(&params);
  }
}

//==============================================================
//==============================================================
void TMap::willBeDeletedStatic(XPLMMapLayerID layer, void * inRefcon)
{
  if (layer == g_map.layer)
  {
    g_map.layer = NULL;
  }
}

//==============================================================
//==============================================================
void TMap::drawMarkingsStatic(XPLMMapLayerID layer, const float * inMapBoundsLeftTopRightBottom, float zoomRatio, float mapUnitsPerUserInterfaceUnit, XPLMMapStyle mapStyle, XPLMMapProjectionID projection, void * inRefcon)
{
  g_map.drawMarkings(layer, inMapBoundsLeftTopRightBottom, zoomRatio, mapUnitsPerUserInterfaceUnit, mapStyle, projection, inRefcon);
}

//==============================================================
//==============================================================
void TMap::drawMarkings(XPLMMapLayerID layer, const float * inMapBoundsLeftTopRightBottom, float zoomRatio, float mapUnitsPerUserInterfaceUnit, XPLMMapStyle mapStyle, XPLMMapProjectionID projection, void * inRefcon)
{
  XPLMSetGraphicsState(
    0 /* no fog */,
    0 /* 0 texture units */,
    0 /* no lighting */,
    0 /* no alpha testing */,
    1 /* do alpha blend */,
    1 /* do depth testing */,
    0 /* no depth writing */
  );

  float x;
  float y;

  if (this->waypointsCount > 1)
  {
    glColor3f(1, 0, 1);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < this->waypointsCount; i++)
    {
      XPLMMapProject(projection, waypoints[i].lat, waypoints[i].lon, &x, &y);
      glVertex2f(x, y);
    }
    glEnd();

    for (int i = 0; i < this->waypointsCount; i++)
    {
      const float width = XPLMMapScaleMeter(projection, this->crossLat, this->crossLon) * 10;
      XPLMMapProject(projection, waypoints[i].lat, waypoints[i].lon, &x, &y);
      glBegin(GL_LINE_LOOP);
      glVertex2f(x - width, y - width);
      glVertex2f(x - width, y + width);
      glVertex2f(x + width, y + width);
      glVertex2f(x + width, y - width);
      glEnd();
    }
  }

  if (this->pHead != this->pTail)
  {
    glColor3f(1, 0, 0);
    glBegin(GL_LINE_STRIP);
    for (int i = this->pHead; i != this->pTail; i++)
    {
      if (i >= MAX_MAP_POINTS) i = 0;
      XPLMMapProject(projection, coords[i].lat, coords[i].lon, &x, &y);
      glVertex2f(x, y);
    }
    glEnd();

    const float width = XPLMMapScaleMeter(projection, this->crossLat, this->crossLon) * 10;

    XPLMMapProject(projection, this->crossLat, this->crossLon, &x, &y);

    glBegin(GL_LINE_STRIP);
    glVertex2f(x - width, y);
    glVertex2f(x + width, y);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex2f(x, y - width);
    glVertex2f(x, y + width);
    glEnd();
  }
}

//==============================================================
//==============================================================
void TMap::addPoint(float lat, float lon)
{
  this->coords[this->pTail].lat = lat;
  this->coords[this->pTail].lon = lon;
  this->pTail++;
  if (this->pTail == MAX_MAP_POINTS) this->pTail = 0;
  if (this->pHead == this->pTail)
  {
    this->pHead++;
    if (this->pHead == MAX_MAP_POINTS)
    {
      this->pHead = 0;
    }
  }
}

//==============================================================
//==============================================================
void TMap::addPointEx(float lat, float lon)
{
  //do not add unititialized
  //of partially initialized coords
  //while (number,0) or ( 0,number) are legal, we do not add them to avoid incorrect path lines
  if ((lat == 0.0f) || (lon == 0.0f)) return;  

  this->crossLat = lat;
  this->crossLon = lon;

  if (this->pHead == this->pTail)
  {
    this->addPoint(lat, lon);
  }
  else
  {
    int pt = this->pTail == 0 ? MAX_MAP_POINTS-1 : this->pTail - 1;

    float dLat = this->coords[pt].lat - lat;
    if (dLat < 0) dLat = -dLat;
    float dLon = this->coords[pt].lon - lon;
    if (dLon < 0) dLon = -dLon;

    if (dLat > LAT_LON_DIFF || dLon > LAT_LON_DIFF)
    {
      this->addPoint(lat, lon);
    }
  }
}

//==============================================================
//==============================================================
void TMap::addDebug(int32_t lat, int32_t lon)
{
  if (this->markingType == MMT_DEBUG_0_1)
  {
    this->addPointEx(lat / INAV_LAT_LON_SCALE, lon / INAV_LAT_LON_SCALE);
  }
}

//==============================================================
//==============================================================
void TMap::addLatLonOSD(float lat, float lon)
{
  if (this->markingType == MMT_LAT_LON_OSD)
  {
    this->addPointEx(lat, lon);
  }
}

//==============================================================
//==============================================================
void TMap::startDownloadWaypoints()
{
  this->waypointsDownloadState = 0;
  this->waypointsCount = 0;

  if (!g_msp.sendCommand(MSP_WP_GETINFO, NULL, 0)) this->waypointsDownloadState = -1;
}

//==============================================================
//==============================================================
void TMap::onWPInfo(const TMSPWPInfo* messageBuffer)
{
  LOG("Got WP Info command, valid = %d, count = %d", messageBuffer->waypointsListValid, messageBuffer->waypointsCount);

  if (this->waypointsDownloadState != 0) return;
  if (!messageBuffer->waypointsListValid || (messageBuffer->waypointsCount == 0))
  {
    this->waypointsDownloadState = -1;
  }
  else
  {
    this->waypointsDownloadState = 1;
    this->waypointsCount = messageBuffer->waypointsCount;

    for (int i = 0; i < this->waypointsCount; i++)
    {
      this->waypoints[i].lat = 0;
      this->waypoints[i].lon = 0;
    }

    this->retrieveNextWaypoint();
  }
}

//==============================================================
//==============================================================
void TMap::onWP(const TMSPWP* messageBuffer)
{
  LOG("Got WP command, index = %d", messageBuffer->index);

  if ((messageBuffer->index < 1) || (messageBuffer->index > this->waypointsCount)) return;

  this->waypoints[messageBuffer->index-1].lat = messageBuffer->lat / INAV_LAT_LON_SCALE;
  this->waypoints[messageBuffer->index-1].lon = messageBuffer->lon / INAV_LAT_LON_SCALE;
  this->waypointsDownloadState++;
  if (this->waypointsDownloadState <= this->waypointsCount )
  {
    this->retrieveNextWaypoint();
  }
}

//==============================================================
//==============================================================
void TMap::retrieveNextWaypoint()
{
  if (!g_msp.sendCommand(MSP_WP, &this->waypointsDownloadState, 1)) this->waypointsDownloadState = -100;
}

