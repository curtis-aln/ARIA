#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
//  OVecDebug<Obj>  —  live diagnostic wrapper for o_vector<Obj>
//
//  Setup (one-time, in o_vector's private section):
//      template <class Obj2> friend class OVecDebug;
//
//  Note: o_vector::emplace() assigns object->id_ but o_vec_object defines
//  o_vec_index. This class uses o_vec_index (the struct definition). If your
//  Obj uses id_, find-replace o_vec_index -> id_ in this file.
//
//  Not thread-safe. Direct calls to the underlying vec_ that bypass
//  OVecDebug will not be tracked.
//
//  Quick-start:
//      o_vector<Cell>  cells(512);
//      OVecDebug<Cell> dbg(cells);
//
//      // Use dbg.* instead of cells.* for tracked operations
//      Cell* c = dbg.emplace();
//      dbg.remove(c);
//      dbg.record_sample();                // or set dbg.auto_history = true
//
//      int snap0 = dbg.take_snapshot("before_wave");
//      // ... do work ...
//      int snap1 = dbg.take_snapshot("after_wave");
//
//      dbg.integrity_check();
//      dbg.print_report();
//      dbg.print_diff(snap0, snap1);
//      dbg.print_churn_histogram();
// ============================================================================

template <class Obj>
class OVecDebug
{
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // ========================================================================
    //  PUBLIC TYPES
    // ========================================================================
public:

    // ------------------------------------------------------------------------
    //  Snapshot — complete state captured at one instant
    // ------------------------------------------------------------------------
    struct Snapshot
    {
        std::string label;
        TimePoint   timestamp;

        int      active_count;   // vec_.active_objs
        int      free_count;     // vec_.free_count
        int      array_size;     // active_count + free_count
        float    fill_ratio;     // active / array_size
        float    fragmentation;  // 0 = compact, 1 = maximally fragmented

        uint64_t total_emplaces;
        uint64_t total_adds;
        uint64_t total_removes;
        uint64_t failed_adds;
        uint64_t total_churn;
    };

    // ------------------------------------------------------------------------
    //  SnapshotDiff — element-wise signed delta between two Snapshots
    // ------------------------------------------------------------------------
    struct SnapshotDiff
    {
        std::string from_label;
        std::string to_label;
        double      elapsed_ms;

        int      delta_active;
        int      delta_free;
        int      delta_array_size;
        float    delta_fill_ratio;
        float    delta_fragmentation;

        uint64_t delta_emplaces;
        uint64_t delta_adds;
        uint64_t delta_removes;
        uint64_t delta_failed_adds;
        uint64_t delta_churn;
    };

    // ------------------------------------------------------------------------
    //  FragmentationReport — spatial layout of active vs inactive slots
    // ------------------------------------------------------------------------
    struct FragmentationReport
    {
        int   hole_count;           // number of inactive gaps between active slots
        int   longest_hole;         // longest contiguous inactive run (slots)
        int   longest_active_run;   // longest contiguous active run (slots)
        int   min_active_index;     // lowest occupied slot index  (-1 if empty)
        int   max_active_index;     // highest occupied slot index (-1 if empty)
        float spread_ratio;         // (max - min + 1) / array_size
        float fragmentation_score;  // holes / active_objs, clamped [0, 1]
    };

    // ------------------------------------------------------------------------
    //  AlertConfig — thresholds that trigger warnings
    // ------------------------------------------------------------------------
    struct AlertConfig
    {
        float    max_fill_ratio = 0.95f;  // fire when fill_ratio exceeds this
        float    max_fragmentation = 0.50f;  // fire when frag score exceeds this
        uint32_t max_slot_churn = 1'000;  // fire when any slot reuse hits this
        bool     print_to_stderr = true;   // automatically write to std::cerr
    };

    // ------------------------------------------------------------------------
    //  Sample — one timestamped history entry
    // ------------------------------------------------------------------------
    struct Sample
    {
        TimePoint time;
        int       active;
        int       free_count;
        int       array_size;
    };

    // ========================================================================
    //  PUBLIC CONFIGURATION
    // ========================================================================

    AlertConfig alert_config;

    // Optional: receives alert strings in addition to (or instead of) stderr
    std::function<void(const std::string&)> alert_callback;

    // When true, every proxy operation automatically calls record_sample()
    bool auto_history = false;

