// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

using base::ASCIIToUTF16;
using std::string;

namespace bookmark_utils {
namespace {

class BookmarkUtilsTest : public ::testing::Test {
 public:
  virtual void TearDown() OVERRIDE {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

 private:
  // Clipboard requires a message loop.
  base::MessageLoopForUI loop;
};

TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesWordPhraseQuery) {
  BookmarkModel model(NULL);
  const BookmarkNode* node1 = model.AddURL(model.other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));
  const BookmarkNode* node2 = model.AddURL(model.other_node(),
                                           0,
                                           ASCIIToUTF16("baz buz"),
                                           GURL("http://www.cnn.com"));
  const BookmarkNode* folder1 = model.AddFolder(model.other_node(),
                                                0,
                                                ASCIIToUTF16("foo"));
  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.word_phrase_query.reset(new base::string16);
  // No nodes are returned for empty string.
  *query.word_phrase_query = ASCIIToUTF16("");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  EXPECT_TRUE(nodes.empty());
  nodes.clear();

  // No nodes are returned for space-only string.
  *query.word_phrase_query = ASCIIToUTF16("   ");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  EXPECT_TRUE(nodes.empty());
  nodes.clear();

  // Node "foo bar" and folder "foo" are returned in search results.
  *query.word_phrase_query = ASCIIToUTF16("foo");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(2U, nodes.size());
  EXPECT_TRUE(nodes[0] == folder1);
  EXPECT_TRUE(nodes[1] == node1);
  nodes.clear();

  // Ensure url matches return in search results.
  *query.word_phrase_query = ASCIIToUTF16("cnn");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node2);
  nodes.clear();

  // Ensure folder "foo" is not returned in more specific search.
  *query.word_phrase_query = ASCIIToUTF16("foo bar");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  // Bookmark Bar and Other Bookmarks are not returned in search results.
  *query.word_phrase_query = ASCIIToUTF16("Bookmark");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();
}

// Check exact matching against a URL query.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesUrl) {
  BookmarkModel model(NULL);
  const BookmarkNode* node1 = model.AddURL(model.other_node(),
                                        0,
                                        ASCIIToUTF16("Google"),
                                        GURL("https://www.google.com/"));
  model.AddURL(model.other_node(),
               0,
               ASCIIToUTF16("Google Calendar"),
               GURL("https://www.google.com/calendar"));

  model.AddFolder(model.other_node(),
                  0,
                  ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.url.reset(new base::string16);
  *query.url = ASCIIToUTF16("https://www.google.com/");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  *query.url = ASCIIToUTF16("calendar");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();

  // Empty URL should not match folders.
  *query.url = ASCIIToUTF16("");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();
}

// Check exact matching against a title query.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesTitle) {
  BookmarkModel model(NULL);
  const BookmarkNode* node1 = model.AddURL(model.other_node(),
                                        0,
                                        ASCIIToUTF16("Google"),
                                        GURL("https://www.google.com/"));
  model.AddURL(model.other_node(),
               0,
               ASCIIToUTF16("Google Calendar"),
               GURL("https://www.google.com/calendar"));

  const BookmarkNode* folder1 = model.AddFolder(model.other_node(),
                                           0,
                                           ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.title.reset(new base::string16);
  *query.title = ASCIIToUTF16("Google");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  *query.title = ASCIIToUTF16("Calendar");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();

  // Title should match folders.
  *query.title = ASCIIToUTF16("Folder");
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == folder1);
  nodes.clear();
}

// Check matching against a query with multiple predicates.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesConjunction) {
  BookmarkModel model(NULL);
  const BookmarkNode* node1 = model.AddURL(model.other_node(),
                                        0,
                                        ASCIIToUTF16("Google"),
                                        GURL("https://www.google.com/"));
  model.AddURL(model.other_node(),
               0,
               ASCIIToUTF16("Google Calendar"),
               GURL("https://www.google.com/calendar"));

  model.AddFolder(model.other_node(),
                  0,
                  ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;

  // Test all fields matching.
  query.word_phrase_query.reset(new base::string16(ASCIIToUTF16("www")));
  query.url.reset(new base::string16(ASCIIToUTF16("https://www.google.com/")));
  query.title.reset(new base::string16(ASCIIToUTF16("Google")));
  GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  scoped_ptr<base::string16>* fields[] = {
    &query.word_phrase_query, &query.url, &query.title };

  // Test two fields matching.
  for (size_t i = 0; i < arraysize(fields); i++) {
    scoped_ptr<base::string16> original_value(fields[i]->release());
    GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
    ASSERT_EQ(1U, nodes.size());
    EXPECT_TRUE(nodes[0] == node1);
    nodes.clear();
    fields[i]->reset(original_value.release());
  }

  // Test two fields matching with one non-matching field.
  for (size_t i = 0; i < arraysize(fields); i++) {
    scoped_ptr<base::string16> original_value(fields[i]->release());
    fields[i]->reset(new base::string16(ASCIIToUTF16("fjdkslafjkldsa")));
    GetBookmarksMatchingProperties(&model, query, 100, string(), &nodes);
    ASSERT_EQ(0U, nodes.size());
    nodes.clear();
    fields[i]->reset(original_value.release());
  }
}

