/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebViewImpl.h"

#include "base/logging.h"
#include "CSSValueKeywords.h"
#include "CompositionUnderlineVectorBuilder.h"
#include "ContextFeaturesClientImpl.h"
#include "DOMWindowBingSearchEngineClientImpl.h"
#include "DatabaseClientImpl.h"
#include "FullscreenController.h"
#include "GeolocationClientProxy.h"
#include "GraphicsLayerFactoryChromium.h"
#include "HTMLNames.h"
#include "LinkHighlight.h"
#include "LinkHighlightHover.h"
#include "LocalFileSystemClient.h"
#include "MIDIClientProxy.h"
#include "PinchViewports.h"
#include "PopupContainer.h"
#include "PrerendererClientImpl.h"
#include "RuntimeEnabledFeatures.h"
#include "SpeechInputClientImpl.h"
#include "SpeechRecognitionClientProxy.h"
#include "StorageQuotaClientImpl.h"
#include "ValidationMessageClientImpl.h"
#include "ViewportAnchor.h"
#include "WebAXObject.h"
#include "WebActiveWheelFlingParameters.h"
#include "WebAutofillClient.h"
#include "WebDevToolsAgentImpl.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebFrameImpl.h"
#include "WebHelperPlugin.h"
#include "WebHelperPluginImpl.h"
#include "WebHitTestResult.h"
#include "WebInputElement.h"
#include "WebInputEventConversion.h"
#include "WebMediaPlayerAction.h"
#include "WebNode.h"
#include "WebPagePopupImpl.h"
#include "WebPlugin.h"
#include "WebPluginAction.h"
#include "WebPluginContainerImpl.h"
#include "WebPopupMenuImpl.h"
#include "WebRange.h"
#include "WebSettingsImpl.h"
#include "WebTextInputInfo.h"
#include "WebViewClient.h"
#include "WebWindowFeatures.h"
#include "WorkerGlobalScopeProxyProviderImpl.h"
#include "base/strings/utf_string_conversions.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/clipboard/DataObject.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/ParentNode.h"
#include "core/dom/Text.h"
#include "core/dom/WheelController.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/TextIterator.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/SmartClip.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/ime/InputMethodContext.h"
#include "core/inspector/InspectorController.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/page/Chrome.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragController.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/InjectedStyleSheets.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "core/page/PageGroupLoadDeferrer.h"
#include "core/page/PagePopupClient.h"
#include "core/page/PointerLockController.h"
#include "core/page/TouchDisambiguation.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderWidget.h"
#include "core/rendering/TextAutosizer.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"
#include "modules/geolocation/GeolocationController.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"
#include "modules/notifications/NotificationController.h"
#include "painting/ContinuousPainter.h"
#include "platform/ContextMenu.h"
#include "platform/ContextMenuItem.h"
#include "platform/Cursor.h"
#include "platform/DragImage.h"
#include "platform/KeyboardCodes.h"
#include "platform/NotImplemented.h"
#include "platform/OverscrollTheme.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/PopupMenuClient.h"
#include "platform/TraceEvent.h"
#include "platform/exported/WebActiveGestureAnimation.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebGestureCurve.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebVector.h"
#include "wtf/CurrentTime.h"
#include "wtf/RefPtr.h"
#include "wtf/TemporaryChange.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLSelectElement.h"

#include "cc/base/switches.h"
#include "base/command_line.h"

#if USE(DEFAULT_RENDER_THEME)
#include "core/rendering/RenderThemeChromiumDefault.h"
#endif

#if defined(S_FP_AUTOLOGIN_SUPPORT)
#include "core/html/HTMLFormElement.h"
#include "WebInputEventFactory.h"
#endif

#if ENABLE(PUSH_API)
#include "modules/push_registration/PushController.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

using namespace WebCore;
using namespace std;

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 32;
static const int nonUserInitiatedPointPadding = 11;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 5;
static const float doubleTapZoomContentMinimumMargin = 2;
static const double doubleTapZoomAnimationDurationInSeconds = 0.25;
static const float doubleTapZoomAlreadyLegibleRatio = 1.2f;

static const double multipleTargetsZoomAnimationDurationInSeconds = 0.25;
static const double findInPageAnimationDurationInSeconds = 0;

// Constants for viewport anchoring on resize.
static const float viewportAnchorXCoord = 0.5f;
static const float viewportAnchorYCoord = 0;

// Constants for zooming in on a focused text field.
static const double scrollAndScaleAnimationDurationInSeconds = 0.2;
static const int minReadableCaretHeight = 18;
static const float minScaleChangeToTriggerZoom = 1.05f;
static const float leftBoxRatio = 0.3f;
static const int caretPadding = 10;

namespace blink {

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
const double WebView::textSizeMultiplierRatio = 1.2;
const double WebView::minTextSizeMultiplier = 0.5;
const double WebView::maxTextSizeMultiplier = 3.0;

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop invocations.
static Vector<PageGroupLoadDeferrer*>& pageGroupLoadDeferrerStack()
{
    DEFINE_STATIC_LOCAL(Vector<PageGroupLoadDeferrer*>, deferrerStack, ());
    return deferrerStack;
}

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
#define COMPILE_ASSERT_MATCHING_ENUM(coreName) \
    COMPILE_ASSERT(int(coreName) == int(Web##coreName), dummy##coreName)
COMPILE_ASSERT_MATCHING_ENUM(DragOperationNone);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationCopy);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationLink);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationGeneric);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationPrivate);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationMove);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationDelete);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationEvery);

static bool shouldUseExternalPopupMenus = false;

static int webInputEventKeyStateToPlatformEventKeyState(int webInputEventKeyState)
{
    int platformEventKeyState = 0;
    if (webInputEventKeyState & WebInputEvent::ShiftKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::ShiftKey;
    if (webInputEventKeyState & WebInputEvent::ControlKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::CtrlKey;
    if (webInputEventKeyState & WebInputEvent::AltKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::AltKey;
    if (webInputEventKeyState & WebInputEvent::MetaKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::MetaKey;
    return platformEventKeyState;
}

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return WebViewImpl::create(client);
}

WebViewImpl* WebViewImpl::create(WebViewClient* client)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return adoptRef(new WebViewImpl(client)).leakRef();
}

void WebView::setUseExternalPopupMenus(bool useExternalPopupMenus)
{
    shouldUseExternalPopupMenus = useExternalPopupMenus;
}

void WebView::updateVisitedLinkState(unsigned long long linkHash)
{
    Page::visitedStateChanged(linkHash);
}

void WebView::resetVisitedLinkState()
{
    Page::allVisitedStateChanged();
}

void WebView::willEnterModalLoop()
{
    PageGroup* pageGroup = PageGroup::sharedGroup();
    if (pageGroup->pages().isEmpty())
        pageGroupLoadDeferrerStack().append(static_cast<PageGroupLoadDeferrer*>(0));
    else {
        // Pick any page in the page group since we are deferring all pages.
        pageGroupLoadDeferrerStack().append(new PageGroupLoadDeferrer(*pageGroup->pages().begin(), true));
    }
}

void WebView::didExitModalLoop()
{
    ASSERT(pageGroupLoadDeferrerStack().size());

    delete pageGroupLoadDeferrerStack().last();
    pageGroupLoadDeferrerStack().removeLast();
}

void WebViewImpl::setMainFrame(WebFrame* frame)
{
    toWebFrameImpl(frame)->initializeAsMainFrame(page());
}

void WebViewImpl::setAutofillClient(WebAutofillClient* autofillClient)
{
    m_autofillClient = autofillClient;
}

void WebViewImpl::setDevToolsAgentClient(WebDevToolsAgentClient* devToolsClient)
{
#ifdef ENABLE_DEV_TOOLS
	if (devToolsClient)
        m_devToolsAgent = adoptPtr(new WebDevToolsAgentImpl(this, devToolsClient));
    else
        m_devToolsAgent.clear();
#endif
}

void WebViewImpl::setPrerendererClient(WebPrerendererClient* prerendererClient)
{
    providePrerendererClientTo(m_page.get(), new PrerendererClientImpl(prerendererClient));
}

void WebViewImpl::setSpellCheckClient(WebSpellCheckClient* spellCheckClient)
{
    m_spellCheckClient = spellCheckClient;
}

void WebViewImpl::setPasswordGeneratorClient(WebPasswordGeneratorClient* client)
{
    m_passwordGeneratorClient = client;
}

WebViewImpl::WebViewImpl(WebViewClient* client)
    : m_client(client)
    , m_autofillClient(0)
    , m_spellCheckClient(0)
    , m_passwordGeneratorClient(0)
    , m_chromeClientImpl(this)
    , m_contextMenuClientImpl(this)
    , m_dragClientImpl(this)
    , m_editorClientImpl(this)
    , m_inspectorClientImpl(this)
    , m_backForwardClientImpl(this)
    , m_spellCheckerClientImpl(this)
    , m_storageClientImpl(this)
    , m_fixedLayoutSizeLock(false)
    , m_shouldAutoResize(false)
    , m_zoomLevel(0)
    , m_minimumZoomLevel(zoomFactorToZoomLevel(minTextSizeMultiplier))
    , m_maximumZoomLevel(zoomFactorToZoomLevel(maxTextSizeMultiplier))
    , m_savedPageScaleFactor(0)
    , m_doubleTapZoomPageScaleFactor(0)
    , m_doubleTapZoomPending(false)
    , m_enableFakePageScaleAnimationForTesting(false)
    , m_fakePageScaleAnimationPageScaleFactor(0)
    , m_fakePageScaleAnimationUseAnchor(false)
    , m_contextMenuAllowed(false)
    , m_doingDragAndDrop(false)
    , m_ignoreInputEvents(false)
    , m_compositorDeviceScaleFactorOverride(0)
    , m_rootLayerScale(1)
    , m_suppressNextKeypressEvent(false)
    , m_imeAcceptEvents(true)
    , m_operationsAllowed(WebDragOperationNone)
    , m_dragOperation(WebDragOperationNone)
    , m_featureSwitchClient(adoptPtr(new ContextFeaturesClientImpl()))
    , m_isTransparent(false)
    , m_tabsToLinks(false)
    , m_layerTreeView(0)
    , m_rootLayer(0)
    , m_rootGraphicsLayer(0)
    , m_graphicsLayerFactory(adoptPtr(new GraphicsLayerFactoryChromium(this)))
    , m_isAcceleratedCompositingActive(false)
    , m_layerTreeViewCommitsDeferred(false)
    , m_compositorCreationFailed(false)
    , m_recreatingGraphicsContext(false)
#if ENABLE(INPUT_SPEECH)
    , m_speechInputClient(SpeechInputClientImpl::create(client))
#endif
    , m_speechRecognitionClient(SpeechRecognitionClientProxy::create(client ? client->speechRecognizer() : 0))
    , m_geolocationClientProxy(adoptPtr(new GeolocationClientProxy(client ? client->geolocationClient() : 0)))
    , m_userMediaClientImpl(this)
    , m_midiClientProxy(adoptPtr(new MIDIClientProxy(client ? client->webMIDIClient() : 0)))
    , m_navigatorContentUtilsClient(NavigatorContentUtilsClientImpl::create(this))
#if ENABLE(BING_SEARCH_ENGINE_SETTING_FROM_JS)
    , m_domWindowBingSearchEngineClient(DOMWindowBingSearchEngineClientImpl::create(this))
#endif
    , m_flingModifier(0)
    , m_flingSourceDevice(false)
    #if defined(S_IME_SCROLL_EVENT)
    , m_contentTopOffset(0)
    #endif
    , m_fullscreenController(FullscreenController::create(this))
    , m_showFPSCounter(false)
    , m_showPaintRects(false)
    , m_showDebugBorders(false)
    , m_continuousPaintingEnabled(false)
    , m_showScrollBottleneckRects(false)
    , m_baseBackgroundColor(Color::white)
    , m_backgroundColorOverride(Color::transparent)
    , m_zoomFactorOverride(0)
    #if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
    , m_autologin_failure(false)
    #endif
    , m_helperPluginCloseTimer(this, &WebViewImpl::closePendingHelperPlugins)
    , m_prevHoverNode(0)
#if defined (SBROWSER_SOFTBITMAP_IMPL)
    , m_pageScaleFactor(1.0f)
#endif    
{
    Page::PageClients pageClients;
    pageClients.chromeClient = &m_chromeClientImpl;
    pageClients.contextMenuClient = &m_contextMenuClientImpl;
    pageClients.editorClient = &m_editorClientImpl;
    pageClients.dragClient = &m_dragClientImpl;
    pageClients.inspectorClient = &m_inspectorClientImpl;
    pageClients.backForwardClient = &m_backForwardClientImpl;
    pageClients.spellCheckerClient = &m_spellCheckerClientImpl;
    pageClients.storageClient = &m_storageClientImpl;

    m_page = adoptPtr(new Page(pageClients));
    provideUserMediaTo(m_page.get(), &m_userMediaClientImpl);
    provideMIDITo(m_page.get(), m_midiClientProxy.get());
#if ENABLE(INPUT_SPEECH)
    provideSpeechInputTo(m_page.get(), m_speechInputClient.get());
#endif
    provideSpeechRecognitionTo(m_page.get(), m_speechRecognitionClient.get());
    provideNotification(m_page.get(), notificationPresenterImpl());
    provideNavigatorContentUtilsTo(m_page.get(), m_navigatorContentUtilsClient.get());
#if ENABLE(BING_SEARCH_ENGINE_SETTING_FROM_JS)
    provideDOMWindowBingSearchEngineTo(m_page.get(), m_domWindowBingSearchEngineClient.get());
#endif
    provideContextFeaturesTo(m_page.get(), m_featureSwitchClient.get());
    if (RuntimeEnabledFeatures::deviceOrientationEnabled())
        DeviceOrientationInspectorAgent::provideTo(m_page.get());
    provideGeolocationTo(m_page.get(), m_geolocationClientProxy.get());
    m_geolocationClientProxy->setController(GeolocationController::from(m_page.get()));

    provideLocalFileSystemTo(m_page.get(), LocalFileSystemClient::create());
    provideDatabaseClientTo(m_page.get(), DatabaseClientImpl::create());
    InspectorIndexedDBAgent::provideTo(m_page.get());
    provideStorageQuotaClientTo(m_page.get(), StorageQuotaClientImpl::create());
    m_validationMessage = ValidationMessageClientImpl::create(*this);
    m_page->setValidationMessageClient(m_validationMessage.get());
    provideWorkerGlobalScopeProxyProviderTo(m_page.get(), WorkerGlobalScopeProxyProviderImpl::create());

    m_page->setGroupType(Page::SharedPageGroup);

    if (m_client) {
        setDeviceScaleFactor(m_client->screenInfo().deviceScaleFactor);
        setVisibilityState(m_client->visibilityState(), true);
#if ENABLE(PUSH_API)
        providePushControllerTo(m_page.get(), m_client->webPushClient());
#endif
    }

    m_inspectorSettingsMap = adoptPtr(new SettingsMap);
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_page);
    ASSERT(!m_helperPluginCloseTimer.isActive());
    ASSERT(m_helperPluginsPendingClose.isEmpty());
}

WebFrameImpl* WebViewImpl::mainFrameImpl()
{
    return m_page ? WebFrameImpl::fromFrame(m_page->mainFrame()) : 0;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const
{
    ASSERT(m_page);
    return m_page->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value)
{
    if (m_page)
        m_page->setTabKeyCyclesThroughElements(value);
}

void WebViewImpl::handleMouseLeave(Frame& mainFrame, const WebMouseEvent& event)
{
    m_client->setMouseOverURL(WebURL());
    PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebViewImpl::handleMouseDown(Frame& mainFrame, const WebMouseEvent& event)
{
    // If there is a popup open, close it as the user is clicking on the page (outside of the
    // popup). We also save it so we can prevent a click on an element from immediately
    // reopening the same popup.
    RefPtr<PopupContainer> selectPopup;
    RefPtr<WebPagePopupImpl> pagePopup;
    if (event.button == WebMouseEvent::ButtonLeft) {
        selectPopup = m_selectPopup;
        pagePopup = m_pagePopup;
        hidePopups();
        ASSERT(!m_selectPopup);
        ASSERT(!m_pagePopup);
    }

    m_lastMouseDownPoint = WebPoint(event.x, event.y);

    if (event.button == WebMouseEvent::ButtonLeft) {
        IntPoint point(event.x, event.y);
        point = m_page->mainFrame()->view()->windowToContents(point);
        HitTestResult result(m_page->mainFrame()->eventHandler().hitTestResultAtPoint(point));
        Node* hitNode = result.innerNonSharedNode();

        // Take capture on a mouse down on a plugin so we can send it mouse events.
        if (hitNode && hitNode->renderer() && hitNode->renderer()->isEmbeddedObject()) {
            m_mouseCaptureNode = hitNode;
            TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
        }
    }

    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

    if (m_selectPopup && m_selectPopup == selectPopup) {
        // That click triggered a select popup which is the same as the one that
        // was showing before the click.  It means the user clicked the select
        // while the popup was showing, and as a result we first closed then
        // immediately reopened the select popup.  It needs to be closed.
        hideSelectPopup();
    }

    if (m_pagePopup && pagePopup && m_pagePopup->hasSamePopupClient(pagePopup.get())) {
        // That click triggered a page popup that is the same as the one we just closed.
        // It needs to be closed.
        closePagePopup(m_pagePopup.get());
    }

    // Dispatch the contextmenu event regardless of if the click was swallowed.
#if OS(WIN)
    // On Windows, we handle it on mouse up, not down.
#elif OS(MACOSX)
    if (event.button == WebMouseEvent::ButtonRight
        || (event.button == WebMouseEvent::ButtonLeft
            && event.modifiers & WebMouseEvent::ControlKey))
        mouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

void WebViewImpl::mouseContextMenu(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_page->contextMenuController().clearContextMenu();

    PlatformMouseEventBuilder pme(mainFrameImpl()->frameView(), event);

    // Find the right target frame. See issue 1186900.
    HitTestResult result = hitTestResultForWindowPos(pme.position());
    Frame* targetFrame;
    if (result.innerNonSharedNode())
        targetFrame = result.innerNonSharedNode()->document().frame();
    else
        targetFrame = m_page->focusController().focusedOrMainFrame();

#if OS(WIN)
    targetFrame->view()->setCursor(pointerCursor());
#endif

    m_contextMenuAllowed = true;
    targetFrame->eventHandler().sendContextMenuEvent(pme);
    m_contextMenuAllowed = false;
    // Actually showing the context menu is handled by the ContextMenuClient
    // implementation...
}

void WebViewImpl::handleMouseUp(Frame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseUp(mainFrame, event);

#if OS(WIN)
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

bool WebViewImpl::handleMouseWheel(Frame& mainFrame, const WebMouseWheelEvent& event)
{
    hidePopups();
    return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

void WebViewImpl::scrollBy(const WebFloatSize& delta)
{
    if (m_flingSourceDevice == WebGestureEvent::Touchpad) {
        WebMouseWheelEvent syntheticWheel;
        const float tickDivisor = WebCore::WheelEvent::TickMultiplier;

        syntheticWheel.deltaX = delta.width;
        syntheticWheel.deltaY = delta.height;
        syntheticWheel.wheelTicksX = delta.width / tickDivisor;
        syntheticWheel.wheelTicksY = delta.height / tickDivisor;
        syntheticWheel.hasPreciseScrollingDeltas = true;
        syntheticWheel.x = m_positionOnFlingStart.x;
        syntheticWheel.y = m_positionOnFlingStart.y;
        syntheticWheel.globalX = m_globalPositionOnFlingStart.x;
        syntheticWheel.globalY = m_globalPositionOnFlingStart.y;
        syntheticWheel.modifiers = m_flingModifier;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
            handleMouseWheel(*m_page->mainFrame(), syntheticWheel);
    } else {
        WebGestureEvent syntheticGestureEvent;

        syntheticGestureEvent.type = WebInputEvent::GestureScrollUpdateWithoutPropagation;
        syntheticGestureEvent.data.scrollUpdate.deltaX = delta.width;
        syntheticGestureEvent.data.scrollUpdate.deltaY = delta.height;
        syntheticGestureEvent.x = m_positionOnFlingStart.x;
        syntheticGestureEvent.y = m_positionOnFlingStart.y;
        syntheticGestureEvent.globalX = m_globalPositionOnFlingStart.x;
        syntheticGestureEvent.globalY = m_globalPositionOnFlingStart.y;
        syntheticGestureEvent.modifiers = m_flingModifier;
        syntheticGestureEvent.sourceDevice = WebGestureEvent::Touchscreen;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
            handleGestureEvent(syntheticGestureEvent);
    }
}

bool WebViewImpl::handleGestureEvent(const WebGestureEvent& event)
{
    bool eventSwallowed = false;
    bool eventCancelled = false; // for disambiguation

    // Special handling for slow-path fling gestures.
    switch (event.type) {
    case WebInputEvent::GestureFlingStart: {
        if (mainFrameImpl()->frame()->eventHandler().isScrollbarHandlingGestures())
            break;
        m_client->cancelScheduledContentIntents();
        m_positionOnFlingStart = WebPoint(event.x / pageScaleFactor(), event.y / pageScaleFactor());
        m_globalPositionOnFlingStart = WebPoint(event.globalX, event.globalY);
        m_flingModifier = event.modifiers;
        m_flingSourceDevice = event.sourceDevice;
        OwnPtr<WebGestureCurve> flingCurve = adoptPtr(Platform::current()->createFlingAnimationCurve(event.sourceDevice, WebFloatPoint(event.data.flingStart.velocityX, event.data.flingStart.velocityY), WebSize()));
        m_gestureAnimation = WebActiveGestureAnimation::createAtAnimationStart(flingCurve.release(), this);
        scheduleAnimation();
        eventSwallowed = true;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    }
    case WebInputEvent::GestureFlingCancel:
        if (endActiveFlingAnimation())
            eventSwallowed = true;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    default:
        break;
    }

    PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);

    // Handle link highlighting outside the main switch to avoid getting lost in the
    // complicated set of cases handled below.
    switch (event.type) {
    case WebInputEvent::GestureShowPress:
#if !defined(S_FOCUS_RING_FIX)
        // Queue a highlight animation, then hand off to regular handler.
        if (settingsImpl()->gestureTapHighlightEnabled())
            enableTapHighlightAtPoint(platformEvent);
#endif		
        break;
#if !defined(S_FOCUS_RING_FIX)
    case WebInputEvent::GestureTapCancel:
#endif		
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureLongPress:
#if defined(S_FOCUS_RING_FIX)		
	// Queue a highlight animation, then hand off to regular handler.
	if (settingsImpl()->gestureTapHighlightEnabled())
		enableTapHighlightAtPoint(platformEvent);
#endif		

        for (size_t i = 0; i < m_linkHighlights.size(); ++i)
            m_linkHighlights[i]->startHighlightAnimationIfNeeded();
        //Samsung TS : resetting WebContentDetectionResult
        if(mainFrameImpl())
            mainFrameImpl()->setContentDetectionResult(WebContentDetectionResult());
        break;
    default:
        break;
    }

    switch (event.type) {
    case WebInputEvent::GestureTap: {
        m_client->cancelScheduledContentIntents();
        if (detectContentOnTouch(platformEvent.position())) {
            eventSwallowed = true;
            break;
        }

        RefPtr<PopupContainer> selectPopup;
        selectPopup = m_selectPopup;
        hideSelectPopup();
        ASSERT(!m_selectPopup);

        // Don't trigger a disambiguation popup on sites designed for mobile devices.
        // Instead, assume that the page has been designed with big enough buttons and links.
        if (event.data.tap.width > 0 && !shouldDisableDesktopWorkarounds()) {
            // FIXME: didTapMultipleTargets should just take a rect instead of
            // an event.
            WebGestureEvent scaledEvent = event;
            scaledEvent.x = event.x / pageScaleFactor();
            scaledEvent.y = event.y / pageScaleFactor();
            scaledEvent.data.tap.width = event.data.tap.width / pageScaleFactor();
            scaledEvent.data.tap.height = event.data.tap.height / pageScaleFactor();
            IntRect boundingBox(scaledEvent.x - scaledEvent.data.tap.width / 2, scaledEvent.y - scaledEvent.data.tap.height / 2, scaledEvent.data.tap.width, scaledEvent.data.tap.height);
            Vector<IntRect> goodTargets;
            Vector<Node*> highlightNodes;
            findGoodTouchTargets(boundingBox, mainFrameImpl()->frame(), goodTargets, highlightNodes);
            // FIXME: replace touch adjustment code when numberOfGoodTargets == 1?
            // Single candidate case is currently handled by: https://bugs.webkit.org/show_bug.cgi?id=85101
            if (goodTargets.size() >= 2 && m_client && m_client->didTapMultipleTargets(scaledEvent, goodTargets)) {
                if (settingsImpl()->gestureTapHighlightEnabled())
                    enableTapHighlights(highlightNodes);
                for (size_t i = 0; i < m_linkHighlights.size(); ++i)
                    m_linkHighlights[i]->startHighlightAnimationIfNeeded();
                eventSwallowed = true;
                eventCancelled = true;
                break;
            }
        }

        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);

        if (m_selectPopup && m_selectPopup == selectPopup) {
            // That tap triggered a select popup which is the same as the one that
            // was showing before the tap. It means the user tapped the select
            // while the popup was showing, and as a result we first closed then
            // immediately reopened the select popup. It needs to be closed.
            hideSelectPopup();
        }

        break;
    }
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap: {
        if (!mainFrameImpl() || !mainFrameImpl()->frameView())
            break;

        m_client->cancelScheduledContentIntents();
        m_page->contextMenuController().clearContextMenu();
        m_contextMenuAllowed = true;
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        m_contextMenuAllowed = false;
        //Samsung TS : resetting WebContentDetectionResult
        mainFrameImpl()->setContentDetectionResult(WebContentDetectionResult());
        break;
    }
    case WebInputEvent::GestureShowPress: {
        m_client->cancelScheduledContentIntents();
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        break;
    }
    case WebInputEvent::GestureDoubleTap:
        if (m_webSettings->doubleTapToZoomEnabled() && minimumPageScaleFactor() != maximumPageScaleFactor()) {
            m_client->cancelScheduledContentIntents();
            animateDoubleTapZoom(platformEvent.position());
        }
        // GestureDoubleTap is currently only used by Android for zooming. For WebCore,
        // GestureTap with tap count = 2 is used instead. So we drop GestureDoubleTap here.
        eventSwallowed = true;
        break;
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GesturePinchBegin:
        m_client->cancelScheduledContentIntents();
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureScrollUpdateWithoutPropagation:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureFlingStart: {
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    m_client->didHandleGestureEvent(event, eventCancelled);
    return eventSwallowed;
}

void WebViewImpl::transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters& parameters)
{
    TRACE_EVENT0("webkit", "WebViewImpl::transferActiveWheelFlingAnimation");
    ASSERT(!m_gestureAnimation);
    m_positionOnFlingStart = parameters.point;
    m_globalPositionOnFlingStart = parameters.globalPoint;
    m_flingModifier = parameters.modifiers;
    OwnPtr<WebGestureCurve> curve = adoptPtr(Platform::current()->createFlingAnimationCurve(parameters.sourceDevice, WebFloatPoint(parameters.delta), parameters.cumulativeScroll));
    m_gestureAnimation = WebActiveGestureAnimation::createWithTimeOffset(curve.release(), this, parameters.startTime);
    scheduleAnimation();
}

bool WebViewImpl::endActiveFlingAnimation()
{
    if (m_gestureAnimation) {
        m_gestureAnimation.clear();
        if (m_layerTreeView)
            m_layerTreeView->didStopFlinging();
        return true;
    }
    return false;
}

bool WebViewImpl::startPageScaleAnimation(const IntPoint& targetPosition, bool useAnchor, float newScale, double durationInSeconds)
{
    WebPoint clampedPoint = targetPosition;
    if (!useAnchor) {
        clampedPoint = clampOffsetAtScale(targetPosition, newScale);
        if (!durationInSeconds) {
            setPageScaleFactor(newScale, clampedPoint);
            return false;
        }
    }
    if (useAnchor && newScale == pageScaleFactor())
        return false;

    if (m_enableFakePageScaleAnimationForTesting) {
        m_fakePageScaleAnimationTargetPosition = targetPosition;
        m_fakePageScaleAnimationUseAnchor = useAnchor;
        m_fakePageScaleAnimationPageScaleFactor = newScale;
    } else {
        if (!m_layerTreeView)
            return false;
        m_layerTreeView->startPageScaleAnimation(targetPosition, useAnchor, newScale, durationInSeconds);
    }
    return true;
}

void WebViewImpl::enableFakePageScaleAnimationForTesting(bool enable)
{
    m_enableFakePageScaleAnimationForTesting = enable;
}

void WebViewImpl::setShowFPSCounter(bool show)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setShowFPSCounter");
        m_layerTreeView->setShowFPSCounter(show);
    }
    m_showFPSCounter = show;
}

void WebViewImpl::setShowPaintRects(bool show)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setShowPaintRects");
        m_layerTreeView->setShowPaintRects(show);
    }
    m_showPaintRects = show;
}

void WebViewImpl::setShowDebugBorders(bool show)
{
    if (m_layerTreeView)
        m_layerTreeView->setShowDebugBorders(show);
    m_showDebugBorders = show;
}

void WebViewImpl::setContinuousPaintingEnabled(bool enabled)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setContinuousPaintingEnabled");
        m_layerTreeView->setContinuousPaintingEnabled(enabled);
    }
    m_continuousPaintingEnabled = enabled;
    m_client->scheduleAnimation();
}

