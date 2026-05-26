import numpy as np


# -----------------------------------------------------------------------------
# User settings
# -----------------------------------------------------------------------------
#
# Time step between saved trajectory frames for the x-axis.
# Examples:
#   1.0   for frame index spacing
#   0.002 for 0.002 ps between saved frames
#
TIME_STEP = 1.0

# Label for the x-axis.
# Examples:
#   "frames"
#   "ps"
#   "ns"
#
TIME_UNIT_LABEL = "frames"

# Optional frame range controls.
# Leave STOP_FRAME = None to use the full trajectory.
START_FRAME = 0
STOP_FRAME = None
FRAME_STRIDE = 1

# Upstream Willard-Chandler cavity table name.
# This table already contains only cavity regions:
#   Filled   = 0
#   Exterior = 0
#
CAVITY_TABLE_NAME = "willard-chandler-cavities"

# Exclude the largest cavity in each frame before summing the remaining cavity
# volumes/surface areas.
#
EXCLUDE_LARGEST_CAVITY = True

# Which metric to use when deciding which cavity is the "largest".
# Allowed values:
#   "surface_area"
#   "volume"
#
EXCLUDE_LARGEST_BY = "surface_area"

# Output table name.
OUTPUT_TABLE_NAME = "WC cavity volume and surface area vs time"


def _frame_range(num_frames):
    start = max(0, int(START_FRAME))
    stop = num_frames - 1 if STOP_FRAME is None else min(int(STOP_FRAME), num_frames - 1)
    stride = max(1, int(FRAME_STRIDE))
    if stop < start:
        return []
    return list(range(start, stop + 1, stride))


def _extract_cavity_metrics(frame_data):
    cavity_table = frame_data.tables.get(CAVITY_TABLE_NAME, None)
    if cavity_table is None:
        return {
            "count_total": 0,
            "count_used": 0,
            "total_volume": 0.0,
            "total_surface_area": 0.0,
            "sum_volume_excluding_largest": 0.0,
            "sum_surface_area_excluding_largest": 0.0,
            "largest_volume": 0.0,
            "largest_surface_area": 0.0,
        }

    metrics = np.asarray(cavity_table.y, dtype=float)
    if metrics.size == 0:
        return {
            "count_total": 0,
            "count_used": 0,
            "total_volume": 0.0,
            "total_surface_area": 0.0,
            "sum_volume_excluding_largest": 0.0,
            "sum_surface_area_excluding_largest": 0.0,
            "largest_volume": 0.0,
            "largest_surface_area": 0.0,
        }

    if metrics.ndim == 1:
        metrics = metrics.reshape(-1, 1)

    if metrics.shape[1] < 2:
        raise RuntimeError(
            f"Upstream table '{CAVITY_TABLE_NAME}' does not contain both Volume "
            "and Surface Area columns in its Y data."
        )

    volumes = np.asarray(metrics[:, 0], dtype=float)
    surface_areas = np.asarray(metrics[:, 1], dtype=float)
    count_total = int(len(volumes))

    largest_index = -1
    if count_total > 0:
        metric_name = str(EXCLUDE_LARGEST_BY).strip().lower()
        if metric_name == "surface_area":
            largest_index = int(np.argmax(surface_areas))
        elif metric_name == "volume":
            largest_index = int(np.argmax(volumes))
        else:
            raise RuntimeError(
                f"Unknown EXCLUDE_LARGEST_BY={EXCLUDE_LARGEST_BY!r}. "
                "Use 'surface_area' or 'volume'."
            )

    keep_mask = np.ones(count_total, dtype=bool)
    if EXCLUDE_LARGEST_CAVITY and largest_index >= 0:
        keep_mask[largest_index] = False

    return {
        "count_total": count_total,
        "count_used": int(np.count_nonzero(keep_mask)),
        "total_volume": float(np.sum(volumes)),
        "total_surface_area": float(np.sum(surface_areas)),
        "sum_volume_excluding_largest": float(np.sum(volumes[keep_mask])),
        "sum_surface_area_excluding_largest": float(np.sum(surface_areas[keep_mask])),
        "largest_volume": float(volumes[largest_index]) if largest_index >= 0 else 0.0,
        "largest_surface_area": float(surface_areas[largest_index]) if largest_index >= 0 else 0.0,
    }


