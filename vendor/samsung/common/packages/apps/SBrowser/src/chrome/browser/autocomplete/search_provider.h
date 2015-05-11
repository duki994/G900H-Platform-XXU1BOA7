// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the Search autocomplete provider.  This provider is
// responsible for all autocomplete entries that start with "Search <engine>
// for ...", including searching for the current input string, search
// history, and search suggestions.  An instance of it gets created and
// managed by the autocomplete controller.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/autocomplete/base_search_provider.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/search_engines/template_url.h"

class Profile;
class SearchProviderTest;
class SuggestionDeletionHandler;
class TemplateURLService;

namespace base {
class Value;
}

namespace net {
class URLFetcher;
}

// Autocomplete provider for searches and suggestions from a search engine.
//
// After construction, the autocomplete controller repeatedly calls Start()
// with some user input, each time expecting to receive a small set of the best
// matches (either synchronously or asynchronously).
//
// Initially the provider creates a match that searches for the current input
// text.  It also starts a task to query the Suggest servers.  When that data
// comes back, the provider creates and returns matches for the best
// suggestions.
class SearchProvider : public BaseSearchProvider {
 public:
  // ID used in creating URLFetcher for default provider's suggest results.
  static const int kDefaultProviderURLFetcherID;

  // ID used in creating URLFetcher for keyword provider's suggest results.
  static const int kKeywordProviderURLFetcherID;

  // ID used in creating URLFetcher for deleting suggestion results.
  static const int kDeletionURLFetcherID;

  SearchProvider(AutocompleteProviderListener* listener, Profile* profile);

  // Extracts the suggest response metadata which SearchProvider previously
  // stored for |match|.
  static std::string GetSuggestMetadata(const AutocompleteMatch& match);

  // AutocompleteProvider:
  virtual void DeleteMatch(const AutocompleteMatch& match) OVERRIDE;
  virtual void ResetSession() OVERRIDE;

  // This URL may be sent with suggest requests; see comments on CanSendURL().
  void set_current_page_url(const GURL& current_page_url) {
    current_page_url_ = current_page_url;
  }

 protected:
  virtual ~SearchProvider();