void WebViewImpl::setShowScrollBottleneckRects(bool show)
{
    if (m_layerTreeView)
        m_layerTreeView->setShowScrollBottleneckRects(show);
    m_showScrollBottleneckRects = show;
}

bool WebViewImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    ASSERT((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));

    // Halt an in-progress fling on a key event.
    endActiveFlingAnimation();

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
    if (m_pagePopup) {
        m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the popup will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }

    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame)
        return false;

#if !OS(MACOSX)
    const WebInputEvent::Type contextMenuTriggeringEventType =
#if OS(WIN)
        WebInputEvent::KeyUp;
#else
        WebInputEvent::RawKeyDown;
#endif

    bool isUnmodifiedMenuKey = !(event.modifiers & WebInputEvent::InputModifiers) && event.windowsKeyCode == VKEY_APPS;
    bool isShiftF10 = event.modifiers == WebInputEvent::ShiftKey && event.windowsKeyCode == VKEY_F10;
    if ((isUnmodifiedMenuKey || isShiftF10) && event.type == contextMenuTriggeringEventType) {
        sendContextMenuEvent(event);
        return true;
    }
#endif // !OS(MACOSX)

    PlatformKeyboardEventBuilder evt(event);

    if (frame->eventHandler().keyEvent(evt)) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plug-in node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Element* element = focusedElement();
            if (!element || !element->renderer() || !element->renderer()->isEmbeddedObject())
                m_suppressNextKeypressEvent = true;
        }
        return true;
    }

    return keyEventDefault(event);
}

bool WebViewImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    ASSERT(event.type == WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
    if (m_pagePopup)
        return m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));

    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return suppress;

    EventHandler& handler = frame->eventHandler();

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return true;

    // Accesskeys are triggered by char events and can't be suppressed.
    if (handler.handleAccessKey(evt))
        return true;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return false;

    if (!suppress && !handler.keyEvent(evt))
        return keyEventDefault(event);

    return true;
}

WebRect WebViewImpl::computeBlockBounds(const WebRect& rect, bool ignoreClipping)
{
    if (!mainFrameImpl())
        return WebRect();

    // Use the rect-based hit test to find the node.
    IntPoint point = mainFrameImpl()->frameView()->windowToContents(IntPoint(rect.x, rect.y));
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent | (ignoreClipping ? HitTestRequest::IgnoreClipping : 0);
    HitTestResult result = mainFrameImpl()->frame()->eventHandler().hitTestResultAtPoint(point, hitType, IntSize(rect.width, rect.height));

    Node* node = result.innerNonSharedNode();
    if (!node)
        return WebRect();

    // Find the block type node based on the hit node.
    while (node && (!node->renderer() || node->renderer()->isInline()))
        node = node->parentNode();

    // Return the bounding box in the window coordinate system.
    if (node) {
        IntRect rect = node->Node::pixelSnappedBoundingBox();
        Frame* frame = node->document().frame();
        return frame->view()->contentsToWindow(rect);
    }
    return WebRect();
}

WebRect WebViewImpl::widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin)
{
    WebSize maxSize;
    if (mainFrame())
        maxSize = mainFrame()->contentsSize();
    IntSize scrollOffset;
    if (mainFrame())
        scrollOffset = mainFrame()->scrollOffset();
    int leftMargin = targetMargin;
    int rightMargin = targetMargin;

    const int absoluteSourceX = source.x + scrollOffset.width();
    if (leftMargin > absoluteSourceX) {
        leftMargin = absoluteSourceX;
        rightMargin = max(leftMargin, minimumMargin);
    }

    const int maximumRightMargin = maxSize.width - (source.width + absoluteSourceX);
    if (rightMargin > maximumRightMargin) {
        rightMargin = maximumRightMargin;
        leftMargin = min(leftMargin, max(rightMargin, minimumMargin));
    }

    const int newWidth = source.width + leftMargin + rightMargin;
    const int newX = source.x - leftMargin;

    ASSERT(newWidth >= 0);
    ASSERT(scrollOffset.width() + newX + newWidth <= maxSize.width);

    return WebRect(newX, source.y, newWidth, source.height);
}

float WebViewImpl::legibleScale() const
{
    // Pages should be as legible as on desktop when at dpi scale, so no
    // need to zoom in further when automatically determining zoom level
    // (after double tap, find in page, etc), though the user should still
    // be allowed to manually pinch zoom in further if they desire.
    float legibleScale = 1;
    if (page())
        legibleScale *= page()->settings().accessibilityFontScaleFactor();
    return legibleScale;
}

void WebViewImpl::computeScaleAndScrollForBlockRect(const WebPoint& hitPoint, const WebRect& blockRect, float padding, float defaultScaleWhenAlreadyLegible, float& scale, WebPoint& scroll)
{
    scale = pageScaleFactor();
    scroll.x = scroll.y = 0;

    WebRect rect = blockRect;

    bool over_view_mode = true;
    double doubleTapTolerance = 0.01;
    if (scale > 0 && scale > minimumPageScaleFactor() + doubleTapTolerance) {
        LOG(INFO) << "WebViewImpl::Doubletap over_view_mode false";
        over_view_mode = false;
    }

    bool enable_doubleTap_patent = false;
    if(CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kEnableDoubleTapPatent)) {
        std::string double_tappatent_str(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(cc::switches::kEnableDoubleTapPatent));
        enable_doubleTap_patent = (double_tappatent_str == "1");
    }

    bool is_tablet = false;
    if(CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kIsTablet)) {
        std::string tablet_str(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(cc::switches::kIsTablet));
        is_tablet = (tablet_str == "1");
    }

    if (!rect.isEmpty()) {
        float defaultMargin = doubleTapZoomContentDefaultMargin;
        float minimumMargin = doubleTapZoomContentMinimumMargin;
        // We want the margins to have the same physical size, which means we
        // need to express them in post-scale size. To do that we'd need to know
        // the scale we're scaling to, but that depends on the margins. Instead
        // we express them as a fraction of the target rectangle: this will be
        // correct if we end up fully zooming to it, and won't matter if we
        // don't.
        rect = widenRectWithinPageBounds(rect,
                static_cast<int>(defaultMargin * rect.width / m_size.width),
                static_cast<int>(minimumMargin * rect.width / m_size.width));
        // Fit block to screen, respecting limits.
        scale = static_cast<float>(m_size.width) / rect.width;
        scale = min(scale, legibleScale());
#ifdef FSN_FONT_SOLUTION
        if(!enable_doubleTap_patent) {
            bool scaleUnchanged = fabs(pageScaleFactor() - scale) < minScaleDifference;
            if(over_view_mode && scaleUnchanged) {
                LOG(INFO) << "WebViewImpl::Doubletap scaleUnchanged";
                scale = 1.2;  //force to higher zoom scale
            }
        }
#endif

        if (pageScaleFactor() < defaultScaleWhenAlreadyLegible)
            scale = max(scale, defaultScaleWhenAlreadyLegible);
        scale = clampPageScaleFactorToLimits(scale);
        if(!enable_doubleTap_patent && is_tablet) {
            scale = 2.0;  //force to tablet zoom scale
        }
    }

    if(!over_view_mode)
        scale = minimumPageScaleFactor(); // Zoom out to minimum scale.
    // FIXME: If this is being called for auto zoom during find in page,
    // then if the user manually zooms in it'd be nice to preserve the
    // relative increase in zoom they caused (if they zoom out then it's ok
    // to zoom them back in again). This isn't compatible with our current
    // double-tap zoom strategy (fitting the containing block to the screen)
    // though.
    float screenWidth = m_size.width / scale;
    float screenHeight = m_size.height / scale;

    if(enable_doubleTap_patent) {
        LOG(INFO) << "WebViewImpl::Doubletap doesn't work blockzoom";
        scale = 1.2;
        if(is_tablet) scale = 2.0;  //force to tablet zoom scale

        rect.x = hitPoint.x-m_size.width/2;
        if (rect.height < screenHeight) {
            rect.y -= rect.y*pageScaleFactor()/scale;
        } else {
            rect.y = hitPoint.y - m_size.height/1.7;
        }
        scroll = WebPoint(rect.x, rect.y);
    } else {
        LOG(INFO) << "WebViewImpl::Doubletap works blockzoom";
        // Scroll to vertically align the block.
        if (rect.height < screenHeight) {
            if (scale) {
                // Samsung UX behavior: Zoom-in content at the same y-axis position.
                rect.y -= rect.y * pageScaleFactor() / scale;
            } else {
                // Vertically center short blocks.
                rect.y -= 0.5 * (screenHeight - rect.height);
            }
        } else {
            // Ensure position we're zooming to (+ padding) isn't off the bottom of
            // the screen.
            rect.y = max<float>(rect.y, hitPoint.y + padding - screenHeight);
        } // Otherwise top align the block.

        // Do the same thing for horizontal alignment.
        if (rect.width < screenWidth) {
            // Samsung UX behavior: Move the content to the screen left.
            rect.x -= 0.01 * (screenWidth - rect.width);
        }
        else
            rect.x = max<float>(rect.x, hitPoint.x + padding - screenWidth);
        scroll.x = rect.x;
        scroll.y = rect.y;
    }
    scale = clampPageScaleFactorToLimits(scale);
    scroll = mainFrameImpl()->frameView()->windowToContents(scroll);
    if (rect.height < screenHeight) {
        scroll = clampOffsetAtScale(scroll, scale);
    }
}

static bool invokesHandCursor(Node* node, bool shiftKey, Frame* frame)
{
    if (!node || !node->renderer())
        return false;

    ECursor cursor = node->renderer()->style()->cursor();
    return cursor == CURSOR_POINTER
        || (cursor == CURSOR_AUTO && frame->eventHandler().useHandCursor(node, node->isLink(), shiftKey));
}

Node* WebViewImpl::bestTapNode(const PlatformGestureEvent& tapEvent)
{
    if (!m_page || !m_page->mainFrame())
        return 0;

    Node* bestTouchNode = 0;

    IntPoint touchEventLocation(tapEvent.position());
    m_page->mainFrame()->eventHandler().adjustGesturePosition(tapEvent, touchEventLocation);

    IntPoint hitTestPoint = m_page->mainFrame()->view()->windowToContents(touchEventLocation);
    HitTestResult result = m_page->mainFrame()->eventHandler().hitTestResultAtPoint(hitTestPoint, HitTestRequest::TouchEvent | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent);
    bestTouchNode = result.targetNode();

    // We might hit something like an image map that has no renderer on it
    // Walk up the tree until we have a node with an attached renderer
    while (bestTouchNode && !bestTouchNode->renderer())
        bestTouchNode = bestTouchNode->parentNode();

    // Check if we're in the subtree of a node with a hand cursor
    // this is the heuristic we use to determine if we show a highlight on tap
    while (bestTouchNode && !invokesHandCursor(bestTouchNode, false, m_page->mainFrame()))
        bestTouchNode = bestTouchNode->parentNode();

    if (!bestTouchNode)
        return 0;

    // We should pick the largest enclosing node with hand cursor set.
    while (bestTouchNode->parentNode() && invokesHandCursor(bestTouchNode->parentNode(), false, m_page->mainFrame()))
        bestTouchNode = bestTouchNode->parentNode();

    return bestTouchNode;
}

//P140427-00252 Draw highlight when user touches email address
void WebViewImpl::enableContentHighlight(WebCore::Node* touchNode)
{
    m_linkHighlights.clear();
    if (!touchNode || !touchNode->renderer() || !touchNode->renderer()->enclosingLayer())
        return;
	
    Color highlightColor = touchNode->renderer()->style()->tapHighlightColor();
    if (!highlightColor.alpha())
        return;
	
    m_linkHighlights.append(LinkHighlightHover::create(touchNode, this));
}

void WebViewImpl::enableHoverHighlight(const PlatformGestureEvent& tapEvent)
{
    Node* touchNode = bestTapNode(tapEvent);

    if (touchNode && m_prevHoverNode == touchNode)
        return;

    // Always clear any existing highlight when this is invoked, even if we don't get a new target to highlight.
    m_linkHighlights.clear();
    m_prevHoverNode = 0;

    if (!touchNode || !touchNode->renderer() || !touchNode->renderer()->enclosingLayer())
        return;

    Color highlightColor = touchNode->renderer()->style()->tapHighlightColor();
    // Safari documentation for -webkit-tap-highlight-color says if the specified color has 0 alpha,
    // then tap highlighting is disabled.
    // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
    if (!highlightColor.alpha())
        return;

    m_prevHoverNode = touchNode;

    m_linkHighlights.append(LinkHighlightHover::create(touchNode, this));
}

void WebViewImpl::enableTapHighlightAtPoint(const PlatformGestureEvent& tapEvent)
{
    Node* touchNode = bestTapNode(tapEvent);

    Vector<Node*> highlightNodes;
    highlightNodes.append(touchNode);

    enableTapHighlights(highlightNodes);
}

void WebViewImpl::enableTapHighlights(Vector<Node*>& highlightNodes)
{
    // Always clear any existing highlight when this is invoked, even if we
    // don't get a new target to highlight.
    m_linkHighlights.clear();

    for (size_t i = 0; i < highlightNodes.size(); ++i) {
        Node* node = highlightNodes[i];

        if (!node || !node->renderer() || !node->renderer()->enclosingLayer())
            continue;

        Color highlightColor = node->renderer()->style()->tapHighlightColor();
        // Safari documentation for -webkit-tap-highlight-color says if the specified color has 0 alpha,
        // then tap highlighting is disabled.
        // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
        if (!highlightColor.alpha())
            continue;

        m_linkHighlights.append(LinkHighlight::create(node, this));
    }
}

