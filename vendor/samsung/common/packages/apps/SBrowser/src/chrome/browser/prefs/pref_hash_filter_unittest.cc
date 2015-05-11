// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_filter.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "chrome/browser/prefs/pref_hash_store_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAtomicPref[] = "atomic_pref";
const char kAtomicPref2[] = "atomic_pref2";
const char kAtomicPref3[] = "pref3";
const char kReportOnlyPref[] = "report_only";
const char kReportOnlySplitPref[] = "report_only_split_pref";
const char kSplitPref[] = "split_pref";

const PrefHashFilter::TrackedPreferenceMetadata kTestTrackedPrefs[] = {
  {
    0, kAtomicPref, PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    1, kReportOnlyPref, PrefHashFilter::NO_ENFORCEMENT,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    2, kSplitPref, PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_SPLIT
  },
  {
    3, kReportOnlySplitPref, PrefHashFilter::NO_ENFORCEMENT,
    PrefHashFilter::TRACKING_STRATEGY_SPLIT
  },
  {
    4, kAtomicPref2, PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    5, kAtomicPref3, PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
};

}  // namespace

// A PrefHashStore that allows simulation of CheckValue results and captures
// checked and stored values.
class MockPrefHashStore : public PrefHashStore {
 public:
  typedef std::pair<const void*, PrefHashFilter::PrefTrackingStrategy>
      ValuePtrStrategyPair;

  MockPrefHashStore() : transactions_expected_(1),
                        transactions_performed_(0),
                        transaction_active_(false) {}

  virtual ~MockPrefHashStore() {
    EXPECT_EQ(transactions_expected_, transactions_performed_);
    EXPECT_FALSE(transaction_active_);
  }

  // Set the result that will be returned when |path| is passed to
  // |CheckValue/CheckSplitValue|.
  void SetCheckResult(const std::string& path,
                      PrefHashStoreTransaction::ValueState result);

  // Set the invalid_keys that will be returned when |path| is passed to
  // |CheckSplitValue|. SetCheckResult should already have been called for
  // |path| with |result == CHANGED| for this to make any sense.
  void SetInvalidKeysResult(
      const std::string& path,
      const std::vector<std::string>& invalid_keys_result);

  void set_transactions_expected(size_t transactions_expected) {
    transactions_expected_ = transactions_expected;
  }

  // Returns the number of paths checked.
  size_t checked_paths_count() const {
    return checked_values_.size();
  }

  // Returns the number of paths stored.
  size_t stored_paths_count() const {
    return stored_values_.size();
  }

  // Returns the pointer value and strategy that was passed to
  // |CheckHash/CheckSplitHash| for |path|. The returned pointer could since
  // have been freed and is thus not safe to dereference.
  ValuePtrStrategyPair checked_value(const std::string& path) const {
    std::map<std::string, ValuePtrStrategyPair>::const_iterator value =
        checked_values_.find(path);
    if (value != checked_values_.end())
      return value->second;
    return std::make_pair(
               reinterpret_cast<void*>(0xBAD),
               static_cast<PrefHashFilter::PrefTrackingStrategy>(-1));
  }

  // Returns the pointer value that was passed to |StoreHash/StoreSplitHash| for
  // |path|. The returned pointer could since have been freed and is thus not
  // safe to dereference.
  ValuePtrStrategyPair stored_value(const std::string& path) const {
    std::map<std::string, ValuePtrStrategyPair>::const_iterator value =
        stored_values_.find(path);
    if (value != stored_values_.end())
      return value->second;
    return std::make_pair(
               reinterpret_cast<void*>(0xBAD),
               static_cast<PrefHashFilter::PrefTrackingStrategy>(-1));
  }

  // PrefHashStore implementation.
  virtual scoped_ptr<PrefHashStoreTransaction> BeginTransaction() OVERRIDE;

 private:
  // A MockPrefHashStoreTransaction is handed to the caller on
  // MockPrefHashStore::BeginTransaction(). It then stores state in its
  // underlying MockPrefHashStore about calls it receives from that same caller
  // which can later be verified in tests.
  class MockPrefHashStoreTransaction : public PrefHashStoreTransaction {
   public:
    explicit MockPrefHashStoreTransaction(MockPrefHashStore* outer)
        : outer_(outer) {}