 private:
  friend class SearchProviderTest;
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, CanSendURL);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInline);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInlineDomainClassify);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, NavigationInlineSchemeSubstring);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, RemoveStaleResultsTest);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, SuggestRelevanceExperiment);
  FRIEND_TEST_ALL_PREFIXES(SearchProviderTest, TestDeleteMatch);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, GetDestinationURL);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedPrefetchTest, ClearPrefetchedResults);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedPrefetchTest, SetPrefetchQuery);

  // Manages the providers (TemplateURLs) used by SearchProvider. Two providers
  // may be used:
  // . The default provider. This corresponds to the user's default search
  //   engine. This is always used, except for the rare case of no default
  //   engine.
  // . The keyword provider. This is used if the user has typed in a keyword.
  class Providers {
   public:
    explicit Providers(TemplateURLService* template_url_service);

    // Returns true if the specified providers match the two providers cached
    // by this class.
    bool equal(const base::string16& default_provider,
               const base::string16& keyword_provider) const {
      return (default_provider == default_provider_) &&
          (keyword_provider == keyword_provider_);
    }

    // Resets the cached providers.
    void set(const base::string16& default_provider,
             const base::string16& keyword_provider) {
      default_provider_ = default_provider;
      keyword_provider_ = keyword_provider;
    }

    TemplateURLService* template_url_service() { return template_url_service_; }
    const base::string16& default_provider() const { return default_provider_; }
    const base::string16& keyword_provider() const { return keyword_provider_; }

    // NOTE: These may return NULL even if the provider members are nonempty!
    const TemplateURL* GetDefaultProviderURL() const;
    const TemplateURL* GetKeywordProviderURL() const;

    // Returns true if there is a valid keyword provider.
    bool has_keyword_provider() const { return !keyword_provider_.empty(); }

   private:
    TemplateURLService* template_url_service_;

    // Cached across the life of a query so we behave consistently even if the
    // user changes their default while the query is running.
    base::string16 default_provider_;
    base::string16 keyword_provider_;

    DISALLOW_COPY_AND_ASSIGN(Providers);
  };

  class CompareScoredResults;

  typedef std::vector<history::KeywordSearchTermVisit> HistoryResults;
  typedef ScopedVector<SuggestionDeletionHandler> SuggestionDeletionHandlers;

  // Removes non-inlineable results until either the top result can inline
  // autocomplete the current input or verbatim outscores the top result.
  static void RemoveStaleResults(const base::string16& input,
                                 int verbatim_relevance,
                                 SuggestResults* suggest_results,
                                 NavigationResults* navigation_results);

  // Recalculates the match contents class of |results| to better display
  // against the current input and user's language.
  void UpdateMatchContentsClass(const base::string16& input_text,
                                Results* results);

  // Calculates the relevance score for the keyword verbatim result (if the
  // input matches one of the profile's keyword).
  static int CalculateRelevanceForKeywordVerbatim(AutocompleteInput::Type type,
                                                  bool prefer_keyword);

  // AutocompleteProvider:
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // BaseSearchProvider:
  virtual const TemplateURL* GetTemplateURL(
      const SuggestResult& result) const OVERRIDE;
  virtual const AutocompleteInput GetInput(
      const SuggestResult& result) const OVERRIDE;
  virtual bool ShouldAppendExtraParams(
      const SuggestResult& result) const OVERRIDE;
  virtual void StopSuggest() OVERRIDE;
  virtual void ClearAllResults() OVERRIDE;

  // This gets called when we have requested a suggestion deletion from the
  // server to handle the results of the deletion.
  void OnDeletionComplete(bool success,
                          SuggestionDeletionHandler* handler);

  // Records in UMA whether the deletion request resulted in success.
  // This is virtual so test code can override it to check that we
  // correctly handle the request result.
  virtual void RecordDeletionResult(bool success);

  // Removes the deleted match from the list of |matches_|.
  void DeleteMatchFromMatches(const AutocompleteMatch& match);

  // Called when timer_ expires.
  void Run();

  // Runs the history query, if necessary. The history query is synchronous.
  // This does not update |done_|.
  void DoHistoryQuery(bool minimal_changes);

  // Determines whether an asynchronous subcomponent query should run for the
  // current input.  If so, starts it if necessary; otherwise stops it.
  // NOTE: This function does not update |done_|.  Callers must do so.
  void StartOrStopSuggestQuery(bool minimal_changes);

  // Returns true when the current query can be sent to the Suggest service.
  // This will be false e.g. when Suggest is disabled, the query contains
  // potentially private data, etc.
  bool IsQuerySuitableForSuggest() const;

  // Removes stale results for both default and keyword providers.  See comments
  // on RemoveStaleResults().
  void RemoveAllStaleResults();

  // Apply calculated relevance scores to the current results.
  void ApplyCalculatedRelevance();
  void ApplyCalculatedSuggestRelevance(SuggestResults* list);
  void ApplyCalculatedNavigationRelevance(NavigationResults* list);

  // Starts a new URLFetcher requesting suggest results from |template_url|;
  // callers own the returned URLFetcher, which is NULL for invalid providers.
  net::URLFetcher* CreateSuggestFetcher(int id,
                                        const TemplateURL* template_url,
                                        const AutocompleteInput& input);

  // Parses results from the suggest server and updates the appropriate suggest
  // and navigation result lists, depending on whether |is_keyword| is true.
  // Returns whether the appropriate result list members were updated.
  bool ParseSuggestResults(base::Value* root_val, bool is_keyword);

  // Converts the parsed results to a set of AutocompleteMatches, |matches_|.
  void ConvertResultsToAutocompleteMatches();

  // Returns an iterator to the first match in |matches_| which might
  // be chosen as default.  If
  // |autocomplete_result_will_reorder_for_default_match| is false,
  // this simply means the first match; otherwise, it means the first
  // match for which the |allowed_to_be_default_match| member is true.
  ACMatches::const_iterator FindTopMatch(
    bool autocomplete_result_will_reorder_for_default_match) const;

  // Checks if suggested relevances violate certain expected constraints.
  // See UpdateMatches() for the use and explanation of these constraints.
  bool IsTopMatchNavigationInKeywordMode(
      bool autocomplete_result_will_reorder_for_default_match) const;
  bool HasKeywordDefaultMatchInKeywordMode() const;
  bool IsTopMatchScoreTooLow(
      bool autocomplete_result_will_reorder_for_default_match) const;
  bool IsTopMatchSearchWithURLInput(
      bool autocomplete_result_will_reorder_for_default_match) const;
  bool HasValidDefaultMatch(
      bool autocomplete_result_will_reorder_for_default_match) const;

  // Updates |matches_| from the latest results; applies calculated relevances
  // if suggested relevances cause undesriable behavior. Updates |done_|.
  void UpdateMatches();

  // Converts an appropriate number of navigation results in
  // |navigation_results| to matches and adds them to |matches|.
  void AddNavigationResultsToMatches(
      const NavigationResults& navigation_results,
      ACMatches* matches);

  // Adds a match for each result in |results| to |map|. |is_keyword| indicates
  // whether the results correspond to the keyword provider or default provider.
  void AddHistoryResultsToMap(const HistoryResults& results,
                              bool is_keyword,
                              int did_not_accept_suggestion,
                              MatchMap* map);

  // Calculates relevance scores for all |results|.
  SuggestResults ScoreHistoryResults(const HistoryResults& results,
                                     bool base_prevent_inline_autocomplete,
                                     bool input_multiple_words,
                                     const base::string16& input_text,
                                     bool is_keyword);

  // Adds matches for |results| to |map|.
  void AddSuggestResultsToMap(const SuggestResults& results,
                              const std::string& metadata,
                              MatchMap* map);

  // Gets the relevance score for the verbatim result.  This value may be
  // provided by the suggest server or calculated locally; if
  // |relevance_from_server| is non-NULL, it will be set to indicate which of
  // those is true.
  int GetVerbatimRelevance(bool* relevance_from_server) const;

  // Calculates the relevance score for the verbatim result from the
  // default search engine.  This version takes into account context:
  // i.e., whether the user has entered a keyword-based search or not.
  int CalculateRelevanceForVerbatim() const;

  // Calculates the relevance score for the verbatim result from the default
  // search engine *ignoring* whether the input is a keyword-based search
  // or not.  This function should only be used to determine the minimum
  // relevance score that the best result from this provider should have.
  // For normal use, prefer the above function.
  int CalculateRelevanceForVerbatimIgnoringKeywordModeState() const;

  // Gets the relevance score for the keyword verbatim result.
  // |relevance_from_server| is handled as in GetVerbatimRelevance().
  // TODO(mpearson): Refactor so this duplication isn't necessary or
  // restructure so one static function takes all the parameters it needs
  // (rather than looking at internal state).
  int GetKeywordVerbatimRelevance(bool* relevance_from_server) const;

  // |time| is the time at which this query was last seen.  |is_keyword|
  // indicates whether the results correspond to the keyword provider or default
  // provider. |use_aggressive_method| says whether this function can use a
  // method that gives high scores (1200+) rather than one that gives lower
  // scores.  When using the aggressive method, scores may exceed 1300
  // unless |prevent_search_history_inlining| is set.
  int CalculateRelevanceForHistory(const base::Time& time,
                                   bool is_keyword,
                                   bool use_aggressive_method,
                                   bool prevent_search_history_inlining) const;

  // Returns an AutocompleteMatch for a navigational suggestion.
  AutocompleteMatch NavigationToMatch(const NavigationResult& navigation);

  // Resets the scores of all |keyword_navigation_results_| matches to
  // be below that of the top keyword query match (the verbatim match
  // as expressed by |keyword_verbatim_relevance_| or keyword query
  // suggestions stored in |keyword_suggest_results_|).  If there
  // are no keyword suggestions and keyword verbatim is suppressed,
  // then drops the suggested relevance scores for the navsuggestions
  // and drops the request to suppress verbatim, thereby introducing the
  // keyword verbatim match which will naturally outscore the navsuggestions.
  void DemoteKeywordNavigationMatchesPastTopQuery();

  // Updates the value of |done_| from the internal state.
  void UpdateDone();

  // The amount of time to wait before sending a new suggest request after the
  // previous one.  Non-const because some unittests modify this value.
  static int kMinimumTimeBetweenSuggestQueriesMs;

  // Maintains the TemplateURLs used.
  Providers providers_;

  // The user's input.
  AutocompleteInput input_;

  // Input when searching against the keyword provider.
  AutocompleteInput keyword_input_;

  // Searches in the user's history that begin with the input text.
  HistoryResults keyword_history_results_;
  HistoryResults default_history_results_;

  // Number of suggest results that haven't yet arrived. If greater than 0 it
  // indicates one of the URLFetchers is still running.
  int suggest_results_pending_;

  // A timer to start a query to the suggest server after the user has stopped
  // typing for long enough.
  base::OneShotTimer<SearchProvider> timer_;

  // The time at which we sent a query to the suggest server.
  base::TimeTicks time_suggest_request_sent_;

  // Fetchers used to retrieve results for the keyword and default providers.
  scoped_ptr<net::URLFetcher> keyword_fetcher_;
  scoped_ptr<net::URLFetcher> default_fetcher_;

  // Results from the default and keyword search providers.
  Results default_results_;
  Results keyword_results_;

  // Each deletion handler in this vector corresponds to an outstanding request
  // that a server delete a personalized suggestion. Making this a ScopedVector
  // causes us to auto-cancel all such requests on shutdown.
  SuggestionDeletionHandlers deletion_handlers_;

  GURL current_page_url_;

  DISALLOW_COPY_AND_ASSIGN(SearchProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_SEARCH_PROVIDER_H_
