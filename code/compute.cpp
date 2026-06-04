#include <fstream>
#include <iostream>
#include <random>
#include "simulate.h"

int main() {
    int    n_jobs;
    double arrival_rate, service_rate, retrial_rate;

    std::cout << "Enter number of jobs: ";
    std::cin  >> n_jobs;
    std::cout << "Enter Job Arrival Rate (lambda): ";
    std::cin  >> arrival_rate;
    std::cout << "Enter Job Service Rate (mu): ";
    std::cin  >> service_rate;
    std::cout << "Enter Re-trial Rate (nu): ";
    std::cin  >> retrial_rate;

    std::ofstream file1("file1.txt"), file2("file2.txt"), file3("file3.txt");
    if (!file1 || !file2 || !file3) {
        std::cerr << "Error: could not open output files.\n";
        return 1;
    }

    std::mt19937 rng(std::random_device{}());
    SimResult result = simulate(n_jobs, arrival_rate, service_rate, retrial_rate, rng);
    const auto& completed = result.completed;

    // file1.txt: per-job row — scheduled_time arrival_time departure_time retry_count
    for (const Job& j : completed) {
        file1 << j.scheduled_time << " "
              << j.arrival_time   << " "
              << j.departure_time << " "
              << j.retry_count    << "\n";
    }

    // file2.txt: simulation parameters and summary metrics
    file2 << n_jobs        << " " << arrival_rate  << " "
          << service_rate  << " " << retrial_rate  << "\n";
    file2 << result.avg_wait_time     << " " << result.avg_sojourn_time << "\n";
    file2 << result.avg_idle_interval << "\n";
    file2 << result.mean_n            << "\n";

    // file3.txt: per-job running blocking probability estimate
    std::vector<double> probs = compute_blocking_probabilities(completed);
    for (double p : probs)
        file3 << p << "\n";

    return 0;
}
