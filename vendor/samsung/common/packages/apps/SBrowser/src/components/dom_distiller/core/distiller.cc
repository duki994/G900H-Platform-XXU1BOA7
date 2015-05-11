// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/distiller_url_fetcher.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
// Maximum number of distilled pages in an article.
const size_t kMaxPagesInArticle = 32;
}

namespace dom_distiller {

DistillerFactoryImpl::DistillerFactoryImpl(
    scoped_ptr<DistillerPageFactory> distiller_page_factory,
    scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory)
  : distiller_page_factory_(distiller_page_factory.Pass()),
    distiller_url_fetcher_factory_(distiller_url_fetcher_factory.Pass()) {}

DistillerFactoryImpl::~DistillerFactoryImpl() {}

scoped_ptr<Distiller> DistillerFactoryImpl::CreateDistiller() {
  scoped_ptr<DistillerImpl> distiller(new DistillerImpl(
      *distiller_page_factory_, *distiller_url_fetcher_factory_));
  distiller->Init();
  return distiller.PassAs<Distiller>();
}

DistillerImpl::DistilledPageData::DistilledPageData() {}

DistillerImpl::DistilledPageData::~DistilledPageData() {}

DistillerImpl::DistillerImpl(
    const DistillerPageFactory& distiller_page_factory,
    const DistillerURLFetcherFactory& distiller_url_fetcher_factory)
    : distiller_url_fetcher_factory_(distiller_url_fetcher_factory),
      max_pages_in_article_(kMaxPagesInArticle) {
  page_distiller_.reset(new PageDistiller(distiller_page_factory));
}

DistillerImpl::~DistillerImpl() { DCHECK(AreAllPagesFinished()); }

void DistillerImpl::Init() {
  DCHECK(AreAllPagesFinished());
  page_distiller_->Init();
}

void DistillerImpl::SetMaxNumPagesInArticle(size_t max_num_pages) {
  max_pages_in_article_ = max_num_pages;
}

bool DistillerImpl::AreAllPagesFinished() const {
  return started_pages_index_.empty() && waiting_pages_.empty();
}

size_t DistillerImpl::TotalPageCount() const {
  return waiting_pages_.size() + started_pages_index_.size() +
         finished_pages_index_.size();
}

void DistillerImpl::AddToDistillationQueue(int page_num, const GURL& url) {
  if (!IsPageNumberInUse(page_num) && url.is_valid() &&
      TotalPageCount() < max_pages_in_article_ &&
      seen_urls_.find(url.spec()) == seen_urls_.end()) {
    waiting_pages_[page_num] = url;
  }
}

bool DistillerImpl::IsPageNumberInUse(int page_num) const {
  return waiting_pages_.find(page_num) != waiting_pages_.end() ||
         started_pages_index_.find(page_num) != started_pages_index_.end() ||
         finished_pages_index_.find(page_num) != finished_pages_index_.end();
}

DistillerImpl::DistilledPageData* DistillerImpl::GetPageAtIndex(size_t index)
    const {
  DCHECK_LT(index, pages_.size());
  DistilledPageData* page_data = pages_[index];
  DCHECK(page_data);
  return page_data;
}

void DistillerImpl::DistillPage(const GURL& url,
                            const DistillerCallback& distillation_cb) {
  DCHECK(AreAllPagesFinished());
  distillation_cb_ = distillation_cb;

  AddToDistillationQueue(0, url);
  DistillNextPage();
}

void DistillerImpl::DistillNextPage() {
  if (!waiting_pages_.empty()) {
    std::map<int, GURL>::iterator front = waiting_pages_.begin();
    int page_num = front->first;
    const GURL url = front->second;

    waiting_pages_.erase(front);
    DCHECK(url.is_valid());
    DCHECK(started_pages_index_.find(page_num) == started_pages_index_.end());
    DCHECK(finished_pages_index_.find(page_num) == finished_pages_index_.end());
    seen_urls_.insert(url.spec());
    pages_.push_back(new DistilledPageData());
    started_pages_index_[page_num] = pages_.size() - 1;
    page_distiller_->DistillPage(
        url,
        base::Bind(&DistillerImpl::OnPageDistillationFinished,
                   base::Unretained(this),
                   page_num,
                   url));
  }
}

void DistillerImpl::OnPageDistillationFinished(
    int page_num,
    const GURL& page_url,
    scoped_ptr<DistilledPageInfo> distilled_page,
    bool distillation_successful) {
  DCHECK(distilled_page.get());
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  if (distillation_successful) {
    DistilledPageData* page_data =
        GetPageAtIndex(started_pages_index_[page_num]);
    DistilledPageProto* current_page = new DistilledPageProto();
    page_data->proto.reset(current_page);
    page_data->page_num = page_num;
    page_data->title = distilled_page->title;

    current_page->set_url(page_url.spec());
    current_page->set_html(distilled_page->html);

    GURL next_page_url(distilled_page->next_page_url);
    if (next_page_url.is_valid()) {
      // The pages should be in same origin.
      DCHECK_EQ(next_page_url.GetOrigin(), page_url.GetOrigin());
      AddToDistillationQueue(page_num + 1, next_page_url);
    }

    GURL prev_page_url(distilled_page->prev_page_url);
    if (prev_page_url.is_valid()) {
      DCHECK_EQ(prev_page_url.GetOrigin(), page_url.GetOrigin());
      AddToDistillationQueue(page_num - 1, prev_page_url);
    }

    for (size_t img_num = 0; img_num < distilled_page->image_urls.size();
         ++img_num) {
      std::string image_id =
          base::IntToString(page_num + 1) + "_" + base::IntToString(img_num);
      FetchImage(page_num, image_id, distilled_page->image_urls[img_num]);
    }

    AddPageIfDone(page_num);
    DistillNextPage();
  } else {
    started_pages_index_.erase(page_num);
    RunDistillerCallbackIfDone();
  }
}

void DistillerImpl::FetchImage(int page_num,
                               const std::string& image_id,
                               const std::string& item) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  DistillerURLFetcher* fetcher =
      distiller_url_fetcher_factory_.CreateDistillerURLFetcher();
  page_data->image_fetchers_.push_back(fetcher);