    virtual ~MockPrefHashStoreTransaction() {
      outer_->transaction_active_ = false;
      ++outer_->transactions_performed_;
    }

    // PrefHashStoreTransaction implementation.
    virtual PrefHashStoreTransaction::ValueState CheckValue(
        const std::string& path, const base::Value* value) const OVERRIDE;
    virtual void StoreHash(const std::string& path,
                           const base::Value* new_value) OVERRIDE;
    virtual PrefHashStoreTransaction::ValueState CheckSplitValue(
        const std::string& path,
        const base::DictionaryValue* initial_split_value,
        std::vector<std::string>* invalid_keys) const OVERRIDE;
    virtual void StoreSplitHash(
        const std::string& path,
        const base::DictionaryValue* split_value) OVERRIDE;

   private:
    MockPrefHashStore* outer_;

    DISALLOW_COPY_AND_ASSIGN(MockPrefHashStoreTransaction);
  };

  // Records a call to this mock's CheckValue/CheckSplitValue methods.
  PrefHashStoreTransaction::ValueState RecordCheckValue(
      const std::string& path,
      const base::Value* value,
      PrefHashFilter::PrefTrackingStrategy strategy);

  // Records a call to this mock's StoreHash/StoreSplitHash methods.
  void RecordStoreHash(const std::string& path,
                       const base::Value* new_value,
                       PrefHashFilter::PrefTrackingStrategy strategy);

  std::map<std::string, PrefHashStoreTransaction::ValueState> check_results_;
  std::map<std::string, std::vector<std::string> > invalid_keys_results_;
  std::map<std::string, ValuePtrStrategyPair> checked_values_;
  std::map<std::string, ValuePtrStrategyPair> stored_values_;

  // Number of transactions that are expected to be performed in the scope of
  // this test (defaults to 1).
  size_t transactions_expected_;

  // Number of transactions that were performed via this MockPrefHashStore.
  // Verified to match |transactions_expected_| when this MockPrefHashStore is
  // deleted.
  size_t transactions_performed_;

  // Whether a transaction is currently active (only one transaction should be
  // active at a time).
  bool transaction_active_;

  DISALLOW_COPY_AND_ASSIGN(MockPrefHashStore);
};

void MockPrefHashStore::SetCheckResult(
    const std::string& path, PrefHashStoreTransaction::ValueState result) {
  check_results_.insert(std::make_pair(path, result));
}

void MockPrefHashStore::SetInvalidKeysResult(
    const std::string& path,
    const std::vector<std::string>& invalid_keys_result) {
  // Ensure |check_results_| has a CHANGED entry for |path|.
  std::map<std::string,
          PrefHashStoreTransaction::ValueState>::const_iterator result =
      check_results_.find(path);
  ASSERT_TRUE(result != check_results_.end());
  ASSERT_EQ(PrefHashStoreTransaction::CHANGED, result->second);

  invalid_keys_results_.insert(std::make_pair(path, invalid_keys_result));
}

scoped_ptr<PrefHashStoreTransaction> MockPrefHashStore::BeginTransaction() {
  EXPECT_FALSE(transaction_active_);
  return scoped_ptr<PrefHashStoreTransaction>(
      new MockPrefHashStoreTransaction(this));
}

PrefHashStoreTransaction::ValueState MockPrefHashStore::RecordCheckValue(
    const std::string& path,
    const base::Value* value,
    PrefHashFilter::PrefTrackingStrategy strategy) {
  // Record that |path| was checked and validate that it wasn't previously
  // checked.
  EXPECT_TRUE(checked_values_.insert(
      std::make_pair(path, std::make_pair(value, strategy))).second);
  std::map<std::string,
           PrefHashStoreTransaction::ValueState>::const_iterator result =
      check_results_.find(path);
  if (result != check_results_.end())
    return result->second;
  return PrefHashStoreTransaction::UNCHANGED;
}

void MockPrefHashStore::RecordStoreHash(
    const std::string& path,
    const base::Value* new_value,
    PrefHashFilter::PrefTrackingStrategy strategy) {
  EXPECT_TRUE(stored_values_.insert(
      std::make_pair(path, std::make_pair(new_value, strategy))).second);
}

PrefHashStoreTransaction::ValueState
MockPrefHashStore::MockPrefHashStoreTransaction::CheckValue(
    const std::string& path, const base::Value* value) const {
  return outer_->RecordCheckValue(path, value,
                                  PrefHashFilter::TRACKING_STRATEGY_ATOMIC);
}

