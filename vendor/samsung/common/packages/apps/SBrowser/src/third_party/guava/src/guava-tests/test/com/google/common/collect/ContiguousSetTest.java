/*
 * Copyright (C) 2011 The Guava Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.common.collect;

import static com.google.common.collect.BoundType.CLOSED;
import static com.google.common.collect.BoundType.OPEN;
import static com.google.common.collect.DiscreteDomains.integers;
import static com.google.common.testing.SerializableTester.reserialize;
import static com.google.common.testing.SerializableTester.reserializeAndAssert;
import static org.junit.contrib.truth.Truth.ASSERT;

import com.google.common.annotations.GwtCompatible;
import com.google.common.annotations.GwtIncompatible;
import com.google.common.testing.EqualsTester;

import junit.framework.TestCase;

import java.util.Set;

/**
 * @author Gregory Kick
 */
@GwtCompatible(emulated = true)
public class ContiguousSetTest extends TestCase {
  private static DiscreteDomain<Integer> NOT_EQUAL_TO_INTEGERS = new DiscreteDomain<Integer>() {
    @Override public Integer next(Integer value) {
      return integers().next(value);
    }

    @Override public Integer previous(Integer value) {
      return integers().previous(value);
    }

    @Override public long distance(Integer start, Integer end) {
      return integers().distance(start, end);
    }

    @Override public Integer minValue() {
      return integers().minValue();
    }

    @Override public Integer maxValue() {
      return integers().maxValue();
    }
  };

  public void testEquals() {
    new EqualsTester()
        .addEqualityGroup(
            ContiguousSet.create(Ranges.closed(1, 3), integers()),
            ContiguousSet.create(Ranges.closedOpen(1, 4), integers()),
            ContiguousSet.create(Ranges.openClosed(0, 3), integers()),
            ContiguousSet.create(Ranges.open(0, 4), integers()),
            ContiguousSet.create(Ranges.closed(1, 3), NOT_EQUAL_TO_INTEGERS),
            ContiguousSet.create(Ranges.closedOpen(1, 4), NOT_EQUAL_TO_INTEGERS),
            ContiguousSet.create(Ranges.openClosed(0, 3), NOT_EQUAL_TO_INTEGERS),
            ContiguousSet.create(Ranges.open(0, 4), NOT_EQUAL_TO_INTEGERS),
            ImmutableSortedSet.of(1, 2, 3))
        .testEquals();
    // not testing hashCode for these because it takes forever to compute
    assertEquals(
        ContiguousSet.create(Ranges.closed(Integer.MIN_VALUE, Integer.MAX_VALUE), integers()),
        ContiguousSet.create(Ranges.<Integer>all(), integers()));
    assertEquals(
        ContiguousSet.create(Ranges.closed(Integer.MIN_VALUE, Integer.MAX_VALUE), integers()),
        ContiguousSet.create(Ranges.atLeast(Integer.MIN_VALUE), integers()));
    assertEquals(
        ContiguousSet.create(Ranges.closed(Integer.MIN_VALUE, Integer.MAX_VALUE), integers()),
        ContiguousSet.create(Ranges.atMost(Integer.MAX_VALUE), integers()));
  }

  @GwtIncompatible("SerializableTester")
  public void testSerialization() {
    ContiguousSet<Integer> empty = ContiguousSet.create(Ranges.closedOpen(1, 1), integers());
    assertTrue(empty instanceof EmptyContiguousSet);
    reserializeAndAssert(empty);

    ContiguousSet<Integer> regular = ContiguousSet.create(Ranges.closed(1, 3), integers());
    assertTrue(regular instanceof RegularContiguousSet);
    reserializeAndAssert(regular);

    /*
     * Make sure that we're using RegularContiguousSet.SerializedForm and not
     * ImmutableSet.SerializedForm, which would be enormous.
     */
    ContiguousSet<Integer> enormous = ContiguousSet.create(Ranges.<Integer>all(), integers());
    assertTrue(enormous instanceof RegularContiguousSet);
    // We can't use reserializeAndAssert because it calls hashCode, which is enormously slow.
    ContiguousSet<Integer> enormousReserialized = reserialize(enormous);
    assertEquals(enormous, enormousReserialized);
  }

  public void testHeadSet() {
    ImmutableSortedSet<Integer> set = ContiguousSet.create(Ranges.closed(1, 3), integers());
    ASSERT.that(set.headSet(1)).isEmpty();
    ASSERT.that(set.headSet(2)).hasContentsInOrder(1);
    ASSERT.that(set.headSet(3)).hasContentsInOrder(1, 2);
    ASSERT.that(set.headSet(4)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.headSet(Integer.MAX_VALUE)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.headSet(1, true)).hasContentsInOrder(1);
    ASSERT.that(set.headSet(2, true)).hasContentsInOrder(1, 2);
    ASSERT.that(set.headSet(3, true)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.headSet(4, true)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.headSet(Integer.MAX_VALUE, true)).hasContentsInOrder(1, 2, 3);
  }

