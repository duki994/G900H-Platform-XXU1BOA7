/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "ArticleRecognition.h"

#include "bindings/v8/ScriptRegexp.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/dom/ClientRect.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Text.h"
#include "core/frame/Frame.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLFormElement.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/Logging.h"
#include "platform/fonts/Character.h"
#include "wtf/text/StringBuilder.h"
#include <string>

using namespace WebCore;
using namespace HTMLNames;

namespace blink {

static unsigned cjkSampleSize = 30;

// Checks whether it is Chinese, Japanese or Korean Page
static inline bool isCJKPage(const String& searchString)
{
    unsigned length = std::min(searchString.length(), cjkSampleSize);
    for (unsigned i = 0; i < length; ++i) {
        if (Character::isCJKIdeographOrSymbol(searchString[i]))
            return true;
    }
    return false;
}

// Takes a search string and a search array and finds whether searchString
// contains any of '|' seperated values present in the searchArray.
static bool regExpSearch(const String& searchString, const char* const* searchArray, unsigned arrayLength)
{
    for (unsigned i = 0; i < arrayLength; ++i) {
        if (searchString.findIgnoringCase(searchArray[i]) != kNotFound)
            return true;
    }
    return false;
}

static inline void textContent(Node& node, StringBuilder& builder)
{
    if (node.parentNode() && node.parentNode()->hasTagName(scriptTag))
        return;

    if (node.isTextNode()) {
        builder.append(toText(node).data());
        return;
    }

    for (Node* child = node.firstChild(); child; child = child->nextSibling())
        textContent(*child, builder);
}

static String visibleTextContent(Node& node)
{
    StringBuilder builder;
    textContent(node, builder);
    return builder.toString();
}

// Returns the ratio of all anchor tags inner text length (within element for which
// query is made) and elements inner Text.
static double linkDensityForNode(Node& node)
{
    if (!node.isElementNode())
        return 0;
    Element& element = toElement(node);
    unsigned textLength = element.innerText().length();
    unsigned linkLength = 0;

    for (Element* currentElement = ElementTraversal::firstWithin(element); currentElement; currentElement = ElementTraversal::next(*currentElement, &element)) {
        if (!currentElement->hasTagName(aTag))
            continue;
        // FIXME: Calling innerText() is very inefficient as it creates a string
        // unnecessarily. Add innerTextLength() method to Element.
        linkLength += currentElement->innerText().length();
    }
    if (!textLength)
        return 0;
    return static_cast<double>(linkLength) / textLength;
}

static int scoreForTag(const QualifiedName& tag)
{
    if (tag.matches(divTag))
        return 5;
    if (tag.matches(articleTag))
        return 25;
    if (tag.matches(preTag) || tag.matches(tdTag) || tag.matches(blockquoteTag))
        return 3;
    if (tag.matches(addressTag) || tag.matches(ulTag) || tag.matches(dlTag) || tag.matches(ddTag)
        || tag.matches(dtTag) || tag.matches(liTag) || tag.matches(formTag)) {
        return -3;
    }
    if (tag.matches(h1Tag) || tag.matches(h2Tag) || tag.matches(h3Tag) || tag.matches(h4Tag)
        || tag.matches(h5Tag) || tag.matches(h6Tag) || tag.matches(thTag)) {
        return -5;
    }
    return 0;
}

static int classWeightForElement(const Element& element)
{
    DEFINE_STATIC_LOCAL(const String, regexPositiveString, ("article|body|content|entry|hentry|main|page|pagination|post|text|blog|story|windowclassic"));
    DEFINE_STATIC_LOCAL(const String, regexNegativeString, ("contents|combx|comment|com-|contact|foot|footer|footnote|masthead|media|meta|outbrain|promo|related|scroll|shoutbox|sidebar|date|sponsor|shopping|tags|script|tool|widget|scbox|rail|reply|div_dispalyslide|galleryad|disqus_thread|cnn_strylftcntnt|topRightNarrow|fs-stylelist-thumbnails|replText|ttalk_layer|disqus_post_message|disqus_post_title|cnn_strycntntrgt|wpadvert|sharedaddy sd-like-enabled sd-sharing-enabled|fs-slideshow-wrapper|fs-stylelist-launch|fs-stylelist-next|fs-thumbnail-194230|reply_box|textClass errorContent|mainHeadlineBrief|mainSlideDetails|curvedContent|photo|home_|XMOD"));
    DEFINE_STATIC_LOCAL(const ScriptRegexp, positiveRegex, (regexPositiveString, TextCaseInsensitive));
    DEFINE_STATIC_LOCAL(const ScriptRegexp, negativeRegex, (regexNegativeString, TextCaseInsensitive));

    int weight = 0;
    const AtomicString& classAttribute = element.getClassAttribute();
    if (!classAttribute.isNull()) {
        if ((positiveRegex.match(classAttribute)) != -1);
            weight += 30;

        if ((negativeRegex.match(classAttribute)) != -1);
            weight -= 25;
    }

    const AtomicString& idAttribute = element.getIdAttribute();
    if (!idAttribute.isNull()) {
        if ((positiveRegex.match(idAttribute)) != -1);
            weight += 30;

        if ((negativeRegex.match(idAttribute)) != -1);
            weight -= 25;
    }
    return weight;
}

static void initializeReadabilityAttributeForElement(Element& element)
{
    // FIXME: Use custom data-* attribute everywhere since readability is not
    // a standard HTML attribute.
    if (int tagScore = scoreForTag(element.tagQName()))
        element.setFloatingPointAttribute(readabilityAttr, tagScore + classWeightForElement(element));
}

static bool isFormPage(Frame* frame)
{
#if !LOG_DISABLED
    // Intended to be used only in LOGS
    double startTime = currentTimeMS();
#endif

    Element* bodyElement = frame ? frame->document()->body() : 0;
    if (!bodyElement)
        return false;

    double formTotalHeight = 0;

    for (Element* element = ElementTraversal::firstWithin(*bodyElement); element; element = ElementTraversal::next(*element, bodyElement)) {
        if (!element->hasTagName(formTag))
            continue;
        HTMLFormElement* formElement = toHTMLFormElement(element);
        formTotalHeight += formElement->getBoundingClientRect()->height();
    }

    WTF_LOG(SamsungReader, "Time taken in calculating form tags : %f ms", (currentTimeMS() - startTime));

    if (formTotalHeight > ((bodyElement->getBoundingClientRect()->height()) * 0.5))
        return true;
    return false;
}

// Utility Function: Calculates max count for BR tag and other Tags and points maxBRContainingElement to element
// containing maximum BR count
static void calculateBRTagAndOtherTagMaxCount(Element& bodyElement, unsigned& brTagMaxCount, unsigned& otherTagMaxCount, unsigned& totalNumberOfBRTags, Element*& maxBRContainingElement)
{
    for (Element* currentElement = ElementTraversal::firstWithin(bodyElement); currentElement; currentElement = ElementTraversal::next(*currentElement, &bodyElement)) {
        if (!currentElement->hasTagName(brTag))
            continue;
        unsigned brTagCount = 0;
        unsigned otherTagCount = 0;
        ++totalNumberOfBRTags;
        unsigned nextCount = 0;
        for (Element* sibling = ElementTraversal::nextSibling(*currentElement); sibling && nextCount < 5; sibling = ElementTraversal::nextSibling(*sibling)) {
            ++nextCount;
            if (sibling->hasTagName(brTag)) {
                ++brTagCount;
                nextCount = 0;
            } else if (sibling->hasTagName(aTag) || sibling->hasTagName(bTag)) {
                ++otherTagCount;
            }
        }

        if (brTagCount > brTagMaxCount) {
            if (currentElement->parentElement()
                && currentElement->parentElement()->getBoundingClientRect()->height() > 200) {
                brTagMaxCount = brTagCount;
                otherTagMaxCount = otherTagCount;
                maxBRContainingElement = currentElement;
            }
        }
    }
}

// Utility Function: Calculates max count for P tag and points maxPContainingElement to element
// containing maximum BR count
static void calculatePTagMaxCount(Element& bodyElement, unsigned& pTagMaxCount, unsigned& totalNumberOfPTags, Element*& maxPContainingElement)
{
    for (Element* currentElement = ElementTraversal::firstWithin(bodyElement); currentElement; currentElement = ElementTraversal::next(*currentElement, &bodyElement)) {
        if (!currentElement->hasTagName(pTag))
            continue;
        ++totalNumberOfPTags;
        unsigned pTagCount = 0;
        for (Element* sibling = ElementTraversal::nextSibling(*currentElement); sibling; sibling = ElementTraversal::nextSibling(*sibling)) {
            if (sibling->hasTagName(pTag))
                ++pTagCount;
        }

        if (pTagCount > pTagMaxCount) {
            if (currentElement->parentElement()
                    && currentElement->parentElement()->getBoundingClientRect()->height() > 200) {
                pTagMaxCount = pTagCount;
                maxPContainingElement = currentElement;
            }
        }
    }
}

static unsigned countNumberOfSpaceSeparatedValues(const String& string)
{
    unsigned numberOfSpaceSeparatedValues = 0;
    unsigned lastSpaceFoundPosition = 0;
    unsigned length = string.length();
    for (unsigned i = 0; i < length; ++i) {
        if (string[i] == ' ') {
            if (!i || string[i-1] == ' ')
                continue;
            lastSpaceFoundPosition = i;
            ++numberOfSpaceSeparatedValues;
        }
    }
    // Case like "a b c d" -> Three spaces but no of values = 4;
    if (lastSpaceFoundPosition < length - 1)
        numberOfSpaceSeparatedValues += 1;
    return numberOfSpaceSeparatedValues;
}

// Utility Function: To populate the Vector with probable nodes likely to
// score more as compared to other Nodes
static void populateScoringNodesVector(const Element& bodyElement, Vector<Node*>& scoringNodes)
{
    static const char* divToPElements[] = {
        "<a>", "<blockquote>", "<dl>", "<div>", "<img", "<ol>", "<p>", "<pre>", "<table>", "<ul>", "<script>", "<article>", "<form>",
        "</a>", "</blockquote>", "</dl>", "</div>", "</ol>", "</p>", "</pre>", "</table>", "</ul>", "</script>", "</article>", "</form>",
    };
    DEFINE_STATIC_LOCAL(const String, regexStringForUnlikelyCandidate, ("combx|comment|community|disqus|extra|foot|header|menu|remark|rss|shoutbox|sidebar|sponsor|ad-break|agegate|pagination|pager|popup|tweet|twitter"));
    DEFINE_STATIC_LOCAL(const String, regexStringForALikelyCandidate, ("and|article|body|column|main|shadow"));
    DEFINE_STATIC_LOCAL(const ScriptRegexp, unlikelyRegex, (regexStringForUnlikelyCandidate, TextCaseInsensitive));
    DEFINE_STATIC_LOCAL(const ScriptRegexp, maybeRegex, (regexStringForALikelyCandidate, TextCaseInsensitive));

    for (Element* currentElement = ElementTraversal::firstWithin(bodyElement); currentElement; currentElement = ElementTraversal::next(*currentElement, &bodyElement)) {
        const AtomicString& classAttribute = currentElement->getClassAttribute();
        const AtomicString& id = currentElement->getIdAttribute();

        bool isUnlikelyToBeACandidate = (unlikelyRegex.match(classAttribute) != -1 || unlikelyRegex.match(id) != -1);
        bool isLikelyToBeACandidate = (maybeRegex.match(classAttribute) == -1 || maybeRegex.match(id) == -1);

        if (isUnlikelyToBeACandidate && !isLikelyToBeACandidate && !currentElement->hasTagName(bodyTag))
            continue;

        RefPtr<ClientRect> currentElementRect = currentElement->getBoundingClientRect();
        if (!currentElementRect->height() && !currentElementRect->width())
            continue;

        if (currentElement->hasTagName(pTag) || currentElement->hasTagName(ulTag)
            || (currentElement->hasTagName(tdTag) && (currentElement->getElementsByTagName("table"))->isEmpty())
            || currentElement->hasTagName(preTag)) {
            scoringNodes.append(currentElement);
        } else if (currentElement->hasTagName(divTag)) {
            String elementInnerHTML = toHTMLElement(currentElement)->innerHTML();
            if (currentElement->parentElement() && !regExpSearch(elementInnerHTML, divToPElements, WTF_ARRAY_LENGTH(divToPElements))) {
                Element* parentElement = currentElement->parentElement();

                const AtomicString& parentClassAttribute = parentElement->getClassAttribute();
                const AtomicString& parentId = parentElement->getIdAttribute();

                bool isUnlikelyParentCandidate = (unlikelyRegex.match(parentClassAttribute) != -1 || unlikelyRegex.match(parentId) != -1);
                bool isLikelyParentCandidate = (maybeRegex.match(parentClassAttribute) == -1 || maybeRegex.match(parentId) == -1);

                if (isUnlikelyParentCandidate && !isLikelyParentCandidate && !currentElement->hasTagName(bodyTag))
                    continue;
                scoringNodes.append(currentElement);
            } else {
                for (Node* child = currentElement->firstChild(); child; child = child->nextSibling()) {
                    if (child->isTextNode())
                        scoringNodes.append(child);
                }
            }
        }
    }
}

static void populateCandidateElementsVector(const Vector<Node*>& scoringNodes, Vector<Element*>& candidateElements, bool& isCJK)
{
    unsigned scoringNodesSize = scoringNodes.size();
    for (unsigned i = 0; i < scoringNodesSize; ++i) {
        String scoringNodeVisibleTextContent;
        if (scoringNodes[i]->isElementNode())
            scoringNodeVisibleTextContent = visibleTextContent(*scoringNodes[i]);

        if (scoringNodeVisibleTextContent.length() < 30)
            continue;

        Element* parentElement = scoringNodes[i]->parentElement();
        if (parentElement && !parentElement->fastHasAttribute(readabilityAttr)) {
            initializeReadabilityAttributeForElement(*parentElement);
            candidateElements.append(parentElement);
        }

        Element* grandParentElement = parentElement ? parentElement->parentElement() : 0;
        if (grandParentElement && !grandParentElement->fastHasAttribute(readabilityAttr)) {
            initializeReadabilityAttributeForElement(*grandParentElement);
            candidateElements.append(parentElement);
        }

        double contentScore = 1 + countNumberOfSpaceSeparatedValues(scoringNodeVisibleTextContent);

        // On detection of CJK character, contentscore is incremented further.
        isCJK = isCJKPage(scoringNodeVisibleTextContent);

        unsigned textLength = scoringNodeVisibleTextContent.length();
        if (isCJK) {
            contentScore += std::min(floor(textLength / 100.), 3.);
            contentScore *= 3;
        } else {
            if (textLength < 25)
                continue;
            contentScore += std::min(floor(textLength / 100.), 3.);
        }

        if (parentElement) {
            double parentElementScore = contentScore + parentElement->getFloatingPointAttribute(readabilityAttr, 0);
            parentElement->setFloatingPointAttribute(readabilityAttr, parentElementScore);
            if (grandParentElement) {
                double GrandParentElementScore = grandParentElement->getFloatingPointAttribute(readabilityAttr, 0) + contentScore / 2;
                grandParentElement->setFloatingPointAttribute(readabilityAttr, GrandParentElementScore);
            }
        }
    }
}

std::string ArticleRecognition::recognizeArticleSimpleNativeRecognitionMode(WebCore::Frame* frame)
{
    static const char* homepage[] = {
        "?mview=desktop", "?ref=smartphone", "apple.com", "query=", "search?", "?from=mobile", "signup", "twitter", "facebook", "youtube", "?f=mnate", "linkedin",
        "romaeo", "chrome:", "gsshop", "gdive", "?nytmobile=0", "?CMP=mobile_site", "?main=true", "home-page", "anonymMain", "index.asp", "?s=&b.x=",
        "eenadu.net", "search.cgi?kwd=opposite", "Main_Page", "index.do"
    };

    Element* bodyElement = frame ? frame->document()->body() : 0;

    if (!bodyElement){
        return "false";
    }

    const KURL& url = frame->document()->baseURI();
    String hostName = url.host();
    std::string page_url(url.string().utf8().data());

    WTF_LOG(SamsungReader, "URL : %s", url.string().utf8().data());
    WTF_LOG(SamsungReader, "HostName : %s", hostName.utf8().data());

    if (url.path() == "/") {
        WTF_LOG(SamsungReader, "Path is empty. This is HOMEPAGE");
        std::string result("false@@");
        result += page_url;
        return result;
    }

    // If any of the '|' separated values in homepage are found in url, return false.
    if (regExpSearch(url, homepage, WTF_ARRAY_LENGTH(homepage))) {
        WTF_LOG(SamsungReader, "This is Homepage");
        std::string result("false@@");
        result += page_url;
        return result;
    }


    unsigned articleTagCount = bodyElement->getElementsByTagName("article")->length();
    if (articleTagCount >= 15) {
        WTF_LOG(SamsungReader, "article Tag >= 15");
        std::string result("false@@");
        result += page_url;
        return result;
    }

    if (isFormPage(frame)) {
        WTF_LOG(SamsungReader, "isFormPage :: This is FORM PAGE on URL :: %s", hostName.utf8().data());
        std::string result("false@@");
        result += page_url;
        return result;
    }
#if !LOG_DISABLED
    double startTime = currentTimeMS();
#endif

    unsigned brTagMaxCount = 0;
    unsigned otherTagMaxCount = 0;
    unsigned totalNumberOfBRTags = 0;
    Element* maxBRContainingElement = 0;
    calculateBRTagAndOtherTagMaxCount(*bodyElement, brTagMaxCount, otherTagMaxCount, totalNumberOfBRTags, maxBRContainingElement);

    unsigned pTagMaxCount = 0;
    unsigned totalNumberOfPTags = 0;
    Element* maxPContainingElement = 0;
    calculatePTagMaxCount(*bodyElement, pTagMaxCount, totalNumberOfPTags, maxPContainingElement);

    WTF_LOG(SamsungReader, "p and br search Time : %f ms", (currentTimeMS() - startTime));
    WTF_LOG(SamsungReader, "brTagMaxCount : %d", brTagMaxCount);
    WTF_LOG(SamsungReader, "pTagMaxCount : %d", pTagMaxCount);
    WTF_LOG(SamsungReader, "otherTagMaxCount : %d", otherTagMaxCount);
    WTF_LOG(SamsungReader, "articleTagCount : %d", articleTagCount);


    if (!brTagMaxCount && !pTagMaxCount && bodyElement->getElementsByTagName("pre")->isEmpty()){
        std::string result("false@@");
        result += page_url;
        return result;
    }

#if !LOG_DISABLED
    startTime = currentTimeMS();
#endif

    unsigned mainBodyTextLength = bodyElement->innerText().length();
    if (!mainBodyTextLength){
        std::string result("false@@");
        result += page_url;
        return result;
    }

    unsigned articleTextLength = 0;
    unsigned articleAnchorTextLength = 0;
    unsigned brTextLength = 0;
    unsigned pTextLength = 0;
    Element* articleElement = 0;

    if (maxBRContainingElement) {
        if (totalNumberOfBRTags > 0 && maxBRContainingElement->parentElement() && brTagMaxCount >= 1)
            brTextLength = maxBRContainingElement->parentElement()->innerText().length();
    }

    if (maxPContainingElement) {
        if (totalNumberOfPTags > 0 && maxPContainingElement->parentElement() && pTagMaxCount >= 1)
            pTextLength = maxPContainingElement->parentElement()->innerText().length();
    }

    articleTextLength = std::max(brTextLength, pTextLength);

    if (brTextLength >= pTextLength) {
        if (maxBRContainingElement && maxBRContainingElement->parentElement())
            articleElement = maxBRContainingElement->parentElement();
    } else {
        if (maxPContainingElement && maxPContainingElement->parentElement())
            articleElement = maxPContainingElement->parentElement();
    }

    if (articleElement) {
        for (Element* element = ElementTraversal::firstWithin(*articleElement); element; element = ElementTraversal::next(*element, articleElement)) {
            if (!element->hasTagName(aTag))
                continue;
            if (element->isFocusable())
                articleAnchorTextLength += element->innerText().length();
        }
    }

    // FIXME: It is inefficient to construct the innerText string for the whole
    // bodyElement when we are only interested in the first 30 elements. Method which
    // does the same can be added to Element.
    String cjkTestString = bodyElement->innerText();

    WTF_LOG(SamsungReader, "textLength Time : %f ms", currentTimeMS() - startTime);
    WTF_LOG(SamsungReader, "innerText substring : %s", cjkTestString.utf8().data());

    // To check if there is any CJK character
    bool isCJKpage = isCJKPage(cjkTestString);

#if !LOG_DISABLED
    if (isCJKpage)
        WTF_LOG(SamsungReader, "It's CJK page");
    startTime = currentTimeMS();
#endif

    unsigned anchorTextLength = 1;
    for (Element* element = ElementTraversal::firstWithin(*bodyElement); element; element = ElementTraversal::next(*element, bodyElement)) {
        if (!element->hasTagName(aTag))
            continue;
        if (element->isFocusable())
            anchorTextLength += element->innerText().length();
    }

    WTF_LOG(SamsungReader, "link Time : %f ms", (currentTimeMS() - startTime));

    double linkDensity =  static_cast<double>(anchorTextLength) / mainBodyTextLength;
    double articleLinkDensity = static_cast<double>(articleAnchorTextLength) / articleTextLength;

    WTF_LOG(SamsungReader, "mainBodyTextLength %u: ", mainBodyTextLength);
    WTF_LOG(SamsungReader, "articleTextLength : %u", articleTextLength);
    WTF_LOG(SamsungReader, "articleAnchorTextLength : %u", articleAnchorTextLength);
    WTF_LOG(SamsungReader, "anchorTextLength : %u", anchorTextLength);
    WTF_LOG(SamsungReader, "linkDensity : %f", linkDensity);
    WTF_LOG(SamsungReader, "articleLinkDensity : %f", articleLinkDensity);

    if (isCJKpage && ((mainBodyTextLength - anchorTextLength) < 300 || articleTextLength < 150
            || articleLinkDensity > 0.5)) {
        WTF_LOG(SamsungReader, "CJK & not Linked textLength < 300 or articleTextLength < 150");
        std::string result("false@@");
        result += page_url;
        return result;
    }

    // FIXME: Why is a separate boolean for kroeftel.de needed?
    bool isKroeftel = equalIgnoringCase(hostName, "kroeftel.de");

    if ((mainBodyTextLength - anchorTextLength) < 500 || (articleTextLength < 200 && !isKroeftel)
            || articleLinkDensity > 0.5) {
        WTF_LOG(SamsungReader, "not Linked textLength < 500 or articleTextLength < 200");
        std::string result("false@@");
        result += page_url;
        return result;
    }

    // FIXME: Why is a separate boolean for naver.com needed?
    bool isNaverNews = (hostName.findIgnoringCase("news.naver.com") != kNotFound);

    if (((mainBodyTextLength > 4000 && linkDensity < 0.63) || (mainBodyTextLength > 3000 && linkDensity < 0.58)
        || (mainBodyTextLength > 2500 && linkDensity < 0.6) || (linkDensity < 0.4)) && (otherTagMaxCount <= 13)) {
        if ((articleTextLength == 743 && pTagMaxCount == 2) || (articleTextLength == 316 && pTagMaxCount == 1)) {
            std::string result("false@@");
            result += page_url;
            return result;
        } else {
            std::string result("true@@");
            result += page_url;
            return result;
        }
    }
    if ((linkDensity < 0.7 && linkDensity > 0.4) && (brTagMaxCount >= 1 || pTagMaxCount >= 5) && (otherTagMaxCount <= 13)){
        std::string result("true@@");
        result += page_url;
        return result;
    }
    if (isNaverNews && (linkDensity < 0.78 && linkDensity > 0.4) && (brTagMaxCount >= 5 || pTagMaxCount >= 5) && (otherTagMaxCount <= 13)){
        std::string result("true@@");
        result += page_url;
        return result;
    }
    std::string result("false@@");
    result += page_url;
    return result;
}

std::string ArticleRecognition::recognizeArticleNativeRecognitionMode(WebCore::Frame* frame)
{
    static const char* homepage[] = {
        "?mview=desktop", "?ref=smartphone", "apple.com", "query=", "|search?", "?from=mobile", "signup", "twitter", "facebook", "youtube", "?f=mnate", "linkedin",
        "romaeo", "chrome:", "gsshop", "gdive", "?nytmobile=0", "?CMP=mobile_site", "?main=true", "home-page", "anonymMain", "thetrainline"
    };

    Element* bodyElement = frame ? frame->document()->body() : 0;

    if (!bodyElement)
        return "false";

    const KURL& url = frame->document()->url();
    std::string page_url(url.string().utf8().data());
    
    WTF_LOG(SamsungReader, "URL: %s", url.string().utf8().data());
    WTF_LOG(SamsungReader, "HostName : %s", url.host().utf8().data());

    if (url.path() == "/") {
        WTF_LOG(SamsungReader, "This is HOME PAGE. Path is empty.");
        std::string result("false@@");
        result += page_url;
        return result;
    }

    // If any of the '|' separated values present in String homepage are found in url, return false.
    // Mostly used for sites using Relative URLs.
    if (regExpSearch(url, homepage, WTF_ARRAY_LENGTH(homepage))) {
        WTF_LOG(SamsungReader, "regExpSearch :: This is HOME PAGE. RegEx present in homepage found");
        std::string result("false@@");
        result += page_url;
        return result;
    }

#if !LOG_DISABLED
    double startTime = currentTimeMS();
#endif

    RefPtr<Element> recogDiv = frame->document()->createElement(divTag, false);

    recogDiv->setAttribute(idAttr, "recog_div");
    recogDiv->setAttribute(styleAttr, "display:none;");

    Vector<Node*> scoringNodes;
    // Populate the above vector
    populateScoringNodesVector(*bodyElement, scoringNodes);

    WTF_LOG(SamsungReader, "populateScoringNodesVector time taken : %f ms", (currentTimeMS() - startTime));
#if !LOG_DISABLED
    startTime = currentTimeMS();
#endif

    Vector<Element*> candidateElements;
    bool isCJK = false;
    // populate candidateElement Vector
    populateCandidateElementsVector(scoringNodes, candidateElements, isCJK);

    WTF_LOG(SamsungReader, "populateCandidateElementsVector time taken : %f ms", (currentTimeMS() - startTime));
#if !LOG_DISABLED
    startTime = currentTimeMS();
#endif

    Element* topCandidate = 0;
    unsigned candidateElementsSize = candidateElements.size();
    for (unsigned i = 0; i < candidateElementsSize; ++i) {
        Element* candidateElement = candidateElements[i];
        // FIXME: Use custom data-* attribute everywhere since readability is not a standard HTML attribute.
        double candidateElementScore = candidateElement->getFloatingPointAttribute(readabilityAttr, 0);

        double topCandidateScore = topCandidate ? topCandidate->getFloatingPointAttribute(readabilityAttr, 0) : 0;

        candidateElementScore *= (1 - linkDensityForNode(*candidateElement));
        candidateElement->setFloatingPointAttribute(readabilityAttr, candidateElementScore);
        if (!topCandidate || candidateElementScore > topCandidateScore)
            topCandidate = candidateElement;
    }

    // After we find top candidates, we check how many similar top-candidates were within a 15% range of this
    // top-candidate - this is needed because on homepages, there are several possible topCandidates which differ
    // by a minute amount in score. The check can be within a 10% range, but to be on the safe-side we are using 15%.
    // Usually, for proper article pages, a clear, definitive topCandidate will be present.
    unsigned neighbourCandidates = 0;
    double topCandidateScore = topCandidate ? topCandidate->getFloatingPointAttribute(readabilityAttr, 0) : 0;
    for (unsigned i = 0; i < candidateElementsSize; ++i) {
        Element* candidateElement = candidateElements[i];
        double candidateElementScore = candidateElement->getFloatingPointAttribute(readabilityAttr, 0);
        if ((candidateElementScore >= topCandidateScore * 0.85) && (candidateElement != topCandidate))
            ++neighbourCandidates;
    }

    // For now, the check for neighbourCandidates has threshold of 2, it can be modified later as and when required.
    if (neighbourCandidates > 2) {
        // disabling reader icon
        std::string result("false@@");
        result += page_url;
        return result;
    }

    WTF_LOG(SamsungReader, "Third loop Time : %f ms", (currentTimeMS() - startTime));
#if !LOG_DISABLED
    startTime = currentTimeMS();
#endif

    unsigned numberOfTRs = 0;

    if (topCandidate) {
        if (topCandidate->hasTagName(trTag) || topCandidate->hasTagName(tbodyTag))
            numberOfTRs = topCandidate->getElementsByTagName("tr")->length();
    }

    if ((topCandidate->renderStyle()->visibility() != VISIBLE) && !neighbourCandidates) {
        // control will come here if there is no other nodes which can be considered as top candidate, & topCandidate is not visible to user.
        std::string result("false@@");
        result += page_url;
        return result;
    }
    if (linkDensityForNode(*topCandidate) > 0.5) {
        // disabling reader icon due to higher link density in topCandidate
        std::string result("false@@");
        result += page_url;
        return result;
    }
    if (topCandidate->hasTagName(bodyTag) || topCandidate->hasTagName(formTag)) {
        // disabling reader icon as invalid topCandidate
        std::string result("false@@");
        result += page_url;
        return result;
    }

    String elementInnerText = topCandidate->innerText();

    int splitLength = countNumberOfSpaceSeparatedValues(elementInnerText);
    unsigned readerTextLength = elementInnerText.length();
    unsigned readerPlength = topCandidate->getElementsByTagName("p")->length();
    double readerScore = topCandidate->getFloatingPointAttribute(readabilityAttr, 0);

    WTF_LOG(SamsungReader, "ReaderScore %f -textLength : %u Trs : %u, Plength : %u, splitLength : %d", readerScore, readerTextLength, numberOfTRs, readerPlength, splitLength);

    // FIXME: Use a meaningful name for these magic numbers instead of using them directly.
    if ((readerScore >= 40 && numberOfTRs < 3 )
        || (readerScore >= 20 && readerScore < 30 && readerTextLength >900 && readerPlength >=2 && numberOfTRs < 3 && !isCJK)
        || (readerScore >= 20 && readerScore < 30 && readerTextLength >1900 && readerPlength >=0 && numberOfTRs < 3 && !isCJK)
        || (readerScore > 15 && readerScore <=40  && splitLength >=100 && numberOfTRs < 3)
        || (readerScore >= 100 && readerTextLength >2000  && splitLength >=250 && numberOfTRs > 200)) {
        if (readerScore >= 40 && readerTextLength < 100){
            std::string result("false@@");
            result += page_url;
            return result;
        }
        std::string result("true@@");
        result += page_url;
        return result;
    }
    std::string result("false@@");
    result += page_url;
    return result;
}

} // namespace blink