    // ========================================================================
    //  PUBLIC COUNTERS (treat as read-only)
    // ========================================================================

    // Lifetime operation totals
    uint64_t total_emplaces = 0;
    uint64_t total_adds = 0;
    uint64_t total_removes = 0;
    uint64_t failed_adds = 0;  // add() returned nullptr
    uint64_t resize_grow_events = 0;  // smart_resize() grew the backing store
    uint64_t resize_shrink_events = 0;  // smart_resize() trimmed the backing store
    uint64_t integrity_failures = 0;  // integrity_check() found at least one error

    // Peak values, each timestamped from OVecDebug construction
    int       peak_active = 0;
    TimePoint peak_active_time;

    int       peak_array_size = 0;
    TimePoint peak_array_size_time;

    int       peak_free_count = 0;
    TimePoint peak_free_count_time;

    // Timestamped history, populated by record_sample() or auto_history
    std::vector<Sample> history;

    // ========================================================================
    //  PRIVATE STATE
    // ========================================================================

    o_vector<Obj>* vec_ = nullptr;
    TimePoint       creation_time_;

    // slot_churn_[i] = number of times slot i was recycled via add().
    // First allocation (emplace into a brand-new slot) does NOT count.
    std::vector<uint32_t> slot_churn_;

    std::vector<Snapshot>  snapshots_;

    // ========================================================================
    //  INTERNAL HELPERS
    // ========================================================================

    // array_size is private in o_vector; every slot is either active or free,
    // so this reconstruction is exact.
    int infer_array_size() const
    {
        return vec_->active_objs + vec_->free_count; // friend access
    }

    float compute_fill_ratio() const
    {
        const int arr = infer_array_size();
        return (arr == 0) ? 0.0f : static_cast<float>(vec_->active_objs) / arr;
    }

    // O(n) spatial scan of active_[] — requires friend access
    FragmentationReport compute_fragmentation() const
    {
        FragmentationReport r{};
        r.min_active_index = -1;
        r.max_active_index = -1;

        const int n = infer_array_size();
        if (n == 0 || vec_->active_objs == 0)
            return r;

        int cur_hole = 0;
        int cur_run = 0;

        for (int i = 0; i < n; ++i)
        {
            if (vec_->active_[i]) // friend access to private bitvector
            {
                // Flush any pending inactive gap
                if (cur_hole > 0)
                {
                    ++r.hole_count;
                    r.longest_hole = std::max(r.longest_hole, cur_hole);
                    cur_hole = 0;
                }
                ++cur_run;
                r.longest_active_run = std::max(r.longest_active_run, cur_run);

                if (r.min_active_index == -1) r.min_active_index = i;
                r.max_active_index = i;
            }
            else
            {
                if (cur_run > 0) cur_run = 0;
                ++cur_hole;
            }
        }
        // Flush an inactive tail (slots past the last active object)
        if (cur_hole > 0 && cur_run == 0)
        {
            ++r.hole_count;
            r.longest_hole = std::max(r.longest_hole, cur_hole);
        }

        if (r.min_active_index >= 0)
        {
            r.spread_ratio = static_cast<float>(
                r.max_active_index - r.min_active_index + 1) / n;

            // More holes than active objects approaches maximum fragmentation
            r.fragmentation_score = std::min(1.0f,
                static_cast<float>(r.hole_count) / std::max(1, vec_->active_objs));
        }

        return r;
    }

    // Refresh all three peak values after every proxy operation
    void update_peaks()
    {
        const auto now = Clock::now();

        if (vec_->active_objs > peak_active)
        {
            peak_active = vec_->active_objs;
            peak_active_time = now;
        }

        const int arr = infer_array_size();
        if (arr > peak_array_size)
        {
            peak_array_size = arr;
            peak_array_size_time = now;
        }

        const int fr = vec_->free_count; // friend
        if (fr > peak_free_count)
        {
            peak_free_count = fr;
            peak_free_count_time = now;
        }
    }

    void check_fill_and_frag_alerts()
    {
        const float fill = compute_fill_ratio();
        if (fill > alert_config.max_fill_ratio)
            trigger_alert("Fill ratio " + fmt(fill) +
                " exceeded threshold " + fmt(alert_config.max_fill_ratio));

        const float frag = compute_fragmentation().fragmentation_score;
        if (frag > alert_config.max_fragmentation)
            trigger_alert("Fragmentation " + fmt(frag) +
                " exceeded threshold " + fmt(alert_config.max_fragmentation));
    }

