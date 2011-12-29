// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PAINT_AGGREGATOR_H_
#define PPAPI_CPP_PAINT_AGGREGATOR_H_

#include <stddef.h>
#include <vector>

#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"

/// @file
/// This file defines the API to aggregate multiple invalidation and scroll
/// commands to produce a scroll and repaint sequence.
namespace pp {

/// This class is responsible for aggregating multiple invalidation and scroll
/// commands to produce a scroll and repaint sequence. You can use this manually
/// to track your updates, but most applications will use the PaintManager to
/// additionally handle the necessary callbacks on top of the PaintAggregator
/// functionality.
///
/// Refer to <code>http://code.google.com/p/ppapi/wiki/2DPaintingModel</code>
/// for further information.
class PaintAggregator {
 public:
  struct PaintUpdate {
    /// Default constructor for creating an is_null() <code>PaintUpdate</code>
    /// object.
    PaintUpdate();

    /// Destructor.
    ~PaintUpdate();

    /// True if there is a scroll applied. This indicates that the scroll delta
    /// and scroll_rect are nonzero (just as a convenience).
    bool has_scroll;

    /// The amount to scroll by. Either the X or Y may be nonzero to indicate a
    /// scroll in that direction, but there will never be a scroll in both
    /// directions at the same time (this will be converted to a paint of the
    /// region instead).
    ///
    /// If there is no scroll, this will be (0, 0).
    Point scroll_delta;

    /// The rectangle that should be scrolled by the scroll_delta. If there is
    /// no scroll, this will be (0, 0, 0, 0). We only track one scroll command
    /// at once. If there are multiple ones, they will be converted to
    /// invalidates.
    Rect scroll_rect;

    /// A list of all the individual dirty rectangles. This is an aggregated
    /// list of all invalidate calls. Different rectangles may be unified to
    /// produce a minimal list with no overlap that is more efficient to paint.
    /// This list also contains the region exposed by any scroll command.
    std::vector<Rect> paint_rects;

    /// The union of all paint_rects.
    Rect paint_bounds;
  };

  /// Default constructor.
  PaintAggregator();

  /// Setter function setting the max ratio of paint rect area to scroll rect
  /// area that we will tolerate before downgrading the scroll into a repaint.
  ///
  /// If the combined area of paint rects contained within the scroll
  /// rect grows too large, then we might as well just treat
  /// the scroll rect as a paint rect.
  ///
  /// @param[in] area The max ratio of paint rect area to scroll rect area that
  /// we will tolerate before downgrading the scroll into a repaint.
  void set_max_redundant_paint_to_scroll_area(float area) {
    max_redundant_paint_to_scroll_area_ = area;
  }

  /// Setter function for setting the maximum number of paint rects. If we
  /// exceed this limit, then we'll start combining paint rects (see
  /// CombinePaintRects). This limiting can be important since there is
  /// typically some overhead in deciding what to paint. If your module is fast
  /// at doing these computations, raise this threshold, if your module is
  /// slow, lower it (probably requires some tuning to find the right value).
  ///
  /// @param[in] max_rects The maximum number of paint rects.
  void set_max_paint_rects(size_t max_rects) {
    max_paint_rects_ = max_rects;
  }

  /// This function determines if there is a pending update. There is a
  /// PendingUpdate if InvalidateRect or ScrollRect were called and
  /// ClearPendingUpdate was not called.
  ///
  /// @return true if there is a pending update, otherwise false.
  bool HasPendingUpdate() const;

  /// This function clears a pending update.
  void ClearPendingUpdate();

  /// This function gets a pending update.
  ///
  /// @return A PaintUpdate containing the pending update.
  PaintUpdate GetPendingUpdate() const;

  /// This function invalidates the rect so it will be repainted.
  ///
  /// @param[in] rect A rect to be repainted.
  void InvalidateRect(const Rect& rect);

  /// This function adds a pending scroll update.
  ///
  /// @param[in] clip_rect The rect to scroll.
  /// @param[in] amount A Point amount to scroll <code>rect</code>.
  void ScrollRect(const Rect& clip_rect, const Point& amount);

 private:
  // This structure is an internal version of PaintUpdate. It's different in
  // two respects:
  //
  //  - The scroll damange (area exposed by the scroll operation, if any) is
  //    maintained separately from the dirty rects generated by calling
  //    InvalidateRect. We need to know this distinction for some operations.
  //
  //  - The paint bounds union is computed on the fly so we don't have to keep
  //    a rectangle up-to-date as we do different operations.
  class InternalPaintUpdate {
   public:
    InternalPaintUpdate();
    ~InternalPaintUpdate();

    // Computes the rect damaged by scrolling within |scroll_rect| by
    // |scroll_delta|. This rect must be repainted. It is not included in
    // paint_rects or in the rect returned by GetPaintBounds.
    Rect GetScrollDamage() const;

    // Returns the smallest rect containing all paint rects, not including the
    // scroll damage rect.
    Rect GetPaintBounds() const;

    Point scroll_delta;
    Rect scroll_rect;

    // Does not include the scroll damage rect.
    std::vector<Rect> paint_rects;
  };

  Rect ScrollPaintRect(const Rect& paint_rect, const Point& amount) const;
  bool ShouldInvalidateScrollRect(const Rect& rect) const;
  void InvalidateScrollRect();
  void CombinePaintRects();

  InternalPaintUpdate update_;

  // If the combined area of paint rects contained within the scroll rect grows
  // too large, then we might as well just treat the scroll rect as a paint
  // rect. This constant sets the max ratio of paint rect area to scroll rect
  // area that we will tolerate before downgrading the scroll into a repaint.
  float max_redundant_paint_to_scroll_area_;

  // The maximum number of paint rects. If we exceed this limit, then we'll
  // start combining paint rects (see CombinePaintRects). This limiting can be
  // important since there is typically some overhead in deciding what to
  // paint. If your plugin is fast at doing these computations, raise this
  // threshold, if your plugin is slow, lower it (probably requires some
  // tuning to find the right value).
  size_t max_paint_rects_;
};

}  // namespace pp

#endif  // PPAPI_CPP_PAINT_AGGREGATOR_H_