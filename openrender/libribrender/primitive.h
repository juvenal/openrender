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
//  $Id: primitive.h,v 1.5 2004/07/14 18:55:46 juvenal Exp $
//

#ifndef PRIMITIVE_H
#define PRIMITIVE_H

// Define the virtual basic primitive class
class Primitive {
    public:
        virtual void dump ()
        virtual bool transformToEyeSpace (Matrix4D t_position, Matrix4D t_vector)
        virtual void doDice (MicroGrid &microgrid, int us, int vs)
        virtual bool splitable ()
        virtual void split (list<Primitive*> &primlist)
        virtual bool eyeBound (BoundBox &bb)
        void dice (MicroGrid &microgrid, float xscale, float yscale);
        bool diceable (float xscale, float yscale);
        void estimateGridSize (float xscale, float yscale, int &us, int &vs);
};

#endif  // PRIMITIVE_H