void MockPrefHashStore::MockPrefHashStoreTransaction::StoreHash(
    const std::string& path,
    const base::Value* new_value) {
  outer_->RecordStoreHash(path, new_value,
                          PrefHashFilter::TRACKING_STRATEGY_ATOMIC);
}

PrefHashStoreTransaction::ValueState
MockPrefHashStore::MockPrefHashStoreTransaction::CheckSplitValue(
    const std::string& path,
    const base::DictionaryValue* initial_split_value,
    std::vector<std::string>* invalid_keys) const {
  EXPECT_TRUE(invalid_keys && invalid_keys->empty());

  std::map<std::string, std::vector<std::string> >::const_iterator
      invalid_keys_result = outer_->invalid_keys_results_.find(path);
  if (invalid_keys_result != outer_->invalid_keys_results_.end()) {
    invalid_keys->insert(invalid_keys->begin(),
                         invalid_keys_result->second.begin(),
                         invalid_keys_result->second.end());
  }

  return outer_->RecordCheckValue(path, initial_split_value,
                                  PrefHashFilter::TRACKING_STRATEGY_SPLIT);
}

void MockPrefHashStore::MockPrefHashStoreTransaction::StoreSplitHash(
    const std::string& path,
    const base::DictionaryValue* new_value) {
  outer_->RecordStoreHash(path, new_value,
                          PrefHashFilter::TRACKING_STRATEGY_SPLIT);
}

// Creates a PrefHashFilter that uses a MockPrefHashStore. The
// MockPrefHashStore (owned by the PrefHashFilter) is returned in
// |mock_pref_hash_store|.
scoped_ptr<PrefHashFilter> CreatePrefHashFilter(
    PrefHashFilter::EnforcementLevel enforcement_level,
    MockPrefHashStore** mock_pref_hash_store,
    const base::Closure& reset_callback) {
  scoped_ptr<MockPrefHashStore> temp_mock_pref_hash_store(
      new MockPrefHashStore);
  if (mock_pref_hash_store)
    *mock_pref_hash_store = temp_mock_pref_hash_store.get();
  return scoped_ptr<PrefHashFilter>(
      new PrefHashFilter(temp_mock_pref_hash_store.PassAs<PrefHashStore>(),
                         kTestTrackedPrefs, arraysize(kTestTrackedPrefs),
                         arraysize(kTestTrackedPrefs), enforcement_level,
                         reset_callback));
}

class PrefHashFilterTest
    : public testing::TestWithParam<PrefHashFilter::EnforcementLevel> {
 public:
  PrefHashFilterTest() : mock_pref_hash_store_(NULL), reset_event_count_(0) {}

  virtual void SetUp() OVERRIDE {
    // Construct a PrefHashFilter and MockPrefHashStore for the test.
    pref_hash_filter_ = CreatePrefHashFilter(
        GetParam(), &mock_pref_hash_store_,
        base::Bind(&PrefHashFilterTest::HandleResetEvent,
                   base::Unretained(this)));
    reset_event_count_ = 0;
  }

  void HandleResetEvent() {
    ++reset_event_count_;
  }

 protected:
  MockPrefHashStore* mock_pref_hash_store_;
  base::DictionaryValue pref_store_contents_;
  scoped_ptr<PrefHashFilter> pref_hash_filter_;

  // The number of times a reset event occurs. A reset event is defined as
  // the discovery of a set of one or more changed tracked preferences during
  // load time.
  int reset_event_count_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashFilterTest);
};

TEST_P(PrefHashFilterTest, EmptyAndUnchanged) {
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  // All paths checked.
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  // No paths stored, since they all return |UNCHANGED|.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
  // Since there was nothing in |pref_store_contents_| the checked value should
  // have been NULL for all tracked preferences.
  for (size_t i = 0; i < arraysize(kTestTrackedPrefs); ++i) {
    ASSERT_EQ(NULL, mock_pref_hash_store_->checked_value(
                        kTestTrackedPrefs[i].name).first);
  }
  ASSERT_EQ(0, reset_event_count_);
}