void WebViewImpl::animateDoubleTapZoom(const IntPoint& point)
{
    if (!mainFrameImpl())
        return;

    WebRect rect(point.x(), point.y(), touchPointPadding, touchPointPadding);
    WebRect blockBounds = computeBlockBounds(rect, false);

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(point, blockBounds, touchPointPadding, minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio, scale, scroll);

    bool stillAtPreviousDoubleTapScale = (pageScaleFactor() == m_doubleTapZoomPageScaleFactor
        && m_doubleTapZoomPageScaleFactor != minimumPageScaleFactor())
        || m_doubleTapZoomPending;

    bool scaleUnchanged = fabs(pageScaleFactor() - scale) < minScaleDifference;
    bool shouldZoomOut = blockBounds.isEmpty() || scaleUnchanged || stillAtPreviousDoubleTapScale;

    // Samsung UX behavior: When content is already in Zoom-in state,
    // zoom-out completely to lowest possible scale (0.25 by default).
    // Two conditions which help to determine we are in zoom-in state -
    // 1. Current scale factor > legibleScale (default of value 1)
    // 2. Current scale factor != last set doubleTapZoomScaleFactor,
    // implying zoom-in was not done on previous Double Tap action.
    bool shouldZoomOutToMinimumScale = pageScaleFactor() > legibleScale()
        && pageScaleFactor() != m_doubleTapZoomPageScaleFactor;
    shouldZoomOut |= shouldZoomOutToMinimumScale;

    bool isAnimating;

    if (shouldZoomOut) {
        scale = minimumPageScaleFactor();
        isAnimating = startPageScaleAnimation(mainFrameImpl()->frameView()->windowToContents(point), true, scale, doubleTapZoomAnimationDurationInSeconds);
    } else {
        isAnimating = startPageScaleAnimation(scroll, false, scale, doubleTapZoomAnimationDurationInSeconds);
    }

    if (isAnimating) {
        m_doubleTapZoomPageScaleFactor = scale;
        m_doubleTapZoomPending = true;
    }
}

void WebViewImpl::zoomToFindInPageRect(const WebRect& rect)
{
    if (!mainFrameImpl())
        return;

    WebRect blockBounds = computeBlockBounds(rect, true);

    if (blockBounds.isEmpty()) {
        // Keep current scale (no need to scroll as x,y will normally already
        // be visible). FIXME: Revisit this if it isn't always true.
        return;
    }

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(WebPoint(rect.x, rect.y), blockBounds, nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

    startPageScaleAnimation(scroll, false, scale, findInPageAnimationDurationInSeconds);
}

bool WebViewImpl::zoomToMultipleTargetsRect(const WebRect& rect)
{
    if (!mainFrameImpl())
        return false;

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(WebPoint(rect.x, rect.y), rect, nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

    if (scale <= pageScaleFactor())
        return false;

    startPageScaleAnimation(scroll, false, scale, multipleTargetsZoomAnimationDurationInSeconds);
    return true;
}

void WebViewImpl::numberOfWheelEventHandlersChanged(unsigned numberOfWheelHandlers)
{
    if (m_client)
        m_client->numberOfWheelEventHandlersChanged(numberOfWheelHandlers);
}

void WebViewImpl::hasTouchEventHandlers(bool hasTouchHandlers)
{
    if (m_client)
        m_client->hasTouchEventHandlers(hasTouchHandlers);
}

bool WebViewImpl::hasTouchEventHandlersAt(const WebPoint& point)
{
    // FIXME: Implement this. Note that the point must be divided by pageScaleFactor.
    return true;
}

#if !OS(MACOSX)
// Mac has no way to open a context menu based on a keyboard event.
bool WebViewImpl::sendContextMenuEvent(const WebKeyboardEvent& event)
{
    // The contextMenuController() holds onto the last context menu that was
    // popped up on the page until a new one is created. We need to clear
    // this menu before propagating the event through the DOM so that we can
    // detect if we create a new menu for this event, since we won't create
    // a new menu if the DOM swallows the event and the defaultEventHandler does
    // not run.
    page()->contextMenuController().clearContextMenu();

    m_contextMenuAllowed = true;
    Frame* focusedFrame = page()->focusController().focusedOrMainFrame();
    bool handled = focusedFrame->eventHandler().sendContextMenuEventForKey();
    m_contextMenuAllowed = false;
    return handled;
}
#endif

bool WebViewImpl::keyEventDefault(const WebKeyboardEvent& event)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    switch (event.type) {
    case WebInputEvent::Char:
        if (event.windowsKeyCode == VKEY_SPACE) {
            int keyCode = ((event.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
            return scrollViewWithKeyboard(keyCode, event.modifiers);
        }
        break;
    case WebInputEvent::RawKeyDown:
        if (event.modifiers == WebInputEvent::ControlKey) {
            switch (event.windowsKeyCode) {
#if !OS(MACOSX)
            case 'A':
                focusedFrame()->executeCommand(WebString::fromUTF8("SelectAll"));
                return true;
            case VKEY_INSERT:
            case 'C':
                focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
                return true;
#endif
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return false;
            }
        }
        if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
            return scrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
        break;
    default:
        break;
    }
    return false;
}

bool WebViewImpl::scrollViewWithKeyboard(int keyCode, int modifiers)
{
    ScrollDirection scrollDirection;
    ScrollGranularity scrollGranularity;
#if OS(MACOSX)
    // Control-Up/Down should be PageUp/Down on Mac.
    if (modifiers & WebMouseEvent::ControlKey) {
      if (keyCode == VKEY_UP)
        keyCode = VKEY_PRIOR;
      else if (keyCode == VKEY_DOWN)
        keyCode = VKEY_NEXT;
    }
#endif
    if (!mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity))
        return false;
    return bubblingScroll(scrollDirection, scrollGranularity);
}

bool WebViewImpl::mapKeyCodeForScroll(int keyCode,
                                      WebCore::ScrollDirection* scrollDirection,
                                      WebCore::ScrollGranularity* scrollGranularity)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *scrollDirection = ScrollLeft;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        *scrollDirection = ScrollRight;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR:  // page up
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT:  // page down
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return true;
}

void WebViewImpl::hideSelectPopup()
{
    if (m_selectPopup)
        m_selectPopup->hidePopup();
}

bool WebViewImpl::bubblingScroll(ScrollDirection scrollDirection, ScrollGranularity scrollGranularity)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;

    return frame->eventHandler().bubblingScroll(scrollDirection, scrollGranularity);
}

void WebViewImpl::popupOpened(PopupContainer* popupContainer)
{
    ASSERT(!m_selectPopup);
    m_selectPopup = popupContainer;
    Document* document = mainFrameImpl()->frame()->document();
    WheelController::from(document)->didAddWheelEventHandler(document);
}

void WebViewImpl::popupClosed(PopupContainer* popupContainer)
{
    ASSERT(m_selectPopup);
    m_selectPopup = 0;
    Document* document = mainFrameImpl()->frame()->document();
    WheelController::from(document)->didRemoveWheelEventHandler(document);
}

PagePopup* WebViewImpl::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    ASSERT(client);
    if (hasOpenedPopup())
        hidePopups();
    ASSERT(!m_pagePopup);

    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypePage);
    ASSERT(popupWidget);
    m_pagePopup = toWebPagePopupImpl(popupWidget);
    if (!m_pagePopup->initialize(this, client, originBoundsInRootView)) {
        m_pagePopup->closePopup();
        m_pagePopup = 0;
    }
    return m_pagePopup.get();
}

void WebViewImpl::closePagePopup(PagePopup* popup)
{
    ASSERT(popup);
    WebPagePopupImpl* popupImpl = toWebPagePopupImpl(popup);
    ASSERT(m_pagePopup.get() == popupImpl);
    if (m_pagePopup.get() != popupImpl)
        return;
    m_pagePopup->closePopup();
    m_pagePopup = 0;
}

WebHelperPlugin* WebViewImpl::createHelperPlugin(const WebString& pluginType, const WebDocument& hostDocument)
{
    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypeHelperPlugin);
    ASSERT(popupWidget);
    WebHelperPluginImpl* helperPlugin = toWebHelperPluginImpl(popupWidget);

    if (!helperPlugin->initialize(pluginType, hostDocument, this)) {
        closeAndDeleteHelperPluginSoon(helperPlugin);
        return 0;
    }

    return helperPlugin;
}

void WebViewImpl::closeAndDeleteHelperPluginSoon(WebHelperPluginImpl* helperPlugin)
{
    m_helperPluginsPendingClose.append(helperPlugin);
    if (!m_helperPluginCloseTimer.isActive())
        m_helperPluginCloseTimer.startOneShot(0);
}

void WebViewImpl::closePendingHelperPlugins(Timer<WebViewImpl>* timer)
{
    ASSERT_UNUSED(timer, !timer || timer == &m_helperPluginCloseTimer);
    ASSERT(!m_helperPluginsPendingClose.isEmpty());

    Vector<WebHelperPluginImpl*> helperPlugins;
    helperPlugins.swap(m_helperPluginsPendingClose);
    for (Vector<WebHelperPluginImpl*>::iterator it = helperPlugins.begin();
        it != helperPlugins.end(); ++it) {
        (*it)->closeAndDelete();
    }
    ASSERT(m_helperPluginsPendingClose.isEmpty());
}

Frame* WebViewImpl::focusedWebCoreFrame() const
{
    return m_page ? m_page->focusController().focusedOrMainFrame() : 0;
}

WebViewImpl* WebViewImpl::fromPage(Page* page)
{
    if (!page)
        return 0;
    return static_cast<WebViewImpl*>(page->chrome().client().webView());
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close()
{
    if (m_page) {
        // Initiate shutdown for the entire frameset.  This will cause a lot of
        // notifications to be sent.
        if (m_page->mainFrame())
            m_page->mainFrame()->loader().frameDetached();

        m_page.clear();
    }

    // Should happen after m_page.clear().
    if (m_devToolsAgent)
        m_devToolsAgent.clear();

    // Helper Plugins must be closed now since doing so accesses RenderViewImpl,
    // which will be destroyed after this function returns.
    if (m_helperPluginCloseTimer.isActive()) {
        m_helperPluginCloseTimer.stop();
        closePendingHelperPlugins(0);
    }

    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = 0;

    deref();  // Balances ref() acquired in WebView::create
}

void WebViewImpl::willStartLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willStartLiveResize();

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willStartLiveResize();
}

WebSize WebViewImpl::size()
{
    return m_size;
}

void WebViewImpl::resize(const WebSize& newSize)
{
    TRACE_EVENT0("webkit", "WebViewImpl::resize");
    LOG(INFO)<<"[SBRCHECK_ROTATE] WebViewImpl::resize: START :: newSize ::"<<newSize.width<<","<<newSize.height<<":old size = :"<<m_size.width<<","<<m_size.height;
    if (m_shouldAutoResize || m_size == newSize)
        return;

    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return;
#if defined(S_PLM_P140903_00631)
    Frame* frame = mainFrameImpl()->frame();
    if (frame && frame->document()) {
        KURL baseURL = frame->document()->baseURI();
        String issueurl("https://www.yahoo.com/movies/showtimes");
        if (!baseURL.isEmpty() && (issueurl == baseURL)) {
            Element* element = focusedElement();
            if (element && element->hasTagName(HTMLNames::selectTag)) {
                if(m_size.width == newSize.width && (newSize.height < m_size.height)){
                   LOG(INFO)<<"[SBRCHECK_ROTATE] WebViewImpl::resize: focused node is <select tag>  so return";
                   return;	
                }
            }
        }
     }
#endif

	#if defined(SBROWSER_PRINT_PAINT_LOG)
    if(m_page)
		m_page->setShouldPrintPaintLog(true);
	#endif
	
    WebSize oldSize = m_size;
    float oldPageScaleFactor = pageScaleFactor();
    int oldContentsWidth = contentsSize().width();

    m_size = newSize;

    bool shouldAnchorAndRescaleViewport = settings()->mainFrameResizesAreOrientationChanges()
        && oldSize.width && oldContentsWidth && newSize.width != oldSize.width;

    ViewportAnchor viewportAnchor(&mainFrameImpl()->frame()->eventHandler());
    if (shouldAnchorAndRescaleViewport) {
        viewportAnchor.setAnchor(view->visibleContentRect(),
                                 FloatSize(viewportAnchorXCoord, viewportAnchorYCoord));
    }

    updatePageDefinedViewportConstraints(mainFrameImpl()->frame()->document()->viewportDescription());
    updateMainFrameLayoutSize();

    WebDevToolsAgentPrivate* agentPrivate = devToolsAgentPrivate();
    if (agentPrivate)
        agentPrivate->webViewResized(newSize);
    WebFrameImpl* webFrame = mainFrameImpl();
    if (webFrame->frameView()) {
        webFrame->frameView()->resize(m_size);
        if (m_pinchViewports)
            m_pinchViewports->setViewportSize(m_size);
    }
    LOG(INFO)<<"[SBRCHECK_ROTATE]WebViewImpl::resize :: CONTENT SIZE :: "<<view->contentsSize().width()<<","<<view->contentsSize().height();
    if (settings()->viewportEnabled() && !m_fixedLayoutSizeLock) {
        // Relayout immediately to recalculate the minimum scale limit.
        if (view->needsLayout()){
			LOG(INFO)<<"[SBRCHECK_ROTATE]WebViewImpl::resize :: needsLayout new size : "<<m_size.width<<","<<m_size.height;
            view->layout();
        	}

        if (shouldAnchorAndRescaleViewport) {
            float viewportWidthRatio = static_cast<float>(newSize.width) / oldSize.width;
            float contentsWidthRatio = static_cast<float>(contentsSize().width()) / oldContentsWidth;
            float scaleMultiplier = viewportWidthRatio / contentsWidthRatio;

            IntSize viewportSize = view->visibleContentRect().size();
            if (scaleMultiplier != 1) {
                float newPageScaleFactor = oldPageScaleFactor * scaleMultiplier;
                viewportSize.scale(pageScaleFactor() / newPageScaleFactor);
                IntPoint scrollOffsetAtNewScale = viewportAnchor.computeOrigin(viewportSize);
                setPageScaleFactor(newPageScaleFactor, scrollOffsetAtNewScale);
            } else {
                IntPoint scrollOffsetAtNewScale = clampOffsetAtScale(viewportAnchor.computeOrigin(viewportSize), pageScaleFactor());
                updateMainFrameScrollPosition(scrollOffsetAtNewScale, false);
            }
        }
    }
    sendResizeEventAndRepaint();
	LOG(INFO)<<"[SBRCHECK_ROTATE]WebViewImpl::resize :: CONTENT SIZE after LAYOUT :: "<<view->contentsSize().width()<<","<<view->contentsSize().height();
    LOG(INFO)<<"[SBRCHECK_ROTATE]WebViewImpl::resize :: LAYOUT SIZE after LAYOUT :: "<<view->layoutSize().width()<<","<<view->layoutSize().height();
    
    LOG(INFO)<<"[SBRCHECK_ROTATE] WebViewImpl::resize: END";
}

void WebViewImpl::willEndLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willEndLiveResize();

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willEndLiveResize();
}

void WebViewImpl::willEnterFullScreen()
{
    m_fullscreenController->willEnterFullScreen();
}

void WebViewImpl::didEnterFullScreen()
{
    m_fullscreenController->didEnterFullScreen();
}

void WebViewImpl::willExitFullScreen()
{
    m_fullscreenController->willExitFullScreen();
}

void WebViewImpl::didExitFullScreen()
{
    m_fullscreenController->didExitFullScreen();
}

void WebViewImpl::animate(double monotonicFrameBeginTime)
{
    TRACE_EVENT0("webkit", "WebViewImpl::animate");

    if (!monotonicFrameBeginTime)
        monotonicFrameBeginTime = monotonicallyIncreasingTime();

    // Create synthetic wheel events as necessary for fling.
    if (m_gestureAnimation) {
        if (m_gestureAnimation->animate(monotonicFrameBeginTime))
            scheduleAnimation();
        else {
            endActiveFlingAnimation();

            PlatformGestureEvent endScrollEvent(PlatformEvent::GestureScrollEnd,
                m_positionOnFlingStart, m_globalPositionOnFlingStart,
                IntSize(), 0, false, false, false, false,
                0, 0, 0, 0);

            mainFrameImpl()->frame()->eventHandler().handleGestureScrollEnd(endScrollEvent);
        }
    }

    if (!m_page)
        return;

    PageWidgetDelegate::animate(m_page.get(), monotonicFrameBeginTime);

    if (m_continuousPaintingEnabled) {
        ContinuousPainter::setNeedsDisplayRecursive(m_rootGraphicsLayer, m_pageOverlays.get());
        m_client->scheduleAnimation();
    }
}

void WebViewImpl::layout()
{
    TRACE_EVENT0("webkit", "WebViewImpl::layout");
    PageWidgetDelegate::layout(m_page.get());
    updateLayerTreeBackgroundColor();

    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
        m_linkHighlights[i]->updateGeometry();
}

void WebViewImpl::enterForceCompositingMode(bool enter)
{
    if (page()->settings().forceCompositingMode() == enter)
        return;

    TRACE_EVENT1("webkit", "WebViewImpl::enterForceCompositingMode", "enter", enter);
    settingsImpl()->setForceCompositingMode(enter);
    if (enter) {
        if (!m_page)
            return;
        Frame* mainFrame = m_page->mainFrame();
        if (!mainFrame)
            return;
        mainFrame->view()->updateCompositingLayersAfterStyleChange();
    }
}

void WebViewImpl::doPixelReadbackToCanvas(WebCanvas* canvas, const IntRect& rect)
{
    ASSERT(m_layerTreeView);

    SkBitmap target;
    target.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height(), rect.width() * 4);
    target.allocPixels();
    m_layerTreeView->compositeAndReadback(target.getPixels(), rect);
#if (!SK_R32_SHIFT && SK_B32_SHIFT == 16)
    // The compositor readback always gives back pixels in BGRA order, but for
    // example Android's Skia uses RGBA ordering so the red and blue channels
    // need to be swapped.
    uint8_t* pixels = reinterpret_cast<uint8_t*>(target.getPixels());
    for (size_t i = 0; i < target.getSize(); i += 4)
        std::swap(pixels[i], pixels[i + 2]);
#endif
    canvas->writePixels(target, rect.x(), rect.y());
}

void WebViewImpl::paintSoftBitmapRootImage(WebCanvas* canvas, const WebRect& rect)
{
    FrameView* view = page()->mainFrame()->view();
    if (!view)
        return;

    PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);
}

void WebViewImpl::paintSoftBitmap(WebCanvas* canvas, const WebRect& rect)
{
#if defined (SBROWSER_SOFTBITMAP_IMPL)
    WebFrameImpl* webframe = mainFrameImpl();
    if (webframe) {
        if(isAcceleratedCompositingActive()) {
            FrameView* view = page()->mainFrame()->view();
            if(view){
                // retain the old behavior.
                PaintBehavior oldPaintBehavior = view->paintBehavior();
                //If h/w rendering is active, disable it
                if (isAcceleratedCompositingActive()) {
                    view->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);
                }
                if (view->needsLayout()) {
                    view->layout();
                }
                webframe->paintSoftBitmap(canvas,rect);
                //After painting enable again
                if (isAcceleratedCompositingActive()) {
                    view->setPaintBehavior(oldPaintBehavior);
                }
            }
        } else
            webframe->paintSoftBitmap(canvas,rect);
    }
#else
    FrameView* view = page()->mainFrame()->view();
    if (!view)
        return;

    PaintBehavior oldPaintBehavior = view->paintBehavior();
    if (isAcceleratedCompositingActive())
        view->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);

    PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);
    if (isAcceleratedCompositingActive())
        view->setPaintBehavior(oldPaintBehavior);
#endif
}
#if defined (S_PLM_P140507_05160)
bool WebViewImpl::hasWebGLOr2DCanvasContent() const
{
   if(compositor() && compositor()->hasWebGLOr2DCanvasContent())
       return true;
 
   return false;	
}
#endif

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect, PaintOptions option)
{
#if !OS(ANDROID)
    // ReadbackFromCompositorIfAvailable is the only option available on non-Android.
    // Ideally, Android would always use ReadbackFromCompositorIfAvailable as well.
    ASSERT(option == ReadbackFromCompositorIfAvailable);
#endif

    if (option == ReadbackFromCompositorIfAvailable && isAcceleratedCompositingActive()) {
        // If a canvas was passed in, we use it to grab a copy of the
        // freshly-rendered pixels.
        if (canvas) {
            // Clip rect to the confines of the rootLayerTexture.
            IntRect resizeRect(rect);
            resizeRect.intersect(IntRect(IntPoint(0, 0), m_layerTreeView->deviceViewportSize()));
            doPixelReadbackToCanvas(canvas, resizeRect);
        }
    } else {
        FrameView* view = page()->mainFrame()->view();
        PaintBehavior oldPaintBehavior = view->paintBehavior();
        if (isAcceleratedCompositingActive()) {
            ASSERT(option == ForceSoftwareRenderingAndIgnoreGPUResidentContent);
            view->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);
        }

        double paintStart = currentTime();
        PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);
        double paintEnd = currentTime();
        double pixelsPerSec = (rect.width * rect.height) / (paintEnd - paintStart);
        blink::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
        blink::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);

        if (isAcceleratedCompositingActive()) {
            ASSERT(option == ForceSoftwareRenderingAndIgnoreGPUResidentContent);
            view->setPaintBehavior(oldPaintBehavior);
        }
    }
}

bool WebViewImpl::isTrackingRepaints() const
{
    if (!page())
        return false;
    FrameView* view = page()->mainFrame()->view();
    return view->isTrackingRepaints();
}