TEST_F(BookmarkUtilsTest, CopyPaste) {
  BookmarkModel model(NULL);
  const BookmarkNode* node = model.AddURL(model.other_node(),
                                          0,
                                          ASCIIToUTF16("foo bar"),
                                          GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(&model, nodes, false);

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.bookmark_bar_node()));

  // Write some text to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(
        ui::Clipboard::GetForCurrentThread(),
        ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(ASCIIToUTF16("foo"));
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(CanPasteFromClipboard(model.bookmark_bar_node()));
}

TEST_F(BookmarkUtilsTest, GetParentForNewNodes) {
  BookmarkModel model(NULL);
  // This tests the case where selection contains one item and that item is a
  // folder.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model.bookmark_bar_node());
  int index = -1;
  const BookmarkNode* real_parent = GetParentForNewNodes(
      model.bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model.bookmark_bar_node());
  EXPECT_EQ(0, index);

  nodes.clear();

  // This tests the case where selection contains one item and that item is an
  // url.
  const BookmarkNode* page1 = model.AddURL(model.bookmark_bar_node(), 0,
                                           ASCIIToUTF16("Google"),
                                           GURL("http://google.com"));
  nodes.push_back(page1);
  real_parent = GetParentForNewNodes(model.bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model.bookmark_bar_node());
  EXPECT_EQ(1, index);

  // This tests the case where selection has more than one item.
  const BookmarkNode* folder1 = model.AddFolder(model.bookmark_bar_node(), 1,
                                                ASCIIToUTF16("Folder 1"));
  nodes.push_back(folder1);
  real_parent = GetParentForNewNodes(model.bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model.bookmark_bar_node());
  EXPECT_EQ(2, index);

  // This tests the case where selection doesn't contain any items.
  nodes.clear();
  real_parent = GetParentForNewNodes(model.bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model.bookmark_bar_node());
  EXPECT_EQ(2, index);
}

// Verifies that meta info is copied when nodes are cloned.
TEST_F(BookmarkUtilsTest, CloneMetaInfo) {
  BookmarkModel model(NULL);
  // Add a node containing meta info.
  const BookmarkNode* node = model.AddURL(model.other_node(),
                                          0,
                                          ASCIIToUTF16("foo bar"),
                                          GURL("http://www.google.com"));
  model.SetNodeMetaInfo(node, "somekey", "somevalue");
  model.SetNodeMetaInfo(node, "someotherkey", "someothervalue");

  // Clone node to a different folder.
  const BookmarkNode* folder = model.AddFolder(model.bookmark_bar_node(), 0,
                                               ASCIIToUTF16("Folder"));
  std::vector<BookmarkNodeData::Element> elements;
  BookmarkNodeData::Element node_data(node);
  elements.push_back(node_data);
  EXPECT_EQ(0, folder->child_count());
  CloneBookmarkNode(&model, elements, folder, 0, false);
  ASSERT_EQ(1, folder->child_count());

  // Verify that the cloned node contains the same meta info.
  const BookmarkNode* clone = folder->GetChild(0);
  ASSERT_TRUE(clone->GetMetaInfoMap());
  EXPECT_EQ(2u, clone->GetMetaInfoMap()->size());
  std::string value;
  EXPECT_TRUE(clone->GetMetaInfo("somekey", &value));
  EXPECT_EQ("somevalue", value);
  EXPECT_TRUE(clone->GetMetaInfo("someotherkey", &value));
  EXPECT_EQ("someothervalue", value);
}

}  // namespace
}  // namespace bookmark_utils
