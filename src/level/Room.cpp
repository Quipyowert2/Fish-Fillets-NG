/*
 * Copyright (C) 2004 Ivo Danihelka (ivo@danihelka.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "Room.h"

#include "WavyPicture.h"
#include "Field.h"
#include "ResSoundPack.h"
#include "Controls.h"
#include "PhaseLocker.h"
#include "Planner.h"
#include "View.h"

#include "Log.h"
#include "Rules.h"
#include "LogicException.h"
#include "LoadException.h"
#include "Unit.h"
#include "TimerAgent.h"
#include "SubTitleAgent.h"
#include "DialogAgent.h"
#include "SoundAgent.h"
#include "ModelList.h"

#include <assert.h>

//-----------------------------------------------------------------
/**
 * Create room holder.
 * @param w room width
 * @param h room height
 * @param picture room background
 * @param locker shared locker for anim
 * @param levelScript shared planner to interrupt
 */
Room::Room(int w, int h, const Path &picture,
        PhaseLocker *locker, Planner *levelScript)
{
    m_locker = locker;
    m_levelScript = levelScript;
    m_bg = new WavyPicture(picture, V2(0, 0));
    m_field = new Field(w, h);
    m_controls = new Controls(m_locker);
    m_view = new View(ModelList(&m_models));
    m_impact = Cube::NONE;
    m_fresh = true;
    m_soundPack = new ResSoundPack();
    m_startTime = TimerAgent::agent()->getCycles();
}
//-----------------------------------------------------------------
/**
 * Delete field and models.
 */
Room::~Room()
{
    killPlan();
    m_soundPack->removeAll();
    delete m_soundPack;
    DialogAgent::agent()->removeAll();
    SubTitleAgent::agent()->removeAll();
    delete m_controls;
    delete m_view;

    //NOTE: models must be removed before field because they unmask self
    Cube::t_models::iterator end = m_models.end();
    for (Cube::t_models::iterator i = m_models.begin(); i != end; ++i) {
        delete (*i);
    }

    delete m_field;
    delete m_bg;
}
//-----------------------------------------------------------------
/**
 * Set waves on background.
 */
    void
Room::setWaves(double amplitude, double periode, double speed)
{
    m_bg->setWamp(amplitude);
    m_bg->setWperiode(periode);
    m_bg->setWspeed(speed);
}
//-----------------------------------------------------------------
void
Room::addDecor(Decor *new_decor)
{
    m_view->addDecor(new_decor);
}
//-----------------------------------------------------------------
void
Room::killPlan()
{
    DialogAgent::agent()->killTalks();
    SubTitleAgent::agent()->killTalks();
    m_levelScript->interruptPlan();
}
//-----------------------------------------------------------------
/**
 * Add model at scene.
 * @param new_model new object
 * @param new_unit driver for the object or NULL
 * @return model index
 */
    int
Room::addModel(Cube *new_model, Unit *new_unit)
{
    new_model->rules()->takeField(m_field);
    m_models.push_back(new_model);

    if (new_unit) {
        new_unit->takeModel(new_model);
        m_controls->addUnit(new_unit);
    }

    int model_index = m_models.size() - 1;
    new_model->setIndex(model_index);
    return model_index;
}
//-----------------------------------------------------------------
/**
 * Return model at index.
 * @throws LogicException when model_index is out of range
 */
Cube *
Room::getModel(int model_index)
{
    Cube *result = NULL;
    if (0 <= model_index && model_index < (int)m_models.size()) {
        result = m_models[model_index];
    }
    else {
        throw LogicException(ExInfo("bad model index")
                .addInfo("model_index", model_index));
    }

    return result;
}
//-----------------------------------------------------------------
/**
 * Return model at location.
 */
Cube *
Room::askField(const V2 &loc)
{
    return m_field->getModel(loc);
}
//-----------------------------------------------------------------
/**
 * Update all models.
 * Prepare new move, let models fall, let models drive, release old position.
 * @return true when room is finished
 */
    bool
Room::nextRound(const InputProvider *input)
{
    bool falling = beginFall();
    if (!falling) {
        m_controls->driving(input);
    }

    return finishRound();
}
//-----------------------------------------------------------------
/**
 * Play sound like some object has fall.
 * NOTE: only one sound is played even more objects have fall
 */
void
Room::playImpact()
{
    switch (m_impact) {
        case Cube::NONE:
            break;
        case Cube::LIGHT:
            playSound("impact_light", 50);
            break;
        case Cube::HEAVY:
            playSound("impact_heavy", 50);
            break;
        default:
            assert(!"unknown impact weight");
    }
    m_impact = Cube::NONE;
}
//-----------------------------------------------------------------
/**
 * Play sound like a fish die.
 * @param model fresh dead fish
 */
void
Room::playDead(Cube *model)
{
    DialogAgent::agent()->killSound(model->getIndex());
    switch (model->getPower()) {
        case Cube::LIGHT:
            playSound("dead_small");
            break;
        case Cube::HEAVY:
            playSound("dead_big");
            break;
        default:
            LOG_WARNING(ExInfo("curious power of dead fish")
                    .addInfo("power", model->getPower()));
            break;
    }
}
//-----------------------------------------------------------------
/**
 * Move all models to new position
 * and check dead fihes.
 */