    void check_churn_alert(uint32_t slot)
    {
        if (slot < slot_churn_.size() &&
            slot_churn_[slot] == alert_config.max_slot_churn)
        {
            trigger_alert("Slot #" + std::to_string(slot) +
                " hit churn threshold of " + std::to_string(alert_config.max_slot_churn));
        }
    }

    void trigger_alert(const std::string& msg)
    {
        const std::string tagged = "[OVecDebug ALERT] " + msg;
        if (alert_config.print_to_stderr)
            std::cerr << tagged << "\n";
        if (alert_callback)
            alert_callback(tagged);
    }

    void ensure_churn_slot(uint32_t idx)
    {
        if (idx >= static_cast<uint32_t>(slot_churn_.size()))
            slot_churn_.resize(idx + 1, 0u);
    }

    void record_sample_internal()
    {
        history.push_back({ Clock::now(), vec_->active_objs, vec_->free_count, infer_array_size() });
    }

    uint64_t sum_churn() const
    {
        uint64_t total = 0;
        for (uint32_t c : slot_churn_) total += c;
        return total;
    }

    // ---- Formatting --------------------------------------------------------

    static std::string ascii_bar(float ratio, int width = 28)
    {
        ratio = std::max(0.0f, std::min(1.0f, ratio));
        const int filled = static_cast<int>(ratio * width);
        std::string bar(1, '[');
        bar.append(filled, '#');
        bar.append(width - filled, '.');
        bar += "] ";
        bar += std::to_string(static_cast<int>(ratio * 100.0f)) + "%";
        return bar;
    }

    double ms_from_creation() const
    {
        return std::chrono::duration<double, std::milli>(
            Clock::now() - creation_time_).count();
    }

    // ========================================================================
    //  CONSTRUCTION
    // ========================================================================
public:

    // Default constructor — call init() before using any other method.
    OVecDebug();

    // Initialising constructor — equivalent to default-construct then init().
    explicit OVecDebug(o_vector<Obj>& vec)
    {
        init(vec);
    }

    // Bind this debugger to a vector. Safe to call once after default
    // construction. Resets all counters and history so the debugger starts
    // clean relative to the vector's current state.
    void init(o_vector<Obj>& vec)
    {
        vec_ = &vec;
        creation_time_ = Clock::now();

        // Reset all lifetime counters so they reflect activity from this
        // init() call onwards, not any previous binding.
        total_emplaces = 0;
        total_adds = 0;
        total_removes = 0;
        failed_adds = 0;
        resize_grow_events = 0;
        resize_shrink_events = 0;
        integrity_failures = 0;
        peak_active = 0;
        peak_array_size = 0;
        peak_free_count = 0;

        slot_churn_.clear();
        history.clear();
        snapshots_.clear();

        slot_churn_.reserve(64);
        history.reserve(256);
        snapshots_.reserve(32);
    }

    // Returns true if init() has been called and a vector is bound.
    [[nodiscard]] bool is_initialised() const { return vec_ != nullptr; }

    OVecDebug(const OVecDebug&) = delete;
    OVecDebug& operator=(const OVecDebug&) = delete;

    // ========================================================================
    //  PROXY OPERATIONS
    //  Use these instead of calling vec_.* directly so every operation is seen.
    // ========================================================================

    // Wraps o_vector::emplace(). Recycled slots (internally routed through add)
    // are detected by checking whether the array grew; if not, the slot was reused.
    Obj* emplace(bool is_active = true, bool try_just_add = true)
    {
        const int arr_before = infer_array_size();

        Obj* obj = vec_->emplace(is_active, try_just_add);
        ++total_emplaces;

        if (obj)
        {
            ensure_churn_slot(obj->id_);

            // If arr stayed the same, emplace internally recycled a free slot
            const bool recycled = (infer_array_size() == arr_before);
            if (recycled)
            {
                ++slot_churn_[obj->id_];
                check_churn_alert(obj->id_);
            }
        }

        update_peaks();
        check_fill_and_frag_alerts();
        if (auto_history) record_sample_internal();
        return obj;
    }