  fetcher->FetchURL(item,
                    base::Bind(&DistillerImpl::OnFetchImageDone,
                               base::Unretained(this),
                               page_num,
                               base::Unretained(fetcher),
                               image_id));
}

void DistillerImpl::OnFetchImageDone(int page_num,
                                     DistillerURLFetcher* url_fetcher,
                                     const std::string& id,
                                     const std::string& response) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  DCHECK(page_data->proto);
  DCHECK(url_fetcher);
  ScopedVector<DistillerURLFetcher>::iterator fetcher_it =
      std::find(page_data->image_fetchers_.begin(),
                page_data->image_fetchers_.end(),
                url_fetcher);

  DCHECK(fetcher_it != page_data->image_fetchers_.end());
  // Delete the |url_fetcher| by DeleteSoon since the OnFetchImageDone
  // callback is invoked by the |url_fetcher|.
  page_data->image_fetchers_.weak_erase(fetcher_it);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, url_fetcher);

  DistilledPageProto_Image* image = page_data->proto->add_image();
  image->set_name(id);
  image->set_data(response);

  AddPageIfDone(page_num);
}

void DistillerImpl::AddPageIfDone(int page_num) {
  DCHECK(started_pages_index_.find(page_num) != started_pages_index_.end());
  DCHECK(finished_pages_index_.find(page_num) == finished_pages_index_.end());
  DistilledPageData* page_data = GetPageAtIndex(started_pages_index_[page_num]);
  if (page_data->image_fetchers_.empty()) {
    finished_pages_index_[page_num] = started_pages_index_[page_num];
    started_pages_index_.erase(page_num);
    RunDistillerCallbackIfDone();
  }
}

void DistillerImpl::RunDistillerCallbackIfDone() {
  DCHECK(!distillation_cb_.is_null());
  if (AreAllPagesFinished()) {
    bool first_page = true;
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    // Stitch the pages back into the article.
    for (std::map<int, size_t>::iterator it = finished_pages_index_.begin();
         it != finished_pages_index_.end();) {
      DistilledPageData* page_data = GetPageAtIndex(it->second);
      *(article_proto->add_pages()) = *(page_data->proto);

      if (first_page) {
        article_proto->set_title(page_data->title);
        first_page = false;
      }

      finished_pages_index_.erase(it++);
    }

    pages_.clear();
    DCHECK_LE(static_cast<size_t>(article_proto->pages_size()),
              max_pages_in_article_);

    DCHECK(pages_.empty());
    DCHECK(finished_pages_index_.empty());
    distillation_cb_.Run(article_proto.Pass());
    distillation_cb_.Reset();
  }
}

}  // namespace dom_distiller