TEST_P(PrefHashFilterTest, FilterTrackedPrefUpdate) {
  base::DictionaryValue root_dict;
  // Ownership of |string_value| is transfered to |root_dict|.
  base::Value* string_value = base::Value::CreateStringValue("string value");
  root_dict.Set(kAtomicPref, string_value);

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(&root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(string_value, stored_value.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC, stored_value.second);

  ASSERT_EQ(0, reset_event_count_);
}

TEST_P(PrefHashFilterTest, FilterSplitPrefUpdate) {
  base::DictionaryValue root_dict;
  // Ownership of |dict_value| is transfered to |root_dict|.
  base::DictionaryValue* dict_value = new base::DictionaryValue;
  dict_value->SetString("a", "foo");
  dict_value->SetInteger("b", 1234);
  root_dict.Set(kSplitPref, dict_value);

  // No path should be stored on FilterUpdate.
  pref_hash_filter_->FilterUpdate(kSplitPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // One path should be stored on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(&root_dict);
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value =
       mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(dict_value, stored_value.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_SPLIT, stored_value.second);

  ASSERT_EQ(0, reset_event_count_);
}

TEST_P(PrefHashFilterTest, FilterUntrackedPrefUpdate) {
  // No transaction should even be started on FilterSerializeData() if there are
  // no updates to perform.
  mock_pref_hash_store_->set_transactions_expected(0);

  base::DictionaryValue root_dict;
  root_dict.Set("untracked", base::Value::CreateStringValue("some value"));
  pref_hash_filter_->FilterUpdate("untracked");

  // No paths should be stored on FilterUpdate.
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // Nor on FilterSerializeData.
  pref_hash_filter_->FilterSerializeData(&root_dict);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());
}

TEST_P(PrefHashFilterTest, MultiplePrefsFilterSerializeData) {
  base::DictionaryValue root_dict;
  // Ownership of the following values is transfered to |root_dict|.
  base::Value* int_value1 = base::Value::CreateIntegerValue(1);
  base::Value* int_value2 = base::Value::CreateIntegerValue(2);
  base::Value* int_value3 = base::Value::CreateIntegerValue(3);
  base::Value* int_value4 = base::Value::CreateIntegerValue(4);
  base::DictionaryValue* dict_value = new base::DictionaryValue;
  dict_value->Set("a", base::Value::CreateBooleanValue(true));
  root_dict.Set(kAtomicPref, int_value1);
  root_dict.Set(kAtomicPref2, int_value2);
  root_dict.Set(kAtomicPref3, int_value3);
  root_dict.Set("untracked", int_value4);
  root_dict.Set(kSplitPref, dict_value);

  // Only update kAtomicPref, kAtomicPref3, and kSplitPref.
  pref_hash_filter_->FilterUpdate(kAtomicPref);
  pref_hash_filter_->FilterUpdate(kAtomicPref3);
  pref_hash_filter_->FilterUpdate(kSplitPref);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // Update kAtomicPref3 again, nothing should be stored still.
  base::Value* int_value5 = base::Value::CreateIntegerValue(5);
  root_dict.Set(kAtomicPref3, int_value5);
  ASSERT_EQ(0u, mock_pref_hash_store_->stored_paths_count());

  // On FilterSerializeData, only kAtomicPref, kAtomicPref3, and kSplitPref
  // should get a new hash.
  pref_hash_filter_->FilterSerializeData(&root_dict);
  ASSERT_EQ(3u, mock_pref_hash_store_->stored_paths_count());
  MockPrefHashStore::ValuePtrStrategyPair stored_value_atomic1 =
      mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(int_value1, stored_value_atomic1.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
            stored_value_atomic1.second);

  MockPrefHashStore::ValuePtrStrategyPair stored_value_atomic3 =
      mock_pref_hash_store_->stored_value(kAtomicPref3);
  ASSERT_EQ(int_value5, stored_value_atomic3.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
            stored_value_atomic3.second);

  MockPrefHashStore::ValuePtrStrategyPair stored_value_split =
       mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(dict_value, stored_value_split.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_SPLIT, stored_value_split.second);
}

TEST_P(PrefHashFilterTest, EmptyAndUnknown) {
  ASSERT_FALSE(pref_store_contents_.Get(kAtomicPref, NULL));
  ASSERT_FALSE(pref_store_contents_.Get(kSplitPref, NULL));
  // NULL values are always trusted by the PrefHashStore.
  mock_pref_hash_store_->SetCheckResult(
      kAtomicPref, PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE);
  mock_pref_hash_store_->SetCheckResult(
      kSplitPref, PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
       mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(NULL, stored_atomic_value.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
            stored_atomic_value.second);

  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
       mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(NULL, stored_split_value.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_SPLIT,
            stored_split_value.second);
}

TEST_P(PrefHashFilterTest, InitialValueUnknown) {
  // Ownership of these values is transfered to |pref_store_contents_|.
  base::StringValue* string_value = new base::StringValue("string value");
  pref_store_contents_.Set(kAtomicPref, string_value);

  base::DictionaryValue* dict_value = new base::DictionaryValue;
  dict_value->SetString("a", "foo");
  dict_value->SetInteger("b", 1234);
  pref_store_contents_.Set(kSplitPref, dict_value);

  ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kSplitPref, NULL));

  mock_pref_hash_store_->SetCheckResult(
      kAtomicPref, PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE);
  mock_pref_hash_store_->SetCheckResult(
      kSplitPref, PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
       mock_pref_hash_store_->stored_value(kAtomicPref);
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
       mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
            stored_atomic_value.second);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_SPLIT,
            stored_split_value.second);
  if (GetParam() == PrefHashFilter::ENFORCE_ON_LOAD) {
    // Ensure the prefs were cleared and the hashes for NULL were restored if
    // the current enforcement level denies seeding.
    ASSERT_FALSE(pref_store_contents_.Get(kAtomicPref, NULL));
    ASSERT_EQ(NULL, stored_atomic_value.first);

    ASSERT_FALSE(pref_store_contents_.Get(kSplitPref, NULL));
    ASSERT_EQ(NULL, stored_split_value.first);

    ASSERT_EQ(1, reset_event_count_);
  } else {
    // Otherwise the values should have remained intact and the hashes should
    // have been updated to match them.
    const base::Value* atomic_value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, &atomic_value_in_store));
    ASSERT_EQ(string_value, atomic_value_in_store);
    ASSERT_EQ(string_value, stored_atomic_value.first);

    const base::Value* split_value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kSplitPref, &split_value_in_store));
    ASSERT_EQ(dict_value, split_value_in_store);
    ASSERT_EQ(dict_value, stored_split_value.first);

    ASSERT_EQ(0, reset_event_count_);
  }
}