    // Wraps o_vector::add(). Recycling a free slot always counts as churn.
    Obj* add()
    {
        Obj* obj = vec_->add();

        if (!obj)
        {
            ++failed_adds;
            return nullptr;
        }

        ++total_adds;
        ensure_churn_slot(obj->id_);
        ++slot_churn_[obj->id_];
        check_churn_alert(obj->id_);

        update_peaks();
        check_fill_and_frag_alerts();
        if (auto_history) record_sample_internal();
        return obj;
    }

    void remove(Obj* obj)
    {
        if (!obj) return;
        ++total_removes;
        vec_->remove(obj);
        if (auto_history) record_sample_internal();
    }

    void remove(unsigned idx)
    {
        ++total_removes;
        vec_->remove(idx);
        if (auto_history) record_sample_internal();
    }

    // Wraps o_vector::smart_resize() and classifies whether it grew or shrank
    void smart_resize()
    {
        const int arr_before = infer_array_size();
        vec_->smart_resize();
        const int arr_after = infer_array_size();

        if (arr_after > arr_before) ++resize_grow_events;
        else if (arr_after < arr_before) ++resize_shrink_events;

        update_peaks();
    }

    // ========================================================================
    //  HISTORY
    // ========================================================================

    void record_sample() { record_sample_internal(); }
    void clear_history() { history.clear(); }

    // ========================================================================
    //  SNAPSHOTS
    // ========================================================================

    // Returns the index of the new snapshot for use with diff/print_diff
    int take_snapshot(const std::string& label = "")
    {
        const FragmentationReport fr = compute_fragmentation();
        Snapshot s;
        s.label = label.empty() ? ("snap_" + std::to_string(snapshots_.size())) : label;
        s.timestamp = Clock::now();
        s.active_count = vec_->active_objs;
        s.free_count = vec_->free_count;  // friend
        s.array_size = infer_array_size();
        s.fill_ratio = compute_fill_ratio();
        s.fragmentation = fr.fragmentation_score;
        s.total_emplaces = total_emplaces;
        s.total_adds = total_adds;
        s.total_removes = total_removes;
        s.failed_adds = failed_adds;
        s.total_churn = sum_churn();
        snapshots_.push_back(std::move(s));
        return static_cast<int>(snapshots_.size()) - 1;
    }

    // Element-wise signed delta from snapshot[a] to snapshot[b]
    SnapshotDiff diff_snapshots(int a, int b) const
    {
        assert(a >= 0 && a < static_cast<int>(snapshots_.size()));
        assert(b >= 0 && b < static_cast<int>(snapshots_.size()));
        const Snapshot& sa = snapshots_[a];
        const Snapshot& sb = snapshots_[b];

        SnapshotDiff d;
        d.from_label = sa.label;
        d.to_label = sb.label;
        d.elapsed_ms = ms_between(sa.timestamp, sb.timestamp);
        d.delta_active = sb.active_count - sa.active_count;
        d.delta_free = sb.free_count - sa.free_count;
        d.delta_array_size = sb.array_size - sa.array_size;
        d.delta_fill_ratio = sb.fill_ratio - sa.fill_ratio;
        d.delta_fragmentation = sb.fragmentation - sa.fragmentation;
        d.delta_emplaces = sb.total_emplaces - sa.total_emplaces;
        d.delta_adds = sb.total_adds - sa.total_adds;
        d.delta_removes = sb.total_removes - sa.total_removes;
        d.delta_failed_adds = sb.failed_adds - sa.failed_adds;
        d.delta_churn = sb.total_churn - sa.total_churn;
        return d;
    }

    const Snapshot& get_snapshot(int i) const { return snapshots_[i]; }
    int             snapshot_count()    const { return static_cast<int>(snapshots_.size()); }

