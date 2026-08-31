#include "Pin.h"
#include "card.h"

#include <algorithm>
#include <cstdint>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

int card_index(const card& value) {
    switch (value.rank) {
        case 9: return value.suit * 6;
        case 11: return value.suit * 6 + 1;
        case 12: return value.suit * 6 + 2;
        case 13: return value.suit * 6 + 3;
        case 14: return value.suit * 6 + 4;
        case 15: return value.suit * 6 + 5;
        default: return -1;
    }
}

py::tuple training_state(Pin& game, int player) {
    constexpr int observation_size = 12 * 24 + 4 * 24 + 4 + 3;
    constexpr int trick_offset = 12 * 24;
    constexpr int context_offset = trick_offset + 4 * 24;
    auto observation = py::array_t<std::int8_t>(observation_size);
    auto action_mask = py::array_t<bool>(17);
    std::fill_n(observation.mutable_data(), observation_size, 0);
    std::fill_n(action_mask.mutable_data(), 17, false);

    const std::vector<card>& hand = game.training_player_hand(player);
    for (std::size_t slot = 0; slot < hand.size() && slot < 12; ++slot) {
        const int index = card_index(hand[slot]);
        if (index >= 0) {
            observation.mutable_data()[slot * 24 + index] = 1;
        }
    }
    const std::vector<card>& trick = game.training_trick();
    for (std::size_t slot = 0; slot < trick.size() && slot < 4; ++slot) {
        const int index = card_index(trick[slot]);
        if (index >= 0) {
            observation.mutable_data()[trick_offset + slot * 24 + index] = 1;
        }
    }

    const int trump = game.training_trump();
    const int phase = game.training_phase();
    const int current_player = game.training_current_player();
    const int us_points = game.training_us_points();
    const int them_points = game.training_them_points();
    if (trump >= 0) {
        observation.mutable_data()[context_offset + trump] = 1;
    }
    observation.mutable_data()[context_offset + 4] = phase == 0;
    observation.mutable_data()[context_offset + 5] = current_player == player;
    const bool our_team = player == 0 || player == 2;
    observation.mutable_data()[context_offset + 6] = our_team
        ? us_points >= them_points
        : them_points >= us_points;
    for (const int action : game.legal_training_actions()) {
        action_mask.mutable_data()[action] = true;
    }
    return py::make_tuple(
        observation, action_mask, phase, current_player, trump, us_points, them_points
    );
}

} // namespace

PYBIND11_MODULE(pinochle_cpp, module) {
    module.doc() = "Non-interactive Pinochle training interface";

    py::class_<card>(module, "Card")
        .def(py::init<int, int, int>(), py::arg("rank"), py::arg("suit"),
             py::arg("copy") = 0)
        .def_readonly("rank", &card::rank)
        .def_readonly("suit", &card::suit)
        .def_readonly("copy", &card::mult)
        .def("rank_name", &card::p_rank)
        .def("suit_name", &card::p_suit);

    py::class_<Pin>(module, "PinochleGame")
        .def(py::init<>())
        .def("reset", &Pin::reset_training, py::arg("seed") = 0)
        .def("training_state", &training_state, py::arg("player"))
        .def("legal_actions", &Pin::legal_training_actions)
        .def("step", [](Pin& game, int action) {
            const TrainingStep result = game.step_training(action);
            return py::make_tuple(result.reward, result.terminated);
        })
        .def_property_readonly("hand", &Pin::training_hand,
                               py::return_value_policy::reference_internal)
        .def("player_hand", &Pin::training_player_hand, py::arg("player"),
             py::return_value_policy::reference_internal)
        .def_property_readonly("trick", &Pin::training_trick,
                               py::return_value_policy::reference_internal)
        .def_property_readonly("trump", &Pin::training_trump)
        .def_property_readonly("phase", &Pin::training_phase)
        .def_property_readonly("current_player", &Pin::training_current_player)
        .def_property_readonly("us_points", &Pin::training_us_points)
        .def_property_readonly("them_points", &Pin::training_them_points);
}
