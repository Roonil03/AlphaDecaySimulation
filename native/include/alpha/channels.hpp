#pragma once
#include <optional>
#include <string>
#include <vector>
namespace alpha {
// Spins are twice J (integer); parity is +1 or -1. Missing quantum numbers
// require an explicitly supplied orbital angular momentum.
struct SpinParity { int twice_j; int parity; };
struct ChannelInput {
    std::string name;
    double excitation_mev = 0;
    std::optional<SpinParity> daughter;
    std::optional<int> l;
    double preformation = 0.1;
    double assault_hz = 1e21;
};
struct ChannelSelection { std::string status; double q_mev = 0; std::optional<int> l; };
ChannelSelection select_channel(double ground_q_mev, std::optional<SpinParity> parent, const ChannelInput&);
struct ChannelRate { std::string status; std::optional<double> log10_rate_hz, log10_partial_half_life_s; double conditional_alpha_branch = 0; std::optional<double> total_branch; };
// log10 penetrabilities are supplied by the barrier model; null denotes no rate.
std::vector<ChannelRate> channel_rates(const std::vector<ChannelInput>&, const std::vector<ChannelSelection>&, const std::vector<std::optional<double>>&, std::optional<double> competing_rate_hz = std::nullopt);
}
