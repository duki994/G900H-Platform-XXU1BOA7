var ConnectionView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function ConnectionView() {
    assertFirstConstructorCall(ConnectionView);

    // Call superclass's constructor.
    superClass.call(this, ConnectionView.MAIN_BOX_ID);

    this.maxNumDelayableRequestsPerClientInput = $(ConnectionView.INPUT1_ID);
    this.maxNumDelayableRequestsPerHostInput = $(ConnectionView.INPUT2_ID);
    this.maxSocketsPerHostInput = $(ConnectionView.INPUT3_ID);

    var form = $(ConnectionView.FORM1_ID);
    form.addEventListener('submit', this.onSubmit1_.bind(this), false);
    form = $(ConnectionView.FORM2_ID);
    form.addEventListener('submit', this.onSubmit2_.bind(this), false);
    form = $(ConnectionView.FORM3_ID);
    form.addEventListener('submit', this.onSubmit3_.bind(this), false);
  }

  ConnectionView.TAB_ID = 'tab-handle-connection';
  ConnectionView.TAB_NAME = 'Connection';
  ConnectionView.TAB_HASH = '#connection';

  // IDs for special HTML elements in connection_view.html
  ConnectionView.MAIN_BOX_ID = 'connection-view-tab-content';

  ConnectionView.FORM1_ID = 'connection-view-max-num-delayable-requests-per-client-form';
  ConnectionView.INPUT1_ID = 'connection-view-max-num-delayable-requests-per-client-input';
  ConnectionView.SUBMIT_BUTTON1_ID = 'connection-view-max-num-delayable-requests-per-client-submit';

  ConnectionView.FORM2_ID = 'connection-view-max-num-delayable-requests-per-host-form';
  ConnectionView.INPUT2_ID = 'connection-view-max-num-delayable-requests-per-host-input';
  ConnectionView.SUBMIT2_BUTTON1_ID = 'connection-view-max-num-delayable-requests-per-host-submit';

  ConnectionView.FORM3_ID = 'connection-view-max-sockets-per-group-form';
  ConnectionView.INPUT3_ID = 'connection-view-max-sockets-per-group-input';
  ConnectionView.SUBMIT_BUTTON3_ID = 'connection-view-max-sockets-per-group-submit';

  cr.addSingletonGetter(ConnectionView);

  ConnectionView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onSubmit1_: function(event) {
      g_browser.setMaxNumDelayableRequestsPerClient(this.maxNumDelayableRequestsPerClientInput.value);
    },

    onSubmit2_: function(event) {
      g_browser.setMaxNumDelayableRequestsPerHost(this.maxNumDelayableRequestsPerHostInput.value);
    },

    onSubmit3_: function(event) {
      g_browser.setMaxSocketsPerGroup(this.maxSocketsPerHostInput.value);
    },
  };

  return ConnectionView;
})();
