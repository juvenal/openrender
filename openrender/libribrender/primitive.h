/*
 *  primitive.h
 *  openRender
 *
 *  Description:
 *    {Description}
 *
 *  Creation:
 *    Sun Oct 27 2002
 *
 *  Original Development:
 *    (C) 2006 by Juvenal A. Silva Jr. <juvenal.silva@v2-home.com.br>
 *
 *  Contributions:
 *
 *  Statement:
 *    This program is free software, you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 2 of the License, or
 *    (at your option) any later version.
 *
 *  $Id: primitive.h,v 1.7 2008/07/15 03:24:58 juvenal.silva Exp $
 */

#ifndef _PRIMITIVE_H
#define _PRIMITIVE_H

// Define the virtual basic primitive class
class Primitive {
    public:
        virtual void dump()
        virtual bool transformToEyeSpace(Matrix4D t_position, Matrix4D t_vector)
        virtual void doDice(MicroGrid &microgrid, int us, int vs)
        virtual bool splitable()
        virtual void split(list<Primitive*> &primlist)
        virtual bool eyeBound(BoundBox &bb)
        void dice(MicroGrid &microgrid, float xscale, float yscale);
        bool diceable(float xscale, float yscale);
        void estimateGridSize(float xscale, float yscale, int &us, int &vs);
};

#endif  /* _PRIMITIVE_H */
