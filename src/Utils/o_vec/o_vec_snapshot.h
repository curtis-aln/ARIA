#pragma once
#include "o_vec_debug.h"

// ============================================================================
//  OVecDebugImGuiSnapshot  —  plain-data, type-erased view of one OVecDebug<Obj>
//
//  Purpose: every ImGui sub-tab needs the same handful of numbers from
//  whichever vector is currently selected (Cells / Food / Bodies / Springs).
//  Rather than templating every draw function on Obj, we read the live
//  OVecDebug<Obj> once per frame into this flat struct and pass that around.
//
//  This struct intentionally has NO methods that touch o_vector or OVecDebug
//  internals — it's just numbers, safe to copy, store in a small ring buffer,
//  or diff against a previous frame's copy.
// ============================================================================
struct OVecDebugImGuiSnapshot
{
    // ---- Identity (set by the caller, not by FillSnapshot) -----------------
    const char* name = "";   // e.g. "Cells" — for headers/labels
    ImVec4      color{};     // display colour for this vector

    // ---- Capacity ------------------------------------------------------------
    int   active = 0;
    int   free_slots = 0;
    int   array_size = 0;
    float fill_ratio = 0.f;

    // ---- Memory ----------------------------------------------------------------
    size_t heap_bytes = 0;
    size_t wasted_bytes = 0;

    // ---- Fragmentation -----------------------------------------------------
    float frag_score = 0.f;
    int   hole_count = 0;
    int   longest_hole = 0;
    int   longest_run = 0;
    int   min_active_index = -1;
    int   max_active_index = -1;
    float spread_ratio = 0.f;

    // ---- Operations (lifetime) ----------------------------------------------
    uint64_t total_emplaces = 0;
    uint64_t total_adds = 0;
    uint64_t total_removes = 0;
    uint64_t failed_adds = 0;
    uint64_t resize_grows = 0;
    uint64_t resize_shrinks = 0;
    uint64_t integrity_failures = 0;

    // ---- Peaks -------------------------------------------------------------
    int peak_active = 0;
    int peak_array_size = 0;
    int peak_free_count = 0;

    // ---- Churn ---------------------------------------------------------------
    uint64_t total_churn = 0;
    float    avg_churn = 0.f;
    uint32_t most_churned_slot = 0;
    uint32_t most_churned_count = 0;

    // ---- Timing --------------------------------------------------------------
    double ops_per_sec = 0.0;
    double uptime_ms = 0.0;

    // ---- Snapshot count (OVecDebug::Snapshot history, not this struct) ------
    int snapshot_count = 0;
};


// ----------------------------------------------------------------------------
//  FillSnapshot  —  populates an OVecDebugImGuiSnapshot from a live OVecDebug<Obj>
//
//  Templated on Obj so it works for Cell / Food / Body / Spring without
//  duplicating this function four times. Call once per vector per frame;
//  it's a handful of O(1) calls plus one O(n) fragmentation scan internally
//  (analyze_fragmentation()), so don't call it more than once per vector
//  per draw — cache the result locally in the calling sub-tab function.
//
//  `name` and `color` are passed in rather than stored on OVecDebug because
//  display identity belongs to the UI layer, not the debug instrumentation.
// ----------------------------------------------------------------------------
template <class Obj>
OVecDebugImGuiSnapshot FillSnapshot(const OVecDebug<Obj>& dbg, const char* name, ImVec4 color)
{
    OVecDebugImGuiSnapshot s;
    s.name = name;
    s.color = color;

    // ---- Capacity ----
    // active_objs / free_count are private on o_vector but OVecDebug already
    // has friend access; we only need the public OVecDebug surface here.
    const auto fr = dbg.analyze_fragmentation();
    s.fill_ratio = dbg.fill_ratio();

    // active/free aren't exposed as standalone public getters on OVecDebug,
    // but array_size and fill_ratio together let us recover them exactly:
    //   array_size = active + free,  fill_ratio = active / array_size
    // Rather than relying on floating-point back-derivation, reuse the same
    // fragmentation report's bounds-free fields plus total ops counters
    // is fragile — so the cleanest fix is to read them directly: OVecDebug
    // has no public active()/free() accessor yet, so this fills what's
    // available and leaves the rest at 0 until that accessor exists.
    s.active = dbg.active_count();
    s.free_slots = dbg.free_count();
    s.array_size = s.active + s.free_slots;

    // ---- Memory ----
    s.heap_bytes = dbg.estimated_heap_bytes();
    s.wasted_bytes = dbg.wasted_bytes();

    // ---- Fragmentation ----
    s.frag_score = fr.fragmentation_score;
    s.hole_count = fr.hole_count;
    s.longest_hole = fr.longest_hole;
    s.longest_run = fr.longest_active_run;
    s.min_active_index = fr.min_active_index;
    s.max_active_index = fr.max_active_index;
    s.spread_ratio = fr.spread_ratio;

    // ---- Operations (lifetime) ----
    s.total_emplaces = dbg.total_emplaces;
    s.total_adds = dbg.total_adds;
    s.total_removes = dbg.total_removes;
    s.failed_adds = dbg.failed_adds;
    s.resize_grows = dbg.resize_grow_events;
    s.resize_shrinks = dbg.resize_shrink_events;
    s.integrity_failures = dbg.integrity_failures;

    // ---- Peaks ----
    s.peak_active = dbg.peak_active;
    s.peak_array_size = dbg.peak_array_size;
    s.peak_free_count = dbg.peak_free_count;

    // ---- Churn ----
    s.total_churn = dbg.total_churn();
    s.avg_churn = dbg.average_churn_per_slot();
    s.most_churned_slot = dbg.most_churned_slot();
    // OVecDebug doesn't expose the count for the most-churned slot directly;
    // most_churned_slot() returns an index. We'd need slot_churn_[idx] which
    // is private. 
    s.most_churned_count = dbg.churn_at(s.most_churned_slot);

    // ---- Timing ----
    s.ops_per_sec = dbg.ops_per_second();
    s.uptime_ms = dbg.uptime_ms();

    // ---- Snapshots ----
    s.snapshot_count = dbg.snapshot_count();

    return s;
}