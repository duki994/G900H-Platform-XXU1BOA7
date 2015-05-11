/*
Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved

* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
* Library General Public License for more details.
*/

#include "config.h"
#include "HTMLTimeElement.h"

#include "HTMLNames.h"
#include "core/dom/Document.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLTimeElement::HTMLTimeElement(Document& document)
    : HTMLElement(timeTag, document)
{
    ASSERT(hasTagName(timeTag));
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLTimeElement> HTMLTimeElement::create(Document& document)
{
    return adoptRef(new HTMLTimeElement(document));
}

String HTMLTimeElement::datetime() const
{
    return fastGetAttribute(datetimeAttr);
}

void HTMLTimeElement::setDatetime(const AtomicString& datetime)
{
    setAttribute(datetimeAttr, datetime);
}

}