void
Room::prepareRound()
{
    bool interrupt = false;

    //NOTE: we must call this functions sequential for all objects
    Cube::t_models::iterator end = m_models.end();
    for (Cube::t_models::iterator i = m_models.begin(); i != end; ++i) {
        (*i)->rules()->occupyNewPos();
    }
    for (Cube::t_models::iterator j = m_models.begin(); j != end; ++j) {
        bool die = (*j)->rules()->checkDead();
        interrupt |= die;
        if (die) {
            playDead(*j);
        }
    }
    for (Cube::t_models::iterator l = m_models.begin(); l != end; ++l) {
        (*l)->rules()->changeState();
    }

    if (interrupt) {
        m_levelScript->interruptPlan();
        m_controls->checkActive();
    }
}
//-----------------------------------------------------------------
/**
 * Let models to go out of screen.
 * @param interactive whether do anim
 * @return true when a model went out
 */
bool
Room::fallout(bool interactive)
{
    bool wentOut = false;
    Cube::t_models::iterator end = m_models.end();
    for (Cube::t_models::iterator i = m_models.begin(); i != end; ++i) {
        int outDepth = (*i)->rules()->actionOut();
        if (outDepth > 0) {
            wentOut = true;
            if (interactive) {
                m_locker->ensurePhases(3);
            }
        }
    }

    if (wentOut) {
        m_levelScript->interruptPlan();
        m_controls->checkActive();
    }
    return wentOut;
}
//-----------------------------------------------------------------
/**
 * Let things fall.
 * @return true when something is falling.
 */
    bool
Room::falldown()
{
    bool result = false;
    m_impact = Cube::NONE;
    Cube::t_models::iterator end = m_models.end();
    for (Cube::t_models::iterator i = m_models.begin(); i != end; ++i) {
        Rules::eFall fall = (*i)->rules()->actionFall();
        if (Rules::FALL_NOW == fall) {
            result = true;
        }
        else if (Rules::FALL_LAST == fall) {
            if (m_impact < (*i)->getWeight()) {
                m_impact = (*i)->getWeight();
            }
        }
    }

    return result;
}
//-----------------------------------------------------------------
/**
 * Let models to release their old position.
 * Check complete room.
 * @param interactive whether ensure phases for motion animation
 * @return true when room is finished
 */
bool
Room::finishRound(bool interactive)
{
    bool room_complete = true;
    if (interactive) {
        m_controls->lockPhases();
    }
    m_view->noteNewRound(m_locker->getLocked());

    Cube::t_models::iterator end = m_models.end();
    for (Cube::t_models::iterator i = m_models.begin(); i != end; ++i) {
        (*i)->rules()->finishRound();
        room_complete &= (*i)->isSatisfy();
    }

    m_fresh = false;
    return room_complete;
}

//-----------------------------------------------------------------
void
Room::switchFish()
{
    m_controls->switchActive();
}
//-----------------------------------------------------------------
void
Room::controlEvent(const KeyStroke &stroke)
{
    m_controls->controlEvent(stroke);
}

//-----------------------------------------------------------------
int
Room::getStepCount() const
{
    return m_controls->getStepCount();
}
//-----------------------------------------------------------------
std::string
Room::getMoves() const
{
    return m_controls->getMoves();
}
//-----------------------------------------------------------------
/**
 * Load this move, let object to fall fast.
 * Don't play sound.
 * @return true for finished level
 * @throws LoadException for bad moves
 */
bool
Room::loadMove(char move)
{
    bool complete = false;
    bool falling = true;
    while (falling) {
        falling = beginFall(false);
        makeMove(move);

        complete = finishRound(false);
        if (complete && falling) {
            throw LoadException(ExInfo("load error - early finished level")
                    .addInfo("move", std::string(1, move)));
        }
    }
    return complete;
}
//-----------------------------------------------------------------
/**
 * Begin round.
 * Let objects fall.
 * First objects can fall out of room (even upward),
 * when nothing is going out, then objects can fall down by gravity.
 *
 * @param interactive whether play sound and do anim
 * @return true when something was falling
 */
bool
Room::beginFall(bool interactive)
{
    m_fresh = true;
    prepareRound();

    bool falling = fallout(interactive);
    if (!falling) {
        falling = falldown();
        if (interactive) {
            playImpact();
        }
    }
    m_fresh = !falling;
    return falling;
}
//-----------------------------------------------------------------
/**
 * Try make single move.
 * @return true for success or false when something has moved before
 * @throws LoadException for bad moves
 */
bool
Room::makeMove(char move)
{
    bool result = false;
    if (m_fresh) {
        if (!m_controls->makeMove(move)) {
            throw LoadException(ExInfo("load error - bad move")
                    .addInfo("move", std::string(1, move)));
        }
        m_fresh = false;
        result = true;
    }
    return result;
}
//-----------------------------------------------------------------
/**
 * Returns true when there is no unit which will be able to move.
 */
bool
Room::cannotMove()
{
    return m_controls->cannotMove();
}
//-----------------------------------------------------------------
/**
 * Returns true when all goals can be solved.
 */
bool
Room::isSolvable()
{
    Cube::t_models::iterator end = m_models.end();
    for (Cube::t_models::iterator i = m_models.begin(); i != end; ++i) {
        if ((*i)->isWrong()) {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------
int
Room::getW() const
{
    return m_field->getW();
}
//-----------------------------------------------------------------
int
Room::getH() const
{
    return m_field->getH();
}
//-----------------------------------------------------------------
int
Room::getCycles() const
{
    return TimerAgent::agent()->getCycles() - m_startTime;
}
//-----------------------------------------------------------------
void
Room::addSound(const std::string &name, const Path &file)
{
    m_soundPack->addSound(name, file);
}
//-----------------------------------------------------------------
void
Room::playSound(const std::string &name, int volume)
{
    SoundAgent::agent()->playSound(
            m_soundPack->getRandomRes(name), volume);
}
//-----------------------------------------------------------------
void
Room::drawOn(SDL_Surface *screen)
{
    m_bg->drawOn(screen);
    m_view->drawOn(screen);
}

