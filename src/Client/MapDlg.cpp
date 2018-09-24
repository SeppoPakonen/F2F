#include "Client.h"

#define IMAGECLASS GoogleMapsImg
#define IMAGEFILE <Client/GoogleMaps.iml>
#include <Draw/iml_source.h>


void MapDlgDlg::SetHome()
{
	map.home = GoogleMapsGpsToPixel(center, ~zoom, Size(640, 640), home);
	gpsy.SetLabel("  " + FormatGPSY(home.y));
	gpsx.SetLabel("  " + FormatGPSX(home.x));
	map.Refresh();
}

void MapDlgDlg::LoadMap()
{
	map.map = GetGoogleMapImage(center.x, center.y, ~zoom, 640, 640, "png", &map.error);
	SetHome();
}

void MapDlgDlg::Move(int x, int y)
{
	center = GoogleMapsPixelToGps(center, ~zoom, Point(250 * x, 250 * y));
	LoadMap();
}

void MapDlgDlg::MapClick(Point p)
{
	home = GoogleMapsPixelToGps(center, ~zoom, Size(640, 640), p);
	SetHome();
	WhenHome(home);
}

void MapDlgDlg::Set(Pointf p)
{
	home = center = p;
	LoadMap();
}

void MapDlgDlg::ZoomIn()
{
	zoom <<= min((int)~zoom + 1, 21);
	LoadMap();
}

void MapDlgDlg::ZoomOut()
{
	zoom <<= max((int)~zoom - 1, 0);
	LoadMap();	
}

MapDlgDlg::MapDlgDlg()
{
	CtrlLayout(*this, "");
	
	Size sz = GetSize();
	Size msz = map.GetSize();
	sz += Size(640 - msz.cx, 640 - msz.cy);
	SetRect(sz);

	for(int i = 0; i < 22; i++)
		zoom.Add(i);	
	zoom <<= 16;
	zoom <<= THISBACK(LoadMap);
	zoomin <<= THISBACK(ZoomIn);
	zoomout <<= THISBACK(ZoomOut);
	
	left <<= THISBACK2(Move, -1, 0);
	left.SetImage(CtrlImg::SmallLeft());
	right <<= THISBACK2(Move, 1, 0);
	right.SetImage(CtrlImg::SmallRight());
	up <<= THISBACK2(Move, 0, -1);
	up.SetImage(CtrlImg::SmallUp());
	down <<= THISBACK2(Move, 0, 1);
	down.SetImage(CtrlImg::SmallDown());
	
	map.WhenLeftClick = THISBACK(MapClick);
}