    // ========================================================================
    //  INTEGRITY CHECK
    //  Four independent checks. Returns true only if all pass.
    // ========================================================================
    bool integrity_check(std::ostream& out = std::cerr) const
    {
        bool ok = true;
        const int n = infer_array_size();

        auto fail = [&](const std::string& msg)
            {
                out << "[INTEGRITY FAIL] " << msg << "\n";
                ok = false;
            };

        // 1. Count set bits in active_[] and compare to active_objs
        {
            int bit_count = 0;
            for (int i = 0; i < n; ++i)
                if (vec_->active_[i]) ++bit_count; // friend

            if (bit_count != vec_->active_objs)
                fail("active_[] bit count=" + std::to_string(bit_count) +
                    " != active_objs=" + std::to_string(vec_->active_objs));
        }

        // 2. Iterate and count active objects; also verify each stored o_vec_index
        //    falls within bounds and matches an active slot
        {
            int iter_count = 0;
            for (const Obj* obj : *vec_)
            {
                ++iter_count;
                const uint32_t stored = obj->id_;

                if (static_cast<int>(stored) >= n)
                    fail("Object id_=" + std::to_string(stored) +
                        " >= array_size=" + std::to_string(n));
                else if (!vec_->active_[stored]) // friend
                    fail("Object id_=" + std::to_string(stored) +
                        " is not marked active in active_[]");
            }

            if (iter_count != vec_->active_objs)
                fail("Iteration yielded " + std::to_string(iter_count) +
                    " objects, expected active_objs=" + std::to_string(vec_->active_objs));
        }

        // 3. Walk the free list: no duplicates, no out-of-range, no active slots
        {
            const int fc = vec_->free_count; // friend
            std::vector<bool> in_free(n, false);

            for (int i = 0; i < fc; ++i)
            {
                const uint32_t idx = vec_->free_list[i]; // friend

                if (static_cast<int>(idx) >= n)
                {
                    fail("free_list[" + std::to_string(i) + "]=" + std::to_string(idx) +
                        " out of range (array_size=" + std::to_string(n) + ")");
                    continue;
                }
                if (in_free[idx])
                    fail("Duplicate in free_list: slot " + std::to_string(idx));

                in_free[idx] = true;

                if (vec_->active_[idx]) // friend
                    fail("Slot " + std::to_string(idx) +
                        " is in free_list but active_[" + std::to_string(idx) + "]=true");
            }
        }

        // 4. Inactive slot count must equal free_count
        {
            int inactive = 0;
            for (int i = 0; i < n; ++i)
                if (!vec_->active_[i]) ++inactive; // friend

            if (inactive != vec_->free_count) // friend
                fail("Inactive slot count=" + std::to_string(inactive) +
                    " != free_count=" + std::to_string(vec_->free_count));
        }

        if (ok)
            out << "[INTEGRITY OK] All checks passed. (array_size=" << n << ")\n";
        else
            ++const_cast<OVecDebug*>(this)->integrity_failures;

        return ok;
    }

    // ========================================================================
    //  ANALYSIS QUERIES
    // ========================================================================

    float               fill_ratio()           const { return compute_fill_ratio(); }
    FragmentationReport analyze_fragmentation() const { return compute_fragmentation(); }

    // Index of the slot that has been recycled the most times
    uint32_t most_churned_slot() const
    {
        if (slot_churn_.empty()) return 0u;
        return static_cast<uint32_t>(
            std::max_element(slot_churn_.begin(), slot_churn_.end()) - slot_churn_.begin());
    }

    uint64_t total_churn()          const { return sum_churn(); }
    float    average_churn_per_slot() const
    {
        return slot_churn_.empty()
            ? 0.0f
            : static_cast<float>(sum_churn()) / static_cast<float>(slot_churn_.size());
    }

    double uptime_ms()    const { return ms_from_creation(); }
    double ops_per_second() const
    {
        const double sec = uptime_ms() / 1000.0;
        return (sec < 1e-6) ? 0.0
            : static_cast<double>(total_emplaces + total_adds + total_removes) / sec;
    }

    // Approximate total heap bytes owned by o_vector's four internal vectors
    size_t estimated_heap_bytes() const
    {
        const size_t arr = static_cast<size_t>(infer_array_size());
        return arr * sizeof(Obj)          // raw_object_store_
            + arr * sizeof(uint32_t)     // all_object_indexes_
            + arr / 8 + 1               // active_  (std::vector<bool> packs bits)
            + arr * sizeof(uint32_t);    // free_list
    }

    // Bytes consumed by inactive slots sitting in raw_object_store_
    size_t wasted_bytes() const
    {
        return static_cast<size_t>(vec_->free_count) * sizeof(Obj); // friend
    }

    // ========================================================================
    //  REPORTING
    // ========================================================================

