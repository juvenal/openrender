/*
 *  instance.h - Definitions for the Instance class
 *  openRender
 *
 *  Description:
 *    This class is reponsible for the initialization of the render instance,
 *    and to perform the calls made on the RiCalls lib entries.
 *
 *  Creation:
 *    Sun Jul 11 2004
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
 *  $Id: instance.h,v 1.5 2008/07/15 03:24:58 juvenal.silva Exp $
 */

#ifndef _INSTANCE_H
#define _INSTANCE_H

// Define the instance class
class Instance {
    protected:
        // Internal Data members
        int             firstFrame       = -1;
        int             lastFrame        = -1;
        bool            inAreaLight      = false;
        bool            inObject         = false;
        String          name;
        World           renderWorld      = new World();
        Frame           renderFrame      = new Frame();
        Renderer        mainRenderer     = new Renderer();
        Attributes      renderAttributes = new Attributes();
        ActiveState     renderState      = ActiveState.OUTSIDE;
        stack           attributeStack;
        stack           transformStack;
        stack           worldStack;
        stack           frameStack;
        stack           stateStack;
        map             objectInstances  = new map<>;
        ObjectInstance  currentObjectInstance;
        // Internal methods
        void            pushState(activeState state);
        void            popState();
        void            pushAttributes();
        void            popAttributes();
        void            newObjectInstance(int objectID);
        ObjectInstance  getObjectInstance(int objectID);
    public:
        Attributes      getAttributes();
        void            attributeBegin();
        void            attributeEnd();
        void            transformBegin();
        void            transformEnd();
        void            saveCurrentTransformAs(String name);
        void            setCurrentTransformTo(String name);
        void            setIdentity();
        void            rotate(float angle, float dx, float dy, float dz);
        void            scale(float sx, float sy, float sz);
        void            translate(float tx, float ty, float tz);
        void            perspective(float fov);
        void            setDetailRange(float minVisible, float lowerTransition, float upperTransition, float maxVisible);
        void            setDetail(Bound3D bounds);
        void            setBounds(Bound3D bounds);
        void            setTransform(Matrix4D matrix);
        void            concatTransform(Matrix4D transform);
        void            setColor(Colour colour);
        void            setOpacity(Opacity opacity);
        void            setShadingInterpolation(String type);
        void            setMatte(bool b);
        void            setGeometricApproximation(String type, float value);
        void            reverseOrientation();
        void            frameBegin(int n);
        void            begin(String renderer);
        void            end();
        void            frameEnd();
        void            motionBegin(float times[]);
        void            motionEnd();
        void            objectBegin(int objectID);
        void            objectEnd();
        void            solidBegin(String operation);
        void            solidEnd();
        void            worldBegin();
        void            worldEnd();
        void            setBasis(Basis uBasis, int uStep, Basis vBasis, int vStep);
        void            setSides(int sides);
        Declaration     getDeclaration(String name);
        void            setDeclaration(String name, String declaration);
        void            setFormat(int xResolution, int yResolution, float PixelAspectRatio);
        void            setFrameAspectRatio(float frameAspectRatio);
        void            setScreenWindow(float left, float right, float bottom, float top);
        void            setCropWindow(float xmin, float xmax, float ymin, float ymax);
        void            setProjection(String name, ParameterList parameters);
        void            setClipping(float near, float far);
        void            setDepthOfField(float fstop);
        void            setDepthOfField(float fstop, float focalLength, float focalDistance);
        void            setShutter(float min, float max);
        void            setPixelVariance(float variation);
        void            setPixelSamples(float xSamples, float ySamples);
        void            setPixelFilter(String type, float xWidth, float yWidth);
        void            setExposure(float gain, float gamma);
        void            setQuantize(String type, int one, int min, int max, float ditherAmplitude);
        void            setDisplay(String name, String type, String mode, ParameterList parameters);
        void            setHider(String type, ParameterList parameters);
        void            setRelativeDetail(float relativeDetail);
        void            setOption(String name, ParameterList parameters);
        void            setAttribute(String name, ParameterList parameters);
        void            setSurface(String name, ParameterList parameters);
        void            setDisplacement(String name, ParameterList parameters);
        void            setAtmosphere(String name, ParameterList parameters);
        void            setExterior(String name, ParameterList parameters);
        void            setInterior(String name, ParameterList parameters);
        void            setImager(String name, ParameterList parameters);
        void            createLightSource(String name, int sequenceID, ParameterList parameters);
        void            turnOnLight(LightShader light);
        void            turnOffLight(LightShader light);
        void            illuminate(int sequenceID, int onOff);
        void            createAreaLightSource(String name, int sequenceID, ParameterList parameters);
        void            setShadingRate(float size);
        void            makeTexture(String picturename, String texturename, String swrap, String twrap, String filter, int swidth, int twidth);
        void            objectInstance(int objectID);
        void            addSphere(float radius, float zMin, float zMax, float tMax, ParameterList parameters);
        void            addTorus(float majorRadius, float minorRadius, float pMin, float pMax, float tMax, ParameterList parameters);
        void            addCylinder(float radius, float zMin, float zMax, float tMax, ParameterList parameters);
        void            addCone(float height, float radius, float tMax, ParameterList parameters);
        void            addDisk(float height, float radius, float tMax, ParameterList parameters);
        void            addParaboloid(float radius, float zMin, float zMax, float tMax, ParameterList parameters);
        void            addHyperboloid(float x1, float y1, float z1, float x2, float y2, float z2, float tMax, ParameterList parameters);
        void            addBilinearPath(ParameterList parameters);
        void            addBicubicPath(ParameterList parameters);
        void            addPointsPolygon(float nVertices[], float vertices[], ParameterList parameters);
        void            addPolygon(ParameterList parameters);
        void            addPoints(ParameterList parameters);
};

#endif /* _INSTANCE_H */
