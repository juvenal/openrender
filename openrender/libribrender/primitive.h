//  openRender
//
//  primitive.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sun Oct 27 2002
//
//  Original Development:
//    (C) 2002 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software, you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: primitive.h,v 1.2 2003/06/08 19:13:51 juvenal Exp $
//

#ifndef PRIMITIVE_H
#define PRIMITIVE_H

// Define the virtual basic primitive class
class Primitive {
  public:
    virtual void dump ()
    virtual bool transformToEyeSpace ( matrix4D t_position, matrix4D t_vector)
    virtual void doDice ( microGrid &microgrid, int us, int vs)
    virtual bool splitable ()
    virtual void split ( list<primitive*> &primlist)
    virtual bool eyeBound ( boundBox &bb)
    void dice ( microGrid &microgrid, float xscale, float yscale);
    bool diceable ( float xscale, float yscale);
    void estimateGridSize ( float xscale, float yscale, int &us, int &vs);
};

#endif  // PRIMITIVE_H