  public void testHeadSet_tooSmall() {
    ASSERT.that(ContiguousSet.create(Ranges.closed(1, 3), integers()).headSet(0)).isEmpty();
  }

  public void testTailSet() {
    ImmutableSortedSet<Integer> set = ContiguousSet.create(Ranges.closed(1, 3), integers());
    ASSERT.that(set.tailSet(Integer.MIN_VALUE)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.tailSet(1)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.tailSet(2)).hasContentsInOrder(2, 3);
    ASSERT.that(set.tailSet(3)).hasContentsInOrder(3);
    ASSERT.that(set.tailSet(Integer.MIN_VALUE, false)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.tailSet(1, false)).hasContentsInOrder(2, 3);
    ASSERT.that(set.tailSet(2, false)).hasContentsInOrder(3);
    ASSERT.that(set.tailSet(3, false)).isEmpty();
  }

  public void testTailSet_tooLarge() {
    ASSERT.that(ContiguousSet.create(Ranges.closed(1, 3), integers()).tailSet(4)).isEmpty();
  }

  public void testSubSet() {
    ImmutableSortedSet<Integer> set = ContiguousSet.create(Ranges.closed(1, 3), integers());
    ASSERT.that(set.subSet(1, 4)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.subSet(2, 4)).hasContentsInOrder(2, 3);
    ASSERT.that(set.subSet(3, 4)).hasContentsInOrder(3);
    ASSERT.that(set.subSet(3, 3)).isEmpty();
    ASSERT.that(set.subSet(2, 3)).hasContentsInOrder(2);
    ASSERT.that(set.subSet(1, 3)).hasContentsInOrder(1, 2);
    ASSERT.that(set.subSet(1, 2)).hasContentsInOrder(1);
    ASSERT.that(set.subSet(2, 2)).isEmpty();
    ASSERT.that(set.subSet(Integer.MIN_VALUE, Integer.MAX_VALUE)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.subSet(1, true, 3, true)).hasContentsInOrder(1, 2, 3);
    ASSERT.that(set.subSet(1, false, 3, true)).hasContentsInOrder(2, 3);
    ASSERT.that(set.subSet(1, true, 3, false)).hasContentsInOrder(1, 2);
    ASSERT.that(set.subSet(1, false, 3, false)).hasContentsInOrder(2);
  }

  public void testSubSet_outOfOrder() {
    ImmutableSortedSet<Integer> set = ContiguousSet.create(Ranges.closed(1, 3), integers());
    try {
      set.subSet(3, 2);
      fail();
    } catch (IllegalArgumentException expected) {}
  }

  public void testSubSet_tooLarge() {
    ASSERT.that(ContiguousSet.create(Ranges.closed(1, 3), integers()).subSet(4, 6)).isEmpty();
  }

  public void testSubSet_tooSmall() {
    ASSERT.that(ContiguousSet.create(Ranges.closed(1, 3), integers()).subSet(-1, 0)).isEmpty();
  }

  public void testFirst() {
    assertEquals(1, ContiguousSet.create(Ranges.closed(1, 3), integers()).first().intValue());
    assertEquals(1, ContiguousSet.create(Ranges.open(0, 4), integers()).first().intValue());
    assertEquals(Integer.MIN_VALUE,
        ContiguousSet.create(Ranges.<Integer>all(), integers()).first().intValue());
  }

  public void testLast() {
    assertEquals(3, ContiguousSet.create(Ranges.closed(1, 3), integers()).last().intValue());
    assertEquals(3, ContiguousSet.create(Ranges.open(0, 4), integers()).last().intValue());
    assertEquals(Integer.MAX_VALUE,
        ContiguousSet.create(Ranges.<Integer>all(), integers()).last().intValue());
  }

  public void testContains() {
    ImmutableSortedSet<Integer> set = ContiguousSet.create(Ranges.closed(1, 3), integers());
    assertFalse(set.contains(0));
    assertTrue(set.contains(1));
    assertTrue(set.contains(2));
    assertTrue(set.contains(3));
    assertFalse(set.contains(4));
    set = ContiguousSet.create(Ranges.open(0, 4), integers());
    assertFalse(set.contains(0));
    assertTrue(set.contains(1));
    assertTrue(set.contains(2));
    assertTrue(set.contains(3));
    assertFalse(set.contains(4));
    assertFalse(set.contains("blah"));
  }