void WebViewImpl::themeChanged()
{
    if (!page())
        return;
    FrameView* view = page()->mainFrame()->view();

    WebRect damagedRect(0, 0, m_size.width, m_size.height);
    view->invalidateRect(damagedRect);
}

void WebViewImpl::enterFullScreenForElement(WebCore::Element* element)
{
    m_fullscreenController->enterFullScreenForElement(element);
}

void WebViewImpl::exitFullScreenForElement(WebCore::Element* element)
{
    m_fullscreenController->exitFullScreenForElement(element);
}

bool WebViewImpl::hasHorizontalScrollbar()
{
    return mainFrameImpl()->frameView()->horizontalScrollbar();
}

bool WebViewImpl::hasVerticalScrollbar()
{
    return mainFrameImpl()->frameView()->verticalScrollbar();
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = 0;

bool WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    TRACE_EVENT0("input", "WebViewImpl::handleInputEvent");
	LOG(INFO)<<"Samsung :: WebViewImpl.cpp :: handleInputEvent :: event received"<<inputEvent.type;
    // If we've started a drag and drop operation, ignore input events until
    // we're done.
    if (m_doingDragAndDrop)
        return true;

    if (m_devToolsAgent && m_devToolsAgent->handleInputEvent(m_page.get(), inputEvent))
        return true;

    // Report the event to be NOT processed by WebKit, so that the browser can handle it appropriately.
    if (m_ignoreInputEvents)
        return false;

    TemporaryChange<const WebInputEvent*> currentEventChange(m_currentInputEvent, &inputEvent);

    if (isPointerLocked() && WebInputEvent::isMouseEventType(inputEvent.type)) {
      pointerLockMouseEvent(inputEvent);
      return true;
    }

#if defined(S_INTUITIVE_HOVER) && !defined(S_UNITTEST_SUPPORT)
    if (inputEvent.type == WebInputEvent::MouseMove) {
        performHitTestOnHover(*static_cast<const WebMouseEvent*>(&inputEvent));
    }
#endif

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
        TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type);
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        RefPtr<Node> node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        AtomicString eventType;
        switch (inputEvent.type) {
        case WebInputEvent::MouseMove:
            eventType = EventTypeNames::mousemove;
            break;
        case WebInputEvent::MouseLeave:
            eventType = EventTypeNames::mouseout;
            break;
        case WebInputEvent::MouseDown:
            eventType = EventTypeNames::mousedown;
            break;
        case WebInputEvent::MouseUp:
            eventType = EventTypeNames::mouseup;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        node->dispatchMouseEvent(
              PlatformMouseEventBuilder(mainFrameImpl()->frameView(), *static_cast<const WebMouseEvent*>(&inputEvent)),
              eventType, static_cast<const WebMouseEvent*>(&inputEvent)->clickCount);
        return true;
    }

    return PageWidgetDelegate::handleInputEvent(m_page.get(), *this, inputEvent);
}

void WebViewImpl::setCursorVisibilityState(bool isVisible)
{
    if (m_page)
        m_page->setIsCursorVisible(isVisible);
}

void WebViewImpl::mouseCaptureLost()
{
    TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
    m_mouseCaptureNode = 0;
}

void WebViewImpl::setFocus(bool enable)
{
    m_page->focusController().setFocused(enable);
    if (enable) {
        m_page->focusController().setActive(true);
        RefPtr<Frame> focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame) {
            Element* element = focusedFrame->document()->focusedElement();
            if (element && focusedFrame->selection().selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                if (element->isTextFormControl()) {
                    element->updateFocusAppearance(true);
                } else if (element->isContentEditable()) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(element, 0, Position::PositionIsOffsetInAnchor);
                    focusedFrame->selection().setSelection(VisibleSelection(position, SEL_DEFAULT_AFFINITY));
                }
            }
        }
        m_imeAcceptEvents = true;
    } else {
        hidePopups();

        // Clear focus on the currently focused frame if any.
        if (!m_page)
            return;

        Frame* frame = m_page->mainFrame();
        if (!frame)
            return;

        RefPtr<Frame> focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame) {
            // Finish an ongoing composition to delete the composition node.
            if (focusedFrame->inputMethodController().hasComposition()) {
                if (m_autofillClient)
                    m_autofillClient->setIgnoreTextChanges(true);

                focusedFrame->inputMethodController().confirmComposition();

                if (m_autofillClient)
                    m_autofillClient->setIgnoreTextChanges(false);
            }
            m_imeAcceptEvents = false;
        }
    }
}

bool WebViewImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
#if defined(S_AUTOFILL_SHOW_FIX)
    Element* element = focusedElement();
    if (element && element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = toHTMLInputElement(element);
        input->setIsCompositionChange(false);
    }
#endif 
#if 0//defined(S_SCROLL_EVENT)
    bool confirm=false;
#endif
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;

    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported(focused))
        return plugin->setComposition(text, underlines, selectionStart, selectionEnd);

    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    InputMethodController& inputMethodController = focused->inputMethodController();
    if (!focused->editor().canEdit() && !inputMethodController.hasComposition())
        return false;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    RefPtr<Range> range = inputMethodController.compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            return false;
    }

    // If we're not going to fire a keypress event, then the keydown event was
    // canceled.  In that case, cancel any existing composition.
    if (text.isEmpty() || m_suppressNextKeypressEvent) {
        // A browser process sent an IPC message which does not contain a valid
        // string, which means an ongoing composition has been canceled.
        // If the ongoing composition has been canceled, replace the ongoing
        // composition string with an empty string and complete it.
        String emptyString;
        Vector<CompositionUnderline> emptyUnderlines;
        inputMethodController.setComposition(emptyString, emptyUnderlines, 0, 0);
        return text.isEmpty();
    }

    // When the range of composition underlines overlap with the range between
    // selectionStart and selectionEnd, WebKit somehow won't paint the selection
    // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
    // But the selection range actually takes effect.
    inputMethodController.setComposition(String(text),
                           CompositionUnderlineVectorBuilder(underlines),
                           selectionStart, selectionEnd);
#if 0//defined(S_SCROLL_EVENT)
    confirm=inputMethodController.hasComposition();
    TextFieldsBoundsChanged();
    return confirm;
#else
    return inputMethodController.hasComposition();
#endif   
}

bool WebViewImpl::confirmComposition()
{
    return confirmComposition(DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(ConfirmCompositionBehavior selectionBehavior)
{
    return confirmComposition(WebString(), selectionBehavior);
}

bool WebViewImpl::confirmComposition(const WebString& text)
{
    return confirmComposition(text, DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(const WebString& text, ConfirmCompositionBehavior selectionBehavior)
{
	LOG(INFO)<<"Samsung :: WebViewImpl.cpp :: confirmComposition :: CALLED";    

#if defined(S_AUTOFILL_SHOW_FIX)
    Element* element = focusedElement();
    if (element && element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = toHTMLInputElement(element);
        input->setIsCompositionChange(text.isEmpty());
    }
#endif
#if 0//defined(S_SCROLL_EVENT)
    bool confirm=false;
#endif	
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;
#if defined(S_PLM_P140812_00507)
    if((focused->selection().isInPasswordField()) && (focused->inputMethodController().hasComposition())) {
        focused->inputMethodController().cancelComposition();
	 LOG(INFO)<<"Samsung :: WebViewImpl.cpp :: confirmComposition :: isInPasswordField";	
       // return false;
    }
#endif
    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported(focused))
        return plugin->confirmComposition(text, selectionBehavior);
#if 0//defined(S_SCROLL_EVENT)
    confirm=focused->inputMethodController().confirmCompositionOrInsertText(text, selectionBehavior == KeepSelection ? InputMethodController::KeepSelection : InputMethodController::DoNotKeepSelection);
    TextFieldsBoundsChanged();
    return confirm;
#else
    return focused->inputMethodController().confirmCompositionOrInsertText(text, selectionBehavior == KeepSelection ? InputMethodController::KeepSelection : InputMethodController::DoNotKeepSelection);
#endif
}

bool WebViewImpl::compositionRange(size_t* location, size_t* length)
{
    Frame* focused = focusedWebCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;

    RefPtr<Range> range = focused->inputMethodController().compositionRange();
    if (!range)
        return false;

    Element* editable = focused->selection().rootEditableElementOrDocumentElement();
    ASSERT(editable);
    PlainTextRange plainTextRange(PlainTextRange::create(*editable, *range.get()));
    if (plainTextRange.isNull())
        return false;
    *location = plainTextRange.start();
    *length = plainTextRange.length();
    return true;
}

WebTextInputInfo WebViewImpl::textInputInfo()
{
    WebTextInputInfo info;

    info.advancedIMEOptions = m_client->advancedIMEOptions();

    Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return info;

    FrameSelection& selection = focused->selection();
    Node* node = selection.selection().rootEditableElement();
    if (!node)
        return info;

    info.inputMode = inputModeOfFocusedElement();

    info.type = textInputType();
    if (info.type == WebTextInputTypeNone)
        return info;

    if (!focused->editor().canEdit())
        return info;

    info.value = plainText(rangeOfContents(node).get());

    if (info.value.isEmpty())
        return info;

    if (RefPtr<Range> range = selection.selection().firstRange()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*node, *range.get()));
        if (plainTextRange.isNotNull()) {
            info.selectionStart = plainTextRange.start();
            info.selectionEnd = plainTextRange.end();
        }
    }

    if (RefPtr<Range> range = focused->inputMethodController().compositionRange()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*node, *range.get()));
        if (plainTextRange.isNotNull()) {
            info.compositionStart = plainTextRange.start();
            info.compositionEnd = plainTextRange.end();
        }
    }

    return info;
}

WebTextInputType WebViewImpl::textInputType()
{
    Element* element = focusedElement();
    if (!element)
        return WebTextInputTypeNone;

    if (element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = toHTMLInputElement(element);

        if (input->isDisabledOrReadOnly())
            return WebTextInputTypeNone;

        if (input->isPasswordField())
            return WebTextInputTypePassword;
        if (input->isSearchField())
            return WebTextInputTypeSearch;
        if (input->isEmailField())
            return WebTextInputTypeEmail;
        if (input->isNumberField())
            return WebTextInputTypeNumber;
        if (input->isTelephoneField())
            return WebTextInputTypeTelephone;
        if (input->isURLField())
            return WebTextInputTypeURL;
        if (input->isDateField())
            return WebTextInputTypeDate;
        if (input->isDateTimeLocalField())
            return WebTextInputTypeDateTimeLocal;
        if (input->isMonthField())
            return WebTextInputTypeMonth;
        if (input->isTimeField())
            return WebTextInputTypeTime;
        if (input->isWeekField())
            return WebTextInputTypeWeek;
        if (input->isTextField())
            return WebTextInputTypeText;

        return WebTextInputTypeNone;
    }

    if (element->hasTagName(HTMLNames::textareaTag)) {
        if (toHTMLTextAreaElement(element)->isDisabledOrReadOnly())
            return WebTextInputTypeNone;
        return WebTextInputTypeTextArea;
    }

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    if (element->isHTMLElement()) {
        if (toHTMLElement(element)->isDateTimeFieldElement())
            return WebTextInputTypeDateTimeField;
    }
#endif

    if (element->shouldUseInputMethod())
        return WebTextInputTypeContentEditable;

    return WebTextInputTypeNone;
}

#if defined (SBROWSER_DEFERS_LOADING)
void WebViewImpl::needToDeferLoading(bool defer)
{
    LOG(INFO)<<" WebViewImpl::needToDeferLoading  "<<"  defer = "<<defer;
    if(m_page) m_page->setDefersLoading(defer);
}
#endif

WebString WebViewImpl::inputModeOfFocusedElement()
{
    if (!RuntimeEnabledFeatures::inputModeAttributeEnabled())
        return WebString();

    Element* element = focusedElement();
    if (!element)
        return WebString();

    if (element->hasTagName(HTMLNames::inputTag)) {
        const HTMLInputElement* input = toHTMLInputElement(element);
        if (input->supportsInputModeAttribute())
            return input->fastGetAttribute(HTMLNames::inputmodeAttr).lower();
        return WebString();
    }
    if (element->hasTagName(HTMLNames::textareaTag)) {
        const HTMLTextAreaElement* textarea = toHTMLTextAreaElement(element);
        return textarea->fastGetAttribute(HTMLNames::inputmodeAttr).lower();
    }

    return WebString();
}

bool WebViewImpl::selectionBounds(WebRect& anchor, WebRect& focus) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();

    if (selection.isCaret()) {
        anchor = focus = selection.absoluteCaretBounds();
    } else {
        RefPtr<Range> selectedRange = selection.toNormalizedRange();
        if (!selectedRange)
            return false;
#if defined(S_PLM_P140624_00882)
	String text = frame->selectedText();
	if (""== text || text.isEmpty())
 	    return false;
#endif

#if defined (S_TEXT_SELECTION_MODIFIEDBOUNDS)
    anchor = frame->editor().firstRectForRange(selectedRange.get());
    focus =frame->editor().lastRectForRange(selectedRange.get());
#else
        RefPtr<Range> range(Range::create(selectedRange->startContainer()->document(),
            selectedRange->startContainer(),
            selectedRange->startOffset(),
            selectedRange->startContainer(),
            selectedRange->startOffset()));
        anchor = frame->editor().firstRectForRange(range.get());

        range = Range::create(selectedRange->endContainer()->document(),
            selectedRange->endContainer(),
            selectedRange->endOffset(),
            selectedRange->endContainer(),
            selectedRange->endOffset());
        focus = frame->editor().firstRectForRange(range.get());
#endif
    }

    IntRect scaledAnchor(frame->view()->contentsToWindow(anchor));
    IntRect scaledFocus(frame->view()->contentsToWindow(focus));
    scaledAnchor.scale(pageScaleFactor());
    scaledFocus.scale(pageScaleFactor());
    anchor = scaledAnchor;
    focus = scaledFocus;

    if (!selection.selection().isBaseFirst())
        std::swap(anchor, focus);
    return true;
}

#if defined(S_PLM_P140830_01765)
bool WebViewImpl::isAnchorAtImage() const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();

    if (selection.isRange()) {
        RefPtr<Range> selectedRange = selection.toNormalizedRange();
        if (!selectedRange)
            return false;
        if((selectedRange->startContainer() && selectedRange->startContainer()->hasTagName(HTMLNames::imgTag))
            || (selectedRange->firstNode() && selectedRange->firstNode()->hasTagName(HTMLNames::imgTag))){
            return true;
        }
        if((selectedRange->startContainer() && selectedRange->startContainer()->hasTagName(HTMLNames::videoTag))
            || (selectedRange->firstNode() && selectedRange->firstNode()->hasTagName(HTMLNames::videoTag))){
            return true;
        }
    }
    return false;
}
#endif

InputMethodContext* WebViewImpl::inputMethodContext()
{
    if (!m_imeAcceptEvents)
        return 0;

    Frame* focusedFrame = focusedWebCoreFrame();
    if (!focusedFrame)
        return 0;

    Element* target = focusedFrame->document()->focusedElement();
    if (target && target->hasInputMethodContext())
        return target->inputMethodContext();

    return 0;
}

WebPlugin* WebViewImpl::focusedPluginIfInputMethodSupported(Frame* frame)
{
    WebPluginContainerImpl* container = WebFrameImpl::pluginContainerFromNode(frame, WebNode(focusedElement()));
    if (container && container->supportsInputMethod())
        return container->plugin();
    return 0;
}

void WebViewImpl::didShowCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowShowEvent();
}

void WebViewImpl::didUpdateCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowUpdateEvent();
}

void WebViewImpl::didHideCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowHideEvent();
}

bool WebViewImpl::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();
    if (!selection.toNormalizedRange())
        return false;
    start = selection.start().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    end = selection.end().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    return true;
}

bool WebViewImpl::isSelectionAnchorFirst() const
{
    if (const Frame* frame = focusedWebCoreFrame())
        return frame->selection().selection().isBaseFirst();
    return false;
}

bool WebViewImpl::setEditableSelectionOffsets(int start, int end)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;
    return focused->inputMethodController().setEditableSelectionOffsets(PlainTextRange(start, end));
}

bool WebViewImpl::setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;
#if defined(S_PLM_P140812_00507)
//Composition from existing text is not needed in password fields.
    if(focused->selection().isInPasswordField())
    	{
    	LOG(INFO)<<"Samsung :: WebViewImpl.cpp :: setCompositionFromExistingText :: isInPasswordField";
        return false;
    	}	
#endif
    if (!focused->editor().canEdit())
        return false;

    InputMethodController& inputMethodController = focused->inputMethodController();
    inputMethodController.cancelComposition();

    if (compositionStart == compositionEnd)
        return true;

    inputMethodController.setCompositionFromExistingText(CompositionUnderlineVectorBuilder(underlines), compositionStart, compositionEnd);

    return true;
}

WebVector<WebCompositionUnderline> WebViewImpl::compositionUnderlines() const
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return WebVector<WebCompositionUnderline>();
    const Vector<CompositionUnderline>& underlines = focused->inputMethodController().customCompositionUnderlines();
    WebVector<WebCompositionUnderline> results(underlines.size());
    for (size_t index = 0; index < underlines.size(); ++index) {
        CompositionUnderline underline = underlines[index];
#if defined(SBROWSER_ENABLE_JPN_COMPOSING_REGION)
        results[index] = WebCompositionUnderline(underline.startOffset, underline.endOffset, 
        		static_cast<WebColor>(underline.color.rgb()), underline.thick , 
        		underline.startHighlightOffset, underline.endHighlightOffset, static_cast<WebColor>(underline.backgroundColor.rgb())
        		);
#else
        results[index] = WebCompositionUnderline(underline.startOffset, underline.endOffset, static_cast<WebColor>(underline.color.rgb()), underline.thick);
#endif
    }
    return results;
}

void WebViewImpl::extendSelectionAndDelete(int before, int after)
{
#if defined(S_AUTOFILL_SHOW_FIX)
    Element* element = focusedElement();
    if (element && element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* input = toHTMLInputElement(element);
        input->setIsCompositionChange(false);
    }
#endif 
    Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return;
    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported(focused)) {
        plugin->extendSelectionAndDelete(before, after);
        return;
    }
    focused->inputMethodController().extendSelectionAndDelete(before, after);
}

bool WebViewImpl::isSelectionEditable() const
{
    if (const Frame* frame = focusedWebCoreFrame())
        return frame->selection().isContentEditable();
    return false;
}

WebColor WebViewImpl::backgroundColor() const
{
    if (isTransparent())
        return Color::transparent;
    if (!m_page)
        return m_baseBackgroundColor;
    if (!m_page->mainFrame())
        return m_baseBackgroundColor;
    FrameView* view = m_page->mainFrame()->view();
    return view->documentBackgroundColor().rgb();
}

bool WebViewImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return false;

    PlainTextRange selectionOffsets = focused->inputMethodController().getSelectionOffsets();
    if (selectionOffsets.isNull())
        return false;

    *location = selectionOffsets.start();
    *length = selectionOffsets.length();
    return true;
}

void WebViewImpl::setTextDirection(WebTextDirection direction)
{
    // The Editor::setBaseWritingDirection() function checks if we can change
    // the text direction of the selected node and updates its DOM "dir"
    // attribute and its CSS "direction" property.
    // So, we just call the function as Safari does.
    const Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return;

    Editor& editor = focused->editor();
    if (!editor.canEdit())
        return;

    switch (direction) {
    case WebTextDirectionDefault:
        editor.setBaseWritingDirection(NaturalWritingDirection);
        break;

    case WebTextDirectionLeftToRight:
        editor.setBaseWritingDirection(LeftToRightWritingDirection);
        break;

    case WebTextDirectionRightToLeft:
        editor.setBaseWritingDirection(RightToLeftWritingDirection);
        break;

    default:
        notImplemented();
        break;
    }
}

bool WebViewImpl::isAcceleratedCompositingActive() const
{
    return m_isAcceleratedCompositingActive;
}

void WebViewImpl::willCloseLayerTreeView()
{
    setIsAcceleratedCompositingActive(false);
    m_layerTreeView = 0;
}

void WebViewImpl::didAcquirePointerLock()
{
    if (page())
        page()->pointerLockController().didAcquirePointerLock();
}

void WebViewImpl::didNotAcquirePointerLock()
{
    if (page())
        page()->pointerLockController().didNotAcquirePointerLock();
}

void WebViewImpl::didLosePointerLock()
{
    if (page())
        page()->pointerLockController().didLosePointerLock();
}

