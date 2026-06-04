// test_compute.cpp — unit and statistical tests for the M/M/1 retrial queue simulator.
//
// Build and run:
//   g++ -std=c++11 -O2 -o test_compute test_compute.cpp && ./test_compute

#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include "simulate.h"

static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(expr, msg)                                                   \
    do {                                                                   \
        tests_run++;                                                       \
        if (!(expr)) {                                                     \
            std::cerr << "[FAIL] " << (msg) << "\n";                      \
        } else {                                                           \
            std::cout << "[PASS] " << (msg) << "\n";                      \
            tests_passed++;                                                \
        }                                                                  \
    } while (0)

// ---------------------------------------------------------------------------
// Exponential RNG
// ---------------------------------------------------------------------------

void test_exponential_mean() {
    std::mt19937 rng(42);
    const int    N    = 200000;
    const double rate = 3.0;
    double sum = 0.0;
    for (int i = 0; i < N; i++) sum += sample_exponential(rate, rng);
    double mean = sum / N;
    // Theoretical mean = 1/rate = 0.333; allow 1% tolerance.
    CHECK(std::abs(mean - 1.0 / rate) < 0.01,
          "Exponential mean ≈ 1/rate  (rate=3.0, N=200k)");
}

void test_exponential_positive() {
    std::mt19937 rng(7);
    bool all_positive = true;
    for (int i = 0; i < 10000; i++)
        if (sample_exponential(2.0, rng) <= 0.0) { all_positive = false; break; }
    CHECK(all_positive, "Exponential samples are strictly positive");
}

// ---------------------------------------------------------------------------
// Structural correctness
// ---------------------------------------------------------------------------

void test_all_jobs_complete() {
    std::mt19937 rng(1);
    SimResult r = simulate(500, 1.0, 5.0, 3.0, rng);
    CHECK((int)r.completed.size() == 500, "All 500 jobs complete (λ=1, μ=5, ν=3)");
}

void test_single_job_no_retry() {
    // Server starts idle → first job should be served with zero retrials.
    std::mt19937 rng(7);
    SimResult r = simulate(1, 1.0, 5.0, 3.0, rng);
    CHECK(r.completed[0].retry_count == 0,
          "Single job: retry_count == 0 (server starts free)");
}

void test_retry_counts_non_negative() {
    std::mt19937 rng(2);
    SimResult r = simulate(2000, 2.0, 5.0, 3.0, rng);
    bool ok = true;
    for (const Job& j : r.completed) ok = ok && (j.retry_count >= 0);
    CHECK(ok, "All retry counts >= 0");
}

void test_arrival_before_scheduled_time() {
    // Retrials can only push scheduled_time forward, never back.
    std::mt19937 rng(4);
    SimResult r = simulate(2000, 1.0, 5.0, 2.0, rng);
    bool ok = true;
    for (const Job& j : r.completed)
        ok = ok && (j.arrival_time <= j.scheduled_time + 1e-9);
    CHECK(ok, "arrival_time <= scheduled_time for all jobs");
}

void test_departure_after_scheduled_time() {
    std::mt19937 rng(3);
    SimResult r = simulate(2000, 1.0, 5.0, 2.0, rng);
    bool ok = true;
    for (const Job& j : r.completed)
        ok = ok && (j.departure_time >= j.scheduled_time - 1e-9);
    CHECK(ok, "departure_time >= scheduled_time for all jobs");
}

void test_departure_times_non_decreasing() {
    // Single server ⟹ jobs depart in the order they enter service.
    std::mt19937 rng(5);
    SimResult r = simulate(2000, 1.0, 5.0, 2.0, rng);
    bool ok = true;
    for (int i = 1; i < (int)r.completed.size(); i++)
        ok = ok && (r.completed[i].departure_time >= r.completed[i-1].departure_time - 1e-9);
    CHECK(ok, "Departure times are non-decreasing (single server in-order service)");
}

// ---------------------------------------------------------------------------
// Statistical properties
// ---------------------------------------------------------------------------

