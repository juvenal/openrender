//  openRender
//
//  microgrid.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Fri Nov 15 2002
//
//  Original Development:
//    (C) 2002 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: microgrid.h,v 1.2 2003/06/08 19:13:51 juvenal Exp $
//

#ifndef MICROGRID_H
#define MICROGRID_H

class microgrid {
  public: 
    // Constructor
    microgrid ( int width = 1, int height = 1);
    // Destructor
    ~microgrid ();
    // Member functions
    void  allocate ( int width, int height);
    void  free ();
    bool  extractMicroPolygon ( microPolygon &m, int u, int v); // Extract micro-polygon from grid
    int   width () {return width;}
    int   height () {return height;}
    void  setSize ( int width, int height);  // Change size and re-initialise microgrid
    void  setTextureCoords ( float _umin, float _vmin, float _umax, float _vmax)
        {umin=_umin; vmin=_vmin; umax=_umax; vmax=_vmax;}
    void  computeNormals ();
    void  shade ( list<Light*> &lights);
    void  displace ();
    void  statistics ( float &zmin, float &zmax, float &maxusize, float &maxvsize);
    // Stream input/output
    friend ostream &operator << ( ostream &io, const MicroGrid &m);
  protected:
    int     width,height;    //
    float   umin,vmin;       //
    float   umax,vmax;       //
    Point3  **point;         // 2D array of points
    Colour  **colour;        // 2D array of micro-polygon colours
    Opacity **opacity;       // 2D array of micro-polygon opacity
    Vector3 **normal;        // 2D array of normals
};




// Reyes includes
#include "point3D.h"
#include "micropolygon.h"
#include "colour.h"
#include "light.h"
#include "surface.h"
#include "displacement_shaders.h"

class MicroGrid
{
  public:
    MicroGrid(int width=1,int height=1);  // Constructor (width,height in micropolygons)
    ~MicroGrid(); // Destructor

    void Allocate(int width,int height);
    void Free();

    // Stream input/output
    friend ostream &operator<<(ostream &io,const MicroGrid &m);
    void WritePostscript(FILE *fp);

    bool ExtractMicroPolygon(MicroPolygon &m,int u,int v); // Extract micro-polygon from grid

    int Width(){return width;}
    int Height(){return height;}

    void SetSize(int width,int height);  // Change size and re-initialise microgrid
    void SetTextureCoords(float _umin,float _vmin,float _umax,float _vmax)
      {umin=_umin; vmin=_vmin; umax=_umax; vmax=_vmax;}

    void MicroGrid::ComputeNormals();
    void MicroGrid::Shade(list<Light*> &lights);
    void MicroGrid::Displace();
    void MicroGrid::Statistics(float &zmin,float &zmax,float &maxusize,float &maxvsize);

    Point3 **point;                  // 2D array of points
    Colour **colour;                 // 2D array of micro-polygon colours
    Opacity **opacity;               // 2D array of micro-polygon opacity
    Vector3 **normal;                // 2D array of normals

  protected:
    int width,height;
    float umin,vmin;
    float umax,vmax;
};


#endif // MICROGRID_H