TEST_P(PrefHashFilterTest, InitialValueTrustedUnknown) {
  // Ownership of this value is transfered to |pref_store_contents_|.
  base::Value* string_value = base::Value::CreateStringValue("test");
  pref_store_contents_.Set(kAtomicPref, string_value);

  base::DictionaryValue* dict_value = new base::DictionaryValue;
  dict_value->SetString("a", "foo");
  dict_value->SetInteger("b", 1234);
  pref_store_contents_.Set(kSplitPref, dict_value);

  ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kSplitPref, NULL));

  mock_pref_hash_store_->SetCheckResult(
      kAtomicPref, PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE);
  mock_pref_hash_store_->SetCheckResult(
      kSplitPref, PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());

  // Seeding is always allowed for trusted unknown values.
  const base::Value* atomic_value_in_store;
  ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, &atomic_value_in_store));
  ASSERT_EQ(string_value, atomic_value_in_store);
  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
       mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(string_value, stored_atomic_value.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
            stored_atomic_value.second);

  const base::Value* split_value_in_store;
  ASSERT_TRUE(pref_store_contents_.Get(kSplitPref, &split_value_in_store));
  ASSERT_EQ(dict_value, split_value_in_store);
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
       mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(dict_value, stored_split_value.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_SPLIT,
            stored_split_value.second);
}