def modify(frame, data, upstream, cache, progress):
    frames = _frame_range(upstream.num_frames)
    if not frames:
        raise RuntimeError("No frames selected. Check START_FRAME / STOP_FRAME / FRAME_STRIDE.")

    cache_key = (
        tuple(frames),
        float(TIME_STEP),
        str(TIME_UNIT_LABEL),
        str(CAVITY_TABLE_NAME),
        bool(EXCLUDE_LARGEST_CAVITY),
        str(EXCLUDE_LARGEST_BY),
        str(OUTPUT_TABLE_NAME),
    )

    if cache.get("cache_key") != cache_key:
        progress.text("Sampling Willard-Chandler cavities over time")

        volumes_excluding_largest = []
        surface_areas_excluding_largest = []
        total_volumes = []
        total_surface_areas = []
        cavity_counts_total = []
        cavity_counts_used = []
        largest_volumes = []
        largest_surface_areas = []

        for index, sample_frame in enumerate(frames):
            progress.fraction(index / max(1, len(frames) - 1))
            progress.check_canceled()

            frame_data = upstream.compute(sample_frame)
            metrics = _extract_cavity_metrics(frame_data)

            volumes_excluding_largest.append(metrics["sum_volume_excluding_largest"])
            surface_areas_excluding_largest.append(metrics["sum_surface_area_excluding_largest"])
            total_volumes.append(metrics["total_volume"])
            total_surface_areas.append(metrics["total_surface_area"])
            cavity_counts_total.append(metrics["count_total"])
            cavity_counts_used.append(metrics["count_used"])
            largest_volumes.append(metrics["largest_volume"])
            largest_surface_areas.append(metrics["largest_surface_area"])

        cache["cache_key"] = cache_key
        cache["frame_indices"] = np.asarray(frames, dtype=float)
        cache["times"] = np.asarray(frames, dtype=float) * float(TIME_STEP)
        cache["volumes_excluding_largest"] = np.asarray(volumes_excluding_largest, dtype=float)
        cache["surface_areas_excluding_largest"] = np.asarray(surface_areas_excluding_largest, dtype=float)
        cache["total_volumes"] = np.asarray(total_volumes, dtype=float)
        cache["total_surface_areas"] = np.asarray(total_surface_areas, dtype=float)
        cache["cavity_counts_total"] = np.asarray(cavity_counts_total, dtype=int)
        cache["cavity_counts_used"] = np.asarray(cavity_counts_used, dtype=int)
        cache["largest_volumes"] = np.asarray(largest_volumes, dtype=float)
        cache["largest_surface_areas"] = np.asarray(largest_surface_areas, dtype=float)

    progress.text(f"Writing cavity time series at frame {frame}")

    frame_indices = np.asarray(cache["frame_indices"], dtype=float)
    current_matches = np.where(frame_indices == float(frame))[0]
    current_index = int(current_matches[0]) if len(current_matches) else 0

    data.create_table(
        OUTPUT_TABLE_NAME,
        x=np.asarray(cache["times"], dtype=float),
        y=np.column_stack(
            (
                np.asarray(cache["volumes_excluding_largest"], dtype=float),
                np.asarray(cache["surface_areas_excluding_largest"], dtype=float),
                np.asarray(cache["total_volumes"], dtype=float),
                np.asarray(cache["total_surface_areas"], dtype=float),
                np.asarray(cache["largest_volumes"], dtype=float),
                np.asarray(cache["largest_surface_areas"], dtype=float),
                np.asarray(cache["cavity_counts_total"], dtype=float),
                np.asarray(cache["cavity_counts_used"], dtype=float),
            )
        ),
        title=OUTPUT_TABLE_NAME,
        axis_label_x=f"Time ({TIME_UNIT_LABEL})",
        axis_label_y="Cavity volume / surface area",
        y_component_names=[
            "Cavity volume sum excluding largest",
            "Cavity surface area sum excluding largest",
            "Total cavity volume",
            "Total cavity surface area",
            "Largest cavity volume",
            "Largest cavity surface area",
            "Total cavity count",
            "Cavity count used",
        ],
    )

    data.attributes["PythonModifier.cavity_time_series_frame_count"] = int(len(cache["frame_indices"]))
    data.attributes["PythonModifier.cavity_time_step"] = float(TIME_STEP)
    data.attributes["PythonModifier.cavity_time_unit"] = str(TIME_UNIT_LABEL)
    data.attributes["PythonModifier.cavity_table_name"] = str(CAVITY_TABLE_NAME)
    data.attributes["PythonModifier.exclude_largest_cavity"] = 1 if EXCLUDE_LARGEST_CAVITY else 0
    data.attributes["PythonModifier.exclude_largest_by"] = str(EXCLUDE_LARGEST_BY)
    data.attributes["PythonModifier.cavity_volume_sum_excluding_largest"] = float(
        cache["volumes_excluding_largest"][current_index]
    )
    data.attributes["PythonModifier.cavity_surface_area_sum_excluding_largest"] = float(
        cache["surface_areas_excluding_largest"][current_index]
    )
    data.attributes["PythonModifier.total_cavity_volume"] = float(cache["total_volumes"][current_index])
    data.attributes["PythonModifier.total_cavity_surface_area"] = float(cache["total_surface_areas"][current_index])
    data.attributes["PythonModifier.largest_cavity_volume"] = float(cache["largest_volumes"][current_index])
    data.attributes["PythonModifier.largest_cavity_surface_area"] = float(
        cache["largest_surface_areas"][current_index]
    )
    data.attributes["PythonModifier.total_cavity_count"] = int(cache["cavity_counts_total"][current_index])
    data.attributes["PythonModifier.cavity_count_used"] = int(cache["cavity_counts_used"][current_index])
