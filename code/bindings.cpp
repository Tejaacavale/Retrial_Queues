#include <pybind11/pybind11.h>
#include <pybind11/stl.h>   // auto-converts std::vector ↔ Python list
#include <random>
#include "simulate.h"

namespace py = pybind11;

PYBIND11_MODULE(retrial_queue, m) {
    m.doc() =
        "M/M/1 retrial queue simulator.\n\n"
        "Typical usage::\n\n"
        "    import retrial_queue\n"
        "    result = retrial_queue.simulate(10000, arrival_rate=1.0,\n"
        "                                    service_rate=5.0, retrial_rate=3.0)\n"
        "    print(result.avg_sojourn_time)\n";

    // --- Job ---
    py::class_<Job>(m, "Job",
        "A single customer's lifecycle: arrival, optional retrials, and departure.")
        .def_readonly("scheduled_time", &Job::scheduled_time,
            "Next scheduled attempt time (== arrival_time unless the job retried).")
        .def_readonly("arrival_time",   &Job::arrival_time,
            "Original arrival time (immutable).")
        .def_readonly("departure_time", &Job::departure_time,
            "Time service completed.")
        .def_readonly("retry_count",    &Job::retry_count,
            "Number of retrials before reaching the server.")
        .def("__repr__", [](const Job& j) {
            return "<Job arrival=" + std::to_string(j.arrival_time)
                 + " departure=" + std::to_string(j.departure_time)
                 + " retries=" + std::to_string(j.retry_count) + ">";
        });

    // --- SimResult ---
    py::class_<SimResult>(m, "SimResult",
        "Simulation output. Array properties return plain Python lists "
        "that convert to numpy arrays with np.array(...).")
        .def_readonly("completed",         &SimResult::completed,
            "List of Job objects in service-completion order.")
        .def_readonly("avg_wait_time",     &SimResult::avg_wait_time,
            "Mean orbit wait: avg time from arrival to entering service.")
        .def_readonly("avg_sojourn_time",  &SimResult::avg_sojourn_time,
            "Mean sojourn time: avg total time from arrival to departure.")
        .def_readonly("avg_idle_interval", &SimResult::avg_idle_interval,
            "Mean server-idle gap between consecutive services.")
        .def_readonly("mean_n",            &SimResult::mean_n,
            "Time-averaged mean jobs in system (Little's Law: E[N]).")
        // Convenience array accessors — avoids iterating over completed in Python
        .def_property_readonly("arrival_times", [](const SimResult& r) {
            std::vector<double> v; v.reserve(r.completed.size());
            for (const Job& j : r.completed) v.push_back(j.arrival_time);
            return v;
        }, "Arrival times of all jobs (sorted by service order).")
        .def_property_readonly("departure_times", [](const SimResult& r) {
            std::vector<double> v; v.reserve(r.completed.size());
            for (const Job& j : r.completed) v.push_back(j.departure_time);
            return v;
        }, "Departure times of all jobs.")
        .def_property_readonly("scheduled_times", [](const SimResult& r) {
            std::vector<double> v; v.reserve(r.completed.size());
            for (const Job& j : r.completed) v.push_back(j.scheduled_time);
            return v;
        }, "Final scheduled (service-entry) times of all jobs.")
        .def_property_readonly("retry_counts", [](const SimResult& r) {
            std::vector<int> v; v.reserve(r.completed.size());
            for (const Job& j : r.completed) v.push_back(j.retry_count);
            return v;
        }, "Number of retrials per job.");

    // --- simulate() ---
    m.def("simulate",
        [](int n_jobs, double arrival_rate, double service_rate,
           double retrial_rate, unsigned int seed) {
            std::mt19937 rng(seed);
            return simulate(n_jobs, arrival_rate, service_rate, retrial_rate, rng);
        },
        py::arg("n_jobs"),
        py::arg("arrival_rate"),
        py::arg("service_rate"),
        py::arg("retrial_rate"),
        py::arg("seed") = std::random_device{}(),
        R"(Simulate the M/M/1 retrial queue.

Parameters
----------
n_jobs : int
    Number of primary customers to simulate.
arrival_rate : float
    λ — Poisson primary arrival rate. System is stable when λ/μ < 1.
service_rate : float
    μ — Exponential service rate.
retrial_rate : float
    ν — Per-customer exponential retrial rate from orbit.
seed : int, optional
    RNG seed. Omit for a random seed; set for reproducible output.

Returns
-------
SimResult
    Simulation results including per-job data and summary metrics.

Examples
--------
>>> import retrial_queue, numpy as np
>>> r = retrial_queue.simulate(10000, arrival_rate=1.0,
...                             service_rate=5.0, retrial_rate=3.0, seed=42)
>>> print(f"E[T] = {r.avg_sojourn_time:.4f}")
>>> arrivals = np.array(r.arrival_times)
)");

    // --- compute_blocking_probabilities() ---
    m.def("compute_blocking_probabilities",
        &compute_blocking_probabilities,
        py::arg("completed"),
        R"(Compute a per-job running estimate of blocking probability.

Parameters
----------
completed : list[Job]
    The completed jobs list from SimResult.completed.

Returns
-------
list[float]
    Running server-utilization estimate at each job's departure.
    Converges to the true blocking probability as n → ∞.
)");
}
