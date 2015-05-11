/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.InspectorView = function()
{
    WebInspector.View.call(this);
    this.markAsRoot();
    this.element.classList.add("fill", "inspector-view");
    this.element.setAttribute("spellcheck", false);

    window.addEventListener("resize", this._onWindowResize.bind(this), true);
    WebInspector.zoomManager.addEventListener(WebInspector.ZoomManager.Events.ZoomChanged, this._onZoomChanged, this);

    // We can use split view either for docking or screencast, but not together.
    var settingName = WebInspector.queryParamsObject["can_dock"] ? "InspectorView.splitView" : "InspectorView.screencastSplitView";
    this._splitView = new WebInspector.SplitView(false, true, settingName, 300, 300);
    this._updateConstraints();
    WebInspector.dockController.addEventListener(WebInspector.DockController.Events.DockSideChanged, this._updateSplitView.bind(this));

    this._splitView.element.id = "inspector-split-view";
    this._splitView.show(this.element);

    // Main part of main split is overlay view.
    this._overlayView = new WebInspector.InspectorView.OverlayView(this._splitView);
    this._overlayView.show(this._splitView.mainElement());

    // Sidebar of main split is artificial element used for positioning.
    this._devtoolsView = new WebInspector.ViewWithResizeCallback(this._onDevToolsViewResized.bind(this));
    this._devtoolsView.show(this._splitView.sidebarElement());
    WebInspector.Dialog.setModalHostView(this._devtoolsView);

    // DevTools sidebar is a vertical split of panels tabbed pane and a drawer.
    this._drawerSplitView = new WebInspector.SplitView(false, true, "Inspector.drawerSplitView", 200, 200);
    this._drawerSplitView.setSidebarElementConstraints(Preferences.minDrawerHeight, Preferences.minDrawerHeight);
    this._drawerSplitView.setMainElementConstraints(25, 25);
    this._drawerSplitView.show(this._devtoolsView.element);

    this._tabbedPane = new WebInspector.TabbedPane();
    this._tabbedPane.setRetainTabOrder(true, WebInspector.moduleManager.orderComparator(WebInspector.Panel, "name", "order"));
    this._tabbedPane.show(this._drawerSplitView.mainElement());
    this._drawer = new WebInspector.Drawer(this._drawerSplitView);

    // Patch tabbed pane header with toolbar actions.
    this._toolbarElement = document.createElement("div");
    this._toolbarElement.className = "toolbar toolbar-background";
    var headerElement = this._tabbedPane.headerElement();
    headerElement.parentElement.insertBefore(this._toolbarElement, headerElement);

    this._leftToolbarElement = this._toolbarElement.createChild("div", "toolbar-controls-left");
    this._toolbarElement.appendChild(headerElement);
    this._rightToolbarElement = this._toolbarElement.createChild("div", "toolbar-controls-right");

    this._errorWarningCountElement = this._rightToolbarElement.createChild("div", "hidden");
    this._errorWarningCountElement.id = "error-warning-count";

    this._closeButtonToolbarItem = document.createElementWithClass("div", "toolbar-close-button-item");
    var closeButtonElement = this._closeButtonToolbarItem.createChild("div", "close-button");
    closeButtonElement.addEventListener("click", WebInspector.close.bind(WebInspector), true);
    this._rightToolbarElement.appendChild(this._closeButtonToolbarItem);

    this.appendToRightToolbar(this._drawer.toggleButtonElement());

    this._history = [];
    this._historyIterator = -1;
    document.addEventListener("keydown", this._keyDown.bind(this), false);
    document.addEventListener("keypress", this._keyPress.bind(this), false);
    this._panelDescriptors = {};

    // Windows and Mac have two different definitions of '[' and ']', so accept both of each.
    this._openBracketIdentifiers = ["U+005B", "U+00DB"].keySet();
    this._closeBracketIdentifiers = ["U+005D", "U+00DD"].keySet();
    this._lastActivePanelSetting = WebInspector.settings.createSetting("lastActivePanel", "elements");

    this._updateSplitView();

    this._loadPanelDesciptors();
}

