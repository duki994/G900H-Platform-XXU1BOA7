/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/css/CSSDefaultStyleSheets.h"

#include "UserAgentStyleSheets.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/rendering/RenderTheme.h"

namespace WebCore {

using namespace HTMLNames;

CSSDefaultStyleSheets& CSSDefaultStyleSheets::instance()
{
    DEFINE_STATIC_LOCAL(CSSDefaultStyleSheets, cssDefaultStyleSheets, ());
    return cssDefaultStyleSheets;
}

static const MediaQueryEvaluator& screenEval()
{
    DEFINE_STATIC_LOCAL(const MediaQueryEvaluator, staticScreenEval, ("screen"));
    return staticScreenEval;
}

static const MediaQueryEvaluator& printEval()
{
    DEFINE_STATIC_LOCAL(const MediaQueryEvaluator, staticPrintEval, ("print"));
    return staticPrintEval;
}

static StyleSheetContents* parseUASheet(const String& str)
{
    RefPtr<StyleSheetContents> sheet = StyleSheetContents::create(CSSParserContext(UASheetMode, 0));
    sheet->parseString(str);
    return sheet.release().leakRef(); // leak the sheet on purpose
}

static StyleSheetContents* parseUASheet(const char* characters, unsigned size)
{
    return parseUASheet(String(characters, size));
}

CSSDefaultStyleSheets::CSSDefaultStyleSheets()
    : m_defaultStyle(0)
    , m_defaultViewportStyle(0)
    , m_defaultQuirksStyle(0)
    , m_defaultPrintStyle(0)
    , m_defaultViewSourceStyle(0)
    , m_defaultXHTMLMobileProfileStyle(0)
    , m_defaultStyleSheet(0)
    , m_viewportStyleSheet(0)
    , m_quirksStyleSheet(0)
    , m_svgStyleSheet(0)
    , m_mediaControlsStyleSheet(0)
    , m_fullscreenStyleSheet(0)
{
    m_defaultStyle = RuleSet::create().leakPtr();
    m_defaultViewportStyle = RuleSet::create().leakPtr();
    m_defaultPrintStyle = RuleSet::create().leakPtr();
    m_defaultQuirksStyle = RuleSet::create().leakPtr();

    // Strict-mode rules.
    String defaultRules = String(htmlUserAgentStyleSheet, sizeof(htmlUserAgentStyleSheet)) + RenderTheme::theme().extraDefaultStyleSheet();
    m_defaultStyleSheet = parseUASheet(defaultRules);
    m_defaultStyle->addRulesFromSheet(m_defaultStyleSheet, screenEval());
#if OS(ANDROID)
    String viewportRules(viewportAndroidUserAgentStyleSheet, sizeof(viewportAndroidUserAgentStyleSheet));
#else
    String viewportRules;
#endif
    m_viewportStyleSheet = parseUASheet(viewportRules);
    m_defaultViewportStyle->addRulesFromSheet(m_viewportStyleSheet, screenEval());
    m_defaultPrintStyle->addRulesFromSheet(m_defaultStyleSheet, printEval());

    // Quirks-mode rules.
    String quirksRules = String(quirksUserAgentStyleSheet, sizeof(quirksUserAgentStyleSheet)) + RenderTheme::theme().extraQuirksStyleSheet();
    m_quirksStyleSheet = parseUASheet(quirksRules);
    m_defaultQuirksStyle->addRulesFromSheet(m_quirksStyleSheet, screenEval());
}

RuleSet* CSSDefaultStyleSheets::defaultViewSourceStyle()
{
    if (!m_defaultViewSourceStyle) {
        m_defaultViewSourceStyle = RuleSet::create().leakPtr();
        m_defaultViewSourceStyle->addRulesFromSheet(parseUASheet(sourceUserAgentStyleSheet, sizeof(sourceUserAgentStyleSheet)), screenEval());
    }
    return m_defaultViewSourceStyle;
}

RuleSet* CSSDefaultStyleSheets::defaultXHTMLMobileProfileStyle()
{
    if (!m_defaultXHTMLMobileProfileStyle) {
        m_defaultXHTMLMobileProfileStyle = RuleSet::create().leakPtr();
        m_defaultXHTMLMobileProfileStyle->addRulesFromSheet(parseUASheet(xhtmlmpUserAgentStyleSheet, sizeof(xhtmlmpUserAgentStyleSheet)), screenEval());
    }
    return m_defaultXHTMLMobileProfileStyle;
}

void CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(Element* element, bool& changedDefaultStyle)
{
    // FIXME: We should assert that the sheet only styles SVG elements.
    if (element->isSVGElement() && !m_svgStyleSheet) {
        m_svgStyleSheet = parseUASheet(svgUserAgentStyleSheet, sizeof(svgUserAgentStyleSheet));
        m_defaultStyle->addRulesFromSheet(m_svgStyleSheet, screenEval());
        m_defaultPrintStyle->addRulesFromSheet(m_svgStyleSheet, printEval());
        changedDefaultStyle = true;
    }

    // FIXME: We should assert that this sheet only contains rules for <video> and <audio>.
    if (!m_mediaControlsStyleSheet && (element->hasTagName(videoTag) || element->hasTagName(audioTag))) {
        String mediaRules = String(mediaControlsUserAgentStyleSheet, sizeof(mediaControlsUserAgentStyleSheet)) + RenderTheme::theme().extraMediaControlsStyleSheet();
        m_mediaControlsStyleSheet = parseUASheet(mediaRules);
        m_defaultStyle->addRulesFromSheet(m_mediaControlsStyleSheet, screenEval());
        m_defaultPrintStyle->addRulesFromSheet(m_mediaControlsStyleSheet, printEval());
        changedDefaultStyle = true;
    }

    // FIXME: This only works because we Force recalc the entire document so the new sheet
    // is loaded for <html> and the correct styles apply to everyone.
    if (!m_fullscreenStyleSheet && FullscreenElementStack::isFullScreen(&element->document())) {
        String fullscreenRules = String(fullscreenUserAgentStyleSheet, sizeof(fullscreenUserAgentStyleSheet)) + RenderTheme::theme().extraFullScreenStyleSheet();
        m_fullscreenStyleSheet = parseUASheet(fullscreenRules);
        m_defaultStyle->addRulesFromSheet(m_fullscreenStyleSheet, screenEval());
        m_defaultQuirksStyle->addRulesFromSheet(m_fullscreenStyleSheet, screenEval());
        changedDefaultStyle = true;
    }

    ASSERT(!m_defaultStyle->features().hasIdsInSelectors());
    ASSERT(m_defaultStyle->features().siblingRules.isEmpty());
}

} // namespace WebCore