void WebViewImpl::didChangeWindowResizerRect()
{
    if (mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->windowResizerRectChanged();
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::settingsImpl()
{
    if (!m_webSettings)
        m_webSettings = adoptPtr(new WebSettingsImpl(&m_page->settings(), &m_page->inspectorController()));
    ASSERT(m_webSettings);
    return m_webSettings.get();
}

WebSettings* WebViewImpl::settings()
{
    return settingsImpl();
}

WebString WebViewImpl::pageEncoding() const
{
    if (!m_page)
        return WebString();

    // FIXME: Is this check needed?
    if (!m_page->mainFrame()->document()->loader())
        return WebString();

    return m_page->mainFrame()->document()->encodingName();
}

void WebViewImpl::setPageEncoding(const WebString& encodingName)
{
    if (!m_page)
        return;

    // Only change override encoding, don't change default encoding.
    // Note that the new encoding must be 0 if it isn't supposed to be set.
    AtomicString newEncodingName;
    if (!encodingName.isEmpty())
        newEncodingName = encodingName;
    m_page->mainFrame()->loader().reload(NormalReload, KURL(), newEncodingName);
}

bool WebViewImpl::dispatchBeforeUnloadEvent()
{
    // FIXME: This should really cause a recursive depth-first walk of all
    // frames in the tree, calling each frame's onbeforeunload.  At the moment,
    // we're consistent with Safari 3.1, not IE/FF.
    Frame* frame = m_page->mainFrame();
    if (!frame)
        return true;

    return frame->loader().shouldClose();
}

void WebViewImpl::dispatchUnloadEvent()
{
    // Run unload handlers.
    m_page->mainFrame()->loader().closeURL();
}

WebFrame* WebViewImpl::mainFrame()
{
    return mainFrameImpl();
}

WebFrame* WebViewImpl::findFrameByName(
    const WebString& name, WebFrame* relativeToFrame)
{
    if (!relativeToFrame)
        relativeToFrame = mainFrame();
    Frame* frame = toWebFrameImpl(relativeToFrame)->frame();
    frame = frame->tree().find(name);
    return WebFrameImpl::fromFrame(frame);
}

WebFrame* WebViewImpl::focusedFrame()
{
    return WebFrameImpl::fromFrame(focusedWebCoreFrame());
}

void WebViewImpl::setFocusedFrame(WebFrame* frame)
{
    if (!frame) {
        // Clears the focused frame if any.
        if (Frame* focusedFrame = focusedWebCoreFrame())
            focusedFrame->selection().setFocused(false);
        return;
    }
    Frame* webcoreFrame = toWebFrameImpl(frame)->frame();
    webcoreFrame->page()->focusController().setFocusedFrame(webcoreFrame);
}

void WebViewImpl::setInitialFocus(bool reverse)
{
    if (!m_page)
        return;
    Frame* frame = page()->focusController().focusedOrMainFrame();
    if (Document* document = frame->document())
        document->setFocusedElement(0);
    page()->focusController().setInitialFocus(reverse ? FocusTypeBackward : FocusTypeForward);
}

void WebViewImpl::clearFocusedNode()
{
    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame)
        return;

    RefPtr<Document> document = frame->document();
    if (!document)
        return;

    RefPtr<Element> oldFocusedElement = document->focusedElement();

    // Clear the focused node.
    document->setFocusedElement(0);

    if (!oldFocusedElement)
        return;

    // If a text field has focus, we need to make sure the selection controller
    // knows to remove selection from it. Otherwise, the text field is still
    // processing keyboard events even though focus has been moved to the page and
    // keystrokes get eaten as a result.
    if (oldFocusedElement->isContentEditable() || oldFocusedElement->isTextFormControl())
        frame->selection().clear();
}

void WebViewImpl::scrollFocusedNodeIntoView()
{
    if (Element* element = focusedElement())
        element->scrollIntoViewIfNeeded(true);
}

void WebViewImpl::scrollFocusedNodeIntoViewCenter()
{
    if (Element* element = focusedElement())
        element->scrollIntoViewCenter(false);
}

void WebViewImpl::scrollFocusedNodeIntoRect(const WebRect& rect)
{
    Frame* frame = page()->mainFrame();
    Element* element = focusedElement();
    if (!frame || !frame->view() || !element)
        return;

    if (!m_webSettings->autoZoomFocusedNodeToLegibleScale()) {
        frame->view()->scrollElementToRect(element, IntRect(rect.x, rect.y, rect.width, rect.height));
        return;
    }

    float scale;
    IntPoint scroll;
    bool needAnimation;
    computeScaleAndScrollForFocusedNode(element, scale, scroll, needAnimation);
    if (needAnimation)
        startPageScaleAnimation(scroll, false, scale, scrollAndScaleAnimationDurationInSeconds);
}

#if defined(S_SCROLL_EVENT)
void WebViewImpl::TextFieldsBoundsChanged()
{
    WebCore::Node* node = focusedElement();
    if(!node)
        return;

    if (!node->hasTagName(HTMLNames::inputTag) && !node->hasTagName(HTMLNames::textareaTag)
        && !node->shouldUseInputMethod())
        return;

    IntRect inputBoxRect = node->Node::pixelSnappedBoundingBox();
    inputBoxRect = node->document().view()->contentsToWindow(inputBoxRect);

    m_client->updateTextFieldBounds(WebRect(inputBoxRect.x(),inputBoxRect.y(),inputBoxRect.width(),inputBoxRect.height()));
}
#endif	
void WebViewImpl::computeScaleAndScrollForFocusedNode(Node* focusedNode, float& newScale, IntPoint& newScroll, bool& needAnimation)
{
    focusedNode->document().updateLayoutIgnorePendingStylesheets();

    // 'caret' is rect encompassing the blinking cursor.
    IntRect textboxRect = focusedNode->document().view()->contentsToWindow(pixelSnappedIntRect(focusedNode->Node::boundingBox()));
    WebRect caret, unusedEnd;
    selectionBounds(caret, unusedEnd);
    IntRect unscaledCaret = caret;
    unscaledCaret.scale(1 / pageScaleFactor());
    caret = unscaledCaret;

    // Pick a scale which is reasonably readable. This is the scale at which
    // the caret height will become minReadableCaretHeight (adjusted for dpi
    // and font scale factor).
    newScale = clampPageScaleFactorToLimits(legibleScale() * minReadableCaretHeight / caret.height);
    const float deltaScale = newScale / pageScaleFactor();

    // Convert the rects to absolute space in the new scale.
    IntRect textboxRectInDocumentCoordinates = textboxRect;
    textboxRectInDocumentCoordinates.move(mainFrame()->scrollOffset());
    IntRect caretInDocumentCoordinates = caret;
    caretInDocumentCoordinates.move(mainFrame()->scrollOffset());

#if defined(S_SCROLL_EVENT)	
    FloatRect rect1;
    if(page())
        rect1=page()->chrome().ScreenwindowRect();
		
    int viewWidth = m_size.width / newScale;
    int viewHeight = m_size.height / newScale;
    if(rect1.height()>rect1.width()){
        //For portrait mode
        viewHeight = ((rect1.height()/2.5)/newScale);
    }
    else if(rect1.height()<=rect1.width()){
        //For Landscape mode
        viewHeight = ((rect1.height()/3.5)/newScale);
    }
#else
    int viewWidth = m_size.width / newScale;
    #if defined(S_IME_SCROLL_EVENT)
    int viewHeight = (m_size.height - m_contentTopOffset) / newScale;
    #else
    int viewHeight = m_size.height / newScale;
    #endif
#endif


    if (textboxRectInDocumentCoordinates.width() <= viewWidth) {
        // Field is narrower than screen. Try to leave padding on left so field's
        // label is visible, but it's more important to ensure entire field is
        // onscreen.
        int idealLeftPadding = viewWidth * leftBoxRatio;
        int maxLeftPaddingKeepingBoxOnscreen = viewWidth - textboxRectInDocumentCoordinates.width();
        newScroll.setX(textboxRectInDocumentCoordinates.x() - min<int>(idealLeftPadding, maxLeftPaddingKeepingBoxOnscreen));
    } else {
        // Field is wider than screen. Try to left-align field, unless caret would
        // be offscreen, in which case right-align the caret.
        newScroll.setX(max<int>(textboxRectInDocumentCoordinates.x(), caretInDocumentCoordinates.x() + caretInDocumentCoordinates.width() + caretPadding - viewWidth));
    }
    if (textboxRectInDocumentCoordinates.height() <= viewHeight) {
        // Field is shorter than screen. Vertically center it.
        newScroll.setY(textboxRectInDocumentCoordinates.y() - (viewHeight - textboxRectInDocumentCoordinates.height()) / 2);
    } else {
        // Field is taller than screen. Try to top align field, unless caret would
        // be offscreen, in which case bottom-align the caret.
        newScroll.setY(max<int>(textboxRectInDocumentCoordinates.y(), caretInDocumentCoordinates.y() + caretInDocumentCoordinates.height() + caretPadding - viewHeight));
    }

    needAnimation = false;
    // If we are at less than the target zoom level, zoom in.
    if (deltaScale > minScaleChangeToTriggerZoom)
        needAnimation = true;
    // If the caret is offscreen, then animate.
    IntRect sizeRect(0, 0, viewWidth, viewHeight);
    if (!sizeRect.contains(caret))
        needAnimation = true;
    // If the box is partially offscreen and it's possible to bring it fully
    // onscreen, then animate.
    if (sizeRect.contains(textboxRectInDocumentCoordinates.width(), textboxRectInDocumentCoordinates.height()) && !sizeRect.contains(textboxRect))
        needAnimation = true;
}

void WebViewImpl::advanceFocus(bool reverse)
{
    page()->focusController().advanceFocus(reverse ? FocusTypeBackward : FocusTypeForward);
}

double WebViewImpl::zoomLevel()
{
    return m_zoomLevel;
}

double WebViewImpl::setZoomLevel(double zoomLevel)
{
    if (zoomLevel < m_minimumZoomLevel)
        m_zoomLevel = m_minimumZoomLevel;
    else if (zoomLevel > m_maximumZoomLevel)
        m_zoomLevel = m_maximumZoomLevel;
    else
        m_zoomLevel = zoomLevel;

    Frame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->plugin()->setZoomLevel(m_zoomLevel, false);
    else {
        float zoomFactor = m_zoomFactorOverride ? m_zoomFactorOverride : static_cast<float>(zoomLevelToZoomFactor(m_zoomLevel));
        frame->setPageZoomFactor(zoomFactor);
    }

    return m_zoomLevel;
}

void WebViewImpl::zoomLimitsChanged(double minimumZoomLevel,
                                    double maximumZoomLevel)
{
    m_minimumZoomLevel = minimumZoomLevel;
    m_maximumZoomLevel = maximumZoomLevel;
    m_client->zoomLimitsChanged(m_minimumZoomLevel, m_maximumZoomLevel);
}

float WebViewImpl::textZoomFactor()
{
    return mainFrameImpl()->frame()->textZoomFactor();
}

float WebViewImpl::setTextZoomFactor(float textZoomFactor)
{
    Frame* frame = mainFrameImpl()->frame();
    if (WebFrameImpl::pluginContainerFromFrame(frame))
        return 1;

    frame->setTextZoomFactor(textZoomFactor);

    return textZoomFactor;
}

void WebViewImpl::fullFramePluginZoomLevelChanged(double zoomLevel)
{
    if (zoomLevel == m_zoomLevel)
        return;

    m_zoomLevel = max(min(zoomLevel, m_maximumZoomLevel), m_minimumZoomLevel);
    m_client->zoomLevelChanged();
}

double WebView::zoomLevelToZoomFactor(double zoomLevel)
{
    return pow(textSizeMultiplierRatio, zoomLevel);
}

double WebView::zoomFactorToZoomLevel(double factor)
{
    // Since factor = 1.2^level, level = log(factor) / log(1.2)
    return log(factor) / log(textSizeMultiplierRatio);
}

float WebViewImpl::pageScaleFactor() const
{
    if (!page())
        return 1;

    return page()->pageScaleFactor();
}

float WebViewImpl::clampPageScaleFactorToLimits(float scaleFactor) const
{
    return m_pageScaleConstraintsSet.finalConstraints().clampToConstraints(scaleFactor);
}

IntPoint WebViewImpl::clampOffsetAtScale(const IntPoint& offset, float scale)
{
    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return offset;

    IntPoint maxScrollExtent(contentsSize().width() - view->scrollOrigin().x(), contentsSize().height() - view->scrollOrigin().y());
    FloatSize scaledSize = view->unscaledVisibleContentSize();
    scaledSize.scale(1 / scale);

    IntPoint clampedOffset = offset;
    clampedOffset = clampedOffset.shrunkTo(maxScrollExtent - expandedIntSize(scaledSize));
    clampedOffset = clampedOffset.expandedTo(-view->scrollOrigin());

    return clampedOffset;
}

void WebViewImpl::setPageScaleFactor(float scaleFactor, const WebPoint& origin)
{
    if (!page())
        return;

    IntPoint newScrollOffset = origin;
    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);
    newScrollOffset = clampOffsetAtScale(newScrollOffset, scaleFactor);

    page()->setPageScaleFactor(scaleFactor, newScrollOffset);
#if defined (SBROWSER_SOFTBITMAP_IMPL)
    m_pageScaleFactor = scaleFactor;
#endif
}

void WebViewImpl::setPageScaleFactorPreservingScrollOffset(float scaleFactor)
{
    if (clampPageScaleFactorToLimits(scaleFactor) == pageScaleFactor())
        return;

    IntPoint scrollOffset(mainFrame()->scrollOffset().width, mainFrame()->scrollOffset().height);
    setPageScaleFactor(scaleFactor, scrollOffset);
}

float WebViewImpl::deviceScaleFactor() const
{
    if (!page())
        return 1;

    return page()->deviceScaleFactor();
}

void WebViewImpl::setDeviceScaleFactor(float scaleFactor)
{
    if (!page())
        return;

    page()->setDeviceScaleFactor(scaleFactor);

    if (m_layerTreeView)
        updateLayerTreeDeviceScaleFactor();
}

void WebViewImpl::enableAutoResizeMode(const WebSize& minSize, const WebSize& maxSize)
{
    m_shouldAutoResize = true;
    m_minAutoSize = minSize;
    m_maxAutoSize = maxSize;
    configureAutoResizeMode();
}

void WebViewImpl::disableAutoResizeMode()
{
    m_shouldAutoResize = false;
    configureAutoResizeMode();
}

void WebViewImpl::setUserAgentPageScaleConstraints(PageScaleConstraints newConstraints)
{
    if (newConstraints == m_pageScaleConstraintsSet.userAgentConstraints())
        return;

    m_pageScaleConstraintsSet.setUserAgentConstraints(newConstraints);

    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    mainFrameImpl()->frameView()->setNeedsLayout();
}

void WebViewImpl::setInitialPageScaleOverride(float initialPageScaleFactorOverride)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    constraints.initialScale = initialPageScaleFactorOverride;

    if (constraints == m_pageScaleConstraintsSet.userAgentConstraints())
        return;

    m_pageScaleConstraintsSet.setNeedsReset(true);
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::setPageScaleFactorLimits(float minPageScale, float maxPageScale)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    constraints.minimumScale = minPageScale;
    constraints.maximumScale = maxPageScale;
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::setIgnoreViewportTagScaleLimits(bool ignore)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    if (ignore) {
        constraints.minimumScale = m_pageScaleConstraintsSet.defaultConstraints().minimumScale;
        constraints.maximumScale = m_pageScaleConstraintsSet.defaultConstraints().maximumScale;
    } else {
        constraints.minimumScale = -1;
        constraints.maximumScale = -1;
    }
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::refreshPageScaleFactorAfterLayout()
{
    if (!mainFrame() || !page() || !page()->mainFrame() || !page()->mainFrame()->view())
        return;
    FrameView* view = page()->mainFrame()->view();

    updatePageDefinedViewportConstraints(mainFrameImpl()->frame()->document()->viewportDescription());
    m_pageScaleConstraintsSet.computeFinalConstraints();

    if (settings()->viewportEnabled() && !m_fixedLayoutSizeLock) {
        int verticalScrollbarWidth = 0;
        if (view->verticalScrollbar() && !view->verticalScrollbar()->isOverlayScrollbar())
            verticalScrollbarWidth = view->verticalScrollbar()->width();
        m_pageScaleConstraintsSet.adjustFinalConstraintsToContentsSize(m_size, contentsSize(), verticalScrollbarWidth);
    }

    float newPageScaleFactor = pageScaleFactor();

    if (m_pageScaleConstraintsSet.needsReset() && m_pageScaleConstraintsSet.finalConstraints().initialScale != -1) {
        newPageScaleFactor = m_pageScaleConstraintsSet.finalConstraints().initialScale;
        m_pageScaleConstraintsSet.setNeedsReset(false);
    }
    setPageScaleFactorPreservingScrollOffset(newPageScaleFactor);

    updateLayerTreeViewport();

    // Relayout immediately to avoid violating the rule that needsLayout()
    // isn't set at the end of a layout.
    if (view->needsLayout())
        view->layout();
}

void WebViewImpl::updatePageDefinedViewportConstraints(const ViewportDescription& description)
{
    if (!settings()->viewportEnabled() || !page() || (!m_size.width && !m_size.height))
        return;

#if defined(SBROWSER_GPU_RASTERIZATION_ENABLE)
    m_matchesHeuristicsForGpuRasterization = description.maxWidth == Length(DeviceWidth)
        && description.minZoom == 1.0
        /*&& description.minZoomIsExplicit*/
        && description.zoom == 1.0
        /*&& description.zoomIsExplicit*/
        && description.userZoom
        /*&& description.userZoomIsExplicit*/;
    if (m_layerTreeView)
        m_layerTreeView->heuristicsForGpuRasterizationUpdated(m_matchesHeuristicsForGpuRasterization);
#endif
    ViewportDescription adjustedDescription = description;
    if (settingsImpl()->viewportMetaLayoutSizeQuirk() && adjustedDescription.type == ViewportDescription::ViewportMeta) {
        if (adjustedDescription.maxWidth.type() == ExtendToZoom)
            adjustedDescription.maxWidth = Length(); // auto
        const int legacyWidthSnappingMagicNumber = 320;
        if (adjustedDescription.maxWidth.isFixed() && adjustedDescription.maxWidth.value() <= legacyWidthSnappingMagicNumber)
            adjustedDescription.maxWidth = Length(DeviceWidth);
        if (adjustedDescription.maxHeight.isFixed() && adjustedDescription.maxWidth.value() <= m_size.height)
            adjustedDescription.maxHeight = Length(DeviceHeight);
        adjustedDescription.minWidth = adjustedDescription.maxWidth;
        adjustedDescription.minHeight = adjustedDescription.maxHeight;
    }
    float oldInitialScale = m_pageScaleConstraintsSet.pageDefinedConstraints().initialScale;
#if defined(S_FIT_TO_SCREEN)
    if(settingsImpl()->fitToScreenEnabled())
        adjustedDescription.minWidthEnabled= true;
#endif
    m_pageScaleConstraintsSet.updatePageDefinedConstraints(adjustedDescription, m_size);

    if (settingsImpl()->clobberUserAgentInitialScaleQuirk()
        && m_pageScaleConstraintsSet.userAgentConstraints().initialScale != -1
        && m_pageScaleConstraintsSet.userAgentConstraints().initialScale * deviceScaleFactor() <= 1) {
        if (description.maxWidth == Length(DeviceWidth)
            || (description.maxWidth.type() == ExtendToZoom && m_pageScaleConstraintsSet.pageDefinedConstraints().initialScale == 1.0f))
            setInitialPageScaleOverride(-1);
    }
    m_pageScaleConstraintsSet.adjustForAndroidWebViewQuirks(adjustedDescription, m_size, page()->settings().layoutFallbackWidth(), deviceScaleFactor(), settingsImpl()->supportDeprecatedTargetDensityDPI(), page()->settings().wideViewportQuirkEnabled(), page()->settings().useWideViewport(), page()->settings().loadWithOverviewMode(), settingsImpl()->viewportMetaNonUserScalableQuirk());
    float newInitialScale = m_pageScaleConstraintsSet.pageDefinedConstraints().initialScale;
	
#if defined(SBROWSER_OVERVIEW_MODE)
    if (oldInitialScale != newInitialScale && (newInitialScale != -1 || (page()->getOverviewToggle()==true))) {
        page()->setOverviewToggle(false);
#else
    if (oldInitialScale != newInitialScale && newInitialScale != -1) {
#endif
        m_pageScaleConstraintsSet.setNeedsReset(true);
        if (mainFrameImpl() && mainFrameImpl()->frameView())
            mainFrameImpl()->frameView()->setNeedsLayout();
    }

    updateMainFrameLayoutSize();
}

void WebViewImpl::updateMainFrameLayoutSize()
{
    if (m_fixedLayoutSizeLock || !mainFrameImpl())
        return;

    RefPtr<FrameView> view = mainFrameImpl()->frameView();
    if (!view)
        return;

    WebSize layoutSize = m_size;

    if (settings()->viewportEnabled()) {
        layoutSize = flooredIntSize(m_pageScaleConstraintsSet.pageDefinedConstraints().layoutSize);

        bool textAutosizingEnabled = page()->settings().textAutosizingEnabled();
        if (textAutosizingEnabled && layoutSize.width != view->layoutSize().width()) {
            TextAutosizer* textAutosizer = page()->mainFrame()->document()->textAutosizer();
            if (textAutosizer)
                textAutosizer->recalculateMultipliers();
        }
    }

    view->setLayoutSize(layoutSize);
}

IntSize WebViewImpl::contentsSize() const
{
    RenderView* root = page()->mainFrame()->contentRenderer();
    if (!root)
        return IntSize();
    return root->documentRect().size();
}

WebSize WebViewImpl::contentsPreferredMinimumSize()
{
    Document* document = m_page->mainFrame()->document();
    if (!document || !document->renderView() || !document->documentElement())
        return WebSize();

    layout();
    FontCachePurgePreventer fontCachePurgePreventer; // Required by minPreferredLogicalWidth().
    IntSize preferredMinimumSize(document->renderView()->minPreferredLogicalWidth(), document->documentElement()->scrollHeight());
    preferredMinimumSize.scale(zoomLevelToZoomFactor(zoomLevel()));
    return preferredMinimumSize;
}

float WebViewImpl::minimumPageScaleFactor() const
{
    return m_pageScaleConstraintsSet.finalConstraints().minimumScale;
}

float WebViewImpl::maximumPageScaleFactor() const
{
    return m_pageScaleConstraintsSet.finalConstraints().maximumScale;
}

void WebViewImpl::saveScrollAndScaleState()
{
    m_savedPageScaleFactor = pageScaleFactor();
    m_savedScrollOffset = mainFrame()->scrollOffset();
}

void WebViewImpl::restoreScrollAndScaleState()
{
    if (!m_savedPageScaleFactor)
        return;

    startPageScaleAnimation(IntPoint(m_savedScrollOffset), false, m_savedPageScaleFactor, scrollAndScaleAnimationDurationInSeconds);
    resetSavedScrollAndScaleState();
}

void WebViewImpl::resetSavedScrollAndScaleState()
{
    m_savedPageScaleFactor = 0;
    m_savedScrollOffset = IntSize();
}

void WebViewImpl::resetScrollAndScaleState()
{
    page()->setPageScaleFactor(1, IntPoint());
#if defined (SBROWSER_SOFTBITMAP_IMPL)
    m_pageScaleFactor = 1;
#endif		

    // Clear out the values for the current history item. This will prevent the history item from clobbering the
    // value determined during page scale initialization, which may be less than 1.
    page()->mainFrame()->loader().saveDocumentAndScrollState();
    page()->mainFrame()->loader().clearScrollPositionAndViewState();
    m_pageScaleConstraintsSet.setNeedsReset(true);

    // Clobber saved scales and scroll offsets.
    if (FrameView* view = page()->mainFrame()->document()->view())
        view->cacheCurrentScrollPosition();
    resetSavedScrollAndScaleState();
}

void WebViewImpl::setFixedLayoutSize(const WebSize& layoutSize)
{
    if (!page())
        return;

    Frame* frame = page()->mainFrame();
    if (!frame)
        return;

    RefPtr<FrameView> view = frame->view();
    if (!view)
        return;

    m_fixedLayoutSizeLock = layoutSize.width || layoutSize.height;

    if (m_fixedLayoutSizeLock)
        view->setLayoutSize(layoutSize);
    else
        updateMainFrameLayoutSize();
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtr<Node> node = result.innerNonSharedNode();
    if (!node->hasTagName(HTMLNames::videoTag) && !node->hasTagName(HTMLNames::audioTag))
        return;

    RefPtr<HTMLMediaElement> mediaElement =
        static_pointer_cast<HTMLMediaElement>(node);
    switch (action.type) {
    case WebMediaPlayerAction::Play:
        if (action.enable)
            mediaElement->play();
        else
            mediaElement->pause();
        break;
    case WebMediaPlayerAction::Mute:
        mediaElement->setMuted(action.enable);
        break;
    case WebMediaPlayerAction::Loop:
        mediaElement->setLoop(action.enable);
        break;
    case WebMediaPlayerAction::Controls:
        mediaElement->setControls(action.enable);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void WebViewImpl::performPluginAction(const WebPluginAction& action,
                                      const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtr<Node> node = result.innerNonSharedNode();
    if (!node->hasTagName(HTMLNames::objectTag) && !node->hasTagName(HTMLNames::embedTag))
        return;

    RenderObject* object = node->renderer();
    if (object && object->isWidget()) {
        Widget* widget = toRenderWidget(object)->widget();
        if (widget && widget->isPluginContainer()) {
            WebPluginContainerImpl* plugin = toWebPluginContainerImpl(widget);
            switch (action.type) {
            case WebPluginAction::Rotate90Clockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Clockwise);
                break;
            case WebPluginAction::Rotate90Counterclockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Counterclockwise);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }
}

WebHitTestResult WebViewImpl::hitTestResultAt(const WebPoint& point)
{
    IntPoint scaledPoint = point;
    scaledPoint.scale(1 / pageScaleFactor(), 1 / pageScaleFactor());
    return hitTestResultForWindowPos(scaledPoint);
}

void WebViewImpl::hoverHighlight(const WebGestureEvent& hoverEvent, bool high_light)
{
    if(high_light){
       PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), hoverEvent);
       if (settingsImpl()->gestureTapHighlightEnabled())
          enableHoverHighlight(platformEvent);
    }
    else
	   m_linkHighlights.clear();
	    
}

void WebViewImpl::copyImageAt(const WebPoint& point)
{
    if (!m_page)
        return;

    HitTestResult result = hitTestResultForWindowPos(point);

    if (result.absoluteImageURL().isEmpty()) {
        // There isn't actually an image at these coordinates.  Might be because
        // the window scrolled while the context menu was open or because the page
        // changed itself between when we thought there was an image here and when
        // we actually tried to retreive the image.
        //
        // FIXME: implement a cache of the most recent HitTestResult to avoid having
        //        to do two hit tests.
        return;
    }

    m_page->mainFrame()->editor().copyImage(result);
}

void WebViewImpl::dragSourceEndedAt(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
    PlatformMouseEvent pme(clientPoint,
                           screenPoint,
                           LeftButton, PlatformEvent::MouseMoved, 0, false, false, false,
                           false, 0);
    m_page->mainFrame()->eventHandler().dragSourceEndedAt(pme,
        static_cast<DragOperation>(operation));
}

void WebViewImpl::dragSourceMovedTo(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
}

void WebViewImpl::dragSourceSystemDragEnded()
{
    // It's possible for us to get this callback while not doing a drag if
    // it's from a previous page that got unloaded.
    if (m_doingDragAndDrop) {
        m_page->dragController().dragEnded();
        m_doingDragAndDrop = false;
    }
}

WebDragOperation WebViewImpl::dragTargetDragEnter(
    const WebDragData& webDragData,
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    ASSERT(!m_currentDragData);

    m_currentDragData = webDragData;
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragEnter, keyModifiers);
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragOver, keyModifiers);
}

