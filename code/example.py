"""
example.py — demonstrates the retrial_queue Python library.

Build the library first:
    cd code
    pip install .          # installs system-wide
    # or
    python setup.py build_ext --inplace  # builds .so next to this file
"""

import retrial_queue
import numpy as np

# --- Basic simulation ---
result = retrial_queue.simulate(
    n_jobs=10000,
    arrival_rate=1.0,   # λ
    service_rate=5.0,   # μ  (ρ = λ/μ = 0.2, stable)
    retrial_rate=3.0,   # ν
    seed=42,            # omit for random seed
)

print("=== Summary metrics ===")
print(f"  E[T] avg sojourn time   : {result.avg_sojourn_time:.4f}")
print(f"  E[W] avg orbit wait     : {result.avg_wait_time:.4f}")
print(f"  E[N] mean jobs in system: {result.mean_n:.4f}")
print(f"  avg idle interval       : {result.avg_idle_interval:.4f}")
print(f"  Little's check λ·E[T]   : {1.0 * result.avg_sojourn_time:.4f}")

# --- Access raw arrays (numpy-ready) ---
arrivals   = np.array(result.arrival_times)
departures = np.array(result.departure_times)
retries    = np.array(result.retry_counts)

print("\n=== Array access ===")
print(f"  arrivals[:5]   : {arrivals[:5]}")
print(f"  departures[:5] : {departures[:5]}")
print(f"  mean retry count: {retries.mean():.4f}")
print(f"  max retry count : {retries.max()}")

# --- Blocking probability ---
probs = retrial_queue.compute_blocking_probabilities(result.completed)
probs = np.array(probs)
print(f"\n=== Blocking probability ===")
print(f"  final estimate: {probs[-1]:.4f}  (expect ≈ ρ = {1.0/5.0:.2f})")

# --- Iterate over individual jobs ---
print("\n=== First 3 jobs ===")
for job in result.completed[:3]:
    print(f"  {job}")

# --- Reproducibility: same seed → identical results ---
r1 = retrial_queue.simulate(1000, 1.0, 5.0, 3.0, seed=99)
r2 = retrial_queue.simulate(1000, 1.0, 5.0, 3.0, seed=99)
assert r1.avg_sojourn_time == r2.avg_sojourn_time, "Reproducibility check failed"
print("\n=== Reproducibility ===")
print(f"  seed=99 twice → E[T] identical: {r1.avg_sojourn_time:.6f}")
