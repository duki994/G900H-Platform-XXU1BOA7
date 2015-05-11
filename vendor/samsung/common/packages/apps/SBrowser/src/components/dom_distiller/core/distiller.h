// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_

#include <string>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/dom_distiller/core/distiller_url_fetcher.h"
#include "components/dom_distiller/core/page_distiller.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace dom_distiller {

class DistillerImpl;

class Distiller {
 public:
  typedef base::Callback<void(scoped_ptr<DistilledArticleProto>)>
      DistillerCallback;
  virtual ~Distiller() {}

  // Distills a page, and asynchrounously returns the article HTML to the
  // supplied callback.
  virtual void DistillPage(const GURL& url,
                           const DistillerCallback& callback) = 0;
};

class DistillerFactory {
 public:
  virtual scoped_ptr<Distiller> CreateDistiller() = 0;
  virtual ~DistillerFactory() {}
};

// Factory for creating a Distiller.
class DistillerFactoryImpl : public DistillerFactory {
 public:
  DistillerFactoryImpl(
      scoped_ptr<DistillerPageFactory> distiller_page_factory,
      scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory);
  virtual ~DistillerFactoryImpl();
  virtual scoped_ptr<Distiller> CreateDistiller() OVERRIDE;

 private:
  scoped_ptr<DistillerPageFactory> distiller_page_factory_;
  scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory_;
};

// Distills a article from a page and associated pages.
class DistillerImpl : public Distiller {
 public:
  DistillerImpl(
      const DistillerPageFactory& distiller_page_factory,
      const DistillerURLFetcherFactory& distiller_url_fetcher_factory);
  virtual ~DistillerImpl();

  // Creates an execution context. This must be called once before any calls are
  // made to distill the page.
  virtual void Init();

  virtual void DistillPage(const GURL& url,
                           const DistillerCallback& callback) OVERRIDE;

  void SetMaxNumPagesInArticle(size_t max_num_pages);

 private:
  // In case of multiple pages, the Distiller maintains state of multiple pages
  // as page numbers relative to the page number where distillation started.
  // E.g. if distillation starts at page 2 for a 3 page article. The relative
  // page numbers assigned to pages will be [-1,0,1].

  // Class representing the state of a page under distillation.
  struct DistilledPageData {
    DistilledPageData();
    virtual ~DistilledPageData();
    // Relative page number of the page.
    int page_num;
    std::string title;
    ScopedVector<DistillerURLFetcher> image_fetchers_;
    scoped_ptr<DistilledPageProto> proto;

   private:
    DISALLOW_COPY_AND_ASSIGN(DistilledPageData);
  };

  void OnFetchImageDone(int page_num,
                        DistillerURLFetcher* url_fetcher,
                        const std::string& id,
                        const std::string& response);

  void OnPageDistillationFinished(int page_num,
                                  const GURL& page_url,
                                  scoped_ptr<DistilledPageInfo> distilled_page,
                                  bool distillation_successful);

  virtual void FetchImage(int page_num,
                          const std::string& image_id,
                          const std::string& item);

  // Distills the next page.
  void DistillNextPage();

  // Adds the |url| to |pages_to_be_distilled| if |page_num| is a valid relative
  // page number and |url| is valid. Ignores duplicate pages and urls.
  void AddToDistillationQueue(int page_num, const GURL& url);

  // Check if |page_num| is a valid relative page number, i.e. page with
  // |page_num| is either under distillation or has already completed
  // distillation.
  bool IsPageNumberInUse(int page_num) const;

  bool AreAllPagesFinished() const;

  // Total number of pages in the article that the distiller knows of, this
  // includes pages that are pending distillation.
  size_t TotalPageCount() const;

  // Runs |distillation_cb_| if all distillation callbacks and image fetches are
  // complete.
  void RunDistillerCallbackIfDone();

  // Checks if page |distilled_page_data| has finished distillation, including
  // all image fetches.
  void AddPageIfDone(int page_num);

  DistilledPageData* GetPageAtIndex(size_t index) const;

  const DistillerURLFetcherFactory& distiller_url_fetcher_factory_;
  scoped_ptr<PageDistiller> page_distiller_;
  DistillerCallback distillation_cb_;

  // Set of pages that are under distillation or have finished distillation.
  // |started_pages_index_| and |finished_pages_index_| maintains the mapping
  // from page number to the indices in |pages_|.
  ScopedVector<DistilledPageData> pages_;

  // Maps page numbers of finished pages to the indices in |pages_|.
  std::map<int, size_t> finished_pages_index_;

  // Maps page numbers of pages under distillation to the indices in |pages_|.
  // If a page is |started_pages_| that means it is still waiting for an action
  // (distillation or image fetch) to finish.
  base::hash_map<int, size_t> started_pages_index_;

  // The list of pages that are still waiting for distillation to start.
  // This is a map, to make distiller prefer distilling lower page numbers
  // first.
  std::map<int, GURL> waiting_pages_;

  // Set to keep track of which urls are already seen by the distiller. Used to
  // prevent distiller from distilling the same url twice.
  base::hash_set<std::string> seen_urls_;

  size_t max_pages_in_article_;

  DISALLOW_COPY_AND_ASSIGN(DistillerImpl);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_H_
