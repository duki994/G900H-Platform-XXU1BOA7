# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from telemetry.page import test_expectations
from telemetry.page.actions import all_page_actions
from telemetry.page.actions import interact
from telemetry.page.actions import navigate
from telemetry.page.actions import page_action


def _GetActionFromData(action_data):
  action_name = action_data['action']
  action = all_page_actions.FindClassWithName(action_name)
  if not action:
    logging.critical('Could not find an action named %s.', action_name)
    logging.critical('Check the page set for a typo and check the error '
                     'log for possible Python loading/compilation errors.')
    raise Exception('Action "%s" not found.' % action_name)
  return action(action_data)


def GetSubactionFromData(page, subaction_data, interactive):
  subaction_name = subaction_data['action']
  if hasattr(page, subaction_name):
    return GetCompoundActionFromPage(page, subaction_name, interactive)
  else:
    return [_GetActionFromData(subaction_data)]


def GetCompoundActionFromPage(page, action_name, interactive=False):
  if interactive:
    return [interact.InteractAction()]

  if not action_name:
    return []

  action_data_list = getattr(page, action_name)
  if not isinstance(action_data_list, list):
    action_data_list = [action_data_list]

  action_list = []
  for subaction_data in action_data_list:
    for _ in xrange(subaction_data.get('repeat', 1)):
      action_list += GetSubactionFromData(page, subaction_data, interactive)
  return action_list


class Failure(Exception):
  """Exception that can be thrown from PageMeasurement to indicate an
  undesired but designed-for problem."""
  pass


