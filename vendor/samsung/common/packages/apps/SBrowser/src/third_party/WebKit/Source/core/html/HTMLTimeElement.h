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

#ifndef HTMLTimeElement_h
#define HTMLTimeElement_h

#include "HTMLElement.h"

namespace WebCore {

class HTMLTimeElement FINAL : public HTMLElement {
public:
    static PassRefPtr<HTMLTimeElement> create(Document&);

    String datetime() const;
    void setDatetime(const AtomicString&);

private:
    HTMLTimeElement(Document&);
};

} // namespace

#endif

