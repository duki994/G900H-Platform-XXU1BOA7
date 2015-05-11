// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var RowAlignment = {
  STRETCH: "stretch",
  LEFT: "left",
  RIGHT: "right",
  CENTER: "center",
}

/**
 * Ratio of key height and font size.
 * @type {number}
 */
var FONT_SIZE_RATIO = 2.5;

/**
 * @type {enum}
 * Possible layout alignments.
 */
var LayoutAlignment = {
  CENTER: "center",
  STRETCH: "stretch",
}

/**
 * The enumeration of swipe directions.
 * @const
 * @type {Enum}
 */
var SwipeDirection = {
  RIGHT: 0x1,
  LEFT: 0x2,
  UP: 0x4,
  DOWN: 0x8
};

/**
 * The default weight of a key in the X direction.
 * @type {number}
 */
var DEFAULT_KEY_WEIGHT_X = 100;

/**
 * The default weight of a key in the Y direction.
 * @type {number}
 */
var DEFAULT_KEY_WEIGHT_Y = 70;

/**
 * The top padding on each key.
 * @type {number}
 */
// TODO(rsadam): Remove this variable once figure out how to calculate this
// number before the key is rendered.
var KEY_PADDING_TOP = 1;
var KEY_PADDING_BOTTOM = 1;