WebInspector.InspectorView.Constraints = {
    OverlayWidth: 50,
    OverlayHeight: 50,
    DevToolsWidth: 180,
    DevToolsHeight: 50
};

WebInspector.InspectorView.prototype = {
    _loadPanelDesciptors: function()
    {
        WebInspector.startBatchUpdate();
        WebInspector.moduleManager.extensions(WebInspector.Panel).forEach(processPanelExtensions.bind(this));
        /**
         * @param {!WebInspector.ModuleManager.Extension} extension
         * @this {!WebInspector.InspectorView}
         */
        function processPanelExtensions(extension)
        {
            this.addPanel(new WebInspector.ModuleManagerExtensionPanelDescriptor(extension));
        }
        WebInspector.endBatchUpdate();
    },

    /**
     * @param {!Element} element
     */
    appendToLeftToolbar: function(element)
    {
        this._leftToolbarElement.appendChild(element);
    },

    /**
     * @param {!Element} element
     */
    appendToRightToolbar: function(element)
    {
        this._rightToolbarElement.insertBefore(element, this._closeButtonToolbarItem);
    },

    /**
     * @return {!Element}
     */
    devtoolsElement: function()
    {
        return this._devtoolsView.element;
    },

    /**
     * @param {!WebInspector.PanelDescriptor} panelDescriptor
     */
    addPanel: function(panelDescriptor)
    {
        var panelName = panelDescriptor.name();
        this._panelDescriptors[panelName] = panelDescriptor;
        this._tabbedPane.appendTab(panelName, panelDescriptor.title(), new WebInspector.View());
        if (this._lastActivePanelSetting.get() === panelName)
            this._tabbedPane.selectTab(panelName);
    },

    /**
     * @param {string} panelName
     * @return {?WebInspector.Panel}
     */
    panel: function(panelName)
    {
        var panelDescriptor = this._panelDescriptors[panelName];
        var panelOrder = this._tabbedPane.allTabs();
        if (!panelDescriptor && panelOrder.length)
            panelDescriptor = this._panelDescriptors[panelOrder[0]];
        return panelDescriptor ? panelDescriptor.panel() : null;
    },

    /**
     * @param {string} panelName
     * @return {?WebInspector.Panel}
     */
    showPanel: function(panelName)
    {
        var panel = this.panel(panelName);
        if (panel)
            this.setCurrentPanel(panel);
        return panel;
    },

    /**
     * @return {!WebInspector.Panel}
     */
    currentPanel: function()
    {
        return this._currentPanel;
    },

    showInitialPanel: function()
    {
        this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabSelected, this._tabSelected, this);
        this._tabSelected();
        this._drawer.showOnLoadIfNecessary();
    },

    _tabSelected: function()
    {
        var panelName = this._tabbedPane.selectedTabId;
        var panel = this._panelDescriptors[this._tabbedPane.selectedTabId].panel();
        this._tabbedPane.changeTabView(panelName, panel);

        this._currentPanel = panel;
        this._lastActivePanelSetting.set(panel.name);
        this._pushToHistory(panel.name);
        WebInspector.userMetrics.panelShown(panel.name);
        panel.focus();
    },

    /**
     * @param {!WebInspector.Panel} x
     */
    setCurrentPanel: function(x)
    {
        if (this._currentPanel === x)
            return;

        this._tabbedPane.changeTabView(x.name, x);
        this._tabbedPane.selectTab(x.name);
    },

    /**
     * @param {string} id
     */
    closeViewInDrawer: function(id)
    {
        this._drawer.closeView(id);
    },

    /**
     * @param {string} id
     * @param {string} title
     * @param {!WebInspector.View} view
     */
    showCloseableViewInDrawer: function(id, title, view)
    {
        this._drawer.showCloseableView(id, title, view);
    },

    showDrawer: function()
    {
        this._drawer.showDrawer();
    },

    /**
     * @return {boolean}
     */
    drawerVisible: function()
    {
        return this._drawer.isShowing();
    },

    /**
     * @param {string} id
     * @param {boolean=} immediate
     */
    showViewInDrawer: function(id, immediate)
    {
        this._drawer.showView(id, immediate);
    },

    /**
     * @return {string}
     */
    selectedViewInDrawer: function()
    {
        return this._drawer.selectedViewId();
    },

    closeDrawer: function()
    {
        this._drawer.closeDrawer();
    },

    /**
     * @return {!Element}
     */
    defaultFocusedElement: function()
    {
        return this._currentPanel ? this._currentPanel.defaultFocusedElement() : null;
    },

    _keyPress: function(event)
    {
        // BUG 104250: Windows 7 posts a WM_CHAR message upon the Ctrl+']' keypress.
        // Any charCode < 32 is not going to be a valid keypress.
        if (event.charCode < 32 && WebInspector.isWin())
            return;
        clearTimeout(this._keyDownTimer);
        delete this._keyDownTimer;
    },

    _keyDown: function(event)
    {
        if (!WebInspector.KeyboardShortcut.eventHasCtrlOrMeta(event))
            return;

        var keyboardEvent = /** @type {!KeyboardEvent} */ (event);
        // Ctrl/Cmd + 1-9 should show corresponding panel.
        var panelShortcutEnabled = WebInspector.settings.shortcutPanelSwitch.get();
        if (panelShortcutEnabled && !event.shiftKey && !event.altKey) {
            var panelIndex = -1;
            if (event.keyCode > 0x30 && event.keyCode < 0x3A)
                panelIndex = event.keyCode - 0x31;
            else if (event.keyCode > 0x60 && event.keyCode < 0x6A && keyboardEvent.location === KeyboardEvent.DOM_KEY_LOCATION_NUMPAD)
                panelIndex = event.keyCode - 0x61;
            if (panelIndex !== -1) {
                var panelName = this._tabbedPane.allTabs()[panelIndex];
                if (panelName) {
                    this.showPanel(panelName);
                    event.consume(true);
                }
                return;
            }
        }

        // BUG85312: On French AZERTY keyboards, AltGr-]/[ combinations (synonymous to Ctrl-Alt-]/[ on Windows) are used to enter ]/[,
        // so for a ]/[-related keydown we delay the panel switch using a timer, to see if there is a keypress event following this one.
        // If there is, we cancel the timer and do not consider this a panel switch.
        if (!WebInspector.isWin() || (!this._openBracketIdentifiers[event.keyIdentifier] && !this._closeBracketIdentifiers[event.keyIdentifier])) {
            this._keyDownInternal(event);
            return;
        }

        this._keyDownTimer = setTimeout(this._keyDownInternal.bind(this, event), 0);
    },

    _keyDownInternal: function(event)
    {
        if (this._openBracketIdentifiers[event.keyIdentifier]) {
            var isRotateLeft = !event.shiftKey && !event.altKey;
            if (isRotateLeft) {
                var panelOrder = this._tabbedPane.allTabs();
                var index = panelOrder.indexOf(this.currentPanel().name);
                index = (index === 0) ? panelOrder.length - 1 : index - 1;
                this.showPanel(panelOrder[index]);
                event.consume(true);
                return;
            }

            var isGoBack = event.altKey;
            if (isGoBack && this._canGoBackInHistory()) {
                this._goBackInHistory();
                event.consume(true);
            }
            return;
        }

        if (this._closeBracketIdentifiers[event.keyIdentifier]) {
            var isRotateRight = !event.shiftKey && !event.altKey;
            if (isRotateRight) {
                var panelOrder = this._tabbedPane.allTabs();
                var index = panelOrder.indexOf(this.currentPanel().name);
                index = (index + 1) % panelOrder.length;
                this.showPanel(panelOrder[index]);
                event.consume(true);
                return;
            }

            var isGoForward = event.altKey;
            if (isGoForward && this._canGoForwardInHistory()) {
                this._goForwardInHistory();
                event.consume(true);
            }
            return;
        }
    },

    _canGoBackInHistory: function()
    {
        return this._historyIterator > 0;
    },

    _goBackInHistory: function()
    {
        this._inHistory = true;
        this.setCurrentPanel(WebInspector.panels[this._history[--this._historyIterator]]);
        delete this._inHistory;
    },

    _canGoForwardInHistory: function()
    {
        return this._historyIterator < this._history.length - 1;
    },

    _goForwardInHistory: function()
    {
        this._inHistory = true;
        this.setCurrentPanel(WebInspector.panels[this._history[++this._historyIterator]]);
        delete this._inHistory;
    },

    _pushToHistory: function(panelName)
    {
        if (this._inHistory)
            return;

        this._history.splice(this._historyIterator + 1, this._history.length - this._historyIterator - 1);
        if (!this._history.length || this._history[this._history.length - 1] !== panelName)
            this._history.push(panelName);
        this._historyIterator = this._history.length - 1;
    },

    _onDevToolsViewResized: function()
    {
        WebInspector.Dialog.modalHostRepositioned();
    },

    _onWindowResize: function()
    {
        this.doResize();
    },

    _updateSplitView: function()
    {
        var dockSide = WebInspector.dockController.dockSide();
        if (dockSide !== WebInspector.DockController.State.Undocked) {
            var vertical = WebInspector.dockController.isVertical();
            this._splitView.setVertical(vertical);
            if (vertical) {
                // Docked to side.
                if (dockSide === WebInspector.DockController.State.DockedToRight)
                    this._overlayView.setMargins(false, true, false, false);
                else
                    this._overlayView.setMargins(false, false, false, true);
                this._splitView.setSecondIsSidebar(dockSide === WebInspector.DockController.State.DockedToRight);
                this._splitView.uninstallResizer(this._tabbedPane.headerElement());
                this._splitView.installResizer(this._splitView.resizerElement());
            } else {
                // Docked to bottom.
                this._overlayView.setMargins(false, false, false, false);
                this._splitView.setSecondIsSidebar(true);
                this._splitView.installResizer(this._splitView.resizerElement());
                this._splitView.installResizer(this._tabbedPane.headerElement());
            }
            this._splitView.showBoth();
        } else {
            this._overlayView.setMargins(false, false, false, false);
            this._splitView.hideMain();
            this._splitView.uninstallResizer(this._tabbedPane.headerElement());
            this._splitView.uninstallResizer(this._splitView.resizerElement());
        }
    },

    _onZoomChanged: function(event)
    {
        this._updateConstraints();
        var data = /** @type {{from: number, to: number}} */ (event.data);
        this._splitView.setSidebarSize(this._splitView.sidebarSize() * data.from / data.to, true);
        this._overlayView.updateMargins();
    },

    _updateConstraints: function()
    {
        var zoomFactor = WebInspector.zoomManager.zoomFactor();
        this._splitView.setSidebarElementConstraints(WebInspector.InspectorView.Constraints.DevToolsWidth / zoomFactor,
            WebInspector.InspectorView.Constraints.DevToolsHeight / zoomFactor);
        this._splitView.setMainElementConstraints(WebInspector.InspectorView.Constraints.OverlayWidth / zoomFactor,
            WebInspector.InspectorView.Constraints.OverlayHeight / zoomFactor);
    },

    /**
     * @param {!WebInspector.View} view
     * @param {boolean} vertical
     */
    showScreencastView: function(view, vertical)
    {
        if (view.parentView() !== this._overlayView)
            view.show(this._overlayView.element);
        this._splitView.setVertical(vertical);
        this._splitView.installResizer(this._splitView.resizerElement());
        this._splitView.showBoth();
    },

    hideScreencastView: function()
    {
        this._splitView.hideMain();
    },

    /**
     * @param {number} errors
     * @param {number} warnings
     */
    setErrorAndWarningCounts: function(errors, warnings)
    {
        if (!errors && !warnings) {
            this._errorWarningCountElement.classList.add("hidden");
            this._tabbedPane.headerResized();
            return;
        }

        this._errorWarningCountElement.classList.remove("hidden");
        this._errorWarningCountElement.removeChildren();

        if (errors) {
            var errorImageElement = this._errorWarningCountElement.createChild("div", "error-icon-small");
            var errorElement = this._errorWarningCountElement.createChild("span");
            errorElement.id = "error-count";
            errorElement.textContent = errors;
        }

        if (warnings) {
            var warningsImageElement = this._errorWarningCountElement.createChild("div", "warning-icon-small");
            var warningsElement = this._errorWarningCountElement.createChild("span");
            warningsElement.id = "warning-count";
            warningsElement.textContent = warnings;
        }

        if (errors) {
            if (warnings) {
                if (errors == 1) {
                    if (warnings == 1)
                        this._errorWarningCountElement.title = WebInspector.UIString("%d error, %d warning", errors, warnings);
                    else
                        this._errorWarningCountElement.title = WebInspector.UIString("%d error, %d warnings", errors, warnings);
                } else if (warnings == 1)
                    this._errorWarningCountElement.title = WebInspector.UIString("%d errors, %d warning", errors, warnings);
                else
                    this._errorWarningCountElement.title = WebInspector.UIString("%d errors, %d warnings", errors, warnings);
            } else if (errors == 1)
                this._errorWarningCountElement.title = WebInspector.UIString("%d error", errors);
            else
                this._errorWarningCountElement.title = WebInspector.UIString("%d errors", errors);
        } else if (warnings == 1)
            this._errorWarningCountElement.title = WebInspector.UIString("%d warning", warnings);
        else if (warnings)
            this._errorWarningCountElement.title = WebInspector.UIString("%d warnings", warnings);
        else
            this._errorWarningCountElement.title = null;

        this._tabbedPane.headerResized();
    },

    __proto__: WebInspector.View.prototype
};

