/**
 * Project: openRender
 *
 * File: objectSink.h
 *
 * Description:
 *   Where CObject::instantiate() reports the objects it creates (spec 031, FR-028). Implemented by the Scene
 *   Interface's RI context, so the renderer core, the preview and the RIB back-ends receive them as before.
 *   Header only; not installed.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef OBJECTSINK_H
#define OBJECTSINK_H

class CObject;

class IObjectSink {
    public:
        virtual ~IObjectSink() = default;
        virtual void addObject(CObject *object) = 0;
};

#endif