    // Single-line status — suitable for per-frame / per-tick logging
    void print_compact(std::ostream& out = std::cout) const
    {
        out << "[OVecDebug]"
            << "  active=" << std::setw(6) << vec_->active_objs
            << "  free=" << std::setw(6) << vec_->free_count    // friend
            << "  arr=" << std::setw(6) << infer_array_size()
            << "  fill=" << ascii_bar(compute_fill_ratio(), 20)
            << "  ops=" << (total_emplaces + total_adds + total_removes)
            << "\n";
    }

    // Full verbose report with all sections
    void print_report(std::ostream& out = std::cout) const
    {
        const auto sep = std::string(58, '-') + "\n";
        const FragmentationReport fr = compute_fragmentation();
        const int   arr = infer_array_size();
        const float uptime = static_cast<float>(uptime_ms());

        out << "\n" << sep;
        out << "  OVecDebug — Full Diagnostic Report\n";
        out << sep;

        // ---- Capacity -------------------------------------------------------
        out << "  [Capacity]\n";
        out << "    Array size (active + free):  " << arr << "\n";
        out << "    Active objects:              " << vec_->active_objs << "\n";
        out << "    Free slots:                  " << vec_->free_count << "\n"; // friend
        out << "    Fill ratio:                  " << ascii_bar(compute_fill_ratio()) << "\n";
        out << "    resize_buffering:            " << vec_->resize_buffering << "\n\n";

        // ---- Memory ---------------------------------------------------------
        out << "  [Memory (estimate)]\n";
        const size_t heap = estimated_heap_bytes();
        out << "    Total heap:  " << heap << " B  ("
            << fmt(heap / 1024.0f, 1) << " KB)\n";
        out << "    Wasted:      " << wasted_bytes() << " B"
            << "  (" << vec_->free_count << " inactive slots × " // friend
            << sizeof(Obj) << " B)\n\n";

        // ---- Peaks ----------------------------------------------------------
        out << "  [Peaks]\n";
        out << "    Peak active:      " << peak_active
            << "  (+" << fmt(ms_between(creation_time_, peak_active_time), 1) << " ms)\n";
        out << "    Peak array size:  " << peak_array_size
            << "  (+" << fmt(ms_between(creation_time_, peak_array_size_time), 1) << " ms)\n";
        out << "    Peak free count:  " << peak_free_count
            << "  (+" << fmt(ms_between(creation_time_, peak_free_count_time), 1) << " ms)\n\n";

        // ---- Fragmentation --------------------------------------------------
        out << "  [Fragmentation]\n";
        out << "    Score:               " << ascii_bar(fr.fragmentation_score) << "\n";
        out << "    Hole count:          " << fr.hole_count << "\n";
        out << "    Longest hole:        " << fr.longest_hole << " slots\n";
        out << "    Longest active run:  " << fr.longest_active_run << " slots\n";
        out << "    Min active index:    " << fr.min_active_index << "\n";
        out << "    Max active index:    " << fr.max_active_index << "\n";
        out << "    Spread ratio:        " << fmt(fr.spread_ratio) << "\n\n";

        // ---- Churn ----------------------------------------------------------
        out << "  [Slot Churn]\n";
        out << "    Total reuse events:  " << sum_churn() << "\n";
        out << "    Avg per slot:        " << fmt(average_churn_per_slot()) << "\n";
        if (!slot_churn_.empty())
        {
            const uint32_t hot = most_churned_slot();
            out << "    Most churned:        slot #" << hot
                << "  (" << slot_churn_[hot] << " reuses)\n";
        }
        out << "\n";

        // ---- Operations -----------------------------------------------------
        out << "  [Operations]\n";
        out << "    Emplaces:          " << total_emplaces << "\n";
        out << "    Adds:              " << total_adds << "\n";
        out << "    Removes:           " << total_removes << "\n";
        out << "    Failed adds:       " << failed_adds << "\n";
        out << "    Resize grows:      " << resize_grow_events << "\n";
        out << "    Resize shrinks:    " << resize_shrink_events << "\n";
        out << "    Integrity fails:   " << integrity_failures << "\n";
        out << "    Uptime:            " << fmt(uptime, 1) << " ms\n";
        out << "    Ops/sec:           " << fmt(static_cast<float>(ops_per_second()), 0) << "\n\n";

        // ---- History sparkline ----------------------------------------------
        if (!history.empty())
        {
            const int max_a = std::max_element(history.begin(), history.end(),
                [](const Sample& a, const Sample& b) { return a.active < b.active; })->active;

            out << "  [History]  " << history.size() << " samples  peak=" << max_a << "\n";
            out << "    Active |";
            for (const Sample& s : history)
            {
                // 9 Unicode block characters from empty → full
                const char* glyphs[] = { " ","▁","▂","▃","▄","▅","▆","▇","█" };
                const int tier = (max_a > 0) ? (s.active * 8 / max_a) : 0;
                out << glyphs[std::max(0, std::min(8, tier))];
            }
            out << "|\n\n";
        }

        // ---- Snapshot table -------------------------------------------------
        if (!snapshots_.empty())
        {
            out << "  [Snapshots]  " << snapshots_.size() << " taken\n";
            out << "    " << std::left
                << std::setw(4) << "#"
                << std::setw(22) << "Label"
                << std::setw(8) << "Active"
                << std::setw(10) << "Fill"
                << std::setw(10) << "Frag"
                << "\n";
            for (int i = 0; i < static_cast<int>(snapshots_.size()); ++i)
            {
                const Snapshot& s = snapshots_[i];
                out << "    " << std::left
                    << std::setw(4) << i
                    << std::setw(22) << s.label.substr(0, 21)
                    << std::setw(8) << s.active_count
                    << std::setw(10) << fmt(s.fill_ratio)
                    << std::setw(10) << fmt(s.fragmentation)
                    << "\n";
            }
            out << "\n";
        }

        out << sep;
    }