/**
 * @constructor
 * @param {!WebInspector.SplitView} splitView
 * @extends {WebInspector.View}
 */
WebInspector.InspectorView.OverlayView = function(splitView)
{
    WebInspector.View.call(this);
    this._margins = {top: 0, left: 0, right: 0, bottom: 0};
    this._splitView = splitView;
}

WebInspector.InspectorView.OverlayView.prototype = {
    /**
     * @param {boolean} top
     * @param {boolean} right
     * @param {boolean} bottom
     * @param {boolean} left
     */
    setMargins: function(top, right, bottom, left)
    {
        this._margins = { top: top, right: right, bottom: bottom, left: left };
        this.updateMargins();
    },

    updateMargins: function()
    {
        var marginValue = Math.round(3 / WebInspector.zoomManager.zoomFactor()) + "px ";
        var margins = this._margins.top ? marginValue : "0 ";
        margins += this._margins.right ? marginValue : "0 ";
        margins += this._margins.bottom ? marginValue : "0 ";
        margins += this._margins.left ? marginValue : "0 ";
        this.element.style.margin = margins;
    },

    onResize: function()
    {
        var dockSide = WebInspector.dockController.dockSide();
        if (dockSide !== WebInspector.DockController.State.Undocked) {
            if (this._setContentsInsetsId)
                window.cancelAnimationFrame(this._setContentsInsetsId);
            this._setContentsInsetsId = window.requestAnimationFrame(this._setContentsInsets.bind(this));
        }
    },

    _setContentsInsets: function()
    {
        delete this._setContentsInsetsId;

        var zoomFactor = WebInspector.zoomManager.zoomFactor();
        var marginValue = Math.round(3 / zoomFactor);
        var insets = {
            top: this._margins.top ? marginValue : 0,
            left: this._margins.left ? marginValue : 0,
            right: this._margins.right ? marginValue : 0,
            bottom: this._margins.bottom ? marginValue : 0};

        var minSize = {
            width: WebInspector.InspectorView.Constraints.OverlayWidth - Math.round(insets.left * zoomFactor) - Math.round(insets.right * zoomFactor),
            height: WebInspector.InspectorView.Constraints.OverlayHeight - Math.round(insets.top * zoomFactor) - Math.round(insets.bottom * zoomFactor)};

        insets[this._splitView.sidebarSide()] += this._splitView.desiredSidebarSize();

        var zoomedInsets = {
            top: Math.round(insets.top * zoomFactor),
            left: Math.round(insets.left * zoomFactor),
            bottom: Math.round(insets.bottom * zoomFactor),
            right: Math.round(insets.right * zoomFactor)};

        InspectorFrontendHost.setContentsResizingStrategy(zoomedInsets, minSize);
    },

    __proto__: WebInspector.View.prototype
}

/**
 * @type {!WebInspector.InspectorView}
 */
WebInspector.inspectorView;
