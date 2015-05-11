# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from document_parser import ParseDocument
from third_party.json_schema_compiler.model import UnixName


class DocumentRenderer(object):
  '''Performs document-level rendering such as the title, references,
  and table of contents: pulling that data out of the document, then
  replacing the $(title), $(ref:...) and $(table_of_contents) tokens with them.

  This can be thought of as a parallel to TemplateRenderer; while
  TemplateRenderer is responsible for interpreting templates and rendering files
  within the template engine, DocumentRenderer is responsible for interpreting
  higher-level document concepts like the title and TOC, then performing string
  replacement for them. The syntax for this replacement is $(...) where ... is
  the concept. Currently title and table_of_contents are supported.
  '''

  def __init__(self, table_of_contents_renderer, ref_resolver):
    self._table_of_contents_renderer = table_of_contents_renderer
    self._ref_resolver = ref_resolver

  def _RenderLinks(self, document, path):
    ''' Replaces all $(ref:...) references in |document| with html links
    '''
    START_REF = '$(ref:'
    END_REF = ')'
    MAX_REF_LENGTH = 100

    new_document = []

    # Keeps track of position within |document|
    cursor_index = 0
    start_ref_index = document.find(START_REF)

    while start_ref_index != -1:
      end_ref_index = document.find(END_REF, start_ref_index)

      if (end_ref_index == -1 or
          end_ref_index - start_ref_index > MAX_REF_LENGTH):
        end_ref_index = document.find(' ', start_ref_index)
        logging.error('%s:%s has no terminating ) at line %s' % (
            path,
            document[start_ref_index:end_ref_index],
            document.count('\n', 0, end_ref_index)))

        new_document.append(document[cursor_index:end_ref_index + 1])
      else:
        ref = document[start_ref_index:end_ref_index]
        ref_parts = ref[len(START_REF):].split(' ', 1)

        # Guess the api name from the html name, replacing '_' with '.' (e.g.
        # if the page is app_window.html, guess the api name is app.window)
        api_name = os.path.splitext(os.path.basename(path))[0].replace('_', '.')
        title = ref_parts[0] if len(ref_parts) == 1 else ref_parts[1]

        ref_dict = self._ref_resolver.SafeGetLink(ref_parts[0],
                                                  namespace=api_name,
                                                  title=title)

        new_document.append(document[cursor_index:start_ref_index])
        new_document.append('<a href=%s>%s</a>' % (ref_dict['href'],
                                                   ref_dict['text']))

      cursor_index = end_ref_index + 1
      start_ref_index = document.find(START_REF, cursor_index)

    new_document.append(document[cursor_index:])

    return ''.join(new_document)

  def Render(self, document, path, render_title=False):
    # Render links first so that parsing and later replacements aren't
    # affected by $(ref...) substitutions
    document = self._RenderLinks(document, path)

    parsed_document = ParseDocument(document, expect_title=render_title)
    toc_text, toc_warnings = self._table_of_contents_renderer.Render(
        parsed_document.sections)

    # Only 1 title and 1 table of contents substitution allowed; in the common
    # case, save necessarily running over the entire file.
    if parsed_document.title:
      document = document.replace('$(title)', parsed_document.title, 1)
    return (document.replace('$(table_of_contents)', toc_text, 1),
            parsed_document.warnings + toc_warnings)