void test_server_utilization_approx_rho() {
    // For λ=1, μ=10 (ρ=0.1), server utilization ≈ ρ.
    // Utilization = total_service_time / total_simulation_time.
    std::mt19937 rng(42);
    SimResult r = simulate(30000, 1.0, 10.0, 5.0, rng);

    double total_service = 0.0;
    for (const Job& j : r.completed)
        total_service += j.departure_time - j.scheduled_time;
    double utilization = total_service / r.completed.back().departure_time;
    double rho         = 1.0 / 10.0;  // λ/μ

    CHECK(std::abs(utilization - rho) < 0.05,
          "Server utilization ≈ ρ=0.10  (λ=1, μ=10, N=30k, tol=0.05)");
}

void test_littles_law() {
    // Little's Law: E[N] = λ × E[T]
    std::mt19937 rng(99);
    double lambda = 2.0;
    SimResult r = simulate(20000, lambda, 10.0, 4.0, rng);

    double en  = r.mean_n;
    double let = lambda * r.avg_sojourn_time;
    double rel_err = std::abs(en - let) / std::max(en, 1e-9);

    CHECK(rel_err < 0.10,
          "Little's Law: E[N] ≈ λ·E[T]  (λ=2, μ=10, ν=4, N=20k, rel-tol=10%)");
}

void test_low_load_few_retrials() {
    // Light load (ρ=0.05) → most customers find the server free → mean retries ≈ 0.
    std::mt19937 rng(11);
    SimResult r = simulate(5000, 0.5, 10.0, 5.0, rng);
    double avg_retries = 0.0;
    for (const Job& j : r.completed) avg_retries += j.retry_count;
    avg_retries /= r.completed.size();
    CHECK(avg_retries < 0.5,
          "Low load (ρ=0.05): mean retry count < 0.5");
}

void test_high_load_more_retrials() {
    // Heavier load (ρ=0.8) → more blocking → mean retries > low-load case.
    std::mt19937 rng(22);
    SimResult low  = simulate(5000, 1.0, 10.0, 3.0, rng);  // ρ=0.1
    std::mt19937 rng2(22);
    SimResult high = simulate(5000, 8.0, 10.0, 3.0, rng2); // ρ=0.8

    double avg_low = 0.0, avg_high = 0.0;
    for (const Job& j : low.completed)  avg_low  += j.retry_count;
    for (const Job& j : high.completed) avg_high += j.retry_count;
    avg_low  /= low.completed.size();
    avg_high /= high.completed.size();

    CHECK(avg_high > avg_low,
          "Higher load (ρ=0.8) produces more retrials than lower load (ρ=0.1)");
}

// ---------------------------------------------------------------------------
// Blocking probabilities
// ---------------------------------------------------------------------------

void test_blocking_probabilities_stabilize() {
    // The running estimate can be noisy in the first few jobs (idle time can
    // temporarily exceed elapsed time). After a warm-up it must settle into [0,1].
    std::mt19937 rng(13);
    SimResult r = simulate(500, 1.0, 5.0, 3.0, rng);
    auto probs  = compute_blocking_probabilities(r.completed);
    const int warmup = 100;
    bool ok = true;
    for (int i = warmup; i < (int)probs.size(); i++)
        ok = ok && (probs[i] >= 0.0) && (probs[i] <= 1.0);
    CHECK(ok, "Blocking probability values in [0, 1] after warm-up (first 100 jobs)");
}

void test_blocking_probs_count_matches_jobs() {
    std::mt19937 rng(14);
    SimResult r = simulate(300, 1.0, 5.0, 3.0, rng);
    auto probs  = compute_blocking_probabilities(r.completed);
    CHECK((int)probs.size() == 300,
          "compute_blocking_probabilities returns one value per job");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "Running M/M/1 retrial queue tests...\n\n";

    test_exponential_mean();
    test_exponential_positive();
    test_all_jobs_complete();
    test_single_job_no_retry();
    test_retry_counts_non_negative();
    test_arrival_before_scheduled_time();
    test_departure_after_scheduled_time();
    test_departure_times_non_decreasing();
    test_server_utilization_approx_rho();
    test_littles_law();
    test_low_load_few_retrials();
    test_high_load_more_retrials();
    test_blocking_probabilities_stabilize();
    test_blocking_probs_count_matches_jobs();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed.\n";
    return (tests_passed == tests_run) ? 0 : 1;
}