void WebViewImpl::dragTargetDragLeave()
{
    ASSERT(m_currentDragData);

    DragData dragData(
        m_currentDragData.get(),
        IntPoint(),
        IntPoint(),
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController().dragExited(&dragData);

    // FIXME: why is the drag scroll timer not stopped here?

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = 0;
}

void WebViewImpl::dragTargetDrop(const WebPoint& clientPoint,
                                 const WebPoint& screenPoint,
                                 int keyModifiers)
{
    ASSERT(m_currentDragData);

    // If this webview transitions from the "drop accepting" state to the "not
    // accepting" state, then our IPC message reply indicating that may be in-
    // flight, or else delayed by javascript processing in this webview.  If a
    // drop happens before our IPC reply has reached the browser process, then
    // the browser forwards the drop to this webview.  So only allow a drop to
    // proceed if our webview m_dragOperation state is not DragOperationNone.

    if (m_dragOperation == WebDragOperationNone) { // IPC RACE CONDITION: do not allow this drop.
        dragTargetDragLeave();
        return;
    }

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController().performDrag(&dragData);

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = 0;
}

void WebViewImpl::spellingMarkers(WebVector<uint32_t>* markers)
{
    Vector<uint32_t> result;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        const Vector<DocumentMarker*>& documentMarkers = frame->document()->markers()->markers();
        for (size_t i = 0; i < documentMarkers.size(); ++i)
            result.append(documentMarkers[i]->hash());
    }
    markers->assign(result);
}

WebDragOperation WebViewImpl::dragTargetDragEnterOrOver(const WebPoint& clientPoint, const WebPoint& screenPoint, DragAction dragAction, int keyModifiers)
{
    ASSERT(m_currentDragData);

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    DragSession dragSession;
    if (dragAction == DragEnter)
        dragSession = m_page->dragController().dragEntered(&dragData);
    else
        dragSession = m_page->dragController().dragUpdated(&dragData);

    DragOperation dropEffect = dragSession.operation;

    // Mask the drop effect operation against the drag source's allowed operations.
    if (!(dropEffect & dragData.draggingSourceOperationMask()))
        dropEffect = DragOperationNone;

     m_dragOperation = static_cast<WebDragOperation>(dropEffect);

    return m_dragOperation;
}

void WebViewImpl::sendResizeEventAndRepaint()
{
    // FIXME: This is wrong. The FrameView is responsible sending a resizeEvent
    // as part of layout. Layout is also responsible for sending invalidations
    // to the embedder. This method and all callers may be wrong. -- eseidel.
    if (mainFrameImpl()->frameView()) {
        // Enqueues the resize event.
        mainFrameImpl()->frame()->document()->enqueueResizeEvent();
    }

    if (m_client) {
        if (isAcceleratedCompositingActive()) {
            updateLayerTreeViewport();
        } else {
            WebRect damagedRect(0, 0, m_size.width, m_size.height);
            m_client->didInvalidateRect(damagedRect);
        }
    }
    if (m_pageOverlays)
        m_pageOverlays->update();
}

void WebViewImpl::configureAutoResizeMode()
{
    if (!mainFrameImpl() || !mainFrameImpl()->frame() || !mainFrameImpl()->frame()->view())
        return;

    mainFrameImpl()->frame()->view()->enableAutoSizeMode(m_shouldAutoResize, m_minAutoSize, m_maxAutoSize);
}

unsigned long WebViewImpl::createUniqueIdentifierForRequest()
{
    return createUniqueIdentifier();
}

void WebViewImpl::inspectElementAt(const WebPoint& point)
{
    if (!m_page)
        return;

    if (point.x == -1 || point.y == -1) {
        m_page->inspectorController().inspect(0);
    } else {
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent | HitTestRequest::IgnorePointerEventsNone;
        HitTestRequest request(hitType);

        FrameView* frameView = m_page->mainFrame()->view();
        IntPoint transformedPoint(point);
        transformedPoint = transformedPoint - frameView->inputEventsOffsetForEmulation();
        transformedPoint.scale(1 / frameView->inputEventsScaleFactor(), 1 / frameView->inputEventsScaleFactor());
        HitTestResult result(m_page->mainFrame()->view()->windowToContents(transformedPoint));
        m_page->mainFrame()->contentRenderer()->hitTest(request, result);
        Node* node = result.innerNode();
        if (!node && m_page->mainFrame()->document())
            node = m_page->mainFrame()->document()->documentElement();
        m_page->inspectorController().inspect(node);
    }
}

WebString WebViewImpl::inspectorSettings() const
{
    return m_inspectorSettings;
}

void WebViewImpl::setInspectorSettings(const WebString& settings)
{
    m_inspectorSettings = settings;
}

bool WebViewImpl::inspectorSetting(const WebString& key, WebString* value) const
{
    if (!m_inspectorSettingsMap->contains(key))
        return false;
    *value = m_inspectorSettingsMap->get(key);
    return true;
}

void WebViewImpl::setInspectorSetting(const WebString& key,
                                      const WebString& value)
{
    m_inspectorSettingsMap->set(key, value);
    client()->didUpdateInspectorSetting(key, value);
}

void WebViewImpl::setCompositorDeviceScaleFactorOverride(float deviceScaleFactor)
{
    m_compositorDeviceScaleFactorOverride = deviceScaleFactor;
    if (page() && m_layerTreeView)
        updateLayerTreeDeviceScaleFactor();
}

void WebViewImpl::setRootLayerTransform(const WebSize& rootLayerOffset, float rootLayerScale)
{
    m_rootLayerScale = rootLayerScale;
    m_rootLayerOffset = rootLayerOffset;
    if (mainFrameImpl())
        mainFrameImpl()->setInputEventsTransformForEmulation(m_rootLayerOffset, m_rootLayerScale);
    updateRootLayerTransform();
}

WebDevToolsAgent* WebViewImpl::devToolsAgent()
{
    return m_devToolsAgent.get();
}

WebAXObject WebViewImpl::accessibilityObject()
{
    if (!mainFrameImpl())
        return WebAXObject();

    Document* document = mainFrameImpl()->frame()->document();
    return WebAXObject(
        document->axObjectCache()->getOrCreate(document->renderer()));
}

void WebViewImpl::performCustomContextMenuAction(unsigned action)
{
    if (!m_page)
        return;
    ContextMenu* menu = m_page->contextMenuController().contextMenu();
    if (!menu)
        return;
    const ContextMenuItem* item = menu->itemWithAction(static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + action));
    if (item)
        m_page->contextMenuController().contextMenuItemSelected(item);
    m_page->contextMenuController().clearContextMenu();
}

void WebViewImpl::showContextMenu()
{
    if (!page())
        return;

    page()->contextMenuController().clearContextMenu();
    m_contextMenuAllowed = true;
    if (Frame* focusedFrame = page()->focusController().focusedOrMainFrame())
        focusedFrame->eventHandler().sendContextMenuEventForKey();
    m_contextMenuAllowed = false;
}

WebString WebViewImpl::getSmartClipData(WebRect rect)
{
    Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return WebString();
    return WebCore::SmartClip(frame).dataForRect(rect).toString();
}

void WebViewImpl::hidePopups()
{
    hideSelectPopup();
    if (m_pagePopup)
        closePagePopup(m_pagePopup.get());
}

void WebViewImpl::setIsTransparent(bool isTransparent)
{
    // Set any existing frames to be transparent.
    Frame* frame = m_page->mainFrame();
    while (frame) {
        frame->view()->setTransparent(isTransparent);
        frame = frame->tree().traverseNext();
    }

    // Future frames check this to know whether to be transparent.
    m_isTransparent = isTransparent;
}

bool WebViewImpl::isTransparent() const
{
    return m_isTransparent;
}

void WebViewImpl::setBaseBackgroundColor(WebColor color)
{
    if (m_baseBackgroundColor == color)
        return;

    m_baseBackgroundColor = color;

    if (m_page->mainFrame())
        m_page->mainFrame()->view()->setBaseBackgroundColor(color);

    updateLayerTreeBackgroundColor();
}

void WebViewImpl::setIsActive(bool active)
{
    if (page())
        page()->focusController().setActive(active);
}

bool WebViewImpl::isActive() const
{
    return page() ? page()->focusController().isActive() : false;
}

void WebViewImpl::setDomainRelaxationForbidden(bool forbidden, const WebString& scheme)
{
    SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden, String(scheme));
}

void WebViewImpl::setWindowFeatures(const WebWindowFeatures& features)
{
    m_page->chrome().setWindowFeatures(features);
}

void WebViewImpl::setSelectionColors(unsigned activeBackgroundColor,
                                     unsigned activeForegroundColor,
                                     unsigned inactiveBackgroundColor,
                                     unsigned inactiveForegroundColor) {
#if USE(DEFAULT_RENDER_THEME)
    RenderThemeChromiumDefault::setSelectionColors(activeBackgroundColor, activeForegroundColor, inactiveBackgroundColor, inactiveForegroundColor);
    RenderTheme::theme().platformColorsDidChange();
#endif
}

void WebView::injectStyleSheet(const WebString& sourceCode, const WebVector<WebString>& patternsIn, WebView::StyleInjectionTarget injectIn)
{
    Vector<String> patterns;
    for (size_t i = 0; i < patternsIn.size(); ++i)
        patterns.append(patternsIn[i]);

    InjectedStyleSheets::instance().add(sourceCode, patterns, static_cast<WebCore::StyleInjectionTarget>(injectIn));
}

void WebView::removeInjectedStyleSheets()
{
    InjectedStyleSheets::instance().removeAll();
}

void WebViewImpl::didCommitLoad(bool isNewNavigation, bool isNavigationWithinPage)
{
    if (isNewNavigation && !isNavigationWithinPage)
        m_pageScaleConstraintsSet.setNeedsReset(true);

    // Make sure link highlight from previous page is cleared.
    m_linkHighlights.clear();
    m_prevHoverNode = 0;

    endActiveFlingAnimation();
    resetSavedScrollAndScaleState();
}

void WebViewImpl::willInsertBody(WebFrameImpl* webframe)
{
    if (webframe != mainFrameImpl())
        return;

    // If we get to the <body> tag and we have no pending stylesheet loads, we
    // can be fairly confident we'll have something sensible to paint soon and
    // can turn off deferred commits.
    if (m_page->mainFrame()->document()->haveStylesheetsLoaded())
        resumeTreeViewCommits();
}

void WebViewImpl::resumeTreeViewCommits()
{
    if (m_layerTreeViewCommitsDeferred) {
        if (m_layerTreeView)
            m_layerTreeView->setDeferCommits(false);
        m_layerTreeViewCommitsDeferred = false;
    }
}

void WebViewImpl::layoutUpdated(WebFrameImpl* webframe)
{
    if (!m_client || webframe != mainFrameImpl())
        return;

    // If we finished a layout while in deferred commit mode,
    // that means it's time to start producing frames again so un-defer.
    resumeTreeViewCommits();

    if (m_shouldAutoResize && mainFrameImpl()->frame() && mainFrameImpl()->frame()->view()) {
        WebSize frameSize = mainFrameImpl()->frame()->view()->frameRect().size();
        if (frameSize != m_size) {
            m_size = frameSize;
            m_client->didAutoResize(m_size);
            sendResizeEventAndRepaint();
        }
    }

    if (m_pageScaleConstraintsSet.constraintsDirty())
        refreshPageScaleFactorAfterLayout();

    m_client->didUpdateLayout();
}

void WebViewImpl::didChangeContentsSize()
{
    m_pageScaleConstraintsSet.didChangeContentsSize(contentsSize(), pageScaleFactor());
}

void WebViewImpl::deviceOrPageScaleFactorChanged()
{
    if (pageScaleFactor() && pageScaleFactor() != 1)
        enterForceCompositingMode(true);
    m_pageScaleConstraintsSet.setNeedsReset(false);
    updateLayerTreeViewport();
}

bool WebViewImpl::useExternalPopupMenus()
{
    return shouldUseExternalPopupMenus;
}

void WebViewImpl::startDragging(Frame* frame,
                                const WebDragData& dragData,
                                WebDragOperationsMask mask,
                                const WebImage& dragImage,
                                const WebPoint& dragImageOffset)
{
    if (!m_client)
        return;
    ASSERT(!m_doingDragAndDrop);
    m_doingDragAndDrop = true;
    m_client->startDragging(WebFrameImpl::fromFrame(frame), dragData, mask, dragImage, dragImageOffset);
}

void WebViewImpl::setIgnoreInputEvents(bool newValue)
{
    ASSERT(m_ignoreInputEvents != newValue);
    m_ignoreInputEvents = newValue;
}

void WebViewImpl::setBackgroundColorOverride(WebColor color)
{
    m_backgroundColorOverride = color;
    updateLayerTreeBackgroundColor();
}

void WebViewImpl::setZoomFactorOverride(float zoomFactor)
{
    m_zoomFactorOverride = zoomFactor;
    setZoomLevel(zoomLevel());
}

void WebViewImpl::addPageOverlay(WebPageOverlay* overlay, int zOrder)
{
    if (!m_pageOverlays)
        m_pageOverlays = PageOverlayList::create(this);

    m_pageOverlays->add(overlay, zOrder);
}

void WebViewImpl::removePageOverlay(WebPageOverlay* overlay)
{
    if (m_pageOverlays && m_pageOverlays->remove(overlay) && m_pageOverlays->empty())
        m_pageOverlays = nullptr;
}

void WebViewImpl::setOverlayLayer(WebCore::GraphicsLayer* layer)
{
    if (m_rootGraphicsLayer) {
        if (layer->parent() != m_rootGraphicsLayer)
            m_rootGraphicsLayer->addChild(layer);
    }
}

NotificationPresenterImpl* WebViewImpl::notificationPresenterImpl()
{
    if (!m_notificationPresenter.isInitialized() && m_client)
        m_notificationPresenter.initialize(m_client->notificationPresenter());
    return &m_notificationPresenter;
}

Element* WebViewImpl::focusedElement() const
{
    Frame* frame = m_page->focusController().focusedFrame();
    if (!frame)
        return 0;

    Document* document = frame->document();
    if (!document)
        return 0;

    return document->focusedElement();
}

HitTestResult WebViewImpl::hitTestResultForWindowPos(const IntPoint& pos)
{
    IntPoint docPoint(m_page->mainFrame()->view()->windowToContents(pos));
    return m_page->mainFrame()->eventHandler().hitTestResultAtPoint(docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent);
}

void WebViewImpl::setTabsToLinks(bool enable)
{
    m_tabsToLinks = enable;
}

bool WebViewImpl::tabsToLinks() const
{
    return m_tabsToLinks;
}

void WebViewImpl::suppressInvalidations(bool enable)
{
    if (m_client)
        m_client->suppressCompositorScheduling(enable);
}

bool WebViewImpl::allowsAcceleratedCompositing()
{
    return !m_compositorCreationFailed;
}

void WebViewImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    suppressInvalidations(true);

    if (page()->settings().pinchVirtualViewportEnabled()) {
        if (!m_pinchViewports)
            m_pinchViewports = PinchViewports::create(this);

        m_pinchViewports->setOverflowControlsHostLayer(layer);
        m_pinchViewports->setViewportSize(mainFrameImpl()->frame()->view()->frameRect().size());
        if (layer) {
            m_rootGraphicsLayer = m_pinchViewports->rootGraphicsLayer();
            m_rootLayer = m_pinchViewports->rootGraphicsLayer()->platformLayer();
        } else {
            m_rootGraphicsLayer = 0;
            m_rootLayer = 0;
        }
    } else {
        m_rootGraphicsLayer = layer;
        m_rootLayer = layer ? layer->platformLayer() : 0;
    }

    setIsAcceleratedCompositingActive(layer);

    updateRootLayerTransform();

    if (m_layerTreeView) {
        if (m_rootLayer) {
            m_layerTreeView->setRootLayer(*m_rootLayer);
            // We register viewport layers here since there may not be a layer
            // tree view prior to this point.
            if (m_pinchViewports) {
                m_pinchViewports->registerViewportLayersWithTreeView(m_layerTreeView);
            } else {
                GraphicsLayer* rootScrollLayer = compositor()->scrollLayer();
                ASSERT(rootScrollLayer);
                WebLayer* pageScaleLayer = rootScrollLayer->parent() ? rootScrollLayer->parent()->platformLayer() : 0;
                m_layerTreeView->registerViewportLayers(pageScaleLayer, rootScrollLayer->platformLayer(), 0);
            }
        } else {
            m_layerTreeView->clearRootLayer();
            if (m_pinchViewports)
                m_pinchViewports->clearViewportLayersForTreeView(m_layerTreeView);
            else
                m_layerTreeView->clearViewportLayers();
        }
    }

    suppressInvalidations(false);
}