    // Top-N most reused slots with a scaled ASCII bar chart
    void print_churn_histogram(std::ostream& out = std::cout, int top_n = 10) const
    {
        if (slot_churn_.empty())
        {
            out << "  [Churn Histogram]  No data yet.\n";
            return;
        }

        std::vector<std::pair<uint32_t, uint32_t>> entries;
        entries.reserve(slot_churn_.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(slot_churn_.size()); ++i)
            if (slot_churn_[i] > 0)
                entries.push_back({ i, slot_churn_[i] });

        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        const int      shown = std::min(top_n, static_cast<int>(entries.size()));
        const uint32_t max_c = shown > 0 ? entries[0].second : 1u;

        out << "  [Churn Histogram]  Top " << shown << " most reused slots:\n";
        for (int i = 0; i < shown; ++i)
        {
            out << "    Slot #" << std::setw(6) << entries[i].first << "  "
                << ascii_bar(static_cast<float>(entries[i].second) / max_c, 24)
                << "  ×" << entries[i].second << "\n";
        }
    }

    // Signed delta between two named snapshots
    void print_diff(int snap_a, int snap_b, std::ostream& out = std::cout) const
    {
        const SnapshotDiff d = diff_snapshots(snap_a, snap_b);
        const auto si = [](int v) { return (v >= 0 ? "+" : "") + std::to_string(v); };
        const auto sf = [](float v) { return (v >= 0 ? "+" : "") + fmt(v); };

        out << "  [Snapshot Diff]  \"" << d.from_label
            << "\"  →  \"" << d.to_label
            << "\"  (" << fmt(static_cast<float>(d.elapsed_ms), 2) << " ms)\n";
        out << "    Active:         " << si(d.delta_active) << "\n";
        out << "    Free:           " << si(d.delta_free) << "\n";
        out << "    Array size:     " << si(d.delta_array_size) << "\n";
        out << "    Fill ratio:     " << sf(d.delta_fill_ratio) << "\n";
        out << "    Fragmentation:  " << sf(d.delta_fragmentation) << "\n";
        out << "    Emplaces:       +" << d.delta_emplaces << "\n";
        out << "    Adds:           +" << d.delta_adds << "\n";
        out << "    Removes:        +" << d.delta_removes << "\n";
        out << "    Failed adds:    +" << d.delta_failed_adds << "\n";
        out << "    Churn events:   +" << d.delta_churn << "\n";
    }

private:
    // Silence "unused lambda capture" warnings on MSVC when fmt is used in lambdas
    static std::string fmt(float v, int decimals = 3)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(decimals) << v;
        return oss.str();
    }

    double ms_between(const TimePoint& a, const TimePoint& b) const
    {
        return std::chrono::duration<double, std::milli>(b - a).count();
    }
};