TEST_P(PrefHashFilterTest, InitialValueChanged) {
  // Ownership of this value is transfered to |pref_store_contents_|.
  base::Value* int_value = base::Value::CreateIntegerValue(1234);
  pref_store_contents_.Set(kAtomicPref, int_value);

  base::DictionaryValue* dict_value = new base::DictionaryValue;
  dict_value->SetString("a", "foo");
  dict_value->SetInteger("b", 1234);
  dict_value->SetInteger("c", 56);
  dict_value->SetBoolean("d", false);
  pref_store_contents_.Set(kSplitPref, dict_value);

  ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kSplitPref, NULL));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        PrefHashStoreTransaction::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        PrefHashStoreTransaction::CHANGED);

  std::vector<std::string> mock_invalid_keys;
  mock_invalid_keys.push_back("a");
  mock_invalid_keys.push_back("c");
  mock_pref_hash_store_->SetInvalidKeysResult(kSplitPref, mock_invalid_keys);

  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
       mock_pref_hash_store_->stored_value(kAtomicPref);
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
       mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
            stored_atomic_value.second);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_SPLIT,
            stored_split_value.second);
  if (GetParam() == PrefHashFilter::ENFORCE_ON_LOAD) {
    // Ensure the atomic pref was cleared and the hash for NULL was restored if
    // the current enforcement level prevents changes.
    ASSERT_FALSE(pref_store_contents_.Get(kAtomicPref, NULL));
    ASSERT_EQ(NULL, stored_atomic_value.first);

    // The split pref on the other hand should only have been stripped of its
    // invalid keys.
    const base::Value* split_value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kSplitPref, &split_value_in_store));
    ASSERT_EQ(2U, dict_value->size());
    ASSERT_FALSE(dict_value->HasKey("a"));
    ASSERT_TRUE(dict_value->HasKey("b"));
    ASSERT_FALSE(dict_value->HasKey("c"));
    ASSERT_TRUE(dict_value->HasKey("d"));
    ASSERT_EQ(dict_value, stored_split_value.first);

    ASSERT_EQ(1, reset_event_count_);
  } else {
    // Otherwise the value should have remained intact and the hash should have
    // been updated to match it.
    const base::Value* atomic_value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, &atomic_value_in_store));
    ASSERT_EQ(int_value, atomic_value_in_store);
    ASSERT_EQ(int_value, stored_atomic_value.first);

    const base::Value* split_value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kSplitPref, &split_value_in_store));
    ASSERT_EQ(dict_value, split_value_in_store);
    ASSERT_EQ(4U, dict_value->size());
    ASSERT_TRUE(dict_value->HasKey("a"));
    ASSERT_TRUE(dict_value->HasKey("b"));
    ASSERT_TRUE(dict_value->HasKey("c"));
    ASSERT_TRUE(dict_value->HasKey("d"));
    ASSERT_EQ(dict_value, stored_split_value.first);

    ASSERT_EQ(0, reset_event_count_);
  }
}

TEST_P(PrefHashFilterTest, EmptyCleared) {
  ASSERT_FALSE(pref_store_contents_.Get(kAtomicPref, NULL));
  ASSERT_FALSE(pref_store_contents_.Get(kSplitPref, NULL));
  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        PrefHashStoreTransaction::CLEARED);
  mock_pref_hash_store_->SetCheckResult(kSplitPref,
                                        PrefHashStoreTransaction::CLEARED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(2u, mock_pref_hash_store_->stored_paths_count());

  // Regardless of the enforcement level, the only thing that should be done is
  // to restore the hash for NULL. The value itself should still be NULL.
  ASSERT_FALSE(pref_store_contents_.Get(kAtomicPref, NULL));
  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
       mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(NULL, stored_atomic_value.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
            stored_atomic_value.second);

  ASSERT_FALSE(pref_store_contents_.Get(kSplitPref, NULL));
  MockPrefHashStore::ValuePtrStrategyPair stored_split_value =
       mock_pref_hash_store_->stored_value(kSplitPref);
  ASSERT_EQ(NULL, stored_split_value.first);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_SPLIT,
            stored_split_value.second);
}

TEST_P(PrefHashFilterTest, InitialValueMigrated) {
  // Only test atomic prefs, split prefs were introduce after the migration.

  // Ownership of this value is transfered to |pref_store_contents_|.
  base::ListValue* list_value = new base::ListValue;
  list_value->Append(base::Value::CreateStringValue("test"));
  pref_store_contents_.Set(kAtomicPref, list_value);

  ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, NULL));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        PrefHashStoreTransaction::MIGRATED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(1u, mock_pref_hash_store_->stored_paths_count());

  MockPrefHashStore::ValuePtrStrategyPair stored_atomic_value =
       mock_pref_hash_store_->stored_value(kAtomicPref);
  ASSERT_EQ(PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
            stored_atomic_value.second);
  if (GetParam() == PrefHashFilter::ENFORCE_ON_LOAD) {
    // Ensure the pref was cleared and the hash for NULL was restored if the
    // current enforcement level prevents migration.
    ASSERT_FALSE(pref_store_contents_.Get(kAtomicPref, NULL));
    ASSERT_EQ(NULL, stored_atomic_value.first);

    ASSERT_EQ(1, reset_event_count_);
  } else {
    // Otherwise the value should have remained intact and the hash should have
    // been updated to match it.
    const base::Value* atomic_value_in_store;
    ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, &atomic_value_in_store));
    ASSERT_EQ(list_value, atomic_value_in_store);
    ASSERT_EQ(list_value, stored_atomic_value.first);

    ASSERT_EQ(0, reset_event_count_);
  }
}