void WebViewImpl::scheduleCompositingLayerSync()
{
    m_layerTreeView->setNeedsAnimate();
}

void WebViewImpl::scrollRootLayerRect(const IntSize&, const IntRect&)
{
    updateLayerTreeViewport();
}

void WebViewImpl::invalidateRect(const IntRect& rect)
{
    if (m_isAcceleratedCompositingActive) {
        ASSERT(m_layerTreeView);
        updateLayerTreeViewport();
    } else if (m_client)
        m_client->didInvalidateRect(rect);
}

WebCore::GraphicsLayerFactory* WebViewImpl::graphicsLayerFactory() const
{
    return m_graphicsLayerFactory.get();
}

WebCore::RenderLayerCompositor* WebViewImpl::compositor() const
{
    if (!page()
        || !page()->mainFrame()
        || !page()->mainFrame()->document()
        || !page()->mainFrame()->document()->renderView())
        return 0;
    return page()->mainFrame()->document()->renderView()->compositor();
}

void WebViewImpl::registerForAnimations(WebLayer* layer)
{
    if (m_layerTreeView)
        m_layerTreeView->registerForAnimations(layer);
}

WebCore::GraphicsLayer* WebViewImpl::rootGraphicsLayer()
{
    return m_rootGraphicsLayer;
}

void WebViewImpl::scheduleAnimation()
{
    if (isAcceleratedCompositingActive()) {
        ASSERT(m_layerTreeView);
        m_layerTreeView->setNeedsAnimate();
        return;
    }
    if (m_client)
        m_client->scheduleAnimation();
}

void WebViewImpl::setIsAcceleratedCompositingActive(bool active)
{
    blink::Platform::current()->histogramEnumeration("GPU.setIsAcceleratedCompositingActive", active * 2 + m_isAcceleratedCompositingActive, 4);

    if (m_isAcceleratedCompositingActive == active)
        return;

    if (!active) {
        m_isAcceleratedCompositingActive = false;
        // We need to finish all GL rendering before sending didDeactivateCompositor() to prevent
        // flickering when compositing turns off. This is only necessary if we're not in
        // force-compositing-mode.
        if (m_layerTreeView && !page()->settings().forceCompositingMode())
            m_layerTreeView->finishAllRendering();
        m_client->didDeactivateCompositor();
        if (!m_layerTreeViewCommitsDeferred
            && blink::Platform::current()->isThreadedCompositingEnabled()) {
            ASSERT(m_layerTreeView);
            // In threaded compositing mode, force compositing mode is always on so setIsAcceleratedCompositingActive(false)
            // means that we're transitioning to a new page. Suppress commits until WebKit generates invalidations so
            // we don't attempt to paint too early in the next page load.
            m_layerTreeView->setDeferCommits(true);
            m_layerTreeViewCommitsDeferred = true;
        }
    } else if (m_layerTreeView) {
        m_isAcceleratedCompositingActive = true;
        updateLayerTreeViewport();
        if (m_pageOverlays)
            m_pageOverlays->update();

        m_client->didActivateCompositor(0);
    } else {
        TRACE_EVENT0("webkit", "WebViewImpl::setIsAcceleratedCompositingActive(true)");

        m_client->initializeLayerTreeView();
        m_layerTreeView = m_client->layerTreeView();
        if (m_layerTreeView) {
            m_layerTreeView->setRootLayer(*m_rootLayer);

            bool visible = page()->visibilityState() == PageVisibilityStateVisible;
            m_layerTreeView->setVisible(visible);
            updateLayerTreeDeviceScaleFactor();
            m_layerTreeView->setPageScaleFactorAndLimits(pageScaleFactor(), minimumPageScaleFactor(), maximumPageScaleFactor());
            updateLayerTreeBackgroundColor();
            m_layerTreeView->setHasTransparentBackground(isTransparent());
#if USE(RUBBER_BANDING)
            RefPtr<Image> overhangImage = OverscrollTheme::theme()->getOverhangImage();
            if (overhangImage)
                m_layerTreeView->setOverhangBitmap(overhangImage->nativeImageForCurrentFrame()->bitmap());
#endif
            updateLayerTreeViewport();
            m_client->didActivateCompositor(0);
            m_isAcceleratedCompositingActive = true;
            m_compositorCreationFailed = false;
            if (m_pageOverlays)
                m_pageOverlays->update();
            m_layerTreeView->setShowFPSCounter(m_showFPSCounter);
            m_layerTreeView->setShowPaintRects(m_showPaintRects);
            m_layerTreeView->setShowDebugBorders(m_showDebugBorders);
            m_layerTreeView->setContinuousPaintingEnabled(m_continuousPaintingEnabled);
            m_layerTreeView->setShowScrollBottleneckRects(m_showScrollBottleneckRects);
        } else {
            m_isAcceleratedCompositingActive = false;
            m_client->didDeactivateCompositor();
            m_compositorCreationFailed = true;
        }
    }
    if (page())
        page()->mainFrame()->view()->setClipsRepaints(!m_isAcceleratedCompositingActive);
}

void WebViewImpl::updateMainFrameScrollPosition(const IntPoint& scrollPosition, bool programmaticScroll)
{
    FrameView* frameView = page()->mainFrame()->view();
    if (!frameView)
        return;

    if (frameView->scrollPosition() == scrollPosition)
        return;

    bool oldProgrammaticScroll = frameView->inProgrammaticScroll();
    frameView->setInProgrammaticScroll(programmaticScroll);
    frameView->notifyScrollPositionChanged(scrollPosition);
    frameView->setInProgrammaticScroll(oldProgrammaticScroll);
}

#if defined(S_IME_SCROLL_EVENT)
void WebViewImpl::setContentTopOffset(float contentTopOffset){
    m_contentTopOffset = contentTopOffset;
    FrameView* frameView = page()->mainFrame()->view();
    if (!frameView)
        return;
    frameView->setContentTopOffset(contentTopOffset);
}
#endif

void WebViewImpl::applyScrollAndScale(const WebSize& scrollDelta, float pageScaleDelta)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    if (pageScaleDelta == 1) {
        TRACE_EVENT_INSTANT2("webkit", "WebViewImpl::applyScrollAndScale::scrollBy", "x", scrollDelta.width, "y", scrollDelta.height);
        WebSize webScrollOffset = mainFrame()->scrollOffset();
        IntPoint scrollOffset(webScrollOffset.width + scrollDelta.width, webScrollOffset.height + scrollDelta.height);
        updateMainFrameScrollPosition(scrollOffset, false);
    } else {
        // The page scale changed, so apply a scale and scroll in a single
        // operation.
        WebSize scrollOffset = mainFrame()->scrollOffset();
        scrollOffset.width += scrollDelta.width;
        scrollOffset.height += scrollDelta.height;

        WebPoint scrollPoint(scrollOffset.width, scrollOffset.height);
        setPageScaleFactor(pageScaleFactor() * pageScaleDelta, scrollPoint);
        m_doubleTapZoomPending = false;
    }
}

void WebViewImpl::didExitCompositingMode()
{
    ASSERT(m_isAcceleratedCompositingActive);
    setIsAcceleratedCompositingActive(false);
    m_compositorCreationFailed = true;
    m_client->didInvalidateRect(IntRect(0, 0, m_size.width, m_size.height));

    // Force a style recalc to remove all the composited layers.
    m_page->mainFrame()->document()->setNeedsStyleRecalc(SubtreeStyleChange);

    if (m_pageOverlays)
        m_pageOverlays->update();
}

bool WebViewImpl::isSelectionWithinVisibleRect() const
{
    Frame* frame = page()->focusController().focusedOrMainFrame();
    if (!frame)
        return false;

    RefPtr<Range> range;
    if (frame->selection().isRange() && (range = frame->selection().toNormalizedRange())) {
        Vector<IntRect> rects;
        range->textBoundingBox(rects);

        IntRect visibleRect = frame->view()->visibleContentRect();
        for (size_t i = 0; i < rects.size(); ++i) {
            if (visibleRect.intersects(rects[i]))
                return true;
        }
    }
    return false;
}

WebRect WebViewImpl::currentSelectionRect() const
{
    if (!page())
        return WebRect();

    Frame* frame = page()->focusController().focusedOrMainFrame();
    if (!frame)
        return WebRect();

    FrameSelection& selection = frame->selection();
    if (!selection.isRange())
        return WebRect();

    RefPtr<Range> range = selection.toNormalizedRange();
    if (!range)
        return WebRect();

    Vector<IntRect> rects;
    IntRect resultRect;
    range->textAndImagesBoundingBox(rects);
    for (size_t i = 0; i < rects.size(); ++i)
        resultRect.unite(rects[i]);
     LOG(INFO)<<"resultRect: "<<"  "<<resultRect.x()<<"  "<<resultRect.y()<<"  x2="<<resultRect.width()<<"  "<<resultRect.height();	
    IntRect selectionRect = frame->view()->contentsToWindow(resultRect);
    selectionRect.scale(pageScaleFactor());
     LOG(INFO)<<"selectionRect: "<<selectionRect.x()<<"  "<<selectionRect.y()<<"  "<<selectionRect.width()<<"  "<<selectionRect.height();
    return WebRect(selectionRect);
}

void WebViewImpl::selectionAsBitmap(SkBitmap& selectedRegion) const
{
    if (!m_page)
        return;

    Frame* frame = m_page->mainFrame();
    if (!frame)
        return;

    OwnPtr<DragImage> selectedImage = frame->dragImageForSelection();
    if (selectedImage)
        selectedRegion = selectedImage->bitmap();
}

void WebViewImpl::updateLayerTreeViewport()
{
    if (!page() || !m_layerTreeView)
        return;

    m_layerTreeView->setPageScaleFactorAndLimits(pageScaleFactor(), minimumPageScaleFactor(), maximumPageScaleFactor());
}

void WebViewImpl::updateLayerTreeBackgroundColor()
{
    if (!m_layerTreeView)
        return;

    m_layerTreeView->setBackgroundColor(alphaChannel(m_backgroundColorOverride) ? m_backgroundColorOverride : backgroundColor());
}

void WebViewImpl::updateLayerTreeDeviceScaleFactor()
{
    ASSERT(page());
    ASSERT(m_layerTreeView);

    float deviceScaleFactor = m_compositorDeviceScaleFactorOverride ? m_compositorDeviceScaleFactorOverride : page()->deviceScaleFactor();
    m_layerTreeView->setDeviceScaleFactor(deviceScaleFactor);
}

void WebViewImpl::updateRootLayerTransform()
{
    if (m_rootGraphicsLayer) {
        WebCore::TransformationMatrix transform;
        transform.translate(m_rootLayerOffset.width, m_rootLayerOffset.height);
        transform = transform.scale(m_rootLayerScale);
        m_rootGraphicsLayer->setTransform(transform);
    }
}

bool WebViewImpl::detectContentOnTouch(const WebPoint& position)
{
    HitTestResult touchHit = hitTestResultForWindowPos(position);

    if (touchHit.isContentEditable())
        return false;

    Node* node = touchHit.innerNode();
    if (!node || !node->isTextNode())
        return false;

    // Ignore when tapping on links or nodes listening to click events, unless the click event is on the
    // body element, in which case it's unlikely that the original node itself was intended to be clickable.
    for (; node && !node->hasTagName(HTMLNames::bodyTag); node = node->parentNode()) {
        if (node->isLink() || node->willRespondToTouchEvents() || node->willRespondToMouseClickEvents())
            return false;
    }

    WebContentDetectionResult content = m_client->detectContentAround(touchHit);
    if (!content.isValid()) {
        mainFrameImpl()->setContentDetectionResult(WebContentDetectionResult());
        return false;
    }

    mainFrameImpl()->setContentDetectionResult(content);
	
	//P140427-00252 Draw highlight when user touches email address
    if(!(content.range()).isNull()){
        RefPtr<Range> range = static_cast<PassRefPtr<Range> >(content.range());
        Node* touchNode = range->firstNode();
        enableContentHighlight( touchNode);
        for (size_t i = 0; i < m_linkHighlights.size(); ++i)
            m_linkHighlights[i]->startHighlightAnimationIfNeeded();
     }

    m_client->scheduleContentIntent(content.intent());
    return true;
}

void WebViewImpl::setVisibilityState(WebPageVisibilityState visibilityState,
                                     bool isInitialState) {
    if (!page())
        return;

    ASSERT(visibilityState == WebPageVisibilityStateVisible || visibilityState == WebPageVisibilityStateHidden || visibilityState == WebPageVisibilityStatePrerender);
    m_page->setVisibilityState(static_cast<PageVisibilityState>(static_cast<int>(visibilityState)), isInitialState);

    if (m_layerTreeView) {
        bool visible = visibilityState == WebPageVisibilityStateVisible;
        m_layerTreeView->setVisible(visible);
    }
}

bool WebViewImpl::requestPointerLock()
{
    return m_client && m_client->requestPointerLock();
}

void WebViewImpl::requestPointerUnlock()
{
    if (m_client)
        m_client->requestPointerUnlock();
}

bool WebViewImpl::isPointerLocked()
{
    return m_client && m_client->isPointerLocked();
}