  public void testContainsAll() {
    ImmutableSortedSet<Integer> set = ContiguousSet.create(Ranges.closed(1, 3), integers());
    for (Set<Integer> subset : Sets.powerSet(ImmutableSet.of(1, 2, 3))) {
      assertTrue(set.containsAll(subset));
    }
    for (Set<Integer> subset : Sets.powerSet(ImmutableSet.of(1, 2, 3))) {
      assertFalse(set.containsAll(Sets.union(subset, ImmutableSet.of(9))));
    }
    assertFalse(set.containsAll(ImmutableSet.of("blah")));
  }

  public void testRange() {
    assertEquals(Ranges.closed(1, 3),
        ContiguousSet.create(Ranges.closed(1, 3), integers()).range());
    assertEquals(Ranges.closed(1, 3),
        ContiguousSet.create(Ranges.closedOpen(1, 4), integers()).range());
    assertEquals(Ranges.closed(1, 3), ContiguousSet.create(Ranges.open(0, 4), integers()).range());
    assertEquals(Ranges.closed(1, 3),
        ContiguousSet.create(Ranges.openClosed(0, 3), integers()).range());

    assertEquals(Ranges.openClosed(0, 3),
        ContiguousSet.create(Ranges.closed(1, 3), integers()).range(OPEN, CLOSED));
    assertEquals(Ranges.openClosed(0, 3),
        ContiguousSet.create(Ranges.closedOpen(1, 4), integers()).range(OPEN, CLOSED));
    assertEquals(Ranges.openClosed(0, 3),
        ContiguousSet.create(Ranges.open(0, 4), integers()).range(OPEN, CLOSED));
    assertEquals(Ranges.openClosed(0, 3),
        ContiguousSet.create(Ranges.openClosed(0, 3), integers()).range(OPEN, CLOSED));

    assertEquals(Ranges.open(0, 4),
        ContiguousSet.create(Ranges.closed(1, 3), integers()).range(OPEN, OPEN));
    assertEquals(Ranges.open(0, 4),
        ContiguousSet.create(Ranges.closedOpen(1, 4), integers()).range(OPEN, OPEN));
    assertEquals(Ranges.open(0, 4),
        ContiguousSet.create(Ranges.open(0, 4), integers()).range(OPEN, OPEN));
    assertEquals(Ranges.open(0, 4),
        ContiguousSet.create(Ranges.openClosed(0, 3), integers()).range(OPEN, OPEN));

    assertEquals(Ranges.closedOpen(1, 4),
        ContiguousSet.create(Ranges.closed(1, 3), integers()).range(CLOSED, OPEN));
    assertEquals(Ranges.closedOpen(1, 4),
        ContiguousSet.create(Ranges.closedOpen(1, 4), integers()).range(CLOSED, OPEN));
    assertEquals(Ranges.closedOpen(1, 4),
        ContiguousSet.create(Ranges.open(0, 4), integers()).range(CLOSED, OPEN));
    assertEquals(Ranges.closedOpen(1, 4),
        ContiguousSet.create(Ranges.openClosed(0, 3), integers()).range(CLOSED, OPEN));
  }

  public void testRange_unboundedRanges() {
    assertEquals(Ranges.closed(Integer.MIN_VALUE, Integer.MAX_VALUE),
        ContiguousSet.create(Ranges.<Integer>all(), integers()).range());
    assertEquals(Ranges.atLeast(Integer.MIN_VALUE),
        ContiguousSet.create(Ranges.<Integer>all(), integers()).range(CLOSED, OPEN));
    assertEquals(Ranges.all(),
        ContiguousSet.create(Ranges.<Integer>all(), integers()).range(OPEN, OPEN));
    assertEquals(Ranges.atMost(Integer.MAX_VALUE),
        ContiguousSet.create(Ranges.<Integer>all(), integers()).range(OPEN, CLOSED));
  }

  public void testIntersection_empty() {
    ContiguousSet<Integer> set = ContiguousSet.create(Ranges.closed(1, 3), integers());
    ContiguousSet<Integer> emptySet = ContiguousSet.create(Ranges.closedOpen(2, 2), integers());
    assertEquals(ImmutableSet.of(), set.intersection(emptySet));
    assertEquals(ImmutableSet.of(), emptySet.intersection(set));
    assertEquals(ImmutableSet.of(),
        ContiguousSet.create(Ranges.closed(-5, -1), integers()).intersection(
            ContiguousSet.create(Ranges.open(3, 64), integers())));
  }

  public void testIntersection() {
    ContiguousSet<Integer> set = ContiguousSet.create(Ranges.closed(1, 3), integers());
    assertEquals(ImmutableSet.of(1, 2, 3),
        ContiguousSet.create(Ranges.open(-1, 4), integers()).intersection(set));
    assertEquals(ImmutableSet.of(1, 2, 3),
        set.intersection(ContiguousSet.create(Ranges.open(-1, 4), integers())));
  }
}