TEST_P(PrefHashFilterTest, DontResetReportOnly) {
  // Ownership of these values is transfered to |pref_store_contents_|.
  base::Value* int_value1 = base::Value::CreateIntegerValue(1);
  base::Value* int_value2 = base::Value::CreateIntegerValue(2);
  base::Value* report_only_val = base::Value::CreateIntegerValue(3);
  base::DictionaryValue* report_only_split_val = new base::DictionaryValue;
  report_only_split_val->SetInteger("a", 1234);
  pref_store_contents_.Set(kAtomicPref, int_value1);
  pref_store_contents_.Set(kAtomicPref2, int_value2);
  pref_store_contents_.Set(kReportOnlyPref, report_only_val);
  pref_store_contents_.Set(kReportOnlySplitPref, report_only_split_val);

  ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref2, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kReportOnlyPref, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kReportOnlySplitPref, NULL));

  mock_pref_hash_store_->SetCheckResult(kAtomicPref,
                                        PrefHashStoreTransaction::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kAtomicPref2,
                                        PrefHashStoreTransaction::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kReportOnlyPref,
                                        PrefHashStoreTransaction::CHANGED);
  mock_pref_hash_store_->SetCheckResult(kReportOnlySplitPref,
                                        PrefHashStoreTransaction::CHANGED);
  pref_hash_filter_->FilterOnLoad(&pref_store_contents_);
  // All prefs should be checked and a new hash should be stored for each tested
  // pref.
  ASSERT_EQ(arraysize(kTestTrackedPrefs),
            mock_pref_hash_store_->checked_paths_count());
  ASSERT_EQ(4u, mock_pref_hash_store_->stored_paths_count());

  // No matter what the enforcement level is, the report only pref should never
  // be reset.
  ASSERT_TRUE(pref_store_contents_.Get(kReportOnlyPref, NULL));
  ASSERT_TRUE(pref_store_contents_.Get(kReportOnlySplitPref, NULL));
  ASSERT_EQ(report_only_val,
            mock_pref_hash_store_->stored_value(kReportOnlyPref).first);
  ASSERT_EQ(report_only_split_val,
            mock_pref_hash_store_->stored_value(kReportOnlySplitPref).first);

  // All other prefs should have been reset if the enforcement level allows it.
  if (GetParam() == PrefHashFilter::ENFORCE_ON_LOAD) {
    ASSERT_FALSE(pref_store_contents_.Get(kAtomicPref, NULL));
    ASSERT_FALSE(pref_store_contents_.Get(kAtomicPref2, NULL));
    ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kAtomicPref).first);
    ASSERT_EQ(NULL, mock_pref_hash_store_->stored_value(kAtomicPref2).first);

    ASSERT_EQ(1, reset_event_count_);
  } else {
    const base::Value* value_in_store;
    const base::Value* value_in_store2;
    ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref, &value_in_store));
    ASSERT_TRUE(pref_store_contents_.Get(kAtomicPref2, &value_in_store2));
    ASSERT_EQ(int_value1, value_in_store);
    ASSERT_EQ(int_value1,
              mock_pref_hash_store_->stored_value(kAtomicPref).first);
    ASSERT_EQ(int_value2, value_in_store2);
    ASSERT_EQ(int_value2,
              mock_pref_hash_store_->stored_value(kAtomicPref2).first);

    ASSERT_EQ(0, reset_event_count_);
  }
}

INSTANTIATE_TEST_CASE_P(
    PrefHashFilterTestInstance, PrefHashFilterTest,
    testing::Values(PrefHashFilter::NO_ENFORCEMENT,
                    PrefHashFilter::ENFORCE_ON_LOAD));
