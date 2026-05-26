import os
import tempfile

import numpy as np


def _load_sorted_velocities(upstream, progress, dtype=np.float32):
    n_frames = int(upstream.num_frames)
    if n_frames < 2:
        raise RuntimeError("Need at least two frames to compute a VACF.")

    first_frame = upstream.compute(0)
    if first_frame.particles is None or "Velocity" not in first_frame.particles:
        raise RuntimeError("The upstream pipeline does not provide a 'Velocity' particle property.")

    first_velocities = np.asarray(first_frame.particles["Velocity"], dtype=dtype)
    if "Particle Identifier" in first_frame.particles:
        first_ids = np.asarray(first_frame.particles["Particle Identifier"], dtype=np.int64)
        first_order = np.argsort(first_ids, kind="stable")
        sorted_ids = first_ids[first_order]
    else:
        first_order = np.arange(len(first_velocities))
        sorted_ids = first_order

    fd, memmap_name = tempfile.mkstemp(prefix="ovito_modifier_vacf_", suffix=".dat")
    os.close(fd)
    velocities = np.memmap(memmap_name, mode="w+", dtype=dtype, shape=(n_frames - 1, len(first_order), 3))

    for frame_index in range(1, n_frames):
        progress.fraction(frame_index / max(1, n_frames - 1))
        progress.check_canceled()
        frame_data = upstream.compute(frame_index)
        if frame_data.particles is None or "Velocity" not in frame_data.particles:
            raise RuntimeError(f"Frame {frame_index} does not contain a 'Velocity' particle property.")

        frame_velocities = np.asarray(frame_data.particles["Velocity"], dtype=dtype)
        if "Particle Identifier" in frame_data.particles:
            frame_ids = np.asarray(frame_data.particles["Particle Identifier"], dtype=np.int64)
            frame_order = np.argsort(frame_ids, kind="stable")
            if not np.array_equal(frame_ids[frame_order], sorted_ids):
                raise RuntimeError(f"Particle identifiers changed between frames 0 and {frame_index}.")
            velocities[frame_index - 1] = frame_velocities[frame_order]
        else:
            velocities[frame_index - 1] = frame_velocities[first_order]

    velocities.flush()
    return memmap_name, n_frames - 1, len(first_order)


def _compute_vacf(velocities, progress, batch_size=4096):
    n_frames, n_particles, n_components = velocities.shape
    nfft = 1 << (2 * n_frames - 1).bit_length()
    summed_acf = np.zeros((n_frames, n_components), dtype=np.float64)

    for start in range(0, n_particles, batch_size):
        end = min(start + batch_size, n_particles)
        progress.text(f"VACF batch {start}:{end} / {n_particles}")
        progress.fraction(start / max(1, n_particles))
        progress.check_canceled()
        batch = velocities[:, start:end, :].astype(np.float64, copy=False)
        spectrum = np.fft.rfft(batch, n=nfft, axis=0)
        acf = np.fft.irfft(spectrum * np.conjugate(spectrum), n=nfft, axis=0)[:n_frames]
        summed_acf += acf.sum(axis=1)

    normalization = (n_particles * (n_frames - np.arange(n_frames))).reshape(-1, 1)
    vacf = summed_acf / normalization
    vacf_isotropic = vacf.mean(axis=1)
    time = np.arange(n_frames, dtype=np.float64)
    return time, vacf_isotropic, vacf


def modify(frame, data, upstream, cache, progress):
    progress.text("Computing velocity autocorrelation")
    cache_key = ("vacf", int(upstream.num_frames))

    if cache.get("cache_key") != cache_key:
        memmap_path, n_frames, n_particles = _load_sorted_velocities(upstream, progress)
        try:
            velocities = np.memmap(memmap_path, mode="r", dtype=np.float32, shape=(n_frames, n_particles, 3))
            time, vacf_isotropic, vacf = _compute_vacf(velocities, progress)
            cache["cache_key"] = cache_key
            cache["time"] = time
            cache["vacf"] = np.column_stack((vacf_isotropic, vacf))
        finally:
            try:
                del velocities
            except UnboundLocalError:
                pass
            if os.path.exists(memmap_path):
                os.unlink(memmap_path)

    progress.text(f"Writing VACF table at frame {frame}")
    data.create_table(
        "VACF",
        x=np.asarray(cache["time"], dtype=float),
        y=np.asarray(cache["vacf"], dtype=float),
        title="Velocity Autocorrelation Function",
        axis_label_x="Lag (frames)",
        axis_label_y="VACF",
        y_component_names=["VACF", "VACF_x", "VACF_y", "VACF_z"],
    )
    data.attributes["PythonModifier.vacf_frame_count"] = int(upstream.num_frames)