void WebViewImpl::pointerLockMouseEvent(const WebInputEvent& event)
{
    AtomicString eventType;
    switch (event.type) {
    case WebInputEvent::MouseDown:
        eventType = EventTypeNames::mousedown;
        break;
    case WebInputEvent::MouseUp:
        eventType = EventTypeNames::mouseup;
        break;
    case WebInputEvent::MouseMove:
        eventType = EventTypeNames::mousemove;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    const WebMouseEvent& mouseEvent = static_cast<const WebMouseEvent&>(event);

    if (page())
        page()->pointerLockController().dispatchLockedMouseEvent(
            PlatformMouseEventBuilder(mainFrameImpl()->frameView(), mouseEvent),
            eventType);
}

bool WebViewImpl::shouldDisableDesktopWorkarounds()
{
    if (!settings()->viewportEnabled())
        return false;

    // A document is considered adapted to small screen UAs if one of these holds:
    // 1. The author specified viewport has a constrained width that is equal to
    //    the initial viewport width.
    // 2. The author has disabled viewport zoom.

    const PageScaleConstraints& constraints = m_pageScaleConstraintsSet.pageDefinedConstraints();

    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return false;

    return mainFrameImpl()->frameView()->layoutSize().width() == m_size.width
        || (constraints.minimumScale == constraints.maximumScale && constraints.minimumScale != -1);
}

WebRect WebViewImpl::focusedElementBounds()
{
    Element* focusedElement = this->focusedElement();
    return focusedElement ? WebRect(getElementBounds(*focusedElement)) : WebRect();
}

void WebViewImpl::onHandleSelectionDrop(int x, int y, const WebString& selectedText)
{
    IntPoint point(x, y);
    point = m_page->mainFrame()->view()->windowToContents(point);
    HitTestResult result(m_page->mainFrame()->eventHandler().hitTestResultAtPoint(point));
    Node* hitNode = result.innerNonSharedNode();
    if (!hitNode || !hitNode->isElementNode())
        return;

    Element* hitElement = toElement(hitNode);
    if (!hitElement || !isFormNavigationTextInput(*hitElement))
        return;

    IntRect bounds = getElementBounds(*hitElement);
    if ((bounds.x() == 0 && bounds.y() == 0) || bounds.isEmpty())
         return;

    if (!fakeMouseClick(bounds.center().x(), bounds.center().y(), hitNode))
        return;

    hitNode->setFocus(true);
    Frame* focused = focusedWebCoreFrame();
    if (!focused)
        return;

    if (focused->inputMethodController().hasComposition())
        focused->inputMethodController().confirmComposition(String(selectedText));
    else
        focused->editor().insertText(String(selectedText), 0);
}

void WebViewImpl::handleSelectionDropOnFocusedInput(const WebString& text, int dropAction) {
    Element* focusElement = focusedElement();
    if (!focusElement || !isFormNavigationTextInput(*focusElement) )
        return;

        Frame* focused = focusedWebCoreFrame();
        
        if (!focused)
            return;
        
        //LOG(INFO)<<" WebViewImpl::handleSelectionDropOnFocusedInput:: text ="<<text.utf8().data();        
        //editor->command("InsertHTML").execute(String(text));

        switch(dropAction) {
        case DROP_PLAIN_TEXT:            
            if (focused->inputMethodController().hasComposition())
                focused->inputMethodController().confirmComposition(String(text));
            else
                focused->editor().insertText(String(text), 0);
            break;
        case DROP_IMAGE_SRC:
            focusedFrame()->executeCommand(WebString::fromUTF8("InsertImage"), text);             
            break;
        case DROP_HTML:
            focusedFrame()->executeCommand(WebString::fromUTF8("InsertHTML"), text);
            break;
        default:
            if (focused->inputMethodController().hasComposition())
                focused->inputMethodController().confirmComposition(String(text));
            else
                focused->editor().insertText(String(text), 0);
            break;
        }
            

}

bool WebViewImpl::getFocusedInputInfo(WebRect& bounds, bool& isMultiLine, bool& isRichContentEditable) 
{
    Element* focusedElement = this->focusedElement();
    if (!focusedElement || !isFormNavigationTextInput(*focusedElement) )
        return false;
    bool isValidInput = true;
    IntRect focusedInputRect = getElementBounds(*focusedElement);
    focusedInputRect.scale(pageScaleFactor());
    bounds = focusedInputRect;
    WebTextInputType inputType = textInputType();
    switch(inputType) {
    case WebTextInputTypeContentEditable:
        isMultiLine = true;
        isRichContentEditable = focusedElement->isContentRichlyEditable();
        break;
    case WebTextInputTypeTextArea:
        isMultiLine = true;
        isRichContentEditable = false;
        break;
    case WebTextInputTypeNone:
        isValidInput = false;
        break;
    default:
        isMultiLine = false;
        isRichContentEditable = false;
        break;
    }
    return isValidInput; //need to update properly
}

bool WebViewImpl::fakeMouseClick(int x, int y, Node* node)
{
    IntPoint mousePos = IntPoint(x, y);

    //Mouse Down
    const PlatformMouseEvent mouseDown(mousePos, mousePos, LeftButton, PlatformEvent::MousePressed, 1, false, false, false, false, currentTime());
    bool handled = mainFrameImpl()->frame()->eventHandler().handleMousePressEvent(mouseDown);

    //MouseUp
    const PlatformMouseEvent mouseUp(mousePos, mousePos, LeftButton, PlatformEvent::MouseReleased, 1, false, false, false, false, currentTime());
    handled |= mainFrameImpl()->frame()->eventHandler().handleMouseReleaseEvent(mouseUp);

    return handled;
}

bool WebViewImpl::isFormNavigationTextInput(Element& element) const
{
    if (element.hasTagName(HTMLNames::inputTag) && toHTMLInputElement(element).isReadOnly())
        return false;

    RenderObject* renderer = element.renderer();
    return renderer && (element.isContentEditable() || renderer->isTextControl());
}

bool WebViewImpl::isSelectElement(const Element& element) const
{
    return element.renderer() && element.hasTagName(HTMLNames::selectTag);
}

IntRect WebViewImpl::getElementBounds(const Element& element) const
{
    element.document().updateLayoutIgnorePendingStylesheets();
    IntRect absoluteRect = pixelSnappedIntRect(element.Node::boundingBox());
    return (element.document().view() ? element.document().view()->contentsToWindow(absoluteRect) : IntRect());
}

bool WebViewImpl::performClickOnElement(Element& element)
{
    if (IntRect() == getElementBounds(element))
        return false;

    // Set focus to false to compare it with focusedElement of document.
    element.focus(false);

    Element* focusElement = focusedElement();

    if (!focusElement || focusElement != &element)
        return false;

    focusElement->dispatchSimulatedClick(0, SendMouseUpDownEvents);

    if (isFormNavigationTextInput(*focusElement)) {
        WebTextInputInfo info = textInputInfo();
        setEditableSelectionOffsets(info.selectionStart, info.selectionEnd);
    }

    m_client->performMouseClick();

#if 0//defined(S_FORM_NAVIGATION_SCROLL)
    TextFieldsBoundsChanged();
#endif

    return true;
}

Element* WebViewImpl::nextTextOrSelectElement(Element* element)
{
    if (!element)
        return 0;

    Element* nextElement = element;

    if (nextElement->isFrameOwnerElement()) {
        HTMLFrameOwnerElement& htmlFrameOwnerElement = *toHTMLFrameOwnerElement(nextElement);

        // Checks if the frame is empty or not.
        if (!htmlFrameOwnerElement.contentFrame())
            return 0;

        Document* ownerDocument = htmlFrameOwnerElement.contentDocument();
        if (!ownerDocument || !(nextElement = ownerDocument->body()))
            return 0;

        // Checks if content editable flag on body has set.
        if (nextElement->isContentEditable())
            return nextElement;
    }

    while ((nextElement = ElementTraversal::next(*nextElement))) {
        if (nextElement->hasTagName(HTMLNames::iframeTag)
            || nextElement->hasTagName(HTMLNames::frameTag)) {
            Element* frameOwnerElement = nextElement;

            nextElement = nextTextOrSelectElement(nextElement);
            if (!nextElement) {
                nextElement = frameOwnerElement;
                continue;
            }
        }

        if (nextElement->isFocusable() && (isFormNavigationTextInput(*nextElement)
            || isSelectElement(*nextElement)))
            break;
    }

    // If couldn't find anything in the current document scope,
    // try finding in other document scope if present any.
    if (!nextElement) {
        if (element->document().frame() != mainFrameImpl()->frame() && !element->isFrameOwnerElement())
            nextElement = nextTextOrSelectElement(ElementTraversal::next(*element->document().ownerElement()));
    }

    return nextElement;
}

Element* WebViewImpl::previousTextOrSelectElement(Element* element)
{
    if (!element)
        return 0;

    Element* previousElement = element;

    if (previousElement->isFrameOwnerElement()) {
        HTMLFrameOwnerElement& htmlFrameOwnerElement = *toHTMLFrameOwnerElement(previousElement);

        // Checks if the frame is empty or not.
        if (!htmlFrameOwnerElement.contentFrame())
            return 0;

        Document* ownerDocument = htmlFrameOwnerElement.contentDocument();
        if (!ownerDocument)
            return 0;

        previousElement = ParentNode::lastElementChild(ownerDocument);
        while (previousElement && ElementTraversal::firstWithin(*previousElement))
            previousElement = ParentNode::lastElementChild(previousElement);

        if (!previousElement || (previousElement->isFocusable()
            && (isFormNavigationTextInput(*previousElement) || isSelectElement(*previousElement))))
            return previousElement;
    }

    while ((previousElement = ElementTraversal::previous(*previousElement))) {
        if (previousElement->hasTagName(HTMLNames::iframeTag)
            || previousElement->hasTagName(HTMLNames::frameTag)) {
            Element* frameOwnerElement = previousElement;

            previousElement = previousTextOrSelectElement(previousElement);
            if (!previousElement) {
                previousElement = frameOwnerElement;
                continue;
            }
        }

        if (previousElement->isFocusable() && (isFormNavigationTextInput(*previousElement)
            || isSelectElement(*previousElement)))
            break;
    }

    // If couldn't find anything in the current document scope,
    // try finding in other document scope if present any.
    if (!previousElement) {
        if (element->document().frame() != mainFrameImpl()->frame() && !element->isFrameOwnerElement())
            previousElement = previousTextOrSelectElement(ElementTraversal::previous(*element->document().ownerElement()));
    }

    return previousElement;
}

bool WebViewImpl::moveFocusToNext()
{
    Element* focusElement = focusedElement();
    if (!focusElement || (!isFormNavigationTextInput(*focusElement) && !isSelectElement(*focusElement)))
        return false;

    Element* nextElement = nextTextOrSelectElement(focusElement);
    if (!nextElement)
        return false;

    if (isSelectElement(*nextElement) && !toHTMLSelectElement(nextElement)->length())
        m_client->messageToClosePopup();

    // Scroll the element into center of screen.
    //nextElement->scrollIntoViewCenter(true /* centerAlways */);

    //P140422-06779:scrolling always into center is not correct if the next/prev element is at the edge of the view.
    nextElement->document().updateLayoutIgnorePendingStylesheets();
    IntRect absoluteRect = pixelSnappedIntRect(nextElement->Node::boundingBox());
	if(nextElement->renderer()){
        nextElement->renderer()->scrollRectToVisible(absoluteRect,
                    ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded
#if defined(S_SCROLL_EVENT)
                   ,true
 #endif
                    );
		}
	else
	    return false;

    bool handled = performClickOnElement(*nextElement);

    if (focusedFrame() && isFormNavigationTextInput(*nextElement))
        focusedFrame()->executeCommand(WebString::fromUTF8("MoveToEndOfDocument"));

    return handled;
}

bool WebViewImpl::moveFocusToPrevious()
{
    Element* focusElement = focusedElement();

    if (!focusElement || (!isFormNavigationTextInput(*focusElement) && !isSelectElement(*focusElement)))
        return false;

    Element* previousElement = previousTextOrSelectElement(focusElement);
    if (!previousElement)
        return false;

    if (isSelectElement(*previousElement) && !toHTMLSelectElement(previousElement)->length())
        m_client->messageToClosePopup();

    // Scroll the element into center of screen.
    // previousElement->scrollIntoViewCenter(true /* centerAlways */);

    //P140422-06779:scrolling always into center is not correct if the next/prev element is at the edge of the view.
    previousElement->document().updateLayoutIgnorePendingStylesheets();
    IntRect absoluteRect = pixelSnappedIntRect(previousElement->Node::boundingBox());
	if(previousElement->renderer())
       previousElement->renderer()->scrollRectToVisible(absoluteRect,
                    ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded
 #if defined(S_SCROLL_EVENT)
                   ,true
 #endif
                    );
	else
	    return false;
    bool handled = performClickOnElement(*previousElement);

    if (focusedFrame() && isFormNavigationTextInput(*previousElement))
        focusedFrame()->executeCommand(WebString::fromUTF8("MoveToEndOfDocument"));

    return handled;
}

int WebViewImpl::getIMEOptions()
{
    int action = FormInputNone;

    Element* focusElement = focusedElement();
    if (!focusElement || (!isSelectElement(*focusElement) && !isFormNavigationTextInput(*focusElement)))
        return action;

    if (Element* next = nextTextOrSelectElement(focusElement)) {
        if (isFormNavigationTextInput(*next))
            action |= FormInputNextText;
        else
            action |= FormInputNextSelect;
    }

    if (Element* prev = previousTextOrSelectElement(focusElement)) {
        if (isFormNavigationTextInput(*prev))
            action |= FormInputPrevText;
        else
            action |= FormInputPrevSelect;
    }

    return action;
}

WebImage WebViewImpl::bitmapFromCachedResource(const WebString& imageUrl)
{
    if (imageUrl.isNull() || !mainFrameImpl())
        return WebImage();
  #if defined(S_PLM_P141212_04905)	
    for (const Frame* frame = mainFrameImpl()->frame(); frame;frame = frame->tree().traverseNext()) {
        if (frame->document()){
            Document* document = frame->document();
  #else
    Document* document = mainFrameImpl()->frame()->document();
  #endif
    ASSERT(document);
    RefPtr<HTMLCollection> images = document->images();
    size_t sourceLength = images->length();
    KURL completeImageUrl = document->completeURL(imageUrl);
    for (size_t i = 0; i < sourceLength; ++i) {
        Element* element = images->item(i);
        ASSERT(element);
  #if defined(S_PLM_P140624_05001)
        if (!(document->completeURL(element->imageSourceURL().string())).string().contains(completeImageUrl))
  #else			
        if (document->completeURL(element->imageSourceURL().string())
            != completeImageUrl)
  #endif 
	  continue;
        Image* cachedImage = element->imageContents();
        if (!cachedImage)
            continue;
        RefPtr<NativeImageSkia> bitmapPtr = cachedImage->nativeImageForCurrentFrame();
        if (bitmapPtr)
            return WebImage(bitmapPtr->bitmap());
    }
  #if defined(S_PLM_P141212_04905)	
        }
     }		
  #endif
    return WebImage();
}
// SBROWSER_HANDLE_MOUSECLICK_CTRL ++
void WebViewImpl::onHandleMouseClickWithCtrlkey(int x, int y) {
  LOG(INFO)<<"WebViewImpl::OnHandleMouseClickWithCtrlkey";
  IntPoint point(x, y);
  point = m_page->mainFrame()->view()->windowToContents(point);
  HitTestResult result(m_page->mainFrame()->eventHandler().hitTestResultAtPoint(point));
  KURL linkURL = result.absoluteLinkURL();
  if (linkURL.isEmpty()) {
    linkURL = result.absoluteImageURL();
  }
  if (!linkURL.isEmpty()) {
    LOG(INFO)<<"WebViewImpl::OnHandleMouseClickWithCtrlkey:: url ="<<linkURL.string().utf8().data();
    m_client->openUrlInNewTab(base::UTF8ToUTF16(linkURL.string().utf8().data()));
  }
}

base::string16 WebViewImpl::getUrlFromElement(const WebElement& url_element){
  if (url_element.isNull())
    return base::string16();
  WebElement element = url_element;
  while (element.isNull() == false && !element.hasTagName("body")) {
    WebString hrefstr = element.getAttribute("href");
    if (hrefstr.length() != 0) {
      LOG(INFO)<<"WebViewImpl::getUrlFromElement:: url ="<<hrefstr.utf8().data();
      return base::UTF8ToUTF16(hrefstr.utf8().data());
    }
    element = element.parentNode().to<WebElement>();
  }
  return base::string16();
}
// SBROWSER_HANDLE_MOUSECLICK_CTRL --

//HIde URL bar --> Fixed element boundsApi++
bool WebViewImpl::isFixed(Node* node){
	if(node && node->renderer() && node->renderer()->isOutOfFlowPositioned() && node->renderer()->style( ) && node->renderer()->style()->position() == 6) // 6 is fixed position
		return true;
	return false;
}

int WebViewImpl::getHeightOfFixedElement(int x, int y){
	IntPoint point(x, y);
    point = m_page->mainFrame()->view()->windowToContents(point);
    HitTestResult result(m_page->mainFrame()->eventHandler().hitTestResultAtPoint(point));
    Node* hitNode = result.innerNonSharedNode();
	while(hitNode && !isFixed(hitNode))
		 hitNode = hitNode->parentNode();
	 if(hitNode){
		IntRect rect = hitNode->renderer()->enclosingLayer()->absoluteBoundingBox();
		IntRect winrect=hitNode->document().view()->contentsToWindow(rect);
		return winrect.height();
	 }
	 return 0;
}

//HIde URL bar --> Fixed element boundsApi--

//S_FP_AUTOLOGIN_SUPPORT++
#if defined(S_FP_AUTOLOGIN_SUPPORT)

#if defined(S_FP_AUTOLOGIN_CAPTCHA_FIX)
bool WebViewImpl::isCaptchaAvailable(HTMLInputElement *password_element)
{
    bool captcha = false;
    bool img_element_found = false;
    Node *pnode = password_element;
    // Assumption is The Captcha is a combination of Image and a Text Field after password field.
    for (Node* node = pnode; node; node = NodeTraversal::next(*node)) {
        if(node->isHTMLElement() && node->hasTagName(HTMLNames::imgTag) && toHTMLElement(node)->formOwner() == password_element->form()){
            img_element_found =true;
        }
        else if(!img_element_found)
            continue;
        else{
            if(node->hasTagName(HTMLNames::inputTag) && toHTMLElement(node)->formOwner() == password_element->form()){
                HTMLInputElement *input = toHTMLInputElement(node);
                    if(input->isTextField() && input->isFocusable()){
                        captcha = true;
                        break;
                    }
            }
        }
    }
    return captcha;
}
#endif

void WebViewImpl::setFocusOnPasswordField(const WebInputElement& element){
    HTMLInputElement *pwdElement = element.operator PassRefPtr<HTMLInputElement>().get();
    pwdElement->focus();
}

void WebViewImpl :: generateEnterEvent(const WebInputElement& element/*, const WebString& password*/){
    HTMLInputElement *inputElement = element.operator PassRefPtr<HTMLInputElement>().get();
#if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
    setAutoLoginFailureFlag(true);
#endif

#if defined(S_FP_AUTOLOGIN_CAPTCHA_FIX)
    bool iscaptcha = isCaptchaAvailable(inputElement);
    LOG(INFO)<<"FP Captcha Available "<<iscaptcha;
    if(iscaptcha){
    // Need to add a API to show alert Popup to User about this 
        if(m_client)
            m_client->autoLoginFailure();
            return;
        }
#endif

#if defined(S_FP_AVOID_AUTOLOGIN_FOR_HIDDEN_FORM)
	 // To Avoid AutoLogin when form is not visible.
	// In this case, User has to manually Login.
        if(!inputElement->isFocusable()){
		LOG(INFO)<<"FP: The element is not focusable, don't submit the form";
	       if(m_client)
	  	    m_client->autoLoginFailure();
		return;
        }
#endif
       		
        //Below Part is commented As we already focused the Password field
	 /*WebString pSuggestedValue = inputElement->suggestedValue();
	 WebString pValue = inputElement->value();
	 
        inputElement->focus();

        // Sometime OnFocus Password field gets cleared.
        // In such cases reset it and then generate Enter Event
        if(!pSuggestedValue.isEmpty() && inputElement->suggestedValue().isEmpty()){
		 LOG(INFO)<<"FP: Resetting Suggested Password Value";
		 inputElement->setSuggestedValue(password);
        }else if(!pValue.isEmpty() && inputElement->value().isEmpty()){
             LOG(INFO)<<"FP: Resetting Password Value";
	      inputElement->setValue(password, DispatchChangeEvent);
        }*/
 
        LOG(INFO)<<"FP: WebViewImpl :: generating enter event";

        WebKeyboardEvent kkeyDown = WebInputEventFactory::keyboardEvent(WebInputEvent::RawKeyDown, 1024, WTF::currentTime(), 66, 0, false);
        handleKeyEvent(kkeyDown);

        WebKeyboardEvent kchar = WebInputEventFactory::keyboardEvent(WebInputEvent::Char, 1024, WTF::currentTime(), 66, 0, false);
        handleCharEvent(kchar);

        WebKeyboardEvent kkeyup = WebInputEventFactory::keyboardEvent(WebInputEvent::KeyUp, 1024, WTF::currentTime(), 66, 0, false);
        handleKeyEvent(kkeyup);

        /***********************Enter event generation - end*******************/

	LOG(INFO)<<"FP: Form Submitted "<<inputElement->form()->wasWebLoginSubmitted();
	if(!inputElement->form()->wasWebLoginSubmitted()){
	      //Enter Key didn't succeed to submit the form.
	      // In such cases... try another approach to submit the form.
	      //Wait for some time as wasWebLoginSubmitted is not always correct.
	      if (trigger_click_timer_.IsRunning()) {
                  trigger_click_timer_.Reset();
	      }else{
                  trigger_click_timer_.Start(FROM_HERE, 
			 	base::TimeDelta::FromMilliseconds(600),
			 	base::Bind(&WebViewImpl::triggerClickOnSubmit,
			 	base::Unretained(this), inputElement->form()));
           }
       }
}

void WebViewImpl::triggerClickOnSubmit(HTMLFormElement* form)
{
    if(!autoLoginFailureFlag()){
    //Aha!! It seems finally enter key has worked.
    //No need to click on the submit element.
        return;
    }
#if defined(S_FP_AUTOLOGIN_LINK_CLICK)
    Node *passwordNode = NULL;
    bool shouldSubmit = false;
#endif
    HTMLFormControlElement* firstSubmitButton =0;
    const Vector<FormAssociatedElement*>& formElements = form->associatedElements();
    bool password_element_found = false;
    for(size_t i = 0; i < formElements.size(); i++) {
        if(!formElements[i]->isFormControlElement()){
            continue;
        }
        HTMLFormControlElement* formElement = static_cast<HTMLFormControlElement*>(formElements[i]);
        if(formElement->hasTagName(HTMLNames::inputTag) && toHTMLInputElement(formElement)->isPasswordField())
        {
#if defined(S_FP_AUTOLOGIN_LINK_CLICK)
        passwordNode = toHTMLInputElement(formElement);
#endif
        password_element_found = true;
        continue;
        }
        if(!password_element_found)
            continue;

        // If We are here to generate click event on submit element, Form must be visible
        // So It doesn't make any sense to consider "Not Visible" elements.
        if(!formElement->isFocusable())
	 	continue;

        if(!formElement->hasTagName(HTMLNames::inputTag)){
            LOG(INFO)<<"FP: !formElement->hasTagName(HTMLNames::inputTag)";
            if(formElement->hasTagName(HTMLNames::buttonTag)){
                formElement->focus();
                formElement->click();
#if defined(S_FP_AUTOLOGIN_LINK_CLICK)
                shouldSubmit = true;
#endif
                LOG(INFO)<<"FP:BUTTON click GENERATE";
                break;
            }
            continue;
        }
    //TODO: Handle firstSubmitButton
        if(!firstSubmitButton && formElement->isSuccessfulSubmitButton()){
            formElement->focus();
            formElement->click();
 #if defined(S_FP_AUTOLOGIN_LINK_CLICK)
            shouldSubmit = true;
 #endif
            LOG(INFO)<<"FP:SUBMIT BUTTON click GENERATE";
            break;
        }
    }
#if defined(S_FP_AUTOLOGIN_LINK_CLICK)
    //we tried enter event and button click still form is not submitted 
    //now we try to click on link if it is available and meet our assumption criteria
    if(!shouldSubmit && passwordNode){
        submitLinkIfPossible(passwordNode);
    }
#endif

#if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
    //It seems the form has an anchor tag and enter key didn't succeed to AutoLogin
    // Till we handle anchor tag cases, Alert User about AutoLogin Failure.
    // Note: form->submit() is removed as there were many side effects observed like in ask.com, hyundaihmall.com
    //This Part is trickier, As we don't how much time it may take to validate the form(if required from the websites)
    //No choice but to guess !!
    if(autologin_alert_timer_.IsRunning()) {
        autologin_alert_timer_.Reset();
    }else{
         autologin_alert_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(300), this,
        &WebViewImpl::autoLoginAlertOnTimer);
    }
#endif
}

#if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
void WebViewImpl::autoLoginAlertOnTimer()
{
    if(autoLoginFailureFlag()){
        LOG(INFO)<<"FP Alert User for Autologin Failure"; 
        if(m_client)
            m_client->autoLoginFailure();
    }
}
#endif

#if defined(S_FP_AUTOLOGIN_LINK_CLICK)
void WebViewImpl::submitLinkIfPossible(Node *passwordNode){
    bool shouldSubmit = false;
    for(Node *pNode = passwordNode; pNode; pNode = NodeTraversal::next(*pNode))
    {
        if(pNode->hasTagName(HTMLNames::aTag) && pNode->hasChildNodes() ){
            unsigned int i;
            HTMLElement * loginElement = toHTMLElement(pNode);
            if(!loginElement->isFocusable() ) {
                LOG(INFO)<<"FP: loginLink not focusable serch for next";
                continue;
            }
            const ElementData * taginfo = loginElement->elementData();
            const Attribute* href_attr = taginfo->getAttributeItem(HTMLNames::hrefAttr);
            const Attribute* id_attr = taginfo->getAttributeItem(HTMLNames::idAttr);
            const Attribute* class_attr = taginfo->getAttributeItem(HTMLNames::classAttr);
            std::vector<String> assumption_login;
            assumption_login.push_back("login");
            assumption_login.push_back("submit");
            assumption_login.push_back("signin");
            assumption_login.push_back("sign-in");
            assumption_login.push_back("button");
            if(href_attr) {
            for(i=0;i<assumption_login.size();i++)
                if(href_attr->value().contains(assumption_login.at(i),false)){
                    shouldSubmit = true;
                    break;
                }
            }
            if(!shouldSubmit && id_attr) {
                for(i=0;i<assumption_login.size();i++)
                    if(id_attr->value().contains(assumption_login.at(i),false)){
                        shouldSubmit = true;
                            break;
                    }
            }
            if( !shouldSubmit && class_attr){
                for(i=0;i<assumption_login.size();i++)
                    if(class_attr->value().contains(assumption_login.at(i),false)) {
                        shouldSubmit = true;
                        break;
                    }
            }
            if(shouldSubmit){
                loginElement->focus();
                loginElement->click();
                LOG(INFO)<<"FP: Attribute  link submitted ";
                break;
            }
            else
                LOG(INFO)<<"FP: Attribute link not submitted ";
            }
    }
}//vishal
#endif

#endif
//S_FP_AUTOLOGIN_SUPPORT--

#if defined(S_INTUITIVE_HOVER)
void WebViewImpl::performHitTestOnHover(const WebMouseEvent& event) {
    HoverContentType type = ContentTypeNone;
    IntPoint point(event.x, event.y);
    HitTestResult result = hitTestResultAt(point);
    Node* hitNode = result.innerNonSharedNode();
    KURL linkURL = result.absoluteLinkURL();
    if (hitNode) {
        if (hitNode->isContentEditable() || hitNode->hasTagName(HTMLNames::inputTag) || hitNode->hasTagName(HTMLNames::textareaTag)) {
            type = ContentTypeEditable;
        }
        else if ((hitNode->isLink() || !linkURL.isEmpty() ) && !hitNode->isTextNode()) {
            type = ContentTypeLinkImage;
        }
        else if (hitNode->isLink() || !linkURL.isEmpty()) {
            type = ContentTypeLink;
        }
        else if (hitNode->isTextNode()) {
            type = ContentTypeText;
        }
        else if (hitNode->renderer() && hitNode->renderer()->isImage()) {
            type = ContentTypeImage;
        }
        else {
            type = ContentTypeNone;
        }
    }
    m_client->hoverHitTestResult((int)type);
}
#endif

// MULTI-SELECTION >>
#if defined(SBROWSER_MULTI_SELECTION)
bool WebViewImpl::getSelectionStartContentBounds(WebRect& anchor)
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();

    if (selection.isCaret()) {
        anchor = selection.absoluteCaretBounds();
    } else {
        RefPtr<Range> selectedRange = selection.toNormalizedRange();
        if (!selectedRange)
            return false;

#if defined (S_TEXT_SELECTION_MODIFIEDBOUNDS)
    anchor = frame->editor().firstRectForRange(selectedRange.get());
#else
        RefPtr<Range> range(Range::create(selectedRange->startContainer()->document(),
            selectedRange->startContainer(),
            selectedRange->startOffset(),
            selectedRange->startContainer(),
            selectedRange->startOffset()));
        anchor = frame->editor().firstRectForRange(range.get());

#endif
    }

    return true;
}
#endif
// MULTI-SELECTION <<
} // namespace blink