class PageTest(object):
  """A class styled on unittest.TestCase for creating page-specific tests."""

  def __init__(self,
               test_method_name,
               action_name_to_run='',
               needs_browser_restart_after_each_run=False,
               discard_first_result=False,
               clear_cache_before_each_run=False,
               attempts=3):
    self.options = None
    try:
      self._test_method = getattr(self, test_method_name)
    except AttributeError:
      raise ValueError, 'No such method %s.%s' % (
        self.__class_, test_method_name)  # pylint: disable=E1101
    self._action_name_to_run = action_name_to_run
    self._needs_browser_restart_after_each_run = (
        needs_browser_restart_after_each_run)
    self._discard_first_result = discard_first_result
    self._clear_cache_before_each_run = clear_cache_before_each_run
    self._close_tabs_before_run = True
    self._attempts = attempts
    assert self._attempts > 0, 'Test attempts must be greater than 0'
    # If the test overrides the TabForPage method, it is considered a multi-tab
    # test.  The main difference between this and a single-tab test is that we
    # do not attempt recovery for the former if a tab or the browser crashes,
    # because we don't know the current state of tabs (how many are open, etc.)
    self.is_multi_tab_test = (self.__class__ is not PageTest and
                              self.TabForPage.__func__ is not
                              self.__class__.__bases__[0].TabForPage.__func__)
    # _exit_requested is set to true when the test requests an early exit.
    self._exit_requested = False

  @property
  def discard_first_result(self):
    """When set to True, the first run of the test is discarded.  This is
    useful for cases where it's desirable to have some test resource cached so
    the first run of the test can warm things up. """
    return self._discard_first_result

  @discard_first_result.setter
  def discard_first_result(self, discard):
    self._discard_first_result = discard

  @property
  def clear_cache_before_each_run(self):
    """When set to True, the browser's disk and memory cache will be cleared
    before each run."""
    return self._clear_cache_before_each_run

  @property
  def close_tabs_before_run(self):
    """When set to True, all tabs are closed before running the test for the
    first time."""
    return self._close_tabs_before_run

  @close_tabs_before_run.setter
  def close_tabs_before_run(self, close_tabs):
    self._close_tabs_before_run = close_tabs

  @property
  def attempts(self):
    """Maximum number of times test will be attempted."""
    return self._attempts

  @attempts.setter
  def attempts(self, count):
    assert self._attempts > 0, 'Test attempts must be greater than 0'
    self._attempts = count

  def RestartBrowserBeforeEachPage(self):
    """ Should the browser be restarted for the page?

    This returns true if the test needs to unconditionally restart the
    browser for each page. It may be called before the browser is started.
    """
    return self._needs_browser_restart_after_each_run

  def StopBrowserAfterPage(self, browser, page):  # pylint: disable=W0613
    """Should the browser be stopped after the page is run?

    This is called after a page is run to decide whether the browser needs to
    be stopped to clean up its state. If it is stopped, then it will be
    restarted to run the next page.

    A test that overrides this can look at both the page and the browser to
    decide whether it needs to stop the browser.
    """
    return False

  def AddCommandLineOptions(self, parser):
    """Override to expose command-line options for this test.

    The provided parser is an optparse.OptionParser instance and accepts all
    normal results. The parsed options are available in Run as
    self.options."""
    pass

  def CustomizeBrowserOptions(self, options):
    """Override to add test-specific options to the BrowserOptions object"""
    pass

  def CustomizeBrowserOptionsForPageSet(self, page_set, options):
    """Set options required for this page set.

    These options will be used every time the browser is started while running
    this page set. They may, however, be further modified by
    CustomizeBrowserOptionsForSinglePage or by the profiler.
    """
    for page in page_set:
      if not self.CanRunForPage(page):
        return
      interactive = options and options.interactive
      for action in GetCompoundActionFromPage(
          page, self._action_name_to_run, interactive):
        action.CustomizeBrowserOptionsForPageSet(options)

  def CustomizeBrowserOptionsForSinglePage(self, page, options):
    """Set options specific to the test and the given page.

    This will be called with the current page when the browser is (re)started.
    Changing options at this point only makes sense if the browser is being
    restarted for each page.
    """
    interactive = options and options.interactive
    for action in GetCompoundActionFromPage(
        page, self._action_name_to_run, interactive):
      action.CustomizeBrowserOptionsForSinglePage(options)

  def WillStartBrowser(self, browser):
    """Override to manipulate the browser environment before it launches."""
    pass

  def DidStartBrowser(self, browser):
    """Override to customize the browser right after it has launched."""
    pass

  def CanRunForPage(self, page):  # pylint: disable=W0613
    """Override to customize if the test can be ran for the given page."""
    return True

  def WillRunTest(self):
    """Override to do operations before the page set(s) are navigated."""
    pass

  def DidRunTest(self, browser, results):
    """Override to do operations after all page set(s) are completed.

    This will occur before the browser is torn down.
    """
    pass

  def WillRunPageRepeats(self, page):
    """Override to do operations before each page is iterated over."""
    pass

  def DidRunPageRepeats(self, page):
    """Override to do operations after each page is iterated over."""
    pass

  def DidStartHTTPServer(self, tab):
    """Override to do operations after the HTTP server is started."""
    pass

  def WillNavigateToPage(self, page, tab):
    """Override to do operations before the page is navigated, notably Telemetry
    will already have performed the following operations on the browser before
    calling this function:
    * Ensure only one tab is open.
    * Call WaitForDocumentReadyStateToComplete on the tab."""
    pass

  def DidNavigateToPage(self, page, tab):
    """Override to do operations right after the page is navigated and after
    all waiting for completion has occurred."""
    pass

  def WillRunActions(self, page, tab):
    """Override to do operations before running the actions on the page."""
    pass

  def DidRunActions(self, page, tab):
    """Override to do operations after running the actions on the page."""
    pass

  def WillRunAction(self, page, tab, action):
    """Override to do operations before running the action on the page."""
    pass

  def DidRunAction(self, page, tab, action):
    """Override to do operations after running the action on the page."""
    pass

  def CreatePageSet(self, args, options):   # pylint: disable=W0613
    """Override to make this test generate its own page set instead of
    allowing arbitrary page sets entered from the command-line."""
    return None

  def CreateExpectations(self, page_set):   # pylint: disable=W0613
    """Override to make this test generate its own expectations instead of
    any that may have been defined in the page set."""
    return test_expectations.TestExpectations()

  def TabForPage(self, page, browser):   # pylint: disable=W0613
    """Override to select a different tab for the page.  For instance, to
    create a new tab for every page, return browser.tabs.New()."""
    return browser.tabs[0]

  def ValidatePageSet(self, page_set):
    """Override to examine the page set before the test run.  Useful for
    example to validate that the pageset can be used with the test."""
    pass

  def Run(self, options, page, tab, results):
    self.options = options
    interactive = options and options.interactive
    compound_action = GetCompoundActionFromPage(
        page, self._action_name_to_run, interactive)
    self.WillRunActions(page, tab)
    self._RunCompoundAction(page, tab, compound_action)
    self.DidRunActions(page, tab)
    try:
      self._test_method(page, tab, results)
    finally:
      self.options = None

  def _RunCompoundAction(self, page, tab, actions, run_setup_methods=True):
    for i, action in enumerate(actions):
      prev_action = actions[i - 1] if i > 0 else None
      next_action = actions[i + 1] if i < len(actions) - 1 else None

      if (action.RunsPreviousAction() and
          next_action and next_action.RunsPreviousAction()):
        raise page_action.PageActionFailed('Consecutive actions cannot both '
                                           'have RunsPreviousAction() == True.')

      if not (next_action and next_action.RunsPreviousAction()):
        action.WillRunAction(page, tab)
        if run_setup_methods:
          self.WillRunAction(page, tab, action)
        try:
          action.RunAction(page, tab, prev_action)
        finally:
          if run_setup_methods:
            self.DidRunAction(page, tab, action)

      # Note that we must not call util.CloseConnections here. Many tests
      # navigate to a URL in the first action and then wait for a condition
      # in the second action. Calling util.CloseConnections here often
      # aborts resource loads performed by the page.

  def RunNavigateSteps(self, page, tab):
    """Navigates the tab to the page URL attribute.

    Runs the 'navigate_steps' page attribute as a compound action.
    """
    navigate_actions = GetCompoundActionFromPage(page, 'navigate_steps')
    if not any(isinstance(action, navigate.NavigateAction)
        for action in navigate_actions):
      raise page_action.PageActionFailed(
          'No NavigateAction in navigate_steps')

    self._RunCompoundAction(page, tab, navigate_actions, False)

  def IsExiting(self):
    return self._exit_requested

  def RequestExit(self):
    self._exit_requested = True

  @property
  def action_name_to_run(self):
    return self._action_name_to_run
