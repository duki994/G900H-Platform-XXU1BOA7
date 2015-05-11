// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadEvent_h
#define GamepadEvent_h

#include "core/events/Event.h"
#include "modules/gamepad/Gamepad.h"

namespace WebCore {

struct GamepadEventInit : public EventInit {
    GamepadEventInit();

    RefPtrWillBeMember<Gamepad> gamepad;
};

class GamepadEvent FINAL : public Event {
public:
    static PassRefPtr<GamepadEvent> create()
    {
        return adoptRef(new GamepadEvent);
    }
    static PassRefPtr<GamepadEvent> create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<Gamepad> gamepad)
    {
        return adoptRef(new GamepadEvent(type, canBubble, cancelable, gamepad));
    }
    static PassRefPtr<GamepadEvent> create(const AtomicString& type, const GamepadEventInit& initializer)
    {
        return adoptRef(new GamepadEvent(type, initializer));
    }
    virtual ~GamepadEvent();

    Gamepad* gamepad() const { return m_gamepad.get(); }

    virtual const AtomicString& interfaceName() const OVERRIDE;

private:
    GamepadEvent();
    GamepadEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<Gamepad>);
    GamepadEvent(const AtomicString&, const GamepadEventInit&);

    RefPtrWillBePersistent<Gamepad> m_gamepad;
};

} // namespace WebCore

#endif // GamepadEvent_h
