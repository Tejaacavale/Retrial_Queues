#pragma once
// simulate.h — M/M/1 retrial queue simulation core
//
// Model: customers arrive (Poisson at rate λ). If the server is free they are
// served immediately; otherwise they join an "orbit" and retry after an
// exponential delay (rate ν per customer). Service time is exponential (rate μ).

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <set>
#include <vector>

// One customer's lifecycle in the simulation.
struct Job {
    double scheduled_time;  // next attempt; starts = arrival, advances on each retrial
    double arrival_time;    // original arrival (immutable)
    double departure_time;  // time service completes; 0 until job is served
    int    retry_count;     // number of retrials before reaching the server

    // Sort by scheduled_time so std::set gives earliest-next-attempt ordering.
    bool operator<(const Job& other) const {
        if (scheduled_time != other.scheduled_time) return scheduled_time < other.scheduled_time;
        if (arrival_time   != other.arrival_time)   return arrival_time   < other.arrival_time;
        if (departure_time != other.departure_time) return departure_time < other.departure_time;
        return retry_count < other.retry_count;
    }
};

// Summary metrics returned by simulate().
struct SimResult {
    std::vector<Job> completed;    // served jobs in service-completion order
    double avg_wait_time;          // mean time from arrival to entering service (orbit wait)
    double avg_sojourn_time;       // mean total time from arrival to departure
    double avg_idle_interval;      // mean server-idle gap between consecutive services
    double mean_n;                 // time-averaged mean jobs in system (via Little's Law)
};

// Draw one Exp(rate) sample using the provided RNG.
inline double sample_exponential(double rate, std::mt19937& rng) {
    return std::exponential_distribution<double>(rate)(rng);
}

// Simulate the M/M/1 retrial queue.
//   n_jobs        : number of primary customers to generate
//   arrival_rate  : λ — Poisson primary arrival rate
//   service_rate  : μ — exponential service rate  (must satisfy λ/μ < 1 for stability)
//   retrial_rate  : ν — per-customer exponential retrial rate from orbit
//   rng           : caller-owned RNG; seed it for reproducibility
inline SimResult simulate(int n_jobs, double arrival_rate, double service_rate,
                          double retrial_rate, std::mt19937& rng) {
    // Phase 1: generate all primary arrivals and place them in the orbit set,
    // sorted by their initial scheduled (= arrival) time.
    std::set<Job> orbit;
    double clock = 0.0;
    for (int i = 0; i < n_jobs; i++) {
        clock += sample_exponential(arrival_rate, rng);
        orbit.insert({clock, clock, 0.0, 0});
    }

    // Phase 2: process jobs in scheduled-time order.
    // After each service, any job whose scheduled attempt falls before the
    // server becomes free must retry: its scheduled_time advances by an
    // exponential retrial delay. We repeat until no job in the orbit is
    // scheduled before the server is free, then serve the earliest remaining job.
    std::vector<Job> completed;
    bool   server_busy    = false;
    double server_free_at = 0.0;

    while (!orbit.empty()) {
        if (server_busy) {
            // Push all jobs that arrive before server is free further into
            // the future. Each such job draws a fresh retrial delay.
            // Repeat until the orbit is clear of pre-free-at events.
            bool any_blocked;
            do {
                any_blocked = false;
                std::set<Job> updated;
                for (Job job : orbit) {
                    if (job.scheduled_time < server_free_at) {
                        job.scheduled_time += sample_exponential(retrial_rate, rng);
                        job.retry_count    += 1;
                        any_blocked = true;
                    }
                    updated.insert(job);
                }
                orbit = std::move(updated);
            } while (any_blocked);

            server_busy = false;
        }

        // Server is free: take the earliest-scheduled job and serve it.
        Job next = *orbit.begin();
        orbit.erase(orbit.begin());
        next.departure_time = next.scheduled_time + sample_exponential(service_rate, rng);
        server_free_at      = next.departure_time;
        server_busy         = true;
        completed.push_back(next);
    }

    // Phase 3: compute summary metrics over all completed jobs.
    int    n             = static_cast<int>(completed.size());
    double total_wait    = 0.0;
    double total_sojourn = 0.0;
    for (const Job& j : completed) {
        total_wait    += j.scheduled_time - j.arrival_time;  // orbit wait
        total_sojourn += j.departure_time - j.arrival_time;  // full sojourn
    }

    double total_idle = 0.0;
    for (int i = 1; i < n; i++) {
        double gap = completed[i].scheduled_time - completed[i - 1].departure_time;
        if (gap > 0.0) total_idle += gap;
    }

    double sim_duration = completed[n - 1].departure_time;

    SimResult r;
    r.completed         = completed;
    r.avg_wait_time     = total_wait    / n;
    r.avg_sojourn_time  = total_sojourn / n;
    r.avg_idle_interval = total_idle    / n;
    r.mean_n            = total_sojourn / sim_duration;  // Little's Law: E[N] = λ·E[T]
    return r;
}

// Compute a per-job running estimate of blocking probability (server utilization).
// For each completed job i, finds the idle gap after its departure and accumulates
// it into idle_sum. Returns 1 - idle_sum/elapsed_time at each step.
inline std::vector<double> compute_blocking_probabilities(const std::vector<Job>& completed) {
    int n = static_cast<int>(completed.size());
    std::vector<double> probs;
    probs.reserve(n);

    double idle_sum = 0.0;
    const double INF = std::numeric_limits<double>::infinity();

    for (int i = 0; i < n; i++) {
        // Find the earliest event (any job's arrival or scheduled attempt)
        // that occurs strictly after job i's departure.
        double next_event = INF;
        for (int j = 0; j < n; j++) {
            if (completed[j].arrival_time > completed[i].departure_time)
                next_event = std::min(next_event, completed[j].arrival_time);
            if (completed[j].scheduled_time > completed[i].departure_time)
                next_event = std::min(next_event, completed[j].scheduled_time);
        }
        if (std::isfinite(next_event))
            idle_sum += next_event - completed[i].departure_time;

        probs.push_back(1.0 - idle_sum / completed[i].departure_time);
    }
    return probs;
}